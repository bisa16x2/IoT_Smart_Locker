#include "bsp/bspWifi.h"
#include "driver/esp8266.h"
#include "hw/hw.h"

static const char *bspWifiResultString(esp8266_result_t result);
static bool bspWifiCheckResult(esp8266_result_t result, const char *step);
static void bspWifiPrintError(const char *step);

static esp8266_t bsp_wifi_ctx;
static bool bsp_wifi_connected = false;
static const access_config_t *bsp_wifi_config = 0;

// Wi-Fi 설정 및 ESP8266 context 초기화
void bspWifiInit(const access_config_t *config) {
    bsp_wifi_config = config;
    bsp_wifi_connected = false;

    esp8266_Init(&bsp_wifi_ctx, (config != 0) ? config->wifi_uart_ch : 0U);
}

// AP 및 TCP server 연결 후 login
bool bspWifiConnect(void) {
    char login_msg[40];
    int len;

    if ((bsp_wifi_config == 0) ||
        (bsp_wifi_config->ap_ssid == 0) ||
        (bsp_wifi_config->ap_pass == 0) ||
        (bsp_wifi_config->server_name == 0) ||
        (bsp_wifi_config->logid == 0) ||
        (bsp_wifi_config->passwd == 0)) {
        return false;
    }

    bsp_wifi_connected = false;
    esp8266_Init(&bsp_wifi_ctx, bsp_wifi_config->wifi_uart_ch);

    if (bspWifiCheckResult(esp8266_TestAT(&bsp_wifi_ctx), "AT") == false) {
        return false;
    }

    if (bspWifiCheckResult(esp8266_SetMode(&bsp_wifi_ctx, ESP8266_WIFI_MODE_STA), "MODE") == false) {
        return false;
    }

    if (bspWifiCheckResult(esp8266_JoinAP(&bsp_wifi_ctx, bsp_wifi_config->ap_ssid, bsp_wifi_config->ap_pass), "AP") == false) {
        return false;
    }

    if (bspWifiCheckResult(esp8266_StartTCP(&bsp_wifi_ctx, bsp_wifi_config->server_name, bsp_wifi_config->server_port), "TCP") == false) {
        return false;
    }

    len = snprintf(login_msg, sizeof(login_msg), "[%s:%s]", bsp_wifi_config->logid, bsp_wifi_config->passwd);
    if ((len <= 0) || (len >= (int)sizeof(login_msg))) {
        bspWifiPrintError("LOGIN_MSG");
        return false;
    }

    if (bspWifiCheckResult(esp8266_SendData(&bsp_wifi_ctx, login_msg), "LOGIN") == false) {
        return false;
    }

    bsp_wifi_connected = true;
    return true;
}

// TCP server 문자열 송신
bool bspWifiSend(const char *data) {
    if ((bsp_wifi_connected == false) || (data == 0)) {
        return false;
    }

    if (esp8266_SendData(&bsp_wifi_ctx, data) != ESP8266_OK) {
        bsp_wifi_connected = false;
        return false;
    }

    return true;
}

// Wi-Fi 연결 상태 반환
bool bspWifiIsConnected(void) {
    return bsp_wifi_connected;
}

static const char *bspWifiResultString(esp8266_result_t result) {
    switch (result) {
        case ESP8266_OK:
            return "OK";
        case ESP8266_TIMEOUT:
            return "TIMEOUT";
        case ESP8266_BUSY:
            return "BUSY";
        case ESP8266_ERROR:
        default:
            return "ERROR";
    }
}

static bool bspWifiCheckResult(esp8266_result_t result, const char *step) {
    hwComPrint("WIFI:");
    hwComPrint(step);
    hwComPrint(":");
    hwComPrint(bspWifiResultString(result));
    hwComPrint("\n");

    if (result == ESP8266_OK) {
        return true;
    }

    return false;
}

static void bspWifiPrintError(const char *step) {
    hwComPrint("WIFI:");
    hwComPrint(step);
    hwComPrint(":ERROR\n");
}
