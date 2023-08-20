set(library_name "ssd1306")
if (NOT TARGET ${library_name})
    add_library(${library_name} INTERFACE)

    target_sources(${library_name} INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/src/ssd1306.c
    )

    target_include_directories(${library_name} INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/src
    )

    target_link_libraries(${library_name} INTERFACE
        pico_stdlib
        hardware_i2c
    )
endif()