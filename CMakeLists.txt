cmake_minimum_required(VERSION 3.10)
project(picohttp
    VERSION 0.1.0
    LANGUAGES C CXX)

# Configure project for C++20 support
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Some project options
option(ENABLE_UNIT_TESTS    "Enable building of unit tests" ON)
option(ENABLE_BENCHMARK     "Enable building benchmark" ON)

add_library(picohttp STATIC
        parser.c
        parser.h
        parser.hpp)

include(CMakePackageConfigHelpers)

set(GENERATED_DIR  ${CMAKE_BINARY_DIR}/generated)
set(VERSION_CONFIG "${GENERATED_DIR}/${PROJECT_NAME}ConfigVersion.cmake")
set(PROJECT_CONFIG "${GENERATED_DIR}/${PROJECT_NAME}Config.cmake")
set(TARGETS_EXPORT_NAME "${PROJECT_NAME}Targets")
set(NAMESPACE "${PROJECT_NAME}::")

# Configure '<PROJECT-NAME>Config.cmake'
configure_package_config_file(
        "cmake/Config.cmake.in"
        "${PROJECT_CONFIG}"
        INSTALL_DESTINATION lib/cmake/${PROJECT_NAME})

# Configure '<PROJECT-NAME>ConfigVersion.cmake'
write_basic_package_version_file(
        "${VERSION_CONFIG}" COMPATIBILITY SameMajorVersion)

# Install library and include files
install(TARGETS picohttp
        EXPORT   "${TARGETS_EXPORT_NAME}"
        LIBRARY  DESTINATION lib
        ARCHIVE  DESTINATION lib
        INCLUDES DESTINATION include)

install(FILES parser.h
        DESTINATION include/picohttp)

install(FILES "${PROJECT_CONFIG}" "${VERSION_CONFIG}"
        DESTINATION "lib/cmake/${PROJECT_NAME}")

install(EXPORT "${TARGETS_EXPORT_NAME}"
        NAMESPACE "${namespace}"
        DESTINATION "lib/cmake/${PROJECT_NAME}")

if (ENABLE_UNIT_TESTS)
    include(3rdParty.cmake)

    add_executable(picohttp-test
            parser.c
            test/test.cpp
            test/main.cpp)
    add_dependencies(picohttp-test catch)
endif()

if (ENABLE_BENCHMARK)
    add_executable(picohttp-bench
            bench.cpp)
    target_link_libraries(picohttp-bench picohttp)
endif()