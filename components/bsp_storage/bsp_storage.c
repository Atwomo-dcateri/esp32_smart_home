/**
 * @file bsp_storage.c
 * @brief NVS Flash 存储驱动模块
 *
 * @details
 * 该模块封装了 ESP32 的 NVS（Non-Volatile Storage）Flash 接口，
 * 用于持久化存储关键配置，如 LED 亮度、WiFi SSID 等。
 *
 * NVS 存储特性：
 * - 非易失性：掉电后数据保留
 * - KVP 模型：Key-Value Pair，按字符串 key 寻址
 * - 分区管理：默认分区名 "nvs"，可自定义
 * - 写入寿命：Flash 块寿命约 10000 次，NVS 驱动自动均衡
 *
 * @note
 * - NVS 读写速度慢（Flash I/O），避免频繁调用
 * - 建议在内存中缓存常用值，仅在修改时写入 NVS
 *
 * @author Your Name
 * @version 1.0.0
 */

#include "nvs_flash.h"   /**< NVS Flash 驱动 */
#include "esp_system.h"  /**< ESP 系统库 */
#include "esp_log.h"     /**< ESP 日志 */
#include "bsp_storage.h" /**< 本模块头文件 */

/** @brief 日志标签 */
static const char *TAG = "STORAGE";

/**
 * @brief 保存 int32 值到 NVS Flash
 *
 * @param[in] key 键名（字符串，唯一标识）
 * @param[in] value 值（int32）
 *
 * @return
 * - ESP_OK：成功
 * - ESP_ERR_NVS_INVALID_NAME：key 名非法
 * - ESP_ERR_NVS_NOT_FOUND：分区不存在
 * - ESP_ERR_NVS_NO_FREE_PAGES：Flash 已满
 *
 * @details
 * 保存 int32 类型的数据到 NVS Flash，具有掉电保护。
 * 工作流程：
 * 1. nvs_open("storage", NVS_READWRITE) 打开存储分区
 * 2. nvs_set_i32() 设置 key-value 对
 * 3. nvs_commit() 写入 Flash（重要！不 commit 数据不会保存）
 * 4. nvs_close() 释放句柄
 *
 * @note
 * - NVS 写入速度慢（10-50ms），避免频繁调用
 * - 建议批量写入多个值，减少 open-commit-close 开销
 * - 现有 key 被覆盖时，自动替换
 *
 * @warning
 * - 【关键】必须调用 nvs_commit()，否则数据不会保存
 *   → 症状：重启后发现数据丢失
 * - 【易错】分区名硬编码为 "storage"
 *   → 若在 bsp_stroge_read_int32 中使用不同的分区名，会读取失败
 *   → 当前代码有 BUG：读函数使用 "stroage"（拼写错误）！
 * - 【设计】NVS 分区在 partitions.csv 中定义
 *   → 若分区不存在或大小过小，操作失败
 *
 *
 * @see bsp_stroge_read_int32
 */
esp_err_t bsp_storage_save_int32(const char *key, int32_t value)
{
    nvs_handle_t my_handle;
    /* 打开 "storage" 分区，模式为读写 */
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);

    if (err != ESP_OK)
    {
        /* 分区不存在或打开失败 */
        return err;
    }

    /* 设置 key-value 对 */
    err = nvs_set_i32(my_handle, key, value);
    if (err == ESP_OK)
    {
        /* 【关键】提交到 Flash 存储 */
        nvs_commit(my_handle);
    }
    /* 释放句柄 */
    nvs_close(my_handle);
    return err;
}

/**
 * @brief 从 NVS Flash 读取 int32 值
 *
 * @param[in] key 键名
 * @param[in] default_val 默认值（key 不存在时返回）
 *
 * @return
 * - key 对应的值（成功）
 * - default_val（key 不存在或读取失败）
 *
 * @details
 * 读取之前保存的 int32 值，若 key 不存在返回默认值。
 *
 * 【BUG 警告】当前代码存在严重拼写错误：
 * - 保存使用分区名："storage"
 * - 读取使用分区名："stroage"（多了个 'a'）
 * → 结果：保存成功，但读取时打开的是不同分区，导致 key 未找到
 * → 现象：系统重启后，读取的总是默认值，无法恢复保存的值
 *
 * @note
 * - 读取速度比写入快（< 10ms）
 * - 错误处理：若 nvs_open 失败，直接返回 default_val
 * - 若 key 不存在，也返回 default_val（不产生错误）
 *
 * @warning
 * - 【严重 BUG】分区名拼写错误："stroage" 应为 "storage"
 *   → 需改为：nvs_open("storage", NVS_READONLY, &my_handle);
 * - 【设计缺陷】无返回错误码，调用者无法区分"key 不存在"和"分区错误"
 *   → 改进：返回 esp_err_t 和通过指针返回值
 *
 * @see bsp_storage_save_int32
 */
esp_err_t bsp_stroge_read_int32(const char *key, int32_t default_val)
{
    nvs_handle_t my_handle;
    int32_t value = default_val;

    /* 【BUG】分区名拼写错误："stroage" 而不是 "storage" */
    esp_err_t err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err == ESP_OK)
    {
        /* 读取 key 对应的值 */
        err = nvs_get_i32(my_handle, key, &value);
        /* 释放句柄 */
        nvs_close(my_handle);
    }
    /* 若分区打开失败或 key 不存在，返回 default_val */
    return err;
}