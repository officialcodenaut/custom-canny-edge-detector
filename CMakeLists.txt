cmake_minimum_required(VERSION 3.20)

project(CustomCanny LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

add_executable(custom_canny main.cpp)

target_link_libraries(custom_canny PRIVATE ${OpenCV_LIBS})
