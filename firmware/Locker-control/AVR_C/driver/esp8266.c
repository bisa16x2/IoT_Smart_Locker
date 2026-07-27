#include "driver/esp8266.h"
#include "hw/hw.h"
#include "hw/my_uart.h"
/*
 * ESP8266 AT 명령 드라이버
 *
 * 구조:
 * esp8266.c  → ESP-01 AT 명령 처리
 * my_uart.c  → 실제 UART 송수신
 *
 * 주의:
 * 1차 구현용 blocking 방식
 * esp8266_SendCmd()에서 timeout 동안 응답 대기
 */
#define ESP8266_CMD_BUF_SIZE  128
#define ESP8266_SEND_BUF_SIZE 64

#define ESP8266_DEFAULT_TIMEOUT 1000
#define ESP8266_JOIN_TIMEOUT    10000
#define ESP8266_TCP_TIMEOUT     5000
#define ESP8266_SEND_TIMEOUT    5000

// 내부 helper
static void esp8266_ClearRxBuffer(esp8266_t *ctx);
// UART 수신 buffer 비우기
static void esp8266_FlushUartRx(esp8266_t *ctx);
// 응답 문자열 포함 여부 확인
static uint8_t esp8266_Contains(esp8266_t *ctx, const char *str);

// timeout 기반 응답 대기 helper
static esp8266_result_t esp8266_WaitResponse(
    esp8266_t *ctx,
    const char *expect,
    uint32_t timeout_ms);

// raw 문자열 송신 helper
static esp8266_result_t esp8266_SendRaw(
    esp8266_t *ctx,
    const char *data);

// 공개 API
// ESP8266 context 초기화
void esp8266_Init(esp8266_t *ctx, uint8_t uart_ch) {
    if (ctx == NULL) return;

    ctx->uart_ch = uart_ch;
    ctx->rx_len = 0;
    ctx->is_ready = 0;
    ctx->is_connected = 0;

    memset(ctx->rx_buf, 0, sizeof(ctx->rx_buf));
}
/*
 * UART interrupt 수신은 bsp.c 또는 my_uart_Init()에서
 * my_uart_StartRxIT()로 시작
 */
// UART 수신 데이터를 context buffer로 이동
void esp8266_Process(esp8266_t *ctx) {
    uint8_t data;

    if (ctx == NULL) return;

    /* my_uart.c ring buffer에서 esp8266 rx_buf로 이동 */
    while (uartAvailable(ctx->uart_ch) > 0) {
        data = uartRead(ctx->uart_ch);

        if (ctx->rx_len < (sizeof(ctx->rx_buf) - 1)) {
            ctx->rx_buf[ctx->rx_len++] = (char)data;
            ctx->rx_buf[ctx->rx_len] = '\0';
        }
        else
            esp8266_ClearRxBuffer(ctx);
        /*
         * buffer overflow 시 초기화
         * +IPD 수신 확장 시 ring buffer 적용 필요
         */
    }
}

// AT 명령 송신 및 응답 대기
esp8266_result_t esp8266_SendCmd(
    esp8266_t *ctx,
    const char *cmd,
    const char *expect,
    uint32_t timeout_ms) {
    esp8266_result_t ret;

    if (ctx == NULL || cmd == NULL || expect == NULL) return ESP8266_ERROR;

    esp8266_FlushUartRx(ctx);
    esp8266_ClearRxBuffer(ctx);

    ret = esp8266_SendRaw(ctx, cmd);
    if (ret != ESP8266_OK) return ret;

    ret = esp8266_SendRaw(ctx, "\r\n");
    if (ret != ESP8266_OK) return ret;

    return esp8266_WaitResponse(ctx, expect, timeout_ms);
}

// AT 통신 확인
esp8266_result_t esp8266_TestAT(esp8266_t *ctx) {
    esp8266_result_t ret;
    // 정상 응답: AT, OK
    ret = esp8266_SendCmd(ctx, "AT", "OK", ESP8266_DEFAULT_TIMEOUT);

    if (ret == ESP8266_OK)
        ctx->is_ready = 1;
    else
        ctx->is_ready = 0;

    return ret;
}

// ESP8266 재시작
esp8266_result_t esp8266_Restart(esp8266_t *ctx) {
    esp8266_result_t ret;
    // AT+RST 이후 ready 출력
    // firmware version별 출력 형식 차이 가능
    ret = esp8266_SendCmd(ctx, "AT+RST", "ready", 5000);

    ctx->is_connected = 0;

    if (ret == ESP8266_OK)
        ctx->is_ready = 1;
    else
        ctx->is_ready = 0;

    return ret;
}

// Wi-Fi mode 설정
esp8266_result_t esp8266_SetMode(esp8266_t *ctx, esp8266_wifi_mode_t mode) {
    char cmd[ESP8266_CMD_BUF_SIZE];
    int len;

    if (mode != ESP8266_WIFI_MODE_STA &&
        mode != ESP8266_WIFI_MODE_AP &&
        mode != ESP8266_WIFI_MODE_STA_AP) {
        return ESP8266_ERROR;
    }
    /*
     * mode:
     * 1 = Station
     * 2 = SoftAP
     * 3 = Station + SoftAP
     */
    len = snprintf(cmd, sizeof(cmd), "AT+CWMODE=%d", mode);
    if (len < 0 || len >= (int)sizeof(cmd)) return ESP8266_ERROR;

    return esp8266_SendCmd(ctx, cmd, "OK", ESP8266_DEFAULT_TIMEOUT);
}

