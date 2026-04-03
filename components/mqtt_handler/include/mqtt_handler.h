#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <stdio.h>
#include <stdint.h>
#include "esp_event.h"

void mqtt_app_start(void);
void mqtt_send_sensor_data(float temp, float humi);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

#endif
