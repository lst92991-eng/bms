cmake_minimum_required(VERSION 3.22)

project(bms_v2_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

enable_testing()

add_library(bms_domain
    ../firmware/domain/bms_fault.c
    ../firmware/domain/bms_safety.c
)

target_include_directories(bms_domain
    PUBLIC
        ../firmware/common
        ../firmware/domain
)

target_compile_options(bms_domain
    PRIVATE
        -Wall
        -Wextra
        -Werror
        -Wconversion
        -Wshadow
        -Wpedantic
)

add_executable(test_bms_fault
    unit/test_bms_fault.c
)

target_link_libraries(test_bms_fault PRIVATE bms_domain)

target_compile_options(test_bms_fault
    PRIVATE
        -Wall
        -Wextra
        -Werror
        -Wconversion
        -Wshadow
        -Wpedantic
)

add_executable(test_bms_safety
    unit/test_bms_safety.c
)

target_link_libraries(test_bms_safety PRIVATE bms_domain)

target_compile_options(test_bms_safety
    PRIVATE
        -Wall
        -Wextra
        -Werror
        -Wconversion
        -Wshadow
        -Wpedantic
)

add_test(NAME bms_fault COMMAND test_bms_fault)
add_test(NAME bms_safety COMMAND test_bms_safety)
