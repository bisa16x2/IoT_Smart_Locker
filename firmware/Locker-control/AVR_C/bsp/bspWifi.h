#ifndef __INCLUDE_BSP__BSP_WIFI_H__
#define __INCLUDE_BSP__BSP_WIFI_H__

#include "common.h"
#include "access_config.h"

void bspWifiInit(const access_config_t *config);
bool bspWifiConnect(void);
bool bspWifiSend(const char *data);
bool bspWifiIsConnected(void);

#endif //__INCLUDE_BSP__BSP_WIFI_H__
