#pragma once


#ifdef __cplusplus

extern "C"{

#endif

#define CURRENT_VERSION "1.0.1" // 发布版本前修改
#define OTA_URL "http://your-server.com/fireware.bin"

void ota_task(void *pvParameter);
void validate_image_at_boot(void);
bool system_self_test(void);
void ota_uppdate_task(void *pvParameter);

#ifdef __cplusplus

}

#endif