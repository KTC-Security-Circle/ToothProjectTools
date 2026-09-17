add_library(projector_service STATIC
  projector_result.cpp
  projector_service.cpp
)

target_include_directories(projector_service PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(projector_service PUBLIC window_service monitor_service structured_light)
target_compile_features(projector_service PUBLIC cxx_std_20)
