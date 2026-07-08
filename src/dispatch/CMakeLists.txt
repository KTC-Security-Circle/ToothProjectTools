add_library(dispatch
    dispatch_handlers.cpp
    dispatch_router.cpp
)

target_include_directories(dispatch PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(dispatch
    PUBLIC
        window
        input
        cv_lib
        cmd
        video
        structured_light
        calibration
        runtime
        common
    PRIVATE
        handler
        logger
)
