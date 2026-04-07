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


bool system_self_test(void) {

    ESP_LOGI("SELF_TEST", "Running system self-test...");
    vTaskDelay(pdMS_TO_TICKS(100));
    return true;
}


/// @brief 验证与取消回滚
/// @param  
void validate_image_at_boot(void) {

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;

    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK)
    {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
        {
            if (system_self_test())
            {
                ESP_LOGI("OTA", "Self-test passed! Marking image as valid.");
                esp_ota_mark_app_valid_cancel_rollback(); // 认为有效取消回滚
            }
        } else {

            ESP_LOGI("OTA", "Self-test failed Rolling backing...");
            esp_ota_mark_app_invalid_rollback_and_reboot(); // 立即回滚并重启

        }
    }
}

/// @brief OTA_下载更新
/// @param pvParameter 
void ota_uppdate_task(void *pvParameter) {

    ESP_LOGI("OTA", "Starting HTTPS OTA update...");
    esp_http_client_config_t http_cfg = {
        .url = "http://your-server.com/esp32.1.0.2.bin",
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_cfg = {

        .http_config = &http_cfg
    };

    esp_err_t ret = esp_https_ota(&ota_cfg);
    if (ret == ESP_OK)
    {
        ESP_LOGI("OTA", "Download complete. Rebooting in 3s...");
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
    } else {

        ESP_LOGI("OTA", "OTA failed! Error code: 0x%x", ret);
        vTaskDelete(NULL);
    }
}