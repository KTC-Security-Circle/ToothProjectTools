add_library(scan_service STATIC
  scan_result.cpp
  scan_event.cpp
  camera_roi_sync_tracker.cpp
  scan_service.cpp
)

target_include_directories(scan_service PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(scan_service PUBLIC projector_service camera_service capture structured_light PRIVATE logger)
target_compile_features(scan_service PUBLIC cxx_std_20)

add_library(scan_dataset_service STATIC
  scan_dataset_result.cpp
  scan_dataset_resolver.cpp
  scan_dataset_validator.cpp
)

target_include_directories(scan_dataset_service PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(scan_dataset_service PUBLIC cv_lib)
target_compile_features(scan_dataset_service PUBLIC cxx_std_20)
