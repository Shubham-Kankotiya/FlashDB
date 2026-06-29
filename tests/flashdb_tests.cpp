#include "../include/RedisCommandHandler.h"
#include "../include/RedisDatabase.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

struct TestCase {
    const char* name;
    std::function<void()> fn;
};

int failures = 0;

void expectTrue(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAILED: " << message << '\n';
    }
}

void expectEq(const std::string& actual, const std::string& expected, const std::string& message) {
    if (actual != expected) {
        ++failures;
        std::cerr << "FAILED: " << message << "\n  expected: " << expected << "\n  actual:   " << actual << '\n';
    }
}

std::string resp(const std::vector<std::string>& tokens) {
    std::string out = "*" + std::to_string(tokens.size()) + "\r\n";
    for (const auto& token : tokens) {
        out += "$" + std::to_string(token.size()) + "\r\n" + token + "\r\n";
    }
    return out;
}

std::filesystem::path tempFile(const std::string& name) {
    return std::filesystem::temp_directory_path() / name;
}

void testPingAndSetGet() {
    RedisCommandHandler handler;
    auto& db = RedisDatabase::getInstance();
    db.flushAll();

    expectEq(handler.processCommand(resp({"PING"})), "+PONG\r\n", "PING response");
    expectEq(handler.processCommand(resp({"SET", "name", "Alice"})), "+OK\r\n", "SET response");
    expectEq(handler.processCommand(resp({"GET", "name"})), "$5\r\nAlice\r\n", "GET response");
    expectEq(handler.processCommand(resp({"TYPE", "name"})), "+string\r\n", "TYPE for string key");
}

void testListOperations() {
    RedisCommandHandler handler;
    auto& db = RedisDatabase::getInstance();
    db.flushAll();

    expectEq(handler.processCommand(resp({"RPUSH", "letters", "a", "b", "c"})), ":3\r\n", "RPUSH length");
    expectEq(handler.processCommand(resp({"LPUSH", "letters", "start"})), ":4\r\n", "LPUSH length");
    expectEq(handler.processCommand(resp({"LLEN", "letters"})), ":4\r\n", "LLEN response");
    expectEq(handler.processCommand(resp({"LINDEX", "letters", "1"})), "$1\r\na\r\n", "LINDEX positive index");
    expectEq(handler.processCommand(resp({"LINDEX", "letters", "-1"})), "$1\r\nc\r\n", "LINDEX negative index");
    expectEq(handler.processCommand(resp({"LPOP", "letters"})), "$5\r\nstart\r\n", "LPOP response");
    expectEq(handler.processCommand(resp({"RPOP", "letters"})), "$1\r\nc\r\n", "RPOP response");
    expectEq(handler.processCommand(resp({"LSET", "letters", "1", "beta"})), "+OK\r\n", "LSET response");
    expectEq(handler.processCommand(resp({"LGET", "letters"})), "*2\r\n$1\r\na\r\n$4\r\nbeta\r\n", "LGET response");
}

void testHashOperations() {
    RedisCommandHandler handler;
    auto& db = RedisDatabase::getInstance();
    db.flushAll();

    expectEq(handler.processCommand(resp({"HSET", "user:1", "name", "Alice"})), ":1\r\n", "HSET response");
    expectEq(handler.processCommand(resp({"HGET", "user:1", "name"})), "$5\r\nAlice\r\n", "HGET response");
    expectEq(handler.processCommand(resp({"HEXISTS", "user:1", "name"})), ":1\r\n", "HEXISTS existing field");
    expectEq(handler.processCommand(resp({"HLEN", "user:1"})), ":1\r\n", "HLEN response");
    expectEq(handler.processCommand(resp({"HMSET", "user:1", "age", "30", "city", "Berlin"})), "+OK\r\n", "HMSET response");
    expectEq(handler.processCommand(resp({"HDEL", "user:1", "age"})), ":1\r\n", "HDEL response");
}

void testDeleteRenameExpirePersistence() {
    RedisCommandHandler handler;
    auto& db = RedisDatabase::getInstance();
    db.flushAll();

    expectEq(handler.processCommand(resp({"SET", "temp", "value"})), "+OK\r\n", "SET for delete");
    expectEq(handler.processCommand(resp({"DEL", "temp"})), ":1\r\n", "DEL returns deleted count");
    expectEq(handler.processCommand(resp({"GET", "temp"})), "$-1\r\n", "GET after DEL");

    expectEq(handler.processCommand(resp({"SET", "old", "value"})), "+OK\r\n", "SET for rename");
    expectEq(handler.processCommand(resp({"RENAME", "old", "new"})), "+OK\r\n", "RENAME response");
    expectEq(handler.processCommand(resp({"GET", "new"})), "$5\r\nvalue\r\n", "GET after RENAME");

    expectEq(handler.processCommand(resp({"SET", "shortlived", "x"})), "+OK\r\n", "SET for expire");
    expectEq(handler.processCommand(resp({"EXPIRE", "shortlived", "1"})), "+OK\r\n", "EXPIRE response");
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    expectEq(handler.processCommand(resp({"GET", "shortlived"})), "$-1\r\n", "GET after expiry");

    auto dumpPath = tempFile("flashdb_tests_dump.rdb");
    expectTrue(db.dump(dumpPath.string()), "dump should succeed");
    db.flushAll();
    expectTrue(db.load(dumpPath.string()), "load should succeed");
    expectEq(handler.processCommand(resp({"GET", "new"})), "$5\r\nvalue\r\n", "GET after load");
    std::filesystem::remove(dumpPath);
}

void testKeysAndFlush() {
    RedisCommandHandler handler;
    auto& db = RedisDatabase::getInstance();
    db.flushAll();

    handler.processCommand(resp({"SET", "a", "1"}));
    handler.processCommand(resp({"LPUSH", "list", "x"}));
    handler.processCommand(resp({"HSET", "hash", "field", "value"}));

    auto keys = handler.processCommand(resp({"KEYS"}));
    expectTrue(keys.find("a") != std::string::npos, "KEYS contains string key");
    expectTrue(keys.find("list") != std::string::npos, "KEYS contains list key");
    expectTrue(keys.find("hash") != std::string::npos, "KEYS contains hash key");

    expectEq(handler.processCommand(resp({"FLUSHALL"})), "+OK\r\n", "FLUSHALL response");
    expectEq(handler.processCommand(resp({"GET", "a"})), "$-1\r\n", "GET after FLUSHALL");
}

} // namespace

int main() {
    const std::vector<TestCase> tests = {
        {"ping/set/get", testPingAndSetGet},
        {"list operations", testListOperations},
        {"hash operations", testHashOperations},
        {"delete/rename/expire/persistence", testDeleteRenameExpirePersistence},
        {"keys/flush", testKeysAndFlush},
    };

    for (const auto& test : tests) {
        const auto before = failures;
        test.fn();
        if (failures == before) {
            std::cout << "PASS " << test.name << '\n';
        }
    }

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return 1;
    }

    std::cout << "All tests passed\n";
    return 0;
}