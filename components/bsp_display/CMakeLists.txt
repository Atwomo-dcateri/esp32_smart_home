idf_component_register(
    SRCS bsp_display.c
    INCLUDE_DIRS include
    REQUIRES
        driver    
        lvgl
        esp_lvgl_port 
        ssd1306
)