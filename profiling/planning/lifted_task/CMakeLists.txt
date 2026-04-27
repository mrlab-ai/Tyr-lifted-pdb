
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

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SUCCESSOR_GENERATOR_CONFIG}")
file(READ "${SUCCESSOR_GENERATOR_CONFIG}" SUCCESSOR_GENERATOR_CONFIG_JSON)
string(JSON num_domains LENGTH "${SUCCESSOR_GENERATOR_CONFIG_JSON}" "domains")

if(num_domains GREATER 0)
    math(EXPR last_domain "${num_domains} - 1")

    foreach(domain_index RANGE 0 ${last_domain})
        string(JSON domain_name MEMBER "${SUCCESSOR_GENERATOR_CONFIG_JSON}" "domains" ${domain_index})
        string(JSON num_tasks LENGTH "${SUCCESSOR_GENERATOR_CONFIG_JSON}" "domains" "${domain_name}" "tasks")

        if(num_tasks GREATER 0)
            math(EXPR last_task "${num_tasks} - 1")

            foreach(task_index RANGE 0 ${last_task})
                string(JSON task_name MEMBER "${SUCCESSOR_GENERATOR_CONFIG_JSON}" "domains" "${domain_name}" "tasks" ${task_index})
                set(run_name "${domain_name}/${task_name}")

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
    endforeach()
endif()
