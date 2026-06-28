cmake_minimum_required(VERSION 3.16)

project(FlashDB LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(flashdb
    src/main.cpp
    src/RedisServer.cpp
    src/RedisCommandHandler.cpp
    src/RedisDatabase.cpp
)

target_include_directories(flashdb PRIVATE include)

if (WIN32)
    target_link_libraries(flashdb PRIVATE ws2_32)
endif()
