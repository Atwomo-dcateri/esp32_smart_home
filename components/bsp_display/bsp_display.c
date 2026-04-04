#include <stdio.h>

#include "driver/i2c.h"
#include "esp_system.h"
#include "esp_log.h"

#include "bsp_display.h"

void bsp_display_i2c_init(void) {

    i2c_config_t cfg = {

        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_param_config(I2C_MASTER_NUM, &cfg);
    i2c_driver_install(I2C_MASTER_NUM, cfg.mode, 0, 0, 0);
    ESP_LOGI("DISPLAY", "I2C Initialied for OLED");
} 