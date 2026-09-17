add_library(calibration
    calibrator.cpp
    stereo_calibrator.cpp
    projector_calibrator.cpp
)

target_include_directories(calibration PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(calibration
     PUBLIC
        cv_lib
        reconstruction
    PRIVATE
        logger
)

add_library(calibration_service STATIC
    atomic_calibration_file.cpp
    calibration_file.cpp
    calibration_service.cpp
    stereo_calibration_service.cpp
)

target_include_directories(calibration_service PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(calibration_service
    PUBLIC cmd_commands capture calibration video
    PRIVATE cv_lib logger
)
