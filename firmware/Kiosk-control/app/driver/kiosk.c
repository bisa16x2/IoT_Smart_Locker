// FSM에 따른 키오스크 화면 제어

#include "kiosk.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define LOCKER_NO_MAX_LEN           4
#define PASSWORD_MAX_LEN            4

#define BT_RX_BUF_SIZE              100
#define KIOSK_SERVER_TIMEOUT_MS     3000
#define KIOSK_RESULT_DISPLAY_MS     3000

typedef enum
{
    KIOSK_STATE_IDLE,

    KIOSK_STATE_LOGIN_INPUT,
    KIOSK_STATE_AUTH_WAIT,
    KIOSK_STATE_AUTH_SUCCESS,
    KIOSK_STATE_AUTH_FAIL,

    KIOSK_STATE_REGISTER_INPUT,
    KIOSK_STATE_REGISTER_WAIT,
    KIOSK_STATE_REGISTER_SUCCESS,
    KIOSK_STATE_REGISTER_FAIL
} kiosk_state_t;

typedef enum
{
    INPUT_MODE_LOCKER_NO,
    INPUT_MODE_PASSWORD
} input_mode_t;

static kiosk_state_t kiosk_state;
static input_mode_t input_mode;

static char locker_no[LOCKER_NO_MAX_LEN + 1];
static char password[PASSWORD_MAX_LEN + 1];

static uint8_t locker_idx;
static uint8_t password_idx;

static char prev_key;

static uint32_t wait_start_tick;
static uint32_t result_start_tick;

static char bt_rx_buf[BT_RX_BUF_SIZE];
static uint16_t bt_rx_idx;

static void kioskShowIdle(void);
static void kioskShowLoginInput(void);
static void kioskShowRegisterInput(void);

static void kioskShowAuthWait(void);
static void kioskShowRegisterWait(void);

static void kioskShowAuthSuccess(void);
static void kioskShowAuthFail(void);
static void kioskShowRegisterSuccess(void);
static void kioskShowRegisterFail(void);
static void kioskShowServerTimeout(void);

static void kioskClearInput(void);
static void kioskProcessKey(char key);

static void kioskRequestLoginAuth(void);
static void kioskRequestRegister(void);

static void kioskHandleServerWait(void);
static void kioskHandleResultDisplay(void);

static void bluetoothSendAuthRequest(void);
static void bluetoothSendRegisterRequest(void);
static bool bluetoothReadLine(char *line, uint16_t line_size);
static void bluetoothClearRxBuffer(void);

void kioskInit(void)
{
    kiosk_state = KIOSK_STATE_IDLE;
    input_mode = INPUT_MODE_LOCKER_NO;
    prev_key = 0;

    wait_start_tick = 0;
    result_start_tick = 0;

    bluetoothClearRxBuffer();

    kioskClearInput();
    kioskShowIdle();
}

void kioskMain(void)
{
    char key;

    if (kiosk_state == KIOSK_STATE_AUTH_WAIT ||
        kiosk_state == KIOSK_STATE_REGISTER_WAIT)
    {
        kioskHandleServerWait();
        return;
    }

    if (kiosk_state == KIOSK_STATE_AUTH_SUCCESS ||
        kiosk_state == KIOSK_STATE_AUTH_FAIL ||
        kiosk_state == KIOSK_STATE_REGISTER_SUCCESS ||
        kiosk_state == KIOSK_STATE_REGISTER_FAIL)
    {
        kioskHandleResultDisplay();
        return;
    }

    key = get_key();

    if (key != 0 && prev_key == 0)
    {
        kioskProcessKey(key);
    }

    prev_key = key;
}

