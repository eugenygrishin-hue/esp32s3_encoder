idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES ir_receiver encoder esp_driver_gpio espressif__knob button esp_driver_rmt)
