cmake_minimum_required(VERSION 3.16)

project(Lab3LLVMCompiler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ZLIB REQUIRED)
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_executable(lab3
    src/main.cpp
)

llvm_map_components_to_libnames(LLVM_LIBS
    Core
    Support
)

target_link_libraries(lab3
    ${LLVM_LIBS}
    ZLIB::ZLIB
)