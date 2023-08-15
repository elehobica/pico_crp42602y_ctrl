cmake_minimum_required(VERSION 3.12)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
#include($ENV{PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)

set(project_name "pico_crp42602y_ctrl" C CXX ASM)
project(${project_name})
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_subdirectory(pico_buttons buttons)

add_executable(${PROJECT_NAME}
    main.cpp
)

# pull in common dependencies
target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    hardware_pwm
    buttons
)

# create map/bin/hex file etc.
pico_add_extra_outputs(${PROJECT_NAME})
