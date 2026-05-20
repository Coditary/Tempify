cmake_minimum_required(VERSION 3.20)

project(snapshot-app VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(snapshot-app src/main.cpp)
