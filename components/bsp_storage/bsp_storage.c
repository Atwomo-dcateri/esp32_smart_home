#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "bsp_storage.h"

static const char *TAG = "STORAGE";

esp_err_t bsp_storage_save_int32(const char* key, int32_t value) {

    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);

    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_set_i32(my_handle, key, value);
    if(err == ESP_OK) {

        nvs_commit(my_handle);
    }
    nvs_close(my_handle);
    return err;
}

int32_t bsp_stroge_read_int32(const char *key, int32_t default_val) {

    nvs_handle_t my_handle;
    int32_t value = default_val;

    esp_err_t err = nvs_open("stroage", NVS_READONLY, &my_handle);
    if (err == ESP_OK)
    {
        nvs_get_i32(my_handle, key, &value);
        nvs_close(my_handle);
    }

    return value;
}