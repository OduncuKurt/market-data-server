cmake_minimum_required(VERSION 3.16)

project(
    MarketDataServer
    VERSION 1.0.0
    LANGUAGES CXX
)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(
    market_server
    src/server.cpp
    src/MarketDataGenerator.cpp
)

add_executable(
    market_client
    src/client.cpp
)

target_include_directories(
    market_server
    PRIVATE
        include
)

target_compile_options(
    market_server
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

target_compile_options(
    market_client
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

target_link_libraries(
    market_server
    PRIVATE
        Threads::Threads
)

target_link_libraries(
    market_client
    PRIVATE
        Threads::Threads
)
