cmake_minimum_required(VERSION 3.13)

project(codecrafters-git)

set(CMAKE_CXX_STANDARD 23) # Enable the C++23 standard
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCE_FILES src/*.cpp src/*.hpp)

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(git ${SOURCE_FILES})


target_include_directories(git PRIVATE src/lib)

target_link_libraries(git PRIVATE OpenSSL::Crypto ZLIB::ZLIB)
