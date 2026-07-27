#ifndef __INCLUDE_DRIVER__ESP8266_H__
#define __INCLUDE_DRIVER__ESP8266_H__

#include "common.h"
#include "hw/my_uart.h"

typedef enum {
    ESP8266_OK = 0,
    ESP8266_ERROR,
    ESP8266_TIMEOUT,
    ESP8266_BUSY
} esp8266_result_t;

typedef enum {
    ESP8266_WIFI_MODE_STA = 1,
    ESP8266_WIFI_MODE_AP = 2,
    ESP8266_WIFI_MODE_STA_AP = 3
} esp8266_wifi_mode_t;

typedef struct {
    uint8_t uart_ch;

    char rx_buf[512];
    uint16_t rx_len;

    uint8_t is_ready;
    uint8_t is_connected;
} esp8266_t;

void esp8266_Init(esp8266_t *ctx, uint8_t uart_ch);
void esp8266_Process(esp8266_t *ctx);

esp8266_result_t esp8266_SendCmd(
    esp8266_t *ctx,
    const char *cmd,
    const char *expect,
    uint32_t timeout_ms);

esp8266_result_t esp8266_TestAT(esp8266_t *ctx);
esp8266_result_t esp8266_Restart(esp8266_t *ctx);
esp8266_result_t esp8266_SetMode(esp8266_t *ctx, esp8266_wifi_mode_t mode);

esp8266_result_t esp8266_JoinAP(esp8266_t *ctx, const char *ssid, const char *password);
esp8266_result_t esp8266_StartTCP(esp8266_t *ctx, const char *ip, uint16_t port);
esp8266_result_t esp8266_SendData(esp8266_t *ctx, const char *data);
esp8266_result_t esp8266_Close(esp8266_t *ctx);

#endif //__INCLUDE_DRIVER__ESP8266_H__
