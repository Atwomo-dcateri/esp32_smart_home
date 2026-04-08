/**
 * @file main.c
 * @brief 智能家居主应用程序
 *
 * @details
 * 该程序实现了一个完整的 ESP32 智能家居控制系统，包含以下主要功能：
 * - WiFi 连接管理（自动重连、手动切换）
 * - MQTT 客户端通信（实时发送传感器数据）
 * - DHT11 温湿度传感器读取
 * - OLED 显示屏驱动与 UI 更新（基于 LVGL）
 * - LED PWM 控制（支持云端与本地控制）
 * - 按键输入处理（队列驱动事件模型）
 * - OTA 固件升级与回滚验证
 * - NVS Flash 存储（持久化 LED 状态等配置）
 *
 * @author Your Name
 * @version 1.0.0
 * @date 2026-04-08
 *
 * @note
 * - 关键架构：采用 FreeRTOS Queue + Task 模型，主逻辑在 logic_task 中阻塞等待按键事件
 * - WiFi 状态由全局变量 wifi_count 标记（0=在线，1=离线）
 * - LVGL 操作必须使用 lvgl_port_lock/unlock 保护，否则触发临界区冲突
 * - MQTT 发送可能阻塞，故仅在 WiFi 在线时调用
 */

/* ===== 系统库头文件 ===== */
#include "freertos/FreeRTOS.h" /**< FreeRTOS 核心库 */
#include "freertos/queue.h"    /**< FreeRTOS 队列（用于按键事件） */
#include "freertos/task.h"     /**< FreeRTOS 任务调度 */
#include "nvs_flash.h"         /**< NVS Flash 存储（持久化配置） */
#include "esp_log.h"           /**< ESP 日志系统 */

/* ===== 硬件驱动与外设库 ===== */
#include "bsp_led.h"     /**< LED PWM 驱动（GPIO5） */
#include "esp_wifi.h"    /**< WiFi 驱动 */
#include "bsp_button.h"  /**< 按键驱动（GPIO3、GPIO6） */
#include "bsp_sensor.h"  /**< DHT11 温湿度传感器 */
#include "bsp_display.h" /**< OLED SSD1306 显示屏（I2C） */

/* ===== 应用层通信与控制 ===== */
#include "mqtt_handler.h" /**< MQTT 客户端（与云端通信） */
#include "wifi_manger.h"  /**< WiFi 管理器（连接、重连逻辑） */
#include "bsp_storage.h"  /**< NVS 存储接口（保存配置） */
#include "esp_http_ota.h" /**< OTA 固件升级管理 */

/** @brief 日志标签，用于区分不同模块的日志输出 */
static const char *TAG = "APP_MAIN";

/**
 * @brief GPIO 事件队列（按键事件通知）
 *
 * 队列大小：10 个事件
 * 元素类型：uint32_t GPIO 引脚号（GPIO3、GPIO6 等）
 * 用途：ISR 将按键对应的 GPIO 号推送到此队列，逻辑任务阻塞取出并处理
 *
 * @warning 不要在 ISR 外的地方直接操作此队列
 */
QueueHandle_t gpio_evt_queue = NULL;

/** @brief 用于显示屏的调试函数（暂未使用） */
void oled_show(uint8_t x, uint8_t y, char *buf);

/**
 * @brief 保存当前 LED 亮度值
 *
 * 范围：0-8191（13bit PWM）
 * 用途：保存最后一次设置的 LED 值，以便在系统读取或云端下发后恢复
 *
 * @note 同时在 NVS Flash 中持久化（key: "led_power"）
 */
uint32_t last_led_ate;

/**
 * @brief WiFi 连接状态标志（0=在线，1=离线）
 *
 * 含义：
 * - wifi_count == 0：WiFi 已连接，可以发送 MQTT
 * - wifi_count == 1：WiFi 已断开，避免 MQTT 阻塞任务
 *
 * @warning 这是一个简化的状态机，生产环境应使用专业的状态结构
 * @note 当按下按键 GPIO3 时，在这两个状态之间切换
 */
static uint8_t wifi_count = 0;