static void kioskProcessKey(char key)
{
    switch (kiosk_state)
    {
        case KIOSK_STATE_IDLE:
            if (key == '*')
            {
                kioskClearInput();
                kiosk_state = KIOSK_STATE_LOGIN_INPUT;
                input_mode = INPUT_MODE_LOCKER_NO;
                kioskShowLoginInput();
            }
            else if (key == '#')
            {
                kioskClearInput();
                kiosk_state = KIOSK_STATE_REGISTER_INPUT;
                input_mode = INPUT_MODE_LOCKER_NO;
                kioskShowRegisterInput();
            }
            break;

        case KIOSK_STATE_LOGIN_INPUT:
        case KIOSK_STATE_REGISTER_INPUT:
            if (key >= '0' && key <= '9')
            {
                if (input_mode == INPUT_MODE_LOCKER_NO)
                {
                    if (locker_idx < LOCKER_NO_MAX_LEN)
                    {
                        locker_no[locker_idx] = key;
                        locker_idx++;
                        locker_no[locker_idx] = '\0';

                        if (kiosk_state == KIOSK_STATE_LOGIN_INPUT)
                        {
                            kioskShowLoginInput();
                        }
                        else
                        {
                            kioskShowRegisterInput();
                        }
                    }
                }
                else if (input_mode == INPUT_MODE_PASSWORD)
                {
                    if (password_idx < PASSWORD_MAX_LEN)
                    {
                        password[password_idx] = key;
                        password_idx++;
                        password[password_idx] = '\0';

                        if (kiosk_state == KIOSK_STATE_LOGIN_INPUT)
                        {
                            kioskShowLoginInput();
                        }
                        else
                        {
                            kioskShowRegisterInput();
                        }
                    }
                }
            }
            else if (key == '#')
            {
                if (input_mode == INPUT_MODE_LOCKER_NO)
                {
                    if (locker_idx > 0)
                    {
                        input_mode = INPUT_MODE_PASSWORD;

                        if (kiosk_state == KIOSK_STATE_LOGIN_INPUT)
                        {
                            kioskShowLoginInput();
                        }
                        else
                        {
                            kioskShowRegisterInput();
                        }
                    }
                }
                else if (input_mode == INPUT_MODE_PASSWORD)
                {
                    if (password_idx == PASSWORD_MAX_LEN)
                    {
                        if (kiosk_state == KIOSK_STATE_LOGIN_INPUT)
                        {
                            kioskRequestLoginAuth();
                        }
                        else
                        {
                            kioskRequestRegister();
                        }
                    }
                    else
                    {
                        lcdClear();
                        lcdSetCursor(0, 0);
                        lcdPrint("PIN must be");
                        lcdSetCursor(1, 0);
                        lcdPrint("4 digits");
                        HAL_Delay(700);

                        if (kiosk_state == KIOSK_STATE_LOGIN_INPUT)
                        {
                            kioskShowLoginInput();
                        }
                        else
                        {
                            kioskShowRegisterInput();
                        }
                    }
                }
            }
            else if (key == '*')
            {
                kioskClearInput();

                if (kiosk_state == KIOSK_STATE_LOGIN_INPUT)
                {
                    kioskShowLoginInput();
                }
                else
                {
                    kioskShowRegisterInput();
                }
            }
            break;

        default:
            break;
    }
}

static void kioskRequestLoginAuth(void)
{
    kiosk_state = KIOSK_STATE_AUTH_WAIT;
    wait_start_tick = HAL_GetTick();

    bluetoothClearRxBuffer();

    kioskShowAuthWait();

    bluetoothSendAuthRequest();
}

static void kioskRequestRegister(void)
{
    kiosk_state = KIOSK_STATE_REGISTER_WAIT;
    wait_start_tick = HAL_GetTick();

    bluetoothClearRxBuffer();

    kioskShowRegisterWait();

    bluetoothSendRegisterRequest();
}

