find_package(benchmark "1.8.3" REQUIRED benchmark Main PATHS ${CMAKE_PREFIX_PATH} NO_DEFAULT_PATH)
# Set result variables
find_package(benchmark)


add_executable(projection_generator "projection_generator.cpp")
target_link_libraries(projection_generator 
    PRIVATE 
        tyr::core 
        benchmark::benchmark 
        Boost::json
)
