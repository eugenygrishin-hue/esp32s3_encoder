idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES knob wifi_manager ssd1306 esp_netif esp_driver_gpio driver)                     				 