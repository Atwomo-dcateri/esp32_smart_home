#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "bsp_led.h"
#include "esp_wifi.h"
#include "bsp_button.h"
#include "bsp_sensor.h"
#include "mqtt_handler.h"
#include "wifi_manger.h"
#include "bsp_display.h"
#include "bsp_storage.h"


static const char *TAG = "APP_MAIN";
QueueHandle_t gpio_evt_queue = NULL;
void oled_show(uint8_t x, uint8_t y, char *buf);
uint32_t last_led_ate;

static uint8_t wifi_count = 0;

void logic_task(void *arg)
{
    
    uint32_t io_num;
    float temp, humi;
    wifi_init_sta(); // 启动wifi

    wifi_wait_connected(); // 等待wifi连接
    bsp_display_init(); // 
    ESP_LOGI(TAG, "Wating for UI...");
    // setup_ui(); //
    bsp_display_pro_ui_init();
    ESP_LOGI(TAG, "Wating for WiFi...");

    mqtt_app_start();

    for (;;)
    {
        
        //if (xQueueReceive(gpio_evt_queue, &io_num, pdTICKS_TO_MS(100)))
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
        {
            if (bsp_dht_read(&temp, &humi) == ESP_OK)
            {
                ESP_LOGW(TAG, "Local Read -> T:%.1f, H:%.1f", temp, humi);

                mqtt_send_sensor_data(temp, humi);

                // char buf[32];
                last_led_ate = bsp_stroge_read_int32("led_power", 1000);
                bsp_display_update_data(temp, humi);
                last_led_ate = bsp_stroge_read_int32("led_power", 0);
                bsp_led_set_smple(last_led_ate);
                
                vTaskDelay(pdMS_TO_TICKS(500));
                last_led_ate = bsp_stroge_read_int32("led_power", 0);
                bsp_led_set_smple(last_led_ate);
            }

            if (io_num == 3 && wifi_count == 0)
            {
                wifi_count = 1;
                ESP_LOGI("WIFI CONTROL", "Stop WIFI...");
                esp_wifi_disconnect();
                esp_wifi_stop();
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            else if (io_num == 3 && wifi_count == 1)
            {
                wifi_count = 0;
                ESP_LOGI("WIFI CONTROL", "Connect WIFI...");
                esp_wifi_connect();
                esp_wifi_start();
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }

        vTaskDelay(pdTICKS_TO_MS(1000));
    }
}

// void logic_task(void *arg) {
//     bsp_display_init(); 
//     bsp_display_pro_ui_init();
//     // 先注释掉传感器读取和 MQTT
//     // bsp_dht_read(...);
//     // mqtt_app_start();

//     for (;;) {
//         ESP_LOGI("DEBUG", "Logic Task Heartbeat...");
//         vTaskDelay(pdMS_TO_TICKS(1000)); // 每一秒打印一次
//     }
// }

void app_main(void)
{

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // wifi_init_sta(); // 启动wifi

    // wifi_wait_connected(); // 等待wifi连接
    bsp_led_init();        // 启动LED
    
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    bsp_button_init(); // 启动按键

    xTaskCreate(logic_task, "logic_task", 8192, NULL, 10, NULL);
    ESP_LOGI(TAG, "Smart Home Terminal Ready. Press the Button to Sample.");
}



// #include <stdio.h>
// #include "driver/i2c.h"
// #include "esp_log.h"
// #include "bsp_display.h"

// void app_main(void) {
    
//     bsp_display_init();
// }