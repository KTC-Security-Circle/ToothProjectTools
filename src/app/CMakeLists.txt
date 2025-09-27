add_executable(structured_light_decoder
    main.cpp
    app_core.cpp
    app_dispatch.cpp
    app_mouse.cpp
)

target_include_directories(structured_light_decoder
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)


target_link_libraries(structured_light_decoder
    PUBLIC
        window
        input
        cv_lib
        cmd
    PRIVATE
        logger
)
