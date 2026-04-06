cmake_minimum_required(VERSION 3.21)
project(InstallTeddy)

include(ExternalProject)

message(STATUS "Dependency \"teddy\"...")

# Attempt to find the installed teddy library
find_package(teddy QUIET PATHS ${CMAKE_INSTALL_PREFIX} NO_DEFAULT_PATH)

if (NOT teddy_FOUND)


    list(APPEND CMAKE_ARGS
            -DCMAKE_BUILD_TYPE=${BUILD_TYPE}
            -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
            -DCMAKE_PREFIX_PATH=${CMAKE_INSTALL_PREFIX}
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    )

    message(STATUS "Preparing external project \"teddy\" with args:")
    foreach(CMAKE_ARG ${CMAKE_ARGS})
        message(STATUS "-- ${CMAKE_ARG}")
    endforeach()
    ExternalProject_Add(
        teddy
        GIT_REPOSITORY https://github.com/MichalMrena/DecisionDiagrams.git
        GIT_TAG v4.1.0
        PREFIX ${CMAKE_BINARY_DIR}/teddy
        CMAKE_ARGS ${CMAKE_ARGS}
    )
else()
    message(STATUS "teddy is already installed.")
endif()