idf_component_register(
    SRCS wifi_manger.c
    INCLUDE_DIRS include
    REQUIRES
        freertos
        esp_wifi
        esp_lvgl_port
        lvgl
        bsp_display
)
