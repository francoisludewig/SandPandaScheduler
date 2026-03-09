cmake_minimum_required(VERSION 3.5.0)

project(SandPandaSchedule VERSION 1.0.0)

set(SOURCES_FILES
        Sources/Configuration/Option.cpp
        Sources/main.cpp
        Sources/Repository/SimulationRepository.cpp
        Sources/Model/SimulationScheduler.cpp
)

set(INCLUDES_FILES
        Includes/Configuration/Option.h
        Includes/Model/Simulation.h
        Includes/Model/SimulationScheduler.h
        Includes/Repository/SimulationRepository.h
)

set(SandPandaSchedule_FILES
        ${INCLUDES_FILES}
        ${SOURCES_FILES}
)

set(CMAKE_CXX_STANDARD 20)

find_package(OpenMP REQUIRED)
find_package(nlohmann_json REQUIRED)

add_executable(
        SandPandaSchedule
        ${SandPandaSchedule_FILES}
)

target_link_libraries(
        SandPandaSchedule
        nlohmann_json::nlohmann_json
        ${GTEST_LIBRARIES}
        ${OpenMP_}
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "-O0 -g -pg -fno-omit-frame-pointer -DNOMP")
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "-O3 -flto -march=native -DNOMP")
else (CMAKE_BUILD_TYPE STREQUAL "OpenMP")
    set(CMAKE_CXX_FLAGS "-O3 -flto -march=native")
endif ()

if (OPENMP_FOUND AND CMAKE_BUILD_TYPE STREQUAL "OpenMP")
    message("OPENMP FOUND")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif ()

install(TARGETS SandPandaSchedule
        CONFIGURATIONS Release
        RUNTIME DESTINATION ~/.local/bin)


enable_testing()

#add_subdirectory("Tests")
