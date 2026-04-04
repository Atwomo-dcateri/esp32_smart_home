#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "bsp_led.h"
#include "bsp_button.h"
#include "bsp_sensor.h"
#include "mqtt_handler.h"
#include "wifi_manger.h"
#include "bsp_display.h"

static const char *TAG = "APP_MAIN";
QueueHandle_t gpio_evt_queue = NULL;
// void oled_show(uint8_t x, uint8_t y, char *buf);

void logic_task(void *arg)
{

    uint32_t io_num;
    float temp, humi;
    bsp_display_i2c_init();

    ESP_LOGI(TAG, "Wating for WiFi...");

    mqtt_app_start();

    for (;;)
    {

        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
        {
            if (bsp_dht_read(&temp, &humi) == ESP_OK)
            {
                ESP_LOGW(TAG, "Local Read -> T:%.1f, H:%.1f", temp, humi);

                mqtt_send_sensor_data(temp, humi);

                char buf[32];
                sprintf(buf, "T: %.1f C", temp);
                //oled_show(0, 16, buf);

                sprintf(buf, "H: %.1f %%", humi);
                //oled_show(0, 32, buf);

                bsp_led_set_smple(1000);
                vTaskDelay(pdMS_TO_TICKS(500));
                bsp_led_set_smple(0);
            }
        }
    }
}

void app_main(void)
{

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta(); // 启动wifi

    wifi_wait_connected(); // 等待wifi连接
    bsp_led_init();        // 启动LED

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    bsp_button_init(); // 启动按键

    xTaskCreate(logic_task, "logic_task", 8192, NULL, 10, NULL);
    ESP_LOGI(TAG, "Smart Home Terminal Ready. Press the Button to Sample.");
}
