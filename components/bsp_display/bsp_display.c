/**
 * @file bsp_display.c
 * @brief BSP 显示模块：I2C SSD1306 + LVGL 初始化与简单 UI 输出。
 *
 * 说明：
 * - 本文件负责初始化 I2C 总线、创建 SSD1306 面板、接入 LVGL Port，并提供 UI 更新接口。
 * - LVGL API 不是线程安全的，因此对 LVGL 的访问需使用 `lvgl_port_lock()/unlock()` 保护。
 */

#include "stdio.h"                 // 标准 IO（本文件目前未直接使用，但保留以兼容历史代码）

#include "bsp_display.h" // 本模块对外头文件（引脚/分辨率/I2C 端口等宏定义）

static lv_display_t *lvgl_disp = NULL;   // LVGL display 句柄（保留以便后续扩展）
static const char *TAG = "BSP_DISPLAY";  // 日志 TAG（用于区分模块日志）

lv_obj_t *ui_temp_label = NULL; // UI：用于显示温湿度/状态的 Label（在 `setup_ui()` 创建）
lv_obj_t *ui_header_bar;
lv_obj_t *ui_wifi_icon;
lv_obj_t *ui_mqtt_icon;
lv_obj_t *ui_main_cont;

bool is_lvgl_ready = false;
/**
 * @brief 初始化 OLED 显示（I2C + SSD1306）并接入 LVGL Port。
 *
 * @details
 * 主要流程：
 * 1) 初始化 I2C 主机（GPIO、上拉、时钟频率）。
 * 2) 创建 esp_lcd I2C Panel IO（配置设备地址与命令/数据位宽等）。
 * 3) 创建 SSD1306 Panel，并 reset/init/点亮。
 * 4) 初始化 LVGL Port，并将该 Panel 注册为 LVGL Display。
 *
 * @note 本函数应在系统启动阶段调用一次。
 */
void bsp_display_init(void) { // 初始化显示硬件与 LVGL 显示适配
    i2c_config_t i2c_cfg = { // 定义 I2C 配置结构体（旧 I2C driver API）
        .mode = I2C_MODE_MASTER, // 设置为主机模式
        .sda_io_num = LCD_PIN_NUM_SDA, // SDA 引脚号（来自 bsp_display.h）
        .scl_io_num = LCD_PIN_NUM_SCL, // SCL 引脚号（来自 bsp_display.h）
        .sda_pullup_en = GPIO_PULLUP_ENABLE, // 使能 SDA 上拉（I2C 必需）
        .scl_pullup_en = GPIO_PULLUP_ENABLE, // 使能 SCL 上拉（I2C 必需）
        .master.clk_speed = 100000, // I2C 时钟频率（100kHz）
        .clk_flags = 0, // 时钟标志位（默认 0）
    }; // i2c_cfg 初始化结束

    ESP_ERROR_CHECK(i2c_param_config(I2C_BUS_PORT, &i2c_cfg)); // 应用 I2C 参数到指定端口
    ESP_ERROR_CHECK(i2c_driver_install(I2C_BUS_PORT, I2C_MODE_MASTER, 0, 0, 0)); // 安装 I2C 驱动（主机）
    vTaskDelay(pdMS_TO_TICKS(100)); // 适当延时，确保 I2C 驱动稳定

    ESP_LOGI(TAG, "Install panel IO"); // 提示：开始创建 Panel IO

    esp_lcd_panel_io_handle_t io_handle = NULL; // Panel IO 句柄（后续用于创建 Panel）

    esp_lcd_panel_io_i2c_config_t io_config = { // 配置 I2C Panel IO：设备地址、相位、位宽等
        .dev_addr = LCD_ADRR, // I2C 设备地址（注意：宏名如拼写错误也会导致问题）
        .control_phase_bytes = 1, // 控制相位字节数（通常 1 字节用于指令/数据控制）
        .lcd_cmd_bits = 8, // 指令位宽（SSD1306 为 8bit command）
        .lcd_param_bits = 8, // 参数位宽（SSD1306 为 8bit data）
        .dc_bit_offset = 6, // D/C 位偏移：用于区分“指令/数据”（依赖具体接线/协议）
        .flags = { // flags 子结构体
            .disable_control_phase = 0, // 0 表示不禁用控制相位（使用控制相位）
        }, // flags 初始化结束
    }; // io_config 初始化结束

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)I2C_BUS_PORT, &io_config, &io_handle)); // 创建 I2C Panel IO

    esp_lcd_panel_handle_t panel_handle = NULL; // SSD1306 Panel 句柄（后续用于 init/onoff）
    esp_lcd_panel_ssd1306_config_t ssd1306_config = { // SSD1306 特定配置
        .height = LCD_V_RES, // 面板高度（像素）
    }; // ssd1306_config 初始化结束

    esp_lcd_panel_dev_config_t panel_config = { // 通用面板配置
        .bits_per_pixel = 1, // SSD1306 单色 1bpp
        .reset_gpio_num = -1, // 未使用硬件复位脚（-1 表示不接）
        .vendor_config = &ssd1306_config, // 指向 SSD1306 vendor 配置
    }; // panel_config 初始化结束

    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle)); // 创建 SSD1306 panel
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle)); // 对面板执行复位（软件复位/序列）

    esp_err_t ret = esp_lcd_panel_init(panel_handle); // 初始化面板（发送 init 序列）
    if (ret != ESP_OK) { // 判断面板初始化是否成功
        ESP_LOGE(TAG, "OLED init failed (0x%x), keep going...", (unsigned)ret); // 记录错误，但不中断（便于调试）
    } else { // 初始化成功
        ESP_LOGI(TAG, "OLED init success"); // 记录成功日志
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true)); // 打开显示（点亮）
    } // if(ret) 结束

    const lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG(); // 获取 LVGL Port 默认配置
    ESP_ERROR_CHECK(lvgl_port_init(&port_cfg)); // 初始化 LVGL Port（创建任务、定时器等）

    const lvgl_port_display_cfg_t disp_cfg = { // LVGL Display 配置：绑定 IO 与 Panel
        .io_handle = io_handle, // 绑定 panel IO（用于刷屏传输）
        .panel_handle = panel_handle, // 绑定 panel 句柄（用于控制/初始化）
        .buffer_size = LCD_H_RES * LCD_V_RES, // 显存缓冲大小（单色时由 port 内部处理像素格式）
        .double_buffer = false, // 是否使用双缓冲（此处使用单缓冲）
        .hres = LCD_H_RES, // 水平分辨率
        .vres = LCD_V_RES, // 垂直分辨率
        .monochrome = true, // 单色屏（SSD1306）
        .rotation = { // 旋转相关设置（在结构体中用子结构体初始化）
            .swap_xy = false, // 是否交换 X/Y
            .mirror_x = true, // 是否镜像 X
            .mirror_y = true, // 是否镜像 Y
        }, // rotation 初始化结束
    }; // disp_cfg 初始化结束

    lvgl_disp = lvgl_port_add_disp(&disp_cfg); // 将面板注册为 LVGL display（返回 display 句柄）
    ESP_LOGI(TAG, "OLED with LVGL initialized"); // 提示：显示初始化完成
} // bsp_display_init 结束

