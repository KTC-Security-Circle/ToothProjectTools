add_executable(structured_light_decoder
    main.cpp
)

target_link_libraries(structured_light_decoder
    PRIVATE
        logger
)
