#ifndef WIFI_MANGER_H
#define WIFI_MANGER_H


#include "esp_err.h"


void wifi_init_sta(void);
void wifi_wait_connected(void);

#endif