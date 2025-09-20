cmake_minimum_required(VERSION 3.13)

project(codecrafters-git CXX) # Add CXX to specify the language

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find the dependencies. Using COMPONENTS ensures we get what we need.
find_package(OpenSSL REQUIRED COMPONENTS Crypto)
find_package(ZLIB REQUIRED)

# Use target_sources for a more modern approach.
add_executable(git)
target_sources(git PRIVATE
    src/main.cpp
    src/lib/commands.cpp
    src/lib/entry.cpp
    src/lib/object_store.cpp
    src/lib/zlib_codec.cpp
)

# Set C++ standard and options on the target
target_compile_features(git PRIVATE cxx_std_20) # Explicitly set C++17

target_include_directories(git PRIVATE src src/lib) # Add src/ for your hpp files

target_link_libraries(git PRIVATE OpenSSL::Crypto ZLIB::ZLIB)
