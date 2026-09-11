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
