cmake_minimum_required(VERSION 3.12)
project(chu_pico C CXX ASM)

# Pull in SDK (must set be before project)
include(pico_sdk_import.cmake)

set(CMAKE_C_STANDARD 11)

pico_sdk_init()

add_subdirectory(modules/aic_pico/firmware aic)
include_directories(modules/aic_pico/firmware/include)

add_subdirectory(src)
