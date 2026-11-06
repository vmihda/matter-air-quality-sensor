set(srcs
    "app_main.cpp"
    "boot_button.cpp"
    "bringup.cpp"
    "demo_mode.cpp"
    "matter_app.cpp"
    "sensors_task.cpp"
    "status_led.cpp"
)

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_driver_gpio esp_timer aq_core aq_drivers esp_matter
)

target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")
