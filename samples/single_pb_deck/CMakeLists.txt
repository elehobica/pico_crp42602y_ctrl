cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
#include($ENV{PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)

set(project_name "single_pb_deck" C CXX ASM)
project(${project_name})
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_subdirectory(../.. pico_crp42602y_ctrl)
add_subdirectory(../lib/pico_buttons pico_buttons)
add_subdirectory(../lib/pico-ssd1306 ssd1306)

add_executable(${PROJECT_NAME}
    main.cpp
    eq_nr.cpp
)

# pull in common dependencies
target_link_libraries(${PROJECT_NAME}
    pico_multicore
    pico_stdlib
    pico_buttons
    pico_crp42602y_ctrl
    ssd1306
)

# create map/bin/hex file etc.
pico_add_extra_outputs(${PROJECT_NAME})
