#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <stdio.h>
#include <stdint.h>

void mqtt_app_start(void);
void mqtt_send_sensor_data(float temp, float humi);

#endif
