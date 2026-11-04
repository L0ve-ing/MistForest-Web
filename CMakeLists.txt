cmake_minimum_required(VERSION 3.16)
project(MistForest VERSION 1.1.8 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
find_package(Threads REQUIRED)

# Build the current development version. Historical versions remain archived.
add_subdirectory(src)
