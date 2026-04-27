
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
