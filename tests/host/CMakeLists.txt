add_test(
    NAME platform_contract
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/test_platform_contract.py
            --source-root ${PROJECT_SOURCE_DIR}
)

add_test(
    NAME repo_hygiene
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/check_repo_hygiene.py
            --source-root ${PROJECT_SOURCE_DIR}
)

add_test(
    NAME bq_safety_contract
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/test_bq_safety_contract.py
            --source-root ${PROJECT_SOURCE_DIR}
)

add_test(
    NAME power_soc_contract
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/test_power_soc_contract.py
            --source-root ${PROJECT_SOURCE_DIR}
)

if(BMS_HOST_TESTS_ONLY)
    add_executable(bms_algorithm_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/test_algorithms.c
        ${PROJECT_SOURCE_DIR}/Com/Com_BatteryParam.c
        ${PROJECT_SOURCE_DIR}/Com/Com_Format.c
        ${PROJECT_SOURCE_DIR}/Com/Com_SOC.c
        ${PROJECT_SOURCE_DIR}/Com/Com_SOH.c
    )

    target_include_directories(bms_algorithm_tests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/Com
    )

    if(MSVC)
        target_compile_options(bms_algorithm_tests PRIVATE /utf-8 /W4 /WX)
    else()
        target_compile_options(bms_algorithm_tests PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
    endif()

    add_test(NAME algorithm_unit COMMAND bms_algorithm_tests)
endif()
