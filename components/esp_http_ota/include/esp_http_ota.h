#pragma once


#ifdef __cplusplus

extern "C"{

#endif

#define CURRENT_VERSION "1.0.1" // 发布版本前修改
// #define OTA_URL "http://10.189.174.22:8080/ota_v2.bin"
//#define OTA_URL "http://10.13.37.1:8080/ota_v2.bin"
#define OTA_URL "http://host.wokwi.internal:8080/ota_v2.bin"

void ota_task(void *pvParameter);
void validate_image_at_boot(void);
bool system_self_test(void);
void ota_update_task(void *pvParameter);

#ifdef __cplusplus

}

#endif
