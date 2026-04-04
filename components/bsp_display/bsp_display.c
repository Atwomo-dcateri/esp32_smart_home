#include "stdio.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"
#include "esp_lcd_panel_ssd1306.h"

#include "esp_system.h"
#include "esp_log.h"
#include "driver/i2c.h"

#include "bsp_display.h"

static lv_display_t *lvgl_disp = NULL;
static const char *TAG = "BSP_DISPLAY";

void bsp_display_init(void) {

    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = LCD_PIN_NUM_SDA,
        .scl_io_num = LCD_PIN_NUM_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
        .clk_flags = 0,

    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_BUS_PORT, &i2c_cfg));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_BUS_PORT, I2C_MODE_MASTER, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI("TAG", "Install panel IO");

    esp_lcd_panel_io_handle_t io_handle = NULL;


    // 2. 创建 Panel IO 句柄 (关键：手动配置结构体
    esp_lcd_panel_io_i2c_config_t io_config = {

        .dev_addr = LCD_ADRR,
        .control_phase_bytes = 1, //
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .dc_bit_offset = 6, //
        .flags = {
            .disable_control_phase = 0, //
        },
    
        
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)I2C_BUS_PORT, &io_config, &io_handle));
    
    // 3. 安装 SSD1306 面板驱动
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_ssd1306_config_t ssd1306_config = {

        .height = LCD_V_RES,
    };

    esp_lcd_panel_dev_config_t panel_config = {

        .bits_per_pixel = 1,
        .reset_gpio_num = -1,
        .vendor_config = &ssd1306_config,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));

    //ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    esp_err_t ret = esp_lcd_panel_init(panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE("OLED", "Init Failed (0x%x), keep going...", ret);
    } else {
        ESP_LOGI("OLED", "Init Success!");
        esp_lcd_panel_disp_on_off(panel_handle, true);
    }

    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    const lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&port_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {

        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = LCD_H_RES * LCD_V_RES,
        .double_buffer = false,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = true,
        /* 核心修正点 1：控制相关的参数要包在 control 里 */

        .rotation = {                        // ← 加上 {}
            .swap_xy  = false,
            .mirror_x = false,
            .mirror_y = false,
        },
    };

    lvgl_disp = lvgl_port_add_disp(&disp_cfg);
    ESP_LOGI(TAG, "OLED with LGVL 9.1 initalized successflly");
}

lv_obj_t *ui_temp_label;

void setup_ui(void) {

    if (lvgl_port_lock(0))// 
    {
        lv_obj_t *src = lv_screen_active();
        lv_obj_set_style_bg_color(src, lv_color_black(), 0);

        ui_temp_label = lv_label_create(src);
        lv_label_set_text(ui_temp_label, "System Ready");
        lv_obj_set_style_text_color(ui_temp_label, lv_color_white(), 0);
        lv_obj_align(ui_temp_label, LV_ALIGN_CENTER, 0, 0);

        lvgl_port_unlock(); //
        ESP_LOGI(TAG, "setup_ui finished and unlocked"); // 添加这一行
    } else {

        ESP_LOGI(TAG, "Failed to lock for setup_ui");
    }
}