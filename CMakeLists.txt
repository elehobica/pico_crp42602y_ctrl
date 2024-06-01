if (NOT TARGET pico_crp42602y_ctrl)
    add_library(pico_crp42602y_ctrl INTERFACE)

    pico_generate_pio_header(pico_crp42602y_ctrl ${CMAKE_CURRENT_LIST_DIR}/crp42602y_measure_pulse.pio)

    target_sources(pico_crp42602y_ctrl INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/crp42602y_ctrl.cpp
        ${CMAKE_CURRENT_LIST_DIR}/crp42602y_counter.cpp
    )

    target_include_directories(pico_crp42602y_ctrl INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
    )

    target_link_libraries(pico_crp42602y_ctrl INTERFACE
        pico_stdlib
        hardware_pio
    )
endif()
