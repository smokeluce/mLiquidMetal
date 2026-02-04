
cmake_minimum_required(VERSION 3.10)
project(mLiquidMetal CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(raylib REQUIRED)

add_executable(mLiquidMetal src/main.cpp)
target_link_libraries(mLiquidMetal raylib)