/**
 * @brief 应用主业务逻辑任务
 *
 * @param[in] arg 任务参数（未使用，传 NULL）
 *
 * @details
 * 本任务是智能家居系统的核心，负责以下功能：
 * 1. 初始化：WiFi、MQTT、显示屏、OTA 验证
 * 2. 事件循环：阻塞等待按键队列，根据不同按键执行相应操作
 * 3. 数据采集与上报：定期读取传感器、更新 UI、发送 MQTT 消息
 * 4. 云端命令响应：通过 MQTT 接收远程控制命令
 *
 * @note
 * - 采用队列驱动模型，避免忙轮询浪费 CPU
 * - LVGL 操作提有互斥锁保护，防止显示内存冲突
 * - MQTT 通信可能阻塞，故仅在 WiFi 连接时调用
 * - WiFi 状态由全局 wifi_count 标记，不建议在其他地方修改
 *
 * @warning
 * - 如果 xQueueReceive 返回 false，说明队列异常，应检查 bsp_button_init
 * - MQTT 连接失败不会导致任务崩溃，但会影响云端通信
 * - NVS 读写性能较低（Flash），避免在热循环中频繁调用
 *
 * @see wifi_init_sta, mqtt_app_start, bsp_dht_read, validate_image_at_boot
 *
 * @return 函数不返回（无限循环）
 */
