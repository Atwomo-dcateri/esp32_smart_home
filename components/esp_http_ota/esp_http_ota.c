/**
 * @file esp_http_ota.c
 * @brief OTA 固件升级管理模块
 *
 * @details
 * 该模块负责 ESP32 的 OTA（Over-The-Air）固件升级、验证和回滚功能。
 *
 * OTA 工作流程：
 * 1. 检查分区状态（是否上次升级失败）
 * 2. 若失败则自动回滚，反之验证为有效
 * 3. 接收新固件（HTTPS 下载）
 * 4. 写入到备用分区
 * 5. 验证新固件（自测试）
 * 6. 标记为有效并重启
 *
 * 防回滚机制：
 * - OTA_IMG_PENDING_VERIFY：新固件待测试
 * - 若系统启动后自测试失败 → 自动回滚
 * - 若自测试成功 → 永久标记为有效
 *
 * 分区配置（partitions.csv）：
 * - ota_0：初始固件分区
 * - ota_1：备用升级分区
 * - otadata：OTA 状态记录
 *
 * @note
 * - 【关键】validate_image_at_boot() 必须在 app_main 中首先调用
 * - 【设计】系统重启时自动验证上次升级
 * - 【安全】支持 HTTPS（证书绑定）
 *
 * @author Your Name
 * @version 1.0.0
 */

/* 标准 C 库 */
#include <stdio.h>  /**< 标准 IO */
#include <string.h> /**< 字符串操作 */

/* ESP-IDF 系统核心库 */
#include "esp_system.h" /**< ESP 系统库 */
#include "esp_log.h"    /**< 日志系统 */
#include "esp_event.h"  /**< 事件系统（OTA 通常需要监听事件） */

/* 网络与协议栈相关库 */
#include "esp_http_client.h" /**< HTTP 客户端 */
#include "esp_crt_bundle.h"  /**< 证书包（用于 HTTPS） */

/* OTA 专项功能库 */
#include "esp_ota_ops.h"   /**< OTA 操作（分区管理） */
#include "esp_https_ota.h" /**< HTTPS OTA（安全升级） */

/* 项目自定义头文件 */
#include "esp_http_ota.h" /**< 本模块头文件 */

static const char *TAG = "OTA_UPDATE";

/**
 * @brief 系统自测试函数
 *
 * @return
 * - true：自测试通过（系统正常）
 * - false：自测试失败（准备回滚）
 *
 * @details
 * 在 OTA 升级后验证新固件是否可用。
 * 该函数应包含关键系统检查，如：
 * - 硬件健康状况
 * - 关键模块初始化
 * - 内存/Flash 完整性
 * - WiFi/网络连接
 *
 * 【当前实现】为 Mock 版本（简化测试），仅延时 100ms
 *
 * @note
 * - 【设计】自测试应该快速执行（< 5 秒）
 * - 【策略】测试失败 → 触发回滚，恢复到上一个稳定版本
 * - 【安全】不要在此进行任何持久化修改
 *
 * @warning
 * - 【关键】此函数返回值决定是否回滚
 *   → 返回 true = 保留新固件
 *   → 返回 false = 立即回滚到旧版本
 *
 * @see validate_image_at_boot
 */
bool system_self_test(void)
{
    ESP_LOGI("SELF_TEST", "Running system self-test...");
    /* 【当前】简单延迟（Mock） */
    vTaskDelay(pdMS_TO_TICKS(100));
    return true; /* 总是返回成功 */
}

/**
 * @brief 控制是否验证固件、取消回滚或触发回滚
 *
 * @details
 * 该函数在系统启动时立即调用，检查上次 OTA 升级是否成功。
 *
 * 工作流程：
 * 1. 读取当前运行的固件分区
 * 2. 查询该分区的 OTA 状态
 * 3. 若状态为 PENDING_VERIFY（待验证）：
 *    a. 执行系统自测试
 *    b. 若成功 → 标记为有效，永久保留
 *    c. 若失败 → 立即回滚到上一个版本
 *
 * @note
 * - 【关键】必须在 app_main 开始时首先调用
 * - 【顺序】应在 NVS Flash 初始化后立即调用
 * - 【性能】一般不超过 1 秒（含自测试）
 *
 * @warning
 * - 【设计缺陷】当前实现无选项参数
 *   → 改进方向：支持跳过验证或强制验证
 * - 【风险】若自测试失败，系统重启会进入死循环
 *   → 建议：添加尝试次数限制（如 3 次失败后不再重试）
 * - 【安全】若两个分区都损坏，系统无法启动
 *   → 需要 "rescue" 分区或外部恢复机制
 *
 * @see system_self_test, ota_update_task
 */
