add_library(capture STATIC
    capture_service.cpp
)

target_include_directories(capture
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(capture
  PUBLIC
    video
  PRIVATE
    cv_lib
    logger
)
