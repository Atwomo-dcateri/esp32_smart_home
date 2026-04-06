#include <string.h>
#include <stdint.h>      // 必须最先包含，提供 int32_t
#include <stdarg.h>

/* FreeRTOS 基础库必须在所有 ESP 组件之前 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

/* ESP 系统基础库 */
#include "esp_system.h"
#include "esp_event.h"   // 必须包含！解决 esp_event_base_t 报错
#include "esp_wifi.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "bsp_display.h"


/* 业务头文件 */
#include "wifi_manger.h"


#define  WIFI_CONNECTION_BIT BIT0
static EventGroupHandle_t s_wifi_event_group;
static const char *TAG = "WIFI_MGR";

/// @brief 事件处理函数，负责连接wifi
/// @param arg 
/// @param event_base 
/// @param event_id 
/// @param event_data 
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {


    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) 
    {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Connect failed/disconnected. Retrying...");
        
        if (is_lvgl_ready && lvgl_port_lock(pdMS_TO_TICKS(100)))
        {
            esp_wifi_connect(); // 只有这一行能让它从失败中恢复
            lv_label_set_text(ui_wifi_icon, LV_SYMBOL_CLOSE);
            lvgl_port_unlock();
        }
        } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "GOT_IP: " IPSTR, IP2STR(&event->ip_info.ip));
        if (is_lvgl_ready && lvgl_port_lock(pdMS_TO_TICKS(100)))
        {
            lv_label_set_text(ui_wifi_icon, LV_SYMBOL_WIFI);
            lvgl_port_unlock();
        }

        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTION_BIT);
    }
}
/// @brief 完成wifi station模式设置
/// @param  
void wifi_init_sta(void) {

    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL);

    wifi_config_t wifi_config = {

        .sta = {
            .ssid = "Wokwi-GUEST",
            .password = "",
        }
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

}


/// @brief 阻塞当前任务等待WiFi连接成功
/// @param  
void wifi_wait_connected(void) {
    // 阻塞等待 BIT0 (WIFI_CONNECTION_BIT)
    // pdFALSE: 退出时不清除位
    // pdTRUE: 等待所有指定的位
    // portMAX_DELAY: 永久等待

    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTION_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
}