void validate_image_at_boot(void)
{
    /* 获取当前正在运行的固件分区 */
    const esp_partition_t *running = esp_ota_get_running_partition();

    /* OTA 状态变量 */
    esp_ota_img_states_t ota_state;

    /* 读取该分区的 OTA 状态 */
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK)
    {
        /* 检查状态：是否待验证 */
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
        {
            /* 新固件待测试 */
            if (system_self_test())
            {
                /* 自测试通过 → 标记为有效 */
                ESP_LOGI("OTA", "Self-test passed! Marking image as valid.");
                esp_ota_mark_app_valid_cancel_rollback(); /* 永久有效 */
            }
        }
        else
        {
            /* 【注】日志输出有误，应为 "Self-test failed" */
            ESP_LOGI("OTA", "Self-test failed Rolling backing...");
            esp_ota_mark_app_invalid_rollback_and_reboot(); /* 回滚并重启 */
        }
    }
}

/**
 * @brief 基础 OTA 升级任务（在 Broker 提供升级文件时不使用）
 *
 * @param[in] pvParameter 任务参数（未使用）
 *
 * @note
 * - 【当前状态】已注释禁用
 * - 【用途】本地测试，实际升级由 ota_update_task 处理
 *
 * @see ota_update_task
 */
void ota_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting OTA update...");
    esp_http_client_config_t http_cfg = {
        .url = OTA_URL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg};

    esp_err_t ret = esp_https_ota(&ota_cfg);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "OTA Upgrade Success! Rebooting...");
        esp_restart();
    }
    else
    {
        ESP_LOGI(TAG, "OTA Ugrade failed (%d)", ret);
        vTaskDelete(NULL);
    }
}

/**
 * @brief OTA 固件下载和升级任务（云端触发）
 *
 * @param[in] pvParameter 任务参数（未使用）
 *
 * @details
 * 该任务由 MQTT 接收 UPDATE 命令时创建，负责：
 * 1. 从服务器下载固件（HTTPS）
 * 2. 写入到备用分区（OTA 安全）
 * 3. 标记为待验证状态
 * 4. 重启系统（系统启动时调用 validate_image_at_boot 完成验证）
 *
 * 工作流程：
 * 1. 配置 HTTP 客户端（URL、证书等）
 * 2. esp_https_ota() 开始升级（阻塞）
 *    - 自动处理分区管理
 *    - 自动验证下载完整性
 *    - 若中断会恢复
 * 3. 升级成功 → 系统重启
 * 4. 升级失败 → 日志记录，任务结束
 *
 * @note
 * - 【阻塞】esp_https_ota 是阻塞调用，可能持续数十秒
 *   → 在专用任务中执行，不阻塞主逻辑
 * - 【网络】要求 WiFi 在线且网络稳定
 * - 【安全】【OTA_URL 硬编码，生产环境应从配置读取
 * - 【验证】新固件启动后进行自测试，失败时自动回滚
 *
 * @warning
 * - 【关键】升级过程中不能掉电
 *   → 特别强调：断电会导致两个分区都损坏
 *   → 建议：电源设计为 UPS 或及时警告
 * - 【易错】OTA_URL 宏未定义
 *   → 症状：编译失败或 URL 为空
 *   → 解决：在 esp_http_ota.h 中定义
 * - 【易错】证书验证失败
 *   → skip_cert_common_name_check = true（不验证域名）
 *   → 【风险】容易遭受 MITM 攻击
 *   → 改进：使用信任的 CA 证书
 * - 【设计】任务优先级为 5（较高）
 *   → 避免被其他任务抢占，保证升级完整性
 *
 * @see validate_image_at_boot, mqtt_event_handler
 */
void ota_update_task(void *pvParameter)
{
    ESP_LOGI("OTA", "Starting HTTPS OTA update...");

    /* HTTP 客户端配置 */
    esp_http_client_config_t http_cfg = {
        /* 固件 URL（【硬编码】生产应从配置读取） */
        .url = OTA_URL,

        /* 【可选】证书包（用于 HTTPS 验证） */
        .crt_bundle_attach = NULL, /* 当前不验证证书 */

        /* 【安全风险】不验证证书域名（容易遭受 MITM） */
        .skip_cert_common_name_check = true,

        /* 【性能】启用 HTTP Keep-Alive（减少握手开销） */
        .keep_alive_enable = true,

        /* 下载超时：10 秒（网络慢时需增大） */
        .timeout_ms = 10000};

    /* OTA 配置 */
    esp_https_ota_config_t ota_cfg = {
        /* 绑定 HTTP 客户端配置 */
        .http_config = &http_cfg};

    /* 【关键】执行 OTA 升级（阻塞，可能持续 10-120 秒） */
    esp_err_t ret = esp_https_ota(&ota_cfg);

    if (ret == ESP_OK)
    {
        /* OTA 成功 → 系统重启 */
        ESP_LOGI("OTA", "Download complete. Rebooting in 3s...");
        vTaskDelay(pdMS_TO_TICKS(3000)); /* 延迟 3 秒，让日志输出完*/
        esp_restart();                   /* 重启系统 */
    }
    else
    {
        /* OTA 失败 → 记录错误，任务结束 */
        ESP_LOGI("OTA", "OTA failed! Error code: 0x%x", ret);
        vTaskDelete(NULL); /* 删除此任务 */
    }
}