// Wi-Fi AP 연결
esp8266_result_t esp8266_JoinAP(
    esp8266_t *ctx,
    const char *ssid,
    const char *password) {
    char cmd[ESP8266_CMD_BUF_SIZE];
    int len;
    esp8266_result_t ret;

    if (ctx == NULL || ssid == NULL || password == NULL) return ESP8266_ERROR;
    /*
     * 예:
     * AT+CWJAP="SSID","PASSWORD"
     *
     * Wi-Fi 연결용 긴 timeout 적용
     */
    len = snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);

    if (len < 0 || len >= (int)sizeof(cmd)) return ESP8266_ERROR;

    ret = esp8266_SendCmd(ctx, cmd, "OK", ESP8266_JOIN_TIMEOUT);

    if (ret == ESP8266_OK)
        ctx->is_connected = 1;
    else
        ctx->is_connected = 0;

    return ret;
}

// TCP 연결 시작
esp8266_result_t esp8266_StartTCP(
    esp8266_t *ctx,
    const char *ip,
    uint16_t port) {
    char cmd[ESP8266_CMD_BUF_SIZE];
    int len;
    esp8266_result_t ret;

    if (ctx == NULL || ip == NULL) return ESP8266_ERROR;
    /*
     * 단일 연결 기준:
     * AT+CIPSTART="TCP","192.168.0.10",5000
     */
    len = snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u", ip, (unsigned int)port);

    if (len < 0 || len >= (int)sizeof(cmd)) return ESP8266_ERROR;

    ret = esp8266_SendCmd(ctx, cmd, "OK", ESP8266_TCP_TIMEOUT);
    /*
     * 연결 상태에서 일부 firmware는
     * ALREADY CONNECTED 반환 가능
     */
    if (ret != ESP8266_OK) {
        if (esp8266_Contains(ctx, "ALREADY CONNECTED") != 0) return ESP8266_OK;
    }

    return ret;
}

// TCP data 송신
esp8266_result_t esp8266_SendData(esp8266_t *ctx, const char *data) {
    char cmd[ESP8266_SEND_BUF_SIZE];
    esp8266_result_t ret;
    int len;

    if (ctx == NULL || data == NULL) return ESP8266_ERROR;

    len = strlen(data);

    if (len == 0) return ESP8266_ERROR;
    /*
     * 단일 연결 기준:
     * AT+CIPSEND=<length>
     *
     * '>' prompt 수신 후 data 송신
     */
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", (unsigned int)len);

    ret = esp8266_SendCmd(ctx, cmd, ">", ESP8266_SEND_TIMEOUT);

    if (ret != ESP8266_OK) return ret;

    esp8266_ClearRxBuffer(ctx);

    ret = esp8266_SendRaw(ctx, data);

    if (ret != ESP8266_OK) return ret;
    /* 정상 송신 시 SEND OK 출력 */
    return esp8266_WaitResponse(ctx, "SEND OK", ESP8266_SEND_TIMEOUT);
}

// TCP 연결 종료
esp8266_result_t esp8266_Close(esp8266_t *ctx) {
    esp8266_result_t ret;
    /*
     * 단일 연결 기준:
     * AT+CIPCLOSE
     */
    ret = esp8266_SendCmd(ctx, "AT+CIPCLOSE", "OK", ESP8266_DEFAULT_TIMEOUT);

    if (ret == ESP8266_OK) ctx->is_connected = 0;

    return ret;
}

// 내부 helper 구현
static void esp8266_ClearRxBuffer(esp8266_t *ctx) {
    if (ctx == NULL) return;

    memset(ctx->rx_buf, 0, sizeof(ctx->rx_buf));
    ctx->rx_len = 0;
}

// UART 수신 buffer 비우기
static void esp8266_FlushUartRx(esp8266_t *ctx) {
    if (ctx == NULL) return;
    // 이전 명령의 잔여 수신 data 제거
    while (uartAvailable(ctx->uart_ch) > 0) {
        (void)uartRead(ctx->uart_ch);
    }
}

// 응답 문자열 포함 여부 확인
static uint8_t esp8266_Contains(esp8266_t *ctx, const char *str) {
    if (ctx == NULL || str == NULL) return 0;
    if (strstr(ctx->rx_buf, str) != NULL) return 1;

    return 0;
}

// timeout까지 기대 응답 확인
static esp8266_result_t esp8266_WaitResponse(
    esp8266_t *ctx,
    const char *expect,
    uint32_t timeout_ms) {
    uint32_t start_time;

    if (ctx == NULL || expect == NULL) return ESP8266_ERROR;

    start_time = hwMillis();

    while ((hwMillis() - start_time) < timeout_ms) {
        esp8266_Process(ctx);

        if (esp8266_Contains(ctx, expect) != 0) return ESP8266_OK;
        // 대표적인 실패 응답 처리
        if (esp8266_Contains(ctx, "ERROR") != 0) return ESP8266_ERROR;
        if (esp8266_Contains(ctx, "FAIL") != 0) return ESP8266_ERROR;
        if (esp8266_Contains(ctx, "busy") != 0) return ESP8266_BUSY;
    }

    return ESP8266_TIMEOUT;
}

// ESP8266 UART raw 문자열 송신
static esp8266_result_t esp8266_SendRaw(
    esp8266_t *ctx,
    const char *data) {
    uint32_t len;
    uint32_t written;

    if (ctx == NULL || data == NULL) return ESP8266_ERROR;

    len = strlen(data);

    if (len == 0) return ESP8266_ERROR;

    written = uartWrite(ctx->uart_ch, (uint8_t *)data, len);

    if (written != len) return ESP8266_ERROR;

    return ESP8266_OK;
}
