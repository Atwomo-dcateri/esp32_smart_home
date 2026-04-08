/**
 * @file bsp_button.c
 * @brief 按键驱动模块 - GPIO 中断处理
 *
 * @details
 * 该模块负责处理两个物理按键的输入：
 * - GPIO6：传感器读取按键
 * - GPIO3：WiFi 开关按键
 *
 * 工作原理：采用中断驱动模型
 * 1. 配置 GPIO 下降沿中断（按键按下时）
 * 2. ISR 将 GPIO 号推送到全局队列
 * 3. logic_task 从队列取出事件并处理
 *
 * @author Your Name
 * @version 1.0.0
 */

#include "bsp_button.h"        /**< 本模块头文件 */
#include "driver/gpio.h"       /**< GPIO 驱动 */
#include "freertos/FreeRTOS.h" /**< FreeRTOS 核心 */
#include "freertos/queue.h"    /**< FreeRTOS 队列 */

/** @brief GPIO6 - 传感器读取按键 */
#define BIN_GPIO1 6

/** @brief GPIO3 - WiFi 开关按键 */
#define BIN_GPIO2 3

/** @brief 外部全局队列句柄（在 main.c 中创建） */
extern QueueHandle_t gpio_evt_queue;

/**
 * @brief GPIO 中断服务程序（ISR）
 * @param[in] arg GPIO 引脚号（作为中断参数传入）
 * @details
 * 当按键被按下时触发下降沿中断，ISR 将 GPIO 号推送到队列。
 * 【执行流程】
 * 1. GPIO 电压 HIGH → LOW（按键按下）
 * 2. ESP32 自动触发 ISR
 * 3. 将 GPIO 号转换为 uint32_t
 * 4. xQueueSendFromISR 推送到 gpio_evt_queue
 * 5. logic_task 被唤醒并取出事件
 *
 * @warning
 * - 【关键】ISR 必须快速返回（< 100us），不能阻塞
 * - 不能调用 vTaskDelay、mutex 等阻塞函数
 * - 若 gpio_evt_queue 为 NULL，推送失败（无错误发出）
 *
 * @see bsp_button_init, gpio_evt_queue
 */
static void gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;                  /* 强制转换 void* 为 GPIO 号 */
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL); /* ISR-safe 队列操作 */
}

/**
 * @brief 按键初始化函数
 *
 * @details
 * 配置 GPIO3 和 GPIO6 为下降沿中断输入，启用上拉电阻。
 *
 * 初始化流程：
 * 1. gpio_config() 配置 GPIO 硬件
 * 2. gpio_install_isr_service() 安装全局 ISR 驱动（全局只调用一次）
 * 3. gpio_isr_handler_add() 为各 GPIO 注册中断回调
 *
 * @note
 * - 必须在 app_main 中的其他任务启动之前调用
 * - gpio_evt_queue 应该在此函数之前创建
 *
 * @warning
 * - 【关键】若 gpio_evt_queue 未创建或为 NULL，ISR 推送会失败 → 按键无响应
 * - 【易错】机械抖动导致多次中断 → 建议在 logic_task 中软件消抖（50ms 间隔）
 * - 【易错】ISR 优先级过高影响 WiFi → 当前优先级 0（默认，安全）
 */
void bsp_button_init(void)
{
    /* === 步骤 1：配置 GPIO === */
    gpio_config_t io_conf = {
        /* 中断类型：下降沿（按键从 HIGH 变为 LOW） */
        .intr_type = GPIO_INTR_NEGEDGE,

        /* GPIO 模式：输入 */
        .mode = GPIO_MODE_INPUT,

        /* GPIO 位掩码：GPIO6 和 GPIO3 */
        /* 1ULL << 6 | 1ULL << 3 = 0x0048（第 3、6 位为 1） */
        .pin_bit_mask = (1ULL << BIN_GPIO1 | 1ULL << BIN_GPIO2),

        /* 启用上拉电阻：按键释放时保持 HIGH，按下时拉低 */
        .pull_up_en = 1};

    /* === 步骤 2：应用配置 === */
    gpio_config(&io_conf);

    /* === 步骤 3：安装全局 ISR 驱动 === */
    /* 【关键】gpio_install_isr_service 全局执行一次，不能重复调用 */
    gpio_install_isr_service(0);

    /* === 步骤 4：为各 GPIO 注册中断处理器 === */
    /* gpio_isr_handler_add(GPIO_NUM, callback, arg) */
    /* 当 alarm 中继时触发事件 GPIO_NUM，调用 callback(arg) */
    gpio_isr_handler_add(BIN_GPIO1, gpio_isr_handler, (void *)BIN_GPIO1); /* GPIO6 */
    gpio_isr_handler_add(BIN_GPIO2, gpio_isr_handler, (void *)BIN_GPIO2); /* GPIO3 */
}
