#include "bsp_sensor.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"

#define DHT_GPIO 4


esp_err_t bsp_dht_read(float *t, float *h) {

    *t = 24.5;
    *h = 60.2;
    return ESP_OK;
}
