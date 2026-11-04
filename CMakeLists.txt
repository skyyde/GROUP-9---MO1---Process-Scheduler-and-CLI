cmake_minimum_required(VERSION 3.16)
project(CSOPESY_MO1 LANGUAGES CXX)

# Build Kyla's Phase 1 and 2 modules. Add the other modules as they are completed.
find_package(Threads REQUIRED)
add_library(phase2_Kyla src/Process_Kyla.cpp)
target_include_directories(phase2_Kyla PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(phase2_Kyla PUBLIC cxx_std_17)
set_target_properties(phase2_Kyla PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(phase2_Kyla PUBLIC Threads::Threads)

add_library(phase1_Kyla src/Config_Kyla.cpp src/Console_Kyla.cpp
    src/TerminalView_Kyla.cpp src/UtilizationReport_Kyla.cpp)
target_link_libraries(phase1_Kyla PUBLIC phase2_Kyla)
target_include_directories(phase1_Kyla PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(phase1_Kyla PUBLIC cxx_std_17)
set_target_properties(phase1_Kyla PROPERTIES CXX_EXTENSIONS OFF)

add_library(phase3_Enrique src/Instruction_Enrique.cpp)
target_link_libraries(phase3_Enrique PUBLIC phase2_Kyla)
target_include_directories(phase3_Enrique PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(phase3_Enrique PUBLIC cxx_std_17)
set_target_properties(phase3_Enrique PROPERTIES CXX_EXTENSIONS OFF)

add_library(phase4_Enrique src/Scheduler_Enrique.cpp src/AutoGenerator_Nicos.cpp)
target_link_libraries(phase4_Enrique PUBLIC phase3_Enrique phase1_Kyla)
target_compile_features(phase4_Enrique PUBLIC cxx_std_17)
set_target_properties(phase4_Enrique PROPERTIES CXX_EXTENSIONS OFF)

add_executable(csopesy src/main_Kyla.cpp)
target_link_libraries(csopesy PRIVATE phase1_Kyla phase4_Enrique)

enable_testing()
add_executable(phase34_tests src/tests/Phase34Tests_Enrique.cpp)
target_link_libraries(phase34_tests PRIVATE phase4_Enrique)
target_compile_definitions(phase34_tests PRIVATE PROGRAM_DIR="${CMAKE_CURRENT_SOURCE_DIR}/programs")
add_test(NAME phase34_tests COMMAND phase34_tests)

foreach(target phase1_Kyla phase2_Kyla phase3_Enrique phase4_Enrique csopesy phase34_tests)
    if(TARGET ${target})
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endif()
endforeach()
