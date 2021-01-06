cmake_minimum_required(VERSION 3.10)
project(picohttpparser
    VERSION 0.1.0
    LANGUAGES C CXX)

# Configure project for C++20 support
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Some project options
option(ENABLE_UNIT_TESTS    "Enable building of unit tests" ON)

add_library(picohttpparser STATIC
        picohttpparser.c)
