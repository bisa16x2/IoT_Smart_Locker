#include "ap/AccessLock.h"
#include "access_config.h"
#include "bsp/bsp.h"
#include "bsp/bspWifi.h"

typedef enum {
    ACCESS_LOCK_STATE_LOCKED = 0,
    ACCESS_LOCK_STATE_UNLOCKED,
    ACCESS_LOCK_STATE_ALERT
} access_lock_state_t;

static access_lock_state_t access_state = ACCESS_LOCK_STATE_LOCKED;

static const access_config_t access_config = {
    .locker_id = 1U,
    .locker_no = "L01",
    .user_id = "NONE",
    .cmd_buf_size = 40U,
    .status_period_ms = 1000UL,
    .door_timeout_ms = 10000UL,

    .ap_ssid = "(Wi-Fi ID)",
    .ap_pass = "(Wi-Fi PASSWORD)",
    .server_name = "(SERVER IP)",
    .server_port = 5000U,
    .logid = "(DEVICE ID)",
    .passwd = "(PASSWORD)",

    .wifi_uart_ch = 1U,
    .wifi_rx_pin = PD6,
    .wifi_tx_pin = PD7,
    .uart_baud = 38400UL,

    .servo_pin = PD3,
    .servo_unlock_value = 24U,
    .servo_lock_value = 7U,

    .hall_adc_channel = 0U,
    .hall_d0_ddr = &DDRD,
    .hall_d0_port = &PORTD,
    .hall_d0_pinr = &PIND,
    .hall_d0_pin = PD2,
    .hall_threshold = 50U,
    .hall_calibration_samples = 16U,

    .flex_adc_channel = 1U,
    .flex_threshold = 50U,
    .flex_calibration_samples = 16U,
    .adc_max_value = 1023U,

    .i2c_freq_hz = 100000UL,
    .i2c_sda_pin = PC4,
    .i2c_scl_pin = PC5,

    .ina219_addr_7bit = 0x40U,
    .ina219_calibration_value = 4096U,
    .ina219_config_value = 0x399FU,

    .led_red_pin = PB0,
    .led_yellow_pin = PB1,
    .led_green_pin = PB2,

    .rfid_cs_ddr = &DDRB,
    .rfid_cs_port = &PORTB,
    .rfid_cs_pin = PB2,
    .rfid_rst_ddr = &DDRB,
    .rfid_rst_port = &PORTB,
    .rfid_rst_pin = PB1,
    .rfid_mosi_pin = PB3,
    .rfid_miso_pin = PB4,
    .rfid_sck_pin = PB5
};


static char access_cmd_buf[40];
static uint8_t access_cmd_len = 0;

static bool access_door_closed = false;
static bool access_item_detected = false;
static bool access_alert_sent = false;

static uint16_t access_pressure_value = 0;
static uint16_t access_pressure_diff = 0;
static float access_current_amp = 0.0f;
static bool access_wifi_ready = false;

static uint32_t access_last_status_ms = 0;
static uint32_t access_unlock_start_ms = 0;

// 대상 locker ID 확인
static bool accessIsTargetLocker(const char *cmd, const char *prefix) {
    uint8_t i = 0;

    while (prefix[i] != '\0') {
        if (cmd[i] != prefix[i]) {
            return false;
        }
        i++;
    }

    if (cmd[i] != (char)('0' + access_config.locker_id)) {
        return false;
    }

    if (cmd[i + 1U] != '\0') {
        return false;
    }

    return true;
}

// door 상태 문자열 반환
static const char *accessDoorStateString(void) {
    if (access_door_closed == true) {
        return "CLOSED";
    }

    return "OPEN";
}

// lock 상태 문자열 반환
static const char *accessLockStateString(void) {
    if (access_state == ACCESS_LOCK_STATE_LOCKED) {
        return "LOCKED";
    }

    return "UNLOCKED";
}

// item 상태 문자열 반환
static const char *accessItemStateString(void) {
    if (access_item_detected == true) {
        return "EXIST";
    }

    return "EMPTY";
}

// alarm 상태 문자열 반환
static const char *accessAlarmStateString(void) {
    if (access_state == ACCESS_LOCK_STATE_ALERT) {
        return "DOOR_TIMEOUT";
    }

    return "NORMAL";
}

// server 메시지 송신
static bool accessSendServerLine(const char *line) {
    if (access_wifi_ready == true) {
        if (bspWifiSend(line) == true) {
            return true;
        }

        access_wifi_ready = false;
    }

    bspComPrint(line);
    return false;
}

// locker 상태 송신
static void accessSendLockerState(void) {
    char current_buf[12];
    char msg[128];
    int len;

    dtostrf(access_current_amp, 1, 3, current_buf);

    len = snprintf(
        msg,
        sizeof(msg),
        "[JBC_SQL]LOCKER_STATE@%s@%s@%s@%s@%s@%u@%u@%s@%s\n",
        access_config.locker_no,
        access_config.user_id,
        accessDoorStateString(),
        accessLockStateString(),
        accessItemStateString(),
        access_pressure_value,
        access_pressure_diff,
        current_buf,
        accessAlarmStateString());

    if ((len > 0) && (len < (int)sizeof(msg))) {
        (void)accessSendServerLine(msg);
    }
}

// debug 상태 송신
static void accessSendDebugStatus(void) {
    bspComPrint("STATUS:");
    bspComPrint(access_config.locker_no);
    bspComPrint(",");
    bspComPrint(accessDoorStateString());
    bspComPrint(",");
    bspComPrint(accessLockStateString());
    bspComPrint(",");
    bspComPrint(accessItemStateString());
    bspComPrint(",P:");
    bspComPrintU16(access_pressure_value);
    bspComPrint(",D:");
    bspComPrintU16(access_pressure_diff);
    bspComPrint("\n");
}