/**
 * @brief 创建简单 UI（背景 + 居中标签）。
 *
 * @details
 * - 该函数创建一个 `lv_label` 并保存到全局 `ui_temp_label`，供后续更新文本。
 * - 如果 UI 已经创建过，将直接返回（避免重复创建对象）。
 *
 * @note 必须在 LVGL 初始化完成后调用（即 `bsp_display_init()` 之后）。
 */
void setup_ui(void) { // 创建 UI 元素（当前仅一个 label）
    if (ui_temp_label != NULL) { // 如果已经创建过 UI，避免重复创建
        return; // 直接返回
    } // 判重结束

    if (!lvgl_port_lock(0)) { // 尝试获取 LVGL 互斥锁（0 表示不等待）
        ESP_LOGW(TAG, "Failed to lock LVGL for setup_ui"); // 记录锁获取失败
        return; // 不持锁无法安全操作 LVGL
    } // lock 判断结束

    lv_obj_t *screen = lv_screen_active(); // 获取当前活动屏幕对象（作为父对象）
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0); // 设置屏幕背景为黑色

    ui_temp_label = lv_label_create(screen); // 在屏幕上创建一个 Label
    lv_label_set_text(ui_temp_label, "System Ready"); // 设置初始文本
    lv_obj_set_style_text_color(ui_temp_label, lv_color_white(), 0); // 设置文本颜色为白色
    lv_obj_align(ui_temp_label, LV_ALIGN_CENTER, 0, 0); // 将 Label 居中对齐

    lvgl_port_unlock(); // 释放 LVGL 互斥锁
    ESP_LOGI(TAG, "setup_ui finished"); // 记录 UI 创建完成
} // setup_ui 结束

/**
 * @brief 更新温湿度显示数据。
 *
 * @param[in] temp 温度值（摄氏度）。
 * @param[in] humi 湿度值（百分比）。
 *
 * @note 需要先调用 `setup_ui()` 创建 `ui_temp_label`。
 */
