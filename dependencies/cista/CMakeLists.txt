cmake_minimum_required(VERSION 3.21)
project(InstallCista)

include(ExternalProject)

message(STATUS "Dependency \"cista\"...")

# Attempt to find the installed loki library
find_package(cista QUIET PATHS ${CMAKE_INSTALL_PREFIX} NO_DEFAULT_PATH)

if (NOT cista_FOUND)


    list(APPEND CMAKE_ARGS
            -DCMAKE_BUILD_TYPE=${BUILD_TYPE}
            -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
            -DCMAKE_PREFIX_PATH=${CMAKE_INSTALL_PREFIX}
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            -DCISTA_INSTALL=ON
    )

    message(STATUS "Preparing external project \"cista\" with args:")
    foreach(CMAKE_ARG ${CMAKE_ARGS})
        message(STATUS "-- ${CMAKE_ARG}")
    endforeach()
    ExternalProject_Add(
        cista
        GIT_REPOSITORY git@github.com:drexlerd/cista.git
        GIT_TAG db34b0a1bbef1f5166bbf360c05fbbd79fe0596e
        PREFIX ${CMAKE_BINARY_DIR}/cista
        CMAKE_ARGS ${CMAKE_ARGS}
    )
else()
    message(STATUS "cista is already installed.")
endif()