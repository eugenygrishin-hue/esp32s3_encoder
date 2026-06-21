idf_component_register(
    SRCS "ir_receiver.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio driver esp_timer esp_driver_rmt
)