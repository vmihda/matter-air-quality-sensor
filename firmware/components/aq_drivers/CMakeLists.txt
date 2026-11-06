idf_component_register(
    SRCS "scd41.c" "pms5003.c"
    INCLUDE_DIRS "include"
    REQUIRES aq_core esp_driver_i2c esp_driver_uart esp_timer
)
