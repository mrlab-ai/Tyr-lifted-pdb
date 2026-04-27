
add_executable(successor_generator
    "successor_generator.cpp"
    "../../json_loader.cpp"
)
target_link_libraries(successor_generator
    PRIVATE 
        tyr::core 
        benchmark::benchmark 
        Boost::json
)

set(SUCCESSOR_GENERATOR_CONFIG
    "${CMAKE_CURRENT_SOURCE_DIR}/successor_generator.json"
)

file(READ "${SUCCESSOR_GENERATOR_CONFIG}" SUCCESSOR_GENERATOR_CONFIG_JSON)
string(JSON num_configs LENGTH "${SUCCESSOR_GENERATOR_CONFIG_JSON}" "cases")

if(num_configs GREATER 0)
    math(EXPR last_config "${num_configs} - 1")

    foreach(config_index RANGE 0 ${last_config})
        string(JSON run_name MEMBER "${SUCCESSOR_GENERATOR_CONFIG_JSON}" "cases" ${config_index})

        add_test(
            NAME "${run_name}"
            COMMAND successor_generator
                "--benchmark_filter=^${run_name}/.*"
                --benchmark_min_time=0.1s
        )

        set_tests_properties("${run_name}"
            PROPERTIES TIMEOUT 1
        )
    endforeach()
endif()
