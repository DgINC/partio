cmake_minimum_required(VERSION 3.10...4.3)

project(partio)

include(FetchContent)

FetchContent_Declare(
        argparse
        GIT_REPOSITORY https://github.com/p-ranav/argparse.git
        GIT_TAG        v3.2  # Can be a tag, branch, or commit hash
)

FetchContent_Declare(
        stduuid
        GIT_REPOSITORY https://github.com/mariusbancila/stduuid.git
        GIT_TAG        v1.2.3  # Can be a tag, branch, or commit hash
)

FetchContent_Declare(
        tomlplusplus_tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG        v3.4.0  # Can be a tag, branch, or commit hash
)

FetchContent_Declare(
        antlr4_runtime
        GIT_REPOSITORY https://github.com/antlr/antlr4.git
        GIT_TAG        4.13.2 # Use a specific version tag
        SOURCE_SUBDIR  runtime/Cpp
)

set(ANTLR4_INSTALL ON CACHE BOOL "" FORCE)
set(WITH_LIBCXX OFF CACHE BOOL "" FORCE)
set(ANTLR_BUILD_CPP_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(argparse)
FetchContent_MakeAvailable(stduuid)
FetchContent_MakeAvailable(tomlplusplus_tomlplusplus)
FetchContent_MakeAvailable(antlr4_runtime)

file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB GENERATED_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/gen/*.cpp")

add_executable(partio ${SOURCES} ${GENERATED_SOURCES})

target_include_directories(partio PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/gen"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories(partio PRIVATE ${antlr4_cpp_runtime_SOURCE_DIR}/runtime/Cpp/runtime)

set_target_properties(partio PROPERTIES
        CXX_STANDARD 26          # Options: 98, 11, 14, 17, 20, 23, 26
        CXX_STANDARD_REQUIRED ON # Fail if the compiler doesn't support it
        CXX_EXTENSIONS OFF       # Use -std=c++17 instead of -std=gnu++17
)

target_link_libraries(partio PRIVATE argparse)
target_link_libraries(partio PRIVATE stduuid)
target_link_libraries(partio PRIVATE tomlplusplus_tomlplusplus)
target_link_libraries(partio PRIVATE antlr4_static)
