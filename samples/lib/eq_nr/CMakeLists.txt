if (NOT TARGET eq_nr)
    add_library(eq_nr INTERFACE)

    target_sources(eq_nr INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/eq_nr.cpp
    )

    target_include_directories(eq_nr INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
    )

    target_link_libraries(eq_nr INTERFACE
        pico_stdlib
    )
endif()