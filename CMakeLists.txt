if (NOT TARGET crp42602y_ctrl)
    add_library(crp42602y_ctrl INTERFACE)

    target_sources(crp42602y_ctrl INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/crp42602y_ctrl.cpp
    )

    target_include_directories(crp42602y_ctrl INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
    )

    target_link_libraries(crp42602y_ctrl INTERFACE
        pico_stdlib
        hardware_pwm
    )
endif()
