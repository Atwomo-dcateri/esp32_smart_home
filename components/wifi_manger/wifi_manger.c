/**
 * @file wifi_manger.c
 * @brief WiFi 连接管理模块
 *
 * @details
 * 该模块负责 ESP32 的 WiFi 初始化、连接、事件处理等功能。
 *
 * 工作原理：
 * 1. 初始化 WiFi STA（Station）模式
 * 2. 注册事件处理器，监听 WiFi/IP 事件
 * 3. 尝试连接到预配置的 AP（接入点）
 * 4. 获得 IP 地址后设置事件组，通知其他任务
 *
 * 事件模型（ESP-IDF）：
 * - WIFI_EVENT_STA_START：WiFi 启动
 * - WIFI_EVENT_STA_DISCONNECTED：断开连接（自动重连）
 * - IP_EVENT_STA_GOT_IP：获得 IP 地址（连接成功）
 *
 * @note
 * - 采用事件驱动，不轮询
 * - UI 更新（LVGL）需持有互斥锁
 * - 硬编码 SSID/密码，生产环境应从 NVS 读取
 *
 * @author Your Name
 * @version 1.0.0
 */

#include <string.h> /**< C 标准库 */
#include <stdint.h> /**< 整数类型定义 */
#include <stdarg.h> /**< 变参函数 */

/* FreeRTOS 基础库必须在所有 ESP 组件之前 */
#include "freertos/FreeRTOS.h"     /**< FreeRTOS 核心 */
#include "freertos/task.h"         /**< FreeRTOS 任务 */
#include "freertos/event_groups.h" /**< FreeRTOS 事件组 */

/* ESP 系统基础库 */
#include "esp_system.h"    /**< ESP 系统库 */
#include "esp_event.h"     /**< ESP 事件系统 */
#include "esp_wifi.h"      /**< WiFi 驱动 */
#include "esp_log.h"       /**< 日志系统 */
#include "lvgl.h"          /**< LVGL 图形库 */
#include "esp_lvgl_port.h" /**< LVGL 适配层 */
#include "bsp_display.h"   /**< 显示驱动 */

/* 业务头文件 */
#include "wifi_manger.h" /**< 本模块头文件 */

/** @brief WiFi 事件组 - 用于同步连接状态 */
#define WIFI_CONNECTION_BIT BIT0

/** @brief 全局 WiFi 事件组句柄（静态，模块内部使用） */
static EventGroupHandle_t s_wifi_event_group;
static const char *TAG = "WIFI_MGR";

/// @brief 事件处理函数，负责连接wifi
/// @param arg
/// @param event_base
/// @param event_id
/// @param event_data
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGW(TAG, "Connect failed/disconnected. Retrying...");

        if (is_lvgl_ready && lvgl_port_lock(pdMS_TO_TICKS(100)))
        {
            esp_wifi_connect(); // 只有这一行能让它从失败中恢复
            lv_label_set_text(ui_wifi_icon, LV_SYMBOL_CLOSE);
            lvgl_port_unlock();
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "GOT_IP: " IPSTR, IP2STR(&event->ip_info.ip));
        if (is_lvgl_ready && lvgl_port_lock(pdMS_TO_TICKS(100)))
        {
            lv_label_set_text(ui_wifi_icon, LV_SYMBOL_WIFI);
            lvgl_port_unlock();
        }

        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTION_BIT);
    }
}
/// @brief 完成wifi station模式设置
/// @param
void wifi_init_sta(void)
{

    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL);

    wifi_config_t wifi_config = {

        .sta = {
            .ssid = "Wokwi-GUEST",
            .password = "",
        }};

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/// @brief 阻塞当前任务等待WiFi连接成功
/// @param
void wifi_wait_connected(void)
{
    // 阻塞等待 BIT0 (WIFI_CONNECTION_BIT)
    // pdFALSE: 退出时不清除位
    // pdTRUE: 等待所有指定的位
    // portMAX_DELAY: 永久等待

    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTION_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
}
