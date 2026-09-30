cmake_minimum_required(VERSION 3.15)

project(image_filter VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES src/main.cpp)

add_executable(top_hat_image_filter ${SOURCES})

target_include_directories(top_hat_image_filter PRIVATE src)

target_compile_features(top_hat_image_filter PRIVATE cxx_std_20)
