idf_component_register(
    SRCS "app_main.cpp" "bringup.cpp"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_driver_gpio aq_core aq_drivers
)

target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")
