#include <stdio.h>
#include <stdint.h>// 一般加上为好

#include "mqtt_client.h"
#include "esp_system.h"// 只要用到esp_xx.h文件加上准没错
#include "esp_log.h"
#include "esp_wifi.h"
#include "bsp_led.h"
#include "esp_lvgl_port.h"         // ESP-LVGL 适配层（port）            // 系统接口（保留）
#include "lvgl.h"                  // LVGL 图形库（lv_obj/lv_label 等）
#include "bsp_display.h"
#include "bsp_storage.h"

#include "mqtt_handler.h"

static esp_mqtt_client_handle_t client;
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {

        .broker.address.uri = "mqtt://broker.emqx.io:1883",
        //.broker.address.uri = "wss://broker.emqx.io:8084",
        .credentials.client_id = "ESP32_Joe_Project", 
        
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void mqtt_send_sensor_data(float temp, float humi) {

    char payload[128];
    sprintf(payload, "{\"temperature\": %.1f, \"humidity\": %.1f}", temp, humi);
    esp_mqtt_client_publish(client, "/my_home/led_control", payload, 0, 1, 0);
    ESP_LOGI("MQTT_MGR", "Data sent: %s", payload);
}
/*

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {

    esp_mqtt_event_handle_t event = event_data;

    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t) event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI("MQTT", "Connected to Broker");
        esp_mqtt_client_subscribe(client, "/my_home/led_control", 0);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI("MQTT", "Topic= %.*s", event->topic_len, event->topic);
        ESP_LOGI("MQTT", "Data= %.*s", event->data_len, event->data);
         if (strstr(event->data, "ON"))
        {

            bsp_led_set_breath(1234);
            ESP_LOGI("ACTUATOP", "LED turned ON via Cloud");
            
        } else if(strstr(event->data, "OFF")) {

            bsp_led_set_breath(0);
            ESP_LOGI("ACTUATOP", "LED turned OFF via Cloud");
        }
        break;
    default: break;
       
    }
}

*/

// mqtt_handler.c 中的事件回调
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            // ESP_LOGI("MQTT", "Connected to Broker");
            // 订阅主题
            esp_mqtt_client_subscribe(client, "/my_home/led_control", 0);
            if (lvgl_port_lock(0))
            {
                lv_label_set_text(ui_mqtt_icon, LV_SYMBOL_REFRESH);
                lvgl_port_unlock();
            }

            break;

        case MQTT_EVENT_DATA:
            // 关键：不要在这里做复杂逻辑，只做核心判断
            if (strstr(event->data, "\"ON\"")) {
                //bsp_led_set_breath(1234);
                
                bsp_led_set_smple(1222);
                last_led_ate = bsp_storage_save_int32("led_power", 1222);
                ESP_LOGI("ACTUATOR", "Cloud Command: LED ON");
            } else if (strstr(event->data, "\"OFF\"")) {
                //bsp_led_set_breath(0);
                bsp_led_set_smple(0);
                last_led_ate = bsp_storage_save_int32("led_power", 0);
                ESP_LOGI("ACTUATOR", "Cloud Command: LED OFF");
            } else if (strstr(event->data, "\"W_OFF\"")) {
                ESP_LOGI("WIFI", "Cloud Command: WIFI OFF");
                esp_wifi_disconnect();
                esp_wifi_stop();
                lv_delay_ms(100);
            }
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE("MQTT", "Event Error"); 
            break;
        
        case MQTT_EVENT_DISCONNECTED:
            
            if (lvgl_port_lock(0))
            {
                lv_label_set_text(ui_mqtt_icon, LV_SYMBOL_CLOSE);
                lvgl_port_unlock();
            }
        
        default:
            break;
    }
}