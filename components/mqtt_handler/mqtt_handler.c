#include <stdio.h>
#include <stdint.h>// 一般加上为好

#include "mqtt_client.h"
#include "esp_system.h"// 只要用到esp_xx.h文件加上准没错
#include "esp_log.h"


#include "mqtt_handler.h"

static esp_mqtt_client_handle_t client;

void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {

        .broker.address.uri = "mqtt://broker.emqx.io:1883",
        //.broker.address.uri = "wss://broker.emqx.io:8084",
        .credentials.client_id = "ESP32_Joe_Project"

    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void mqtt_send_sensor_data(float temp, float humi) {

    char payload[64];
    sprintf(payload, "{\"temperature\": %.1f, \"humidity\": %.1f}", temp, humi);
    esp_mqtt_client_publish(client, "smarthome/sensor", payload, 0, 1, 0);
    ESP_LOGI("MQTT_MGR", "Data sent: %s", payload);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
           // ESP_LOGI(TAG, "MQTT Connected to Broker!");
            // 连上后，立刻订阅网页发送命令的主题
            esp_mqtt_client_subscribe(client, "smarthome/command", 0);
            break;

        case MQTT_EVENT_DATA: // 重点：这里处理网页发来的数据
            //ESP_LOGI(TAG, "Received logic from Web!");
            printf("Topic = %.*s\r\n", event->topic_len, event->topic);
            printf("Data = %.*s\r\n", event->data_len, event->data);
            
            // 解析数据（比如收到 "led_on" 就开灯）
            if (strncmp(event->data, "led_on", event->data_len) == 0) {
                //bsp_led_set_breath(500); 
            }
            break;
        default:
            break;
    }
}