#include <stdio.h>
#include "bsp_sensor.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h"


#define DHT_GPIO 4


esp_err_t bsp_dht_read(float *t, float *h) {

    *t = 24.5;
    *h = 60.2;
    return ESP_OK;
}