static void kioskHandleServerWait(void)
{
    char line[BT_RX_BUF_SIZE] = {0};

    if (bluetoothReadLine(line, sizeof(line)) == true)
    {
        HAL_UART_Transmit(&huart2, (uint8_t *)"RX_LINE=", 8, 100);
        HAL_UART_Transmit(&huart2, (uint8_t *)line, strlen(line), 100);
        HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n", 2, 100);

        if (kiosk_state == KIOSK_STATE_AUTH_WAIT)
        {
            if (strstr(line, "AUTH_SUCCESS") != NULL)
            {
                kiosk_state = KIOSK_STATE_AUTH_SUCCESS;
                result_start_tick = HAL_GetTick();
                kioskShowAuthSuccess();
                return;
            }

            if (strstr(line, "AUTH_FAIL") != NULL ||
                strstr(line, "AUTH_ERROR") != NULL)
            {
                kiosk_state = KIOSK_STATE_AUTH_FAIL;
                result_start_tick = HAL_GetTick();
                kioskShowAuthFail();
                return;
            }
        }
        else if (kiosk_state == KIOSK_STATE_REGISTER_WAIT)
        {
            if (strstr(line, "REGISTER_SUCCESS") != NULL)
            {
                kiosk_state = KIOSK_STATE_REGISTER_SUCCESS;
                result_start_tick = HAL_GetTick();
                kioskShowRegisterSuccess();
                return;
            }

            if (strstr(line, "REGISTER_FAIL") != NULL ||
                strstr(line, "REGISTER_ERROR") != NULL)
            {
                kiosk_state = KIOSK_STATE_REGISTER_FAIL;
                result_start_tick = HAL_GetTick();
                kioskShowRegisterFail();
                return;
            }
        }
    }

    if ((HAL_GetTick() - wait_start_tick) >= KIOSK_SERVER_TIMEOUT_MS)
    {
        result_start_tick = HAL_GetTick();

        memset(bt_rx_buf, 0, sizeof(bt_rx_buf));
        bt_rx_idx = 0;

        if (kiosk_state == KIOSK_STATE_AUTH_WAIT)
        {
            kiosk_state = KIOSK_STATE_AUTH_FAIL;
        }
        else
        {
            kiosk_state = KIOSK_STATE_REGISTER_FAIL;
        }

        kioskShowServerTimeout();
    }
}

static void kioskHandleResultDisplay(void)
{
    if ((HAL_GetTick() - result_start_tick) >= KIOSK_RESULT_DISPLAY_MS)
    {
        kioskClearInput();
        kiosk_state = KIOSK_STATE_IDLE;
        input_mode = INPUT_MODE_LOCKER_NO;
        kioskShowIdle();
    }
}

static void bluetoothSendAuthRequest(void)
{
    char tx_buf[64];

    snprintf(tx_buf, sizeof(tx_buf),
             "AUTH:%s:%s\n",
             locker_no,
             password);

    HAL_UART_Transmit(&huart1,
                      (uint8_t *)tx_buf,
                      strlen(tx_buf),
                      100);

    HAL_UART_Transmit(&huart2,
                      (uint8_t *)tx_buf,
                      strlen(tx_buf),
                      100);
}

static void bluetoothSendRegisterRequest(void)
{
    char tx_buf[64];

    snprintf(tx_buf, sizeof(tx_buf),
             "REGISTER:%s:%s\n",
             locker_no,
             password);

    HAL_UART_Transmit(&huart1,
                      (uint8_t *)tx_buf,
                      strlen(tx_buf),
                      100);

    HAL_UART_Transmit(&huart2,
                      (uint8_t *)tx_buf,
                      strlen(tx_buf),
                      100);
}

static void bluetoothClearRxBuffer(void)
{
    uint8_t dummy;

    memset(bt_rx_buf, 0, sizeof(bt_rx_buf));
    bt_rx_idx = 0;

    while (HAL_UART_Receive(&huart1, &dummy, 1, 0) == HAL_OK)
    {
    }
}

static bool bluetoothReadLine(char *line, uint16_t line_size)
{
    uint8_t ch;

    while (HAL_UART_Receive(&huart1, &ch, 1, 10) == HAL_OK)
    {
        if (ch == '\r')
        {
            continue;
        }

        if (ch == '\n')
        {
            bt_rx_buf[bt_rx_idx] = '\0';

            strncpy(line, bt_rx_buf, line_size - 1);
            line[line_size - 1] = '\0';

            memset(bt_rx_buf, 0, sizeof(bt_rx_buf));
            bt_rx_idx = 0;

            return true;
        }

        if (bt_rx_idx < BT_RX_BUF_SIZE - 1)
        {
            bt_rx_buf[bt_rx_idx] = ch;
            bt_rx_idx++;
        }
        else
        {
            memset(bt_rx_buf, 0, sizeof(bt_rx_buf));
            bt_rx_idx = 0;
        }
    }

    return false;
}

