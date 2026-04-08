/**
 * @file mqtt_handler.c
 * @brief MQTT 客户端通信模块
 *
 * @details
 * 该模块负责与 MQTT Broker 通信，用于云端数据上传和远程命令下发。
 *
 * 连接配置：
 * - Broker：mqtt://broker.emqx.io:1883（EMQX 公共 Broker）
 * - Client ID：ESP32_Joe_Project
 * - Topic（发布）：/my_home/led_control（温湿度数据）
 * - Topic（订阅）：/my_home/led_control（接收云端命令）
 *
 * 云端命令格式（JSON）：
 * - "ON"：点亮 LED（占空比 1222）
 * - "OFF"：关闭 LED（占空比 0）
 * - "W_OFF"：断开 WiFi
 * - "UPDATE"：触发 OTA 固件升级
 *
 * @note
 * - MQTT 发布是阻塞操作（延迟 100-1000ms）
 * - 仅在 WiFi 在线时调用 mqtt_send_sensor_data()，避免任务阻塞
 * - 订阅主题使用 QoS=0（不保证消息送达，但低开销）
 *
 * @author Your Name
 * @version 1.0.0
 */

#include <stdio.h>  /**< 标准 IO */
#include <stdint.h> /**< 整数类型定义 */

#include "mqtt_client.h"   /**< ESP-IDF MQTT 客户端库 */
#include "esp_system.h"    /**< ESP 系统库 */
#include "esp_log.h"       /**< 日志系统 */
#include "esp_wifi.h"      /**< WiFi 驱动 */
#include "bsp_led.h"       /**< LED PWM 控制 */
#include "esp_lvgl_port.h" /**< LVGL 适配层 */
#include "lvgl.h"          /**< LVGL 图形库 */
#include "bsp_display.h"   /**< 显示驱动 */
#include "bsp_storage.h"   /**< NVS 存储 */
#include "esp_http_ota.h"  /**< OTA 升级管理 */

#include "mqtt_handler.h" /**< 本模块头文件 */

/** @brief MQTT 客户端句柄（全局，模块内部使用） */
static esp_mqtt_client_handle_t client;
/** @brief MQTT 事件处理函数声明 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

/**
 * @brief 初始化并启动 MQTT 客户端
 *
 * @details
 * 配置 MQTT 客户端连接参数，初始化并启动连接。
 *
 * 流程：
 * 1. 配置 MQTT 参数（Broker URI、Client ID）
 * 2. esp_mqtt_client_init() 创建客户端实例
 * 3. 注册事件处理器
 * 4. esp_mqtt_client_start() 启动连接（非阻塞）
 *
 * @note
 * - 此函数返回后，MQTT 连接在后台进行
 * - 实际连接成功在 MQTT_EVENT_CONNECTED 事件中体现
 * - 建议在 WiFi 连接成功后调用此函数
 *
 * @warning
 * - 【关键】Broker 地址硬编码，生产环境应从配置读取
 * - 【易错】Client ID 需唯一，否则与其他设备冲突
 *
 * @see mqtt_send_sensor_data, mqtt_event_handler
 */
void mqtt_app_start(void)
{
    /* MQTT 客户端配置 */
    esp_mqtt_client_config_t mqtt_cfg = {
        /* Broker 地址：EMQX 公共 MQTT Broker */
        .broker.address.uri = "mqtt://broker.emqx.io:1883",

        /* 【备选】加密连接（需 EMQX 支持 WSS） */
        /* .broker.address.uri = "wss://broker.emqx.io:8084", */

        /* 客户端 ID：需唯一标识该设备 */
        .credentials.client_id = "ESP32_Joe_Project",
    };

    /* 初始化客户端（创建但不连接） */
    client = esp_mqtt_client_init(&mqtt_cfg);

    /* 注册事件回调（接收所有 MQTT 事件） */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    /* 启动客户端（开始连接 Broker） */
    esp_mqtt_client_start(client);
}

/**
 * @brief 发送传感器数据到 MQTT Broker
 *
 * @param[in] temp 温度值（°C）
 * @param[in] humi 湿度值（%RH）
 *
 * @details
 * 构造 JSON 格式的数据包，发布到 "/my_home/led_control" 主题。
 *
 * 数据格式示例：
 * @code
 * {"temperature": 25.3, "humidity": 62.5}
 * @endcode
 *
 * MQTT 发布参数：
 * - topic："/my_home/led_control"
 * - QoS：1（保证送达，最多一次）
 * - retain：0（不保留消息）
 *
 * @note
 * - 【性能】esp_mqtt_client_publish 是阻塞操作
 *   → 可能延迟 100-1000ms（网络状况决定）
 *   → 仅在 WiFi 在线时调用，避免任务长时间阻塞
 * - 【安全】数据未加密，仅适合局域网 Broker
 *   → 生产环境应使用 TLS/SSL（wss://）及认证
 *
 * @warning
 * - 【易错】Buffer 溢出风险
 *   → payload 大小固定 128 字节
 *   → 若数据过长会截断或覆盖栈
 *   → 改进：动态分配或增大 buffer
 * - 【易错】sprintf 不安全
 *   → 建议改用 snprintf() 指定最大长度
 *   → 例：snprintf(payload, sizeof(payload), ...)
 *
 * @see mqtt_app_start
 */
