cmake_minimum_required(VERSION 3.10)
project(RankEvolution)

set(CMAKE_CXX_STANDARD 17)

find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Python3 COMPONENTS Development REQUIRED)
include_directories(${Python3_INCLUDE_DIRS})

add_executable(RankEvolution main.cpp)
target_link_libraries(RankEvolution CURL::libcurl nlohmann_json::nlohmann_json ${Python3_LIBRARIES})
