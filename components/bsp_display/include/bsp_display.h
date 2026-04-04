#ifndef BSP_DISPLAY
#define BAP_DISPLAY


#define I2C_MASTER_SDA_IO 8
#define I2C_MASTER_SCL_IO 9
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 400000

void bsp_display_i2c_init(void);

#endif
