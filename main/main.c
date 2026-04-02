#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "bsp_led.h"

static const char *TAG = "MAIN";
void app_main(void)
{

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    bsp_led_init();
    ESP_LOGI(TAG, "BSP LED Initialized");
    
    while (1)
    {
        ESP_LOGI(TAG, "LED Breathing up...");
        bsp_led_set_breath(8191);
        vTaskDelay(pdMS_TO_TICKS(500));

        ESP_LOGI(TAG, "LED Breathing down...");
        bsp_led_set_breath(0);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
