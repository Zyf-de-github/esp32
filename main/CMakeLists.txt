idf_component_register(SRCS "softap_example_main.c"
                       PRIV_REQUIRES 
                           nvs_flash
                           esp_wifi
                           esp_event
                           lwip
                           esp_driver_uart
                           esp_driver_gpio
                       INCLUDE_DIRS ".")