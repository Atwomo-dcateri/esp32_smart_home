#ifndef BSP_DISPLAY
#define BAP_DISPLAY

#define I2C_BUS_PORT            0
#define LCD_PIXEL_CLOCK_HZ      (100 * 1000)
#define LCD_PIN_NUM_SDA         1
#define LCD_PIN_NUM_SCL         2
#define LCD_H_RES               128
#define LCD_V_RES               64
#define LCD_ADRR                0X3C  // 0x3C通常

void bsp_display_init();
void setup_ui(void);

#endif