void logic_task(void *arg)
{
    /* ===== 局部变量声明 ===== */
    uint32_t io_num;  /**< 从队列取出的 GPIO 引脚号 */
    float temp, humi; /**< 温湿度传感器读数 */

    /* ===== 初始化阶段 ===== */

    /* 1. WiFi 初始化与连接 */
    wifi_init_sta();       // 启动 WiFi STA 模式，配置 SSID/密码、注册事件处理器
    wifi_wait_connected(); // 阻塞当前任务，等待获得 IP 地址

    /* 2. 显示屏初始化 */
    bsp_display_init();        // 初始化 I2C、SSD1306 Panel、LVGL Port
    bsp_display_pro_ui_init(); // 创建 UI 元素（Header Bar、WiFi/MQTT 图标等）

    /* 3. MQTT 初始化与连接 */
    mqtt_app_start(); // 连接到 MQTT Broker，注册事件处理器，订阅主题

    /* 4. OTA 固件升级验证 */
    validate_image_at_boot(); // 检查当前固件是否需要回滚（如果上次升级失败）

    /* 5. 显示版本号（可选调试信息） */
    if (lvgl_port_lock(pdMS_TO_TICKS(100)) && is_lvgl_ready)
    {
        lv_label_set_text_fmt(ui_temp_label, "Ver: %s", CURRENT_VERSION);
        lvgl_port_unlock();
    }

    /* ===== 主事件循环 ===== */
    for (;;)
    {

        /* === 事件等待与处理 === */
        /*
         * 关键点：使用 portMAX_DELAY 阻塞等待，而不是轮询
         * - 优点：CPU 利用率低，省电，响应及时
         * - 当按键被按下时，ISR 将 GPIO 号推送到队列，唤醒此任务
         */
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
        {

            /* *** WiFi 控制逻辑（GPIO3） *** */
            /*
             * 【易出错点】GPIO3 是 WiFi 开关按键
             * - 第一次按下：断开 WiFi（wifi_count = 1）
             * - 第二次按下：重新连接 WiFi（wifi_count = 0）
             *
             * 常见错误：
             * 1. 顺序错误：esp_wifi_connect() 必须在 esp_wifi_start() 之后
             * 2. 状态不同步：修改了一处后忘记同步其他地方的 wifi_count
             * 3. 黑屏问题：若未持有 LVGL 锁就更新 UI，可能导致显示异常
             */
            if (io_num == 3)
            {
                if (wifi_count == 0)
                {
                    /* 当前在线 → 断开连接 */
                    wifi_count = 1;
                    ESP_LOGW("WIFI", "Manual Stop...");
                    esp_wifi_disconnect(); // 断开 TCP 连接
                    esp_wifi_stop();       // 关闭 WiFi MAC
                }
                else
                {
                    /* 当前离线 → 重新连接 */
                    wifi_count = 0;
                    ESP_LOGI("WIFI", "Manual Start...");
                    esp_wifi_start();   // 【关键】必须先 start
                    esp_wifi_connect(); // 然后再 connect，否则失败
                }
                continue; /* 跳过传感器读取，避免在网络关闭时发送 MQTT 导致阻塞 */
            }

            /* *** 传感器数据采集与上报（GPIO6 或其他） *** */
            /*
             * 【易出错点】传感器读取需要验证返回值
             * - ESP_OK：成功读取，可以使用数据
             * - 其他值：传感器故障或通信错误，不应使用数据
             */
            if (bsp_dht_read(&temp, &humi) == ESP_OK)
            {
                ESP_LOGI(TAG, "T:%.1f, H:%.1f", temp, humi);

                /* === 云端上报（仅在 WiFi 在线时） === */
                /*
                 * 【易出错点】MQTT 发送是阻塞操作，可能超时
                 * - 如果 WiFi 断开，esp_mqtt_client_publish 可能挂起 1-10 秒
                 * - 解决方案：先检查 wifi_count，只在在线时发送
                 *
                 * 性能考虑：
                 * - mqtt_send_sensor_data 内部调用 esp_mqtt_client_publish
                 * - 该函数可能在网络不稳定时阻塞，影响任务响应性
                 */
                if (wifi_count == 0)
                {
                    mqtt_send_sensor_data(temp, humi); // 发送 JSON 格式数据到 "/my_home/led_control"
                }

                /* === 本地 UI 更新（总是执行） === */
                /*
                 * 【关键】LVGL 操作必须持有互斥锁
                 * - 如果不持锁，LVGL 可能在 UI 线程与主线程间冲突
                 * - 后果：内存崩溃、显示异常、系统重启
                 *
                 * bsp_display_update_data 内部已持锁，此处安全
                 */
                bsp_display_update_data(temp, humi); // 更新屏幕显示温湿度值

                /* === LED 状态恢复 === */
                /*
                 * 【设计说明】LED 状态持久化到 NVS Flash
                 * - 云端或本地修改 LED 时，保存到 Flash（key: "led_power"）
                 * - 系统重启后，从 Flash 恢复上次设置
                 *
                 * 【易出错点】NVS 默认值选择不当
                 * - bsp_stroge_read_int32(..., 0) 如果 Flash 未初始化，用 0 作默认值
                 * - 一般建议用有意义的默认值（如 4096 = 50% 亮度）
                 *
                 * 【性能注意】NVS 读取速度慢（Flash I/O），避免频繁调用
                 * - 可考虑缓存 LED 值到内存，仅在必要时读 Flash
                 */
                last_led_ate = bsp_stroge_read_int32("led_power", 0);
                bsp_led_set_smple(last_led_ate); // 设置 LED 亮度（PWM 占空比）
            }
        }

        /*
         * 【设计语境】为什么没有 vTaskDelay？
         * - 采用事件驱动模型，不需要定时轮询
         * - 按键事件会唤醒任务，无需周期性检查
         * - 这样可以最小化 CPU 功耗，特别适合电池供电场景
         *
         * 如果需要定时采样（如每 5 秒读一次传感器），应考虑：
         * 1. 添加一个计时器任务，定期推送虚拟事件到队列
         * 2. 或使用 FreeRTOS 的 xTaskDelayUntil 实现定时循环
         */
    }
}

/**
 * @brief 应用程序主入口函数
 *
 * @details
 * 该函数是 ESP32 启动后首先执行的代码，负责系统级初始化：
 * 1. NVS Flash 初始化和恢复（持久化存储）
 * 2. 硬件初始化（LED、按键等）
 * 3. IPC 机制初始化（队列、任务）
 * 4. 启动后台逻辑任务
 *
 * @note
 * - 每个 ESP32 应用都需要实现此函数，由 ESP-IDF 框架自动调用
 * - 建议不在此函数中阻塞等待，以免延迟启动过程
 * - 所有耗时的初始化应该在后台任务（如 logic_task）中进行
 *
 * @warning
 * - NVS Flash 初始化失败会导致配置丢失，但系统仍可运行
 * - 队列创建失败会导致按键事件无法正确处理，系统异常
 *
 * @see logic_task, bsp_button_init, bsp_led_init, nvs_flash_init
 *
 * @return 无返回值（启动后一直在运行）
 */
