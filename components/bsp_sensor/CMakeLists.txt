idf_component_register(
    SRCS bsp_sensor.c
    INCLUDE_DIRS include
    REQUIRES 
        driver
        esp_rom
)
