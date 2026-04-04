#ifndef BSP_SENSOR_H
#define BSP_SENSOR_H
#include "esp_err.h"

esp_err_t bsp_dht_read(float *t, float *h);

#endif
