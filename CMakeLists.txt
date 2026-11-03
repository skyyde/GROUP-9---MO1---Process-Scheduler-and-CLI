cmake_minimum_required(VERSION 3.16)
project(CSOPESY_MO1 LANGUAGES CXX)

# Shared Phase 1 code only; future placeholders deliberately stay out of the build.
add_library(phase1_Kyla src/Config_Kyla.cpp src/Console_Kyla.cpp)
target_include_directories(phase1_Kyla PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(phase1_Kyla PUBLIC cxx_std_17)
set_target_properties(phase1_Kyla PROPERTIES CXX_EXTENSIONS OFF)

add_executable(csopesy src/main_Kyla.cpp)
target_link_libraries(csopesy PRIVATE phase1_Kyla)

include(CTest)
if(BUILD_TESTING)
    # The test runner has its own entry point, never linked into the application.
    add_executable(phase1_tests tests/Phase1Tests_Kyla.cpp)
    target_link_libraries(phase1_tests PRIVATE phase1_Kyla)
    add_test(NAME Phase1Tests_Kyla COMMAND phase1_tests)
endif()

foreach(target phase1_Kyla csopesy phase1_tests)
    if(TARGET ${target})
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /permissive-)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endif()
endforeach()
