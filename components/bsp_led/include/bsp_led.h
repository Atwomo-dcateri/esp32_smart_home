#ifndef BSH_LED_H
#define BSH_LED_H
#include <stdint.h>


void bsp_led_init(void);
void bsp_led_set_breath(uint32_t duty);

#endif