void mqtt_send_sensor_data(float temp, float humi)
{
    /* 数据缓冲区（固定 128 字节） */
    char payload[128];

    /* 【易错】sprintf 可能溢出 → 建议改用 snprintf */
    sprintf(payload, "{\"%temperature\": %.1f, \"humidity\": %.1f}", temp, humi);

    /* 发布到 MQTT Broker
     * esp_mqtt_client_publish(client, topic, data, len, qos, retain)
     * - client：MQTT 客户端句柄
     * - topic：消息主题
     * - data：消息内容
     * - len：0 表示自动计算长度（如果 data 是 null-terminated 字符串）
     * - qos：1 表示 QoS 1（保证至少送达一次）
     * - retain：0 表示不保留
     */
    esp_mqtt_client_publish(client, "/my_home/led_control", payload, 0, 1, 0);

    /* 日志记录 */
    ESP_LOGI("MQTT_MGR", "Data sent: %s", payload);
}

/**
 * @brief MQTT 事件回调处理函数
 *
 * @param[in] handler_args 处理器参数（未使用）
 * @param[in] base 事件基类型（MQTT_EVENTS）
 * @param[in] event_id 具体事件 ID
 * @param[in] event_data 事件数据指针
 *
 * @details
 * 处理所有 MQTT 事件，包括连接、数据接收、错误等。
 *
 * 主要事件：
 * 1. MQTT_EVENT_CONNECTED：连接成功 → 订阅主题
 * 2. MQTT_EVENT_DATA：接收到消息 → 解析命令并执行
 * 3. MQTT_EVENT_DISCONNECTED：连接断开 → 更新 UI
 * 4. MQTT_EVENT_ERROR：错误发生 → 记录日志
 *
 * 云端命令处理：
 * - "ON"：LED 亮起（占空比 1222）
 * - "OFF"：LED 关闭（占空比 0）
 * - "W_OFF"：WiFi 断开
 * - "UPDATE"：触发 OTA 固件升级
 *
 * @note
 * - 【关键】LVGL 操作需持有互斥锁
 * - 【设计】此函数在 MQTT 后台任务中执行
 *   → 应该快速返回，复杂逻辑放到其他任务
 * - 【安全】event_data 的强制转换容易出错
 *
 * @warning
 * - 【易错】event->data 不一定是 null-terminated 字符串
 *   → 应该使用 event->data_len 而不是 strlen
 * - 【易错】strstr 搜索不准确
 *   → "ON" 可能匹配 "DONE"、"LOAN" 等
 *   → 改进：JSON 解析或更严格的匹配
 * - 【性能】在事件处理中创建任务（xTaskCreate）
 *   → OTA 升级任务可能阻塞，考虑队列通知主任务
 *
 * @see mqtt_app_start
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        /* MQTT 连接成功 */
        /* ESP_LOGI("MQTT", "Connected to Broker"); */

        /* 订阅接收云端命令的主题 */
        esp_mqtt_client_subscribe(client, "/my_home/led_control", 0);

        /* 更新 UI：刷新图标 */
        if (lvgl_port_lock(0))
        {
            lv_label_set_text(ui_mqtt_icon, LV_SYMBOL_REFRESH);
            lvgl_port_unlock();
        }
        break;

    case MQTT_EVENT_DATA:
        /* 【关键】接收到消息，需要快速处理，避免阻塞事件循环 */

        /* 【易错】event->data 的长度由 event->data_len 指定 */
        /* 不能直接用 strlen，因为可能不是 null-terminated */

        if (strstr(event->data, "\"ON\""))
        {
            /* 云端命令：LED 开启（占空比 1222） */
            bsp_led_set_smple(1222);
            last_led_ate = bsp_storage_save_int32("led_power", 1222);
            ESP_LOGI("ACTUATOR", "Cloud Command: LED ON");
        }
        else if (strstr(event->data, "\"OFF\""))
        {
            /* 云端命令：LED 关闭（占空比 0） */
            bsp_led_set_smple(0);
            last_led_ate = bsp_storage_save_int32("led_power", 0);
            ESP_LOGI("ACTUATOR", "Cloud Command: LED OFF");
        }
        else if (strstr(event->data, "\"W_OFF\""))
        {
            /* 云端命令：WiFi 断开 */
            ESP_LOGI("WIFI", "Cloud Command: WIFI OFF");
            esp_wifi_disconnect();
            esp_wifi_stop();
            lv_delay_ms(100);
        }
        else if (strstr(event->data, "\"UPDATE\""))
        {
            /* 云端命令：触发 OTA 固件升级 */
            if (lvgl_port_lock(0))
            {
                lv_label_set_text(ui_temp_label, "Updating...\nDo Not Power Off");
                lvgl_port_unlock();
            }

            /* 创建 OTA 升级任务 */
            xTaskCreate(&ota_update_task, "ota_upadte_task", 8192, NULL, 5, NULL);
        }
        break;

    case MQTT_EVENT_ERROR:
        /* MQTT 错误 */
        ESP_LOGE("MQTT", "Event Error");
        break;

    case MQTT_EVENT_DISCONNECTED:
        /* MQTT 连接断开 */
        if (lvgl_port_lock(0))
        {
            lv_label_set_text(ui_mqtt_icon, LV_SYMBOL_CLOSE);
            lvgl_port_unlock();
        }
        break;

    default:
        break;
    }
}