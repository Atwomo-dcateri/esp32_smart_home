#include "bsp_button.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"


#define BIN_GPIO 6

extern QueueHandle_t gpio_evt_queue;

static void IRAM_ATTR gpio_isr_handler(void *arg) {

    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}


void bsp_button_init(void) {

    gpio_config_t io_conf = {

        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BIN_GPIO),
        .pull_up_en = 1
    };

    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BIN_GPIO, gpio_isr_handler, (void *)BIN_GPIO);
}