void bsp_display_update_data(float temp, float humi) { // 刷新 Label 文本以显示温湿度
    if (ui_temp_label == NULL) { // 若 UI 尚未创建
        return; // 避免空指针访问
    } // 判空结束

    int h_int = (int)humi;
    int h_dec = (int)((humi - h_int) * 10);
    int t_int = (int)temp;
    int t_dec = (int)((temp - t_int) * 10);

    if (!lvgl_port_lock(0))
    {           // 获取 LVGL 锁（避免多线程竞争）
        return; // 无锁则不更新
    } // lock 判断结束

    lv_label_set_text_fmt(ui_temp_label, "temp = %d.%dC\n humi = %d.%d%%", t_int, t_dec, h_int, h_dec); // 按格式更新显示文本
    //lv_label_set_text_fmt(ui_temp_label, "temp = %d C\n humi = %d%%", 20, 10); // 按格式更新显示文本
    
    lvgl_port_unlock(); // 解锁释放资源
} // bsp_display_update_data 结束

/**
 * @brief 在屏幕左上角显示一条状态文本。
 *
 * @param[in] status 要显示的状态字符串（以 `\\0` 结尾）。
 *
 * @details
 * - 首次调用时创建一个静态 `status_label`，后续只更新文本以减少对象创建。
 * - 使用字体 `lv_font_montserrat_10`，需要在 `lv_conf.h` 中启用该字体。
 */
void bsp_display_show_status(const char *status) { // 创建/更新状态栏文本
    static lv_obj_t *status_label = NULL; // 静态保存 label 句柄，避免重复创建

    if (status == NULL) { // 若传入空指针
        return; // 不处理
    } // 判空结束

    if (!lvgl_port_lock(0)) { // 获取 LVGL 锁（保护 LVGL 对象操作）
        return; // 无锁则不操作
    } // lock 判断结束

    if (status_label == NULL) { // 第一次调用：尚未创建 label
        status_label = lv_label_create(lv_screen_active()); // 在当前活动屏幕创建 label
        lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0); // 设置字体（传入字体对象地址）
        lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 5, 5); // 左上角对齐并留边距
    } // 创建结束
    
    lv_label_set_text(status_label, status); // 更新状态文本
    lvgl_port_unlock(); // 释放 LVGL 锁
} // bsp_display_show_status 结束

/// @brief 初始化状态栏
/// @param  
void bsp_display_pro_ui_init(void) {
    // 使用阻塞锁，确保在初始化时能拿到锁
    if (lvgl_port_lock(-1)) {
        lv_obj_t *scr = lv_screen_active(); 
        lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

        // 1. 先创建容器（Header Bar）
        ui_header_bar = lv_obj_create(scr); 
        lv_obj_set_size(ui_header_bar, 128, 16);
        lv_obj_align(ui_header_bar, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_opa(ui_header_bar, 0, 0);
        lv_obj_set_style_border_width(ui_header_bar, 0, 0);
        lv_obj_set_style_pad_all(ui_header_bar, 2, 0);
        lv_obj_set_flex_flow(ui_header_bar, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(ui_header_bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        // 2. 【关键：先创建 WiFi 标签】
        ui_wifi_icon = lv_label_create(ui_header_bar);
        lv_label_set_text(ui_wifi_icon, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(ui_wifi_icon, lv_color_white(), 0); // 确认是给 wifi_icon 设置

        // 3. 【关键：必须在这里创建 MQTT 标签】
        // 你之前的代码漏了这一行，导致下一行 set_style 访问了 NULL 指针，引发看门狗错误
        ui_mqtt_icon = lv_label_create(ui_header_bar); 
        lv_label_set_text(ui_mqtt_icon, LV_SYMBOL_CLOSE); 
        lv_obj_set_style_text_color(ui_mqtt_icon, lv_color_white(), 0); 

        // 4. 创建主数据区容器
        ui_main_cont = lv_obj_create(scr);
        lv_obj_set_size(ui_main_cont, 128, 48);
        lv_obj_align(ui_main_cont, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_opa(ui_main_cont, 0, 0); // 确保背景不挡住
        lv_obj_set_style_border_side(ui_main_cont, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_border_color(ui_main_cont, lv_color_white(), 0);

        // 5. 创建主温湿度标签
        ui_temp_label = lv_label_create(ui_main_cont);
        lv_label_set_text(ui_temp_label, "Init...");
        lv_obj_set_style_text_color(ui_temp_label, lv_color_white(), 0);
        lv_obj_center(ui_temp_label);

        lvgl_port_unlock();
        ESP_LOGI("UI", "Pro UI Init Finished"); // 打印这行代表函数顺利跑完
        is_lvgl_ready = true;
    }
}