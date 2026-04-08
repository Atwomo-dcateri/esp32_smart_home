/**
 * @file bsp_led.c
 * @brief LED PWM 驱动模块 - LEDC 控制
 *
 * @details
 * 该模块负责通过 ESP32 硬件 PWM（LEDC - LED PWM Controller）控制 LED 亮度。
 *
 * 硬件配置：
 * - GPIO 引脚：GPIO5
 * - PWM 频率：5 kHz
 * - 精度：13-bit（0-8191，约 0-100% 亮度）
 * - 驱动：LEDC Low Speed Component
 *
 * 工作模式：
 * - bsp_led_set_smple()：直接设置占空比，立即生效
 * - （已注释）bsp_led_set_breath()：渐变呼吸灯，但因 IRAM 不足而禁用
 *
 * @note
 * - LED 亮度值范围：0-8191（0% 到 100%）
 * - ledc_set_duty() 仅改变缓冲值，需 ledc_update_duty() 刷新到硬件
 * - 建议：使用缓存而非频繁调用 NVS 读取 LED 值
 *
 * @author Your Name
 * @version 1.0.0
 */

#include "bsp_led.h"     /**< 本模块头文件 */
#include "driver/ledc.h" /**< LEDC PWM 驱动 */

/**
 * @brief LED 初始化函数
 *
 * @details
 * 配置 LEDC 定时器和通道，初始化 GPIO5 PWM 输出。
 *
 * 初始化内容：
 * 1. LEDC 定时器：频率 5kHz，精度 13-bit，自动时钟选择
 * 2. LEDC 通道：绑定 GPIO5，初始占空比 0
 * 3. 启用 PWM 输出
 *
 * @note
 * - 必须在 app_main 中首先调用，早于 xTaskCreate
 * - 初始化后 LED 亮度为 0（灭）
 * - 时钟自动选择（LEDC_AUTO_CLK），无需手动配置
 *
 * @warning
 * - 【关键】ledc_set_duty 设置缓冲值，需 ledc_update_duty 才能写入硬件
 * - 【易错】占空比超过 8191 会导致不可预测行为 → 需范围检查
 * - 【设计】呼吸灯功能已禁用，原因：IRAM 不足导致中断处理函数溢出
 */
void bsp_led_init(void)
{
    /* === 阶段 1：LEDC 定时器配置 === */
    /* 定时器产生 PWM 时钟信号，为通道提供时间基准 */
    ledc_timer_config_t ledc_timer = {
        /* 速率：低速模式（适合 LED 控制） */
        .speed_mode = LEDC_LOW_SPEED_MODE,

        /* 精度：13-bit，即占空比范围 0-8191 */
        .duty_resolution = LEDC_TIMER_13_BIT,

        /* 定时器号：TIMER_0（ESP32 支持多个定时器） */
        .timer_num = LEDC_TIMER_0,

        /* 频率：5kHz（人眼感知阈值在 60Hz 以上，5kHz 完全平滑） */
        .freq_hz = 5000,

        /* 时钟源：自动选择（驱动根据频率自适应） */
        .clk_cfg = LEDC_AUTO_CLK};
    ledc_timer_config(&ledc_timer);

    /* === 阶段 2：LEDC 通道配置 === */
    /* 通道将定时器的 PWM 信号输出到指定 GPIO */
    ledc_channel_config_t ledc_channel = {
        /* 速率：与定时器一致 */
        .speed_mode = LEDC_LOW_SPEED_MODE,

        /* 通道号：CHANNEL_0 */
        .channel = LEDC_CHANNEL_0,

        /* 关联的定时器：TIMER_0 */
        .timer_sel = LEDC_TIMER_0,

        /* 中断：禁用（LED 不需要中断） */
        .intr_type = LEDC_INTR_DISABLE,

        /* GPIO 引脚号：GPIO5 */
        .gpio_num = 5,

        /* 初始占空比：0（LED 初始灭） */
        .duty = 0,

        /* 高点位置：0（标准 PWM 波形） */
        .hpoint = 0};
    ledc_channel_config(&ledc_channel);

    /* 【说明】不调用 ledc_fade_func_install，见下方注释代码 */
}

/**
 * @brief LED 呼吸灯函数（已禁用）
 * @details
 * 使用 LEDC 硬件渐变功能实现流畅的呼吸灯效果。
 * @note
 * - 此函数已注释禁用，原因：
 *   1. IRAM（内存中的 RAM）空间不足
 *   2. 呼吸灯的中断处理函数被放入 Flash（速度慢）
 *   3. 中断延迟过长导致看门狗重启（系统崩溃）
 * @alternative 解决方案：在主任务中软件实现渐变
 *
void bsp_led_set_breath(uint32_t duty) {
    ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty, 500);
    ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
}
 */

/**
 * @brief LED 直接设置亮度（推荐）
 *
 * @param[in] duty 占空比值，范围 0-8191
 *  - 0：LED 灭
 *  - 4096：约 50% 亮度
 *  - 8191：最大亮度（100%）
 *
 * @details
 * 直接设置 LED 亮度，立即生效。无需中断，稳定可靠。
 *
 * 工作原理：
 * 1. ledc_set_duty() 更新通道的占空比缓冲值
 * 2. ledc_update_duty() 将缓冲值写入硬件逻辑
 * 3. 下一个 PWM 周期开始时，新占空比生效
 * \n延迟 < 200us（5kHz 周期是 200us）
 *
 * @note
 * - 安全、快速、不占用中断资源
 * - 可在任何时刻调用，包括中断处理中（避免频繁调用）
 * - 建议配合 NVS 持久化存储 LED 值
 *
 * @warning
 * - 【易错】只调用 ledc_set_duty 而不调用 ledc_update_duty
 *   → 占空比不会改变，LED 亮度无变化
 * - 【易错】传入超过 8191 的值
 *   → 会被截断或导致不可预测行为 → 需参数验证
 * - 【设计】不建议高频调用（> 1000Hz）
 *   → 原因：虽然可行，但浪费 CPU 和 Flash 写入周期
 *
 * @see bsp_led_init
 */
void bsp_led_set_smple(uint32_t duty)
{
    /* 设置占空比缓冲值，范围 0 到（1 << 13）- 1 = 8191 */
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);

    /* 【关键】更新硬件，缓冲值 → 硬件寄存器 */
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}