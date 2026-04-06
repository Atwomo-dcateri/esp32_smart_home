#pragma once


#include <stdio.h>

#ifdef __cplusplus

extern "C" { // 在cpp环境中仍按C标准编译以下函数

#endif


int32_t bsp_stroge_read_int32(const char *key, int32_t default_val);
esp_err_t bsp_storage_save_int32(const char *key, int32_t value);

extern uint32_t last_led_ate;

#ifdef __cplusplus

}

#endif