static void kioskShowIdle(void)
{
    lcdClear();

    lcdSetCursor(0, 0);
    lcdPrint("* Login");

    lcdSetCursor(1, 0);
    lcdPrint("# Register");
}

static void kioskShowLoginInput(void)
{
    lcdClear();

    if (input_mode == INPUT_MODE_LOCKER_NO)
    {
        lcdSetCursor(0, 0);
        lcdPrint("Login Locker");

        lcdSetCursor(1, 0);
        lcdPrintf("No:%s", locker_no);

        lcdSetCursor(1, 3 + locker_idx);
    }
    else
    {
        lcdSetCursor(0, 0);
        lcdPrintf("Locker:%s", locker_no);

        lcdSetCursor(1, 0);
        lcdPrint("PIN:");

        for (uint8_t i = 0; i < password_idx; i++)
        {
            lcdPrint("*");
        }

        lcdSetCursor(1, 4 + password_idx);
    }
}

static void kioskShowRegisterInput(void)
{
    lcdClear();

    if (input_mode == INPUT_MODE_LOCKER_NO)
    {
        lcdSetCursor(0, 0);
        lcdPrint("Register Locker");

        lcdSetCursor(1, 0);
        lcdPrintf("No:%s", locker_no);

        lcdSetCursor(1, 3 + locker_idx);
    }
    else
    {
        lcdSetCursor(0, 0);
        lcdPrintf("Reg Locker:%s", locker_no);

        lcdSetCursor(1, 0);
        lcdPrint("PIN:");

        for (uint8_t i = 0; i < password_idx; i++)
        {
            lcdPrint("*");
        }

        lcdSetCursor(1, 4 + password_idx);
    }
}

static void kioskShowAuthWait(void)
{
    lcdClear();

    lcdSetCursor(0, 0);
    lcdPrint("Checking...");

    lcdSetCursor(1, 0);
    lcdPrint("Please wait");
}

static void kioskShowRegisterWait(void)
{
    lcdClear();

    lcdSetCursor(0, 0);
    lcdPrint("Registering...");

    lcdSetCursor(1, 0);
    lcdPrint("Please wait");
}

static void kioskShowAuthSuccess(void)
{
    lcdClear();

    lcdSetCursor(0, 0);
    lcdPrint("Auth Success");

    lcdSetCursor(1, 0);
    lcdPrint("Door open");
}

static void kioskShowAuthFail(void)
{
    lcdClear();

    lcdSetCursor(0, 0);
    lcdPrint("Auth Fail");

    lcdSetCursor(1, 0);
    lcdPrint("Try again");
}

static void kioskShowRegisterSuccess(void)
{
    lcdClear();

    lcdSetCursor(0, 0);
    lcdPrint("Register OK");

    lcdSetCursor(1, 0);
    lcdPrintf("Locker:%s", locker_no);
}

static void kioskShowRegisterFail(void)
{
    lcdClear();

    lcdSetCursor(0, 0);
    lcdPrint("Register Fail");

    lcdSetCursor(1, 0);
    lcdPrint("Try again");
}

static void kioskShowServerTimeout(void)
{
    lcdClear();

    lcdSetCursor(0, 0);
    lcdPrint("Server timeout");

    lcdSetCursor(1, 0);
    lcdPrint("Try again");
}

static void kioskClearInput(void)
{
    memset(locker_no, 0, sizeof(locker_no));
    memset(password, 0, sizeof(password));

    locker_idx = 0;
    password_idx = 0;

    input_mode = INPUT_MODE_LOCKER_NO;
}