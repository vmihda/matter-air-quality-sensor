# Host tests for aq_core. They build without ESP-IDF.
cmake_minimum_required(VERSION 3.16)
project(aq_core_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
add_compile_options(-Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-sanitize-recover=all)
add_link_options(-fsanitize=address,undefined)

enable_testing()

set(AQ_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../components/aq_core)

function(add_host_test name)
    add_executable(${name} ${name}.c ${ARGN})
    target_include_directories(${name} PRIVATE ${AQ_CORE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${name} PRIVATE m)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_host_test(test_aq_engine ${AQ_CORE_DIR}/aq_engine.c ${AQ_CORE_DIR}/aq_settings.c)
add_host_test(test_aq_settings ${AQ_CORE_DIR}/aq_settings.c ${AQ_CORE_DIR}/aq_engine.c)
add_host_test(test_scd4x_proto ${AQ_CORE_DIR}/scd4x_proto.c)
add_host_test(test_pms5003_proto ${AQ_CORE_DIR}/pms5003_proto.c)
add_host_test(test_status_policy ${AQ_CORE_DIR}/status_policy.c)
add_host_test(test_ens160_proto ${AQ_CORE_DIR}/ens160_proto.c)
