idf_component_register(
    SRCS mqtt_handler.c
    INCLUDE_DIRS include
    REQUIRES
        mqtt
        bsp_led
        lvgl
        esp_lvgl_port 
        ssd1306
        bsp_display
        wifi_manger
)
