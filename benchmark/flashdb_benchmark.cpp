#include "../include/RedisCommandHandler.h"
#include "../include/RedisDatabase.h"

#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string resp(const std::vector<std::string>& tokens) {
    std::string out = "*" + std::to_string(tokens.size()) + "\r\n";
    for (const auto& token : tokens) {
        out += "$" + std::to_string(token.size()) + "\r\n" + token + "\r\n";
    }
    return out;
}

void runBenchmark(const std::string& name, const std::function<void()>& fn, int iterations) {
    using clock = std::chrono::steady_clock;
    const auto start = clock::now();
    for (int i = 0; i < iterations; ++i) {
        fn();
    }
    const auto end = clock::now();
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    const double perOp = static_cast<double>(micros) / static_cast<double>(iterations);
    const double opsPerSec = 1000000.0 / perOp;
    std::cout << name << ": " << iterations << " ops, " << micros << " us total, " << perOp << " us/op, " << opsPerSec << " ops/s\n";
}

} // namespace

int main() {
    RedisCommandHandler handler;
    auto& db = RedisDatabase::getInstance();
    db.flushAll();

    runBenchmark("SET via command handler", [&]() {
        handler.processCommand(resp({"SET", "bench:key", "value"}));
    }, 50000);

    runBenchmark("GET via command handler", [&]() {
        handler.processCommand(resp({"GET", "bench:key"}));
    }, 50000);

    runBenchmark("LPUSH/LLEN combined", [&]() {
        handler.processCommand(resp({"LPUSH", "bench:list", "x"}));
        handler.processCommand(resp({"LLEN", "bench:list"}));
    }, 20000);

    runBenchmark("HSET/HGET combined", [&]() {
        handler.processCommand(resp({"HSET", "bench:hash", "field", "value"}));
        handler.processCommand(resp({"HGET", "bench:hash", "field"}));
    }, 20000);

    return 0;
}