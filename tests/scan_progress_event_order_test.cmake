file(READ "${SCAN_SERVICE_SOURCE}" scan_service_source)

string(FIND "${scan_service_source}"
       "const auto show_result = projector_service_.showPattern(config.projector_role, index);"
       show_pattern_position)
if(show_pattern_position EQUAL -1)
    message(FATAL_ERROR "scan loop showPattern call was not found")
endif()

string(SUBSTRING "${scan_service_source}" ${show_pattern_position} -1 scan_loop_tail)
string(FIND "${scan_loop_tail}" "pushEvent(\"scan_pattern_shown\"" pattern_shown_position)
string(FIND "${scan_loop_tail}" "photodiode_source->waitForTransition(expected" photodiode_wait_position)
string(FIND "${scan_loop_tail}" "pushEvent(\"scan_frame_selected\"" frame_selected_position)
string(FIND "${scan_service_source}" "pushEvent(\"scan_frame_captured\"" frame_captured_position)
string(FIND "${scan_loop_tail}" "save_queue.enqueue" save_enqueue_position)

if(pattern_shown_position EQUAL -1 OR
   photodiode_wait_position EQUAL -1 OR
   frame_selected_position EQUAL -1 OR frame_captured_position EQUAL -1 OR save_enqueue_position EQUAL -1)
    message(FATAL_ERROR "scan progress event sequence is incomplete")
endif()
if(NOT pattern_shown_position LESS photodiode_wait_position OR
   NOT photodiode_wait_position LESS frame_selected_position OR
   NOT frame_selected_position LESS save_enqueue_position)
    message(FATAL_ERROR
            "expected scan_pattern_shown -> photodiode wait -> scan_frame_selected -> save enqueue")
endif()

string(SUBSTRING "${scan_loop_tail}" ${pattern_shown_position} 500 pattern_shown_event)
foreach(required_field scan_id pattern_index pattern_count pattern_kind expected_marker_state)
    string(FIND "${pattern_shown_event}" "{\"${required_field}\"" field_position)
    if(field_position EQUAL -1)
        message(FATAL_ERROR "scan_pattern_shown is missing ${required_field}")
    endif()
endforeach()

string(FIND "${scan_service_source}" "item.saved_images == images_per_pattern" stereo_complete_position)
string(FIND "${scan_service_source}" "save_queue.closeAndWait();" drain_position)
string(FIND "${scan_service_source}" "pushEvent(\"scan_completed\"" scan_completed_position)
string(FIND "${scan_service_source}" "pushEvent(\"scan_failed\"" scan_failed_position)
if(stereo_complete_position EQUAL -1 OR drain_position EQUAL -1 OR
   scan_completed_position EQUAL -1 OR scan_failed_position EQUAL -1)
    message(FATAL_ERROR "save completion/failure handling is incomplete")
endif()
if(NOT drain_position LESS scan_completed_position)
    message(FATAL_ERROR "scan_completed must follow save queue drain")
endif()
