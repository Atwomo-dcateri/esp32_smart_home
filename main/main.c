#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "bsp_led.h"
#include "bsp_button.h"
#include "bsp_sensor.h"

static const char *TAG = "APP_MAIN";
QueueHandle_t gpio_evt_queue = NULL;

void logic_task(void* arg) {
    uint32_t io_num;
    float temp, humi;

    for (;;) {

        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
        {
            ESP_LOGI(TAG, "Event: Button Pressed! Triggring Sample...");

            if (bsp_dht_read(&temp, &humi) == ESP_OK)
            {
                ESP_LOGW(TAG, "Sensor Data -> Temp: %.1f°C, Humi: %.1f%%", temp, humi);
    
            }

            bsp_led_set_breath(8981);
            vTaskDelay(pdMS_TO_TICKS(500));
            bsp_led_set_breath(0);
        }
        
    }
}

void app_main(void) {

    ESP_ERROR_CHECK(nvs_flash_init());

    bsp_led_init();

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    bsp_button_init();

    xTaskCreate(logic_task, "logic_task", 4096, NULL, 10, NULL);
    ESP_LOGI(TAG, "Smart Home Terminal Ready. Press the Button to Sample.");
}
