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

        .broker.address.uri = "mqtt://broker.emqx.io",
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);
}

void mqtt_send_sensor_data(float temp, float humi) {

    char payload[128];
    sprintf(payload, "{\"temperature\": %.1f, \"humidity\": %.1f}", temp, humi);
    esp_mqtt_client_publish(client, "/my_home/sensor_data", payload, 0, 1, 0);
    ESP_LOGI("MQTT_MGR", "Data sent: %s", payload);
}