// 잠금 상태 전환
static void accessSetLocked(void) {
    bspLockerLock();
    access_state = ACCESS_LOCK_STATE_LOCKED;
    access_alert_sent = false;

    bspLockerSetIndicator(BSP_LOCKER_INDICATOR_LOCKED);
}

// 잠금 해제 상태 전환
static void accessSetUnlocked(void) {
    bspLockerUnlock();
    access_state = ACCESS_LOCK_STATE_UNLOCKED;
    access_unlock_start_ms = bspMillis();
    access_alert_sent = false;

    bspLockerSetIndicator(BSP_LOCKER_INDICATOR_UNLOCKED);
}

// 경보 상태 전환
static void accessSetAlert(const char *reason) {
    access_state = ACCESS_LOCK_STATE_ALERT;

    bspLockerSetIndicator(BSP_LOCKER_INDICATOR_ALERT);

    if (access_alert_sent == false) {
        char msg[96];
        int len;

        len = snprintf(
            msg,
            sizeof(msg),
            "[JBC_SQL]LOCKER_ALERT@%s@%s@MANAGER@Locker door timeout detected\n",
            access_config.locker_no,
            reason);

        if ((len > 0) && (len < (int)sizeof(msg))) {
            (void)accessSendServerLine(msg);
        }

        access_alert_sent = true;
    }
}

// sensor 상태 갱신
static void accessUpdateSensors(void) {
    (void)bspLockerUpdate();

    access_door_closed = bspLockerIsDoorClosed();
    access_item_detected = bspLockerIsItemDetected();
    access_pressure_value = bspLockerGetPressureValue();
    access_pressure_diff = bspLockerGetPressureDiff();
    access_current_amp = bspLockerGetCurrentAmp();
}

// 수신 명령 처리
static void accessHandleCommand(const char *cmd) {
    if (strcmp(cmd, "STATUS?") == 0) {
        accessSendDebugStatus();
        accessSendLockerState();
        return;
    }

    if (strcmp(cmd, "PING") == 0) {
        bspComPrint("PONG\n");
        return;
    }

    if (strcmp(cmd, "CAL:DOOR") == 0) {
        if (bspLockerCalibrateDoor(16) == true) {
            bspComPrint("OK:CAL:DOOR\n");
        }
        else {
            bspComPrint("ERR:CAL:DOOR\n");
        }
        return;
    }

    if (strcmp(cmd, "CAL:ITEM") == 0) {
        if (bspLockerCalibrateItem(16) == true) {
            bspComPrint("OK:CAL:ITEM\n");
        }
        else {
            bspComPrint("ERR:CAL:ITEM\n");
        }
        return;
    }

    if (accessIsTargetLocker(cmd, "UNLOCK:") == true) {
        accessSetUnlocked();
        bspComPrint("OK:UNLOCK\n");
        accessSendLockerState();
        return;
    }

    if (accessIsTargetLocker(cmd, "LOCK:") == true) {
        accessSetLocked();
        bspComPrint("OK:LOCK\n");
        accessSendLockerState();
        return;
    }

    bspComPrint("ERR:BAD_CMD\n");
}

// UART 수신 polling
static void accessPollUart(void) {
    char c;

    while (bspComAvailable() > 0) {
        c = (char)bspComRead();

        if ((c == '\r') || (c == '\n')) {
            if (access_cmd_len > 0) {
                access_cmd_buf[access_cmd_len] = '\0';
                accessHandleCommand(access_cmd_buf);
                access_cmd_len = 0;
            }
        }
        else if (access_cmd_len < (sizeof(access_cmd_buf) - 1U)) {
            access_cmd_buf[access_cmd_len++] = c;
        }
        else {
            access_cmd_len = 0;
            bspComPrint("ERR:CMD_OVERFLOW\n");
        }
    }
}

// locker 상태 갱신
static void accessUpdateState(void) {
    uint32_t now = bspMillis();

    if ((access_state == ACCESS_LOCK_STATE_UNLOCKED) &&
        (access_door_closed == false) &&
        ((now - access_unlock_start_ms) >= access_config.door_timeout_ms)) {
        accessSetAlert("DOOR_TIMEOUT");
    }

    if ((access_state == ACCESS_LOCK_STATE_ALERT) &&
        (access_door_closed == true)) {
        accessSetLocked();
        bspComPrint("OK:AUTO_LOCK\n");
        accessSendLockerState();
    }
}

// AccessLock 초기화
void AccessLockInit(void) {
    bspInit(&access_config);

    access_wifi_ready = bspWifiConnect();
    if (access_wifi_ready == true) {
        bspComPrint("WIFI:CONNECTED\n");
    }
    else {
        bspComPrint("WIFI:DISCONNECTED\n");
    }

    accessUpdateSensors();
    accessSetLocked();

    access_last_status_ms = bspMillis();

    bspComPrint("BOOT:ACCESS_LOCKER_READY\n");
    accessSendLockerState();
}

// AccessLock 주기 작업
void AccessLockMain(void) {
    uint32_t now;

    accessPollUart();
    accessUpdateSensors();
    accessUpdateState();

    now = bspMillis();
    if ((now - access_last_status_ms) >= access_config.status_period_ms) {
        access_last_status_ms = now;
        accessSendLockerState();
    }
}
