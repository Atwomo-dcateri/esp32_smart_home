#ifndef BSP_DISPLAY
#define BAP_DISPLAY

#include "driver/i2c.h"            // I2C 驱动（用于配置与安装 I2C 主机）
#include "esp_log.h"               // ESP 日志（ESP_LOGx）
#include "esp_lcd_panel_io.h"      // LCD Panel IO 抽象（I2C/SPI 等）
#include "esp_lcd_panel_ops.h"     // LCD Panel 操作接口（reset/init/onoff 等）
#include "esp_lcd_panel_ssd1306.h" // SSD1306 面板驱动
#include "esp_lcd_panel_vendor.h"  // LCD Vendor 相关定义
#include "esp_lvgl_port.h"         // ESP-LVGL 适配层（port）
#include "esp_system.h"            // 系统接口（保留）
#include "lvgl.h"                  // LVGL 图形库（lv_obj/lv_label 等）

#define I2C_BUS_PORT            0
#define LCD_PIXEL_CLOCK_HZ      (100 * 1000)
#define LCD_PIN_NUM_SDA         1
#define LCD_PIN_NUM_SCL         2
#define LCD_H_RES               128
#define LCD_V_RES               64
#define LCD_ADRR                0X3C  // 0x3C通常

extern lv_obj_t *ui_mqtt_icon;
extern lv_obj_t *ui_wifi_icon;
extern bool is_lvgl_ready;

void bsp_display_init();
void setup_ui(void);
void bsp_display_update_data(float temp, float humi);
void bsp_display_show_status(const char *status);
void bsp_display_pro_ui_init(void);

#endif