void app_main(void)
{

    /* ===== 第一阶段：NVS Flash 存储初始化 ===== */
    /*
     * 【关键点】NVS Flash 用于持久化配置，包括：
     * - WiFi SSID/密码
     * - LED 亮度值
     * - 系统时间
     * - OTA 升级标志
     *
     * 初始化流程：
     * 1. 调用 nvs_flash_init() 打开默认 NVS Partition
     * 2. 如果 Flash 格式不兼容或无空间，执行擦除和重新初始化
     * 3. 若再次失败，记录错误并继续（有风险，可能丢失配置）
     *
     * 【易出错点】
     * - 如果多次调用 nvs_flash_init，第二次会返回错误
     * - 解决方案：使用 ESP_ERROR_CHECK 或手动检查错误码
     * - 不应该在此处调用 nvs_flash_erase，除非确认需要重置
     */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        /*
         * 两种情况下需要擦除重初始化：
         * 1. ESP_ERR_NVS_NO_FREE_PAGES：Flash 分区已满，无法分配新页面
         *    → 原因：长期运行后频繁写入导致
         *    → 解决：擦除后重新初始化
         *
         * 2. ESP_ERR_NVS_NEW_VERSION_FOUND：NVS 版本不兼容
         *    → 原因：ESP-IDF 版本升级，NVS 格式改变
         *    → 解决：擦除旧数据，创建新格式
         *
         * 【警告】擦除会丢失所有持久化数据！
         */
        ESP_ERROR_CHECK(nvs_flash_erase()); // 完整擦除 NVS Partition
        ret = nvs_flash_init();             // 重新初始化
    }
    ESP_ERROR_CHECK(ret); // 若仍失败，任务会在此卡住并重启

    /* ===== 第二阶段：硬件外设初始化 ===== */

    /* LED 初始化 */
    /*
     * 【硬件配置】
     * - GPIO 引脚：GPIO5
     * - 功能：LEDC PWM（Low Speed PWM Controller）
     * - 频率：5 kHz
     * - 精度：13-bit（0-8191）
     * - 用途：控制 LED 亮度
     *
     * 初始化内容（见 bsp_led_init）：
     * 1. 配置 LEDC 定时器：速率、分辨率、频率
     * 2. 配置 LEDC 通道：绑定到 GPIO5、设置初始占空比为 0
     * 3. 启动 PWM 输出
     *
     * 【易出错点】
     * - GPIO5 不能与其他专用功能冲突（如 JTAG 引脚）
     * - ledc_set_duty 设置的是占空比，需配合 ledc_update_duty 刷新
     * - 占空比范围必须在 [0, 8191] 内
     */
    bsp_led_init();

    /* ===== 第三阶段：IPC 通信机制初始化 ===== */

    /* 创建 GPIO 事件队列 */
    /*
     * 【设计意图】
     * - 采用队列而非全局变量存储按键事件，确保事件不丢失
     * - ISR 可以快速推送事件，不需要执行复杂逻辑
     * - 主任务在空闲时阻塞等待，充分利用 CPU 及时性和省电特性
     *
     * 【参数说明】
     * - 队列深度（10）：最多缓存 10 个按键事件
     *   → 如果快速连续按键超过 10 次，第 11 个事件会丢失
     *   → 解决：可增大队列深度，但会占用更多内存
     *
     * - 元素大小（sizeof(uint32_t)）：每个事件存储一个 GPIO 引脚号
     *   → GPIO 号的范围：0-48（ESP32-S3）
     *   → 可存储多个不同的 GPIO 号，由 bsp_button_init 的 ISR 推送
     *
     * 【易出错点】
     * - 若 xQueueCreate 返回 NULL，说明内存不足
     *   → 后果：gpio_evt_queue 为空，无法正确处理按键
     * - 应该检查 (gpio_evt_queue == NULL) 并在启动时报错
     */
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    if (gpio_evt_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create GPIO queue!");
        // 可选：return 或 while(1) 阻塞等待重启
    }

    /* ===== 第四阶段：外设驱动初始化 ===== */

    /* 按键初始化 */
    /*
     * 【按键配置】
     * - GPIO 引脚：GPIO3（WiFi 开关）、GPIO6（传感器读取）
     * - 功能：数字输入，上拉电阻启用
     * - 触发方式：下降沿（按键按下时）
     * - 中断处理：ISR 将 GPIO 号推送到 gpio_evt_queue
     *
     * 初始化内容（见 bsp_button_init）：
     * 1. GPIO 配置：输入模式，启用上拉，下降沿触发
     * 2. ISR 驱动安装：esp_idf_interrupt_service
     * 3. GPIO3 和 GPIO6 的中断处理器注册
     *
     * 【工作流程】
     * - 用户按下按键
     * - GPIO 电压从 HIGH 变为 LOW
     * - 触发下降沿中断
     * - ISR 执行：xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL)
     * - logic_task 从队列中取出事件，处理对应的按键逻辑
     *
     * 【易出错点】
     * - ISR 优先级过高可能影响其他中断（如 WiFi ISR）
     *   → 建议 ISR 优先级低于 WiFi（通常 < 14）
     * - 未安装 ISR 驱动导致中断不触发
     *   → 解决：确保 gpio_install_isr_service 在注册处理器之前调用
     * - 机械抖动（debounce）可能导致多次中断
     *   →当前代码未处理，可在 ISR 或主任务中添加消抖逻辑
     */
    bsp_button_init();

    /* ===== 第五阶段：启动后台任务 ===== */

    /* 创建逻辑任务（logic_task） */
    /*
     * 【任务配置】
     * - 函数指针：logic_task
     * - 任务名称："logic_task"（仅用于调试）
     * - 栈大小：8192 字节
     *   → 计算：局部变量 + 函数调用栈 + 库函数开销
     *   → 建议：至少 4096，复杂逻辑用 8192+
     *   → 过小导致栈溢出（内存崩溃）；过大浪费 RAM
     *
     * - 参数：NULL（arg 在 logic_task 中未使用）
     *
     * - 优先级：10（1-24，数字越大优先级越高）
     *   → 该优先级高于大多数系统任务（如 WiFi）
     *   → 但低于时间关键任务（如时钟中断处理）
     *   → 权衡：响应快但不抢占系统任务
     *
     * - 任务句柄：NULL（不需要后续操作该任务，如暂停、删除）
     *   → 如果将来需要操作此任务，应将 &logic_task_handle 传入
     *   → 例如：xTaskCreate(..., &logic_task_handle);
     *          然后：vTaskSuspend(logic_task_handle); // 暂停任务
     *
     * 【执行流程】
     * 1. xTaskCreate 返回后，logic_task 被加到就绪队列
     * 2. vTaskStartScheduler（由 app_main 返回前自动调用）启动调度器
     * 3. 调度器将 logic_task 分配到 CPU，开始执行 wifi_init_sta() 等
     *
     * 【易出错点】
     * - 栈大小不足（< 4096）：logic_task 可能因栈溢出而崩溃
     *   → 症状：重启循环、看门狗重启、内存错误
     *   → 检查：在配置中增加栈大小，测试稳定性
     *
     * - 任务优先级设置不当：
     *   → 过高（> 20）：可能阻塞 WiFi/网络中断处理
     *   → 过低（< 5）：响应延迟，用户感觉卡顿
     *
     * - 任务入口函数 logic_task 中有无限循环（for(;;)）
     *   → 这是正常的，FreeRTOS 任务通常不返回
     *   → 若任务意外返回，FreeRTOS 会自动删除它
     */
    xTaskCreate(logic_task, "logic_task", 8192, NULL, 10, NULL);

    /* 打印启动完成消息 */
    /*
     * 【日志说明】
     * - TAG = "APP_MAIN"：标注日志来自主程序
     * - ESP_LOGI：INFO 级别日志（会在串口输出）
     * - 内容：提示用户系统已就绪，可以开始交互
     *
     * 【调试建议】
     * - 可在此处添加更多诊断信息，如 RAM/ROM 使用情况、任务列表等
     * - 例如：vTaskList，打印所有任务的栈使用情况
     */
    ESP_LOGI(TAG, "Smart Home Terminal Ready. Press the Button to Sample.");
}