add_executable(structured_light_decoder
    main.cpp
    systems/sys_init.cpp
    systems/sys_update.cpp
    systems/sys_input.cpp
    systems/sys_render.cpp
    app.cpp
    dispatch/dispatch_handlers.cpp
    dispatch/dispatch_router.cpp
    dispatch/logic/reconstruct.cpp
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
        video
        structured_light
        calibration
    PRIVATE
        logger
)
