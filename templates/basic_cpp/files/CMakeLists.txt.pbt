cmake_minimum_required(VERSION 3.20)

project({{ project_slug }} VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable({{ project_slug }} src/main.cpp)
