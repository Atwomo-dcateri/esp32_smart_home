/* 1. 标准 C 库 */
#include <stdio.h>
#include <string.h>

/* 2. ESP-IDF 系统核心库 */
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"  // OTA 任务通常需要监听事件

/* 3. 网络与协议栈相关库 */
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

/* 4. OTA 专项功能库 */
#include "esp_ota_ops.h"
#include "esp_https_ota.h"

/* 5. (如果有) 项目自定义头文件 */
// #include "my_display_ui.h"
#include "esp_http_ota.h"

static const char *TAG = "OTA_UPDATE";

#define OTA_URL "http://your-server.com/fireware.bin"


void ota_task(void *pvParameter) {

    ESP_LOGI(TAG, "Starting OTA update...");
    esp_http_client_config_t http_cfg = {

        .url = OTA_URL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_cfg = {

        .http_config = &http_cfg
    };

    esp_err_t ret = esp_https_ota(&ota_cfg);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "OTA Upgrade Success! Rebooting...");
        esp_restart();
    } else {
        ESP_LOGI(TAG, "OTA Ugrade failed (%d)", ret);
        vTaskDelete(NULL);
    }
}

 
