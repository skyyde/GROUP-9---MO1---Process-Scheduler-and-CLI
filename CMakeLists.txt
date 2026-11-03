cmake_minimum_required(VERSION 3.16)
project(CSOPESY_MO1 LANGUAGES CXX)

# Build Kyla's Phase 1 and 2 modules. Add the other modules as they are completed.
find_package(Threads REQUIRED)
add_library(phase2_Kyla src/Process_Kyla.cpp)
target_include_directories(phase2_Kyla PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(phase2_Kyla PUBLIC cxx_std_17)
set_target_properties(phase2_Kyla PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(phase2_Kyla PUBLIC Threads::Threads)

add_library(phase1_Kyla src/Config_Kyla.cpp src/Console_Kyla.cpp)
target_link_libraries(phase1_Kyla PUBLIC phase2_Kyla)
target_include_directories(phase1_Kyla PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(phase1_Kyla PUBLIC cxx_std_17)
set_target_properties(phase1_Kyla PROPERTIES CXX_EXTENSIONS OFF)

add_executable(csopesy src/main_Kyla.cpp)
target_link_libraries(csopesy PRIVATE phase1_Kyla)

foreach(target phase1_Kyla phase2_Kyla csopesy)
    if(TARGET ${target})
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /permissive-)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endif()
endforeach()
