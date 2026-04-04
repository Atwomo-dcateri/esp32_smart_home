#include "bsp_led.h"
#include "driver/ledc.h"

void bsp_led_init(void) {

    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK

    };

    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {

        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = 5,
        .duty = 0,
        .hpoint = 0
    };

    ledc_channel_config(&ledc_channel); 
    // ledc_fade_func_install(0); 不用注册硬件中断
}


// void bsp_led_set_breath(uint32_t duty) {

//     ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty, 500);
//     ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
    
// } iram太小，无法处理这个中断，放入flash后，执行太慢，触发看门狗导致程序崩溃


void bsp_led_set_smple(uint32_t duty) {

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}