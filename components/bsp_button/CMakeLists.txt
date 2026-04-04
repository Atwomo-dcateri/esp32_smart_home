idf_component_register(
    SRCS bsp_button.c
    INCLUDE_DIRS include
    REQUIRES 
        driver
        freertos     
)