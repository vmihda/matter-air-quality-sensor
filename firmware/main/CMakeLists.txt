set(srcs
    "app_main.cpp"
    "boot_button.cpp"
    "bringup.cpp"
    "matter_app.cpp"
    "ota_guard.cpp"
    "sensors_task.cpp"
    "service_console.cpp"
    "settings_store.cpp"
    "status_led.cpp"
    "web_ui.cpp"
)

# Only the demo build needs it.
if(CONFIG_AQ_DEMO_VALUES)
    list(APPEND srcs "demo_mode.cpp")
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_driver_gpio esp_timer esp_netif esp_http_server app_update esp_app_format
             console mbedtls aq_core aq_drivers esp_matter
)

target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")

# The device page is stored and sent compressed.
idf_build_get_property(python PYTHON)
set(page_source "${CMAKE_CURRENT_SOURCE_DIR}/web_ui_page.html")
set(page_gzip "${CMAKE_CURRENT_BINARY_DIR}/web_ui_page.html.gz")
add_custom_command(
    OUTPUT "${page_gzip}"
    COMMAND "${python}" "${CMAKE_CURRENT_SOURCE_DIR}/gzip_file.py" "${page_source}" "${page_gzip}"
    DEPENDS "${page_source}" "${CMAKE_CURRENT_SOURCE_DIR}/gzip_file.py"
    VERBATIM
)
target_add_binary_data(${COMPONENT_LIB} "${page_gzip}" BINARY DEPENDS "${page_gzip}")
