#include "driver/driver.h"
#include "driver/ina219.h"
#include "driver/led.h"
#include "driver/servo.h"
#include "driver/szh_sen.h"
#include "driver/ts0224.h"

static bool driver_door_closed = false;
static bool driver_item_detected = false;
static uint16_t driver_pressure_value = 0;
static uint16_t driver_pressure_diff = 0;
static const access_config_t *driver_config = 0;

// 장치 driver 초기화
void driverInit(const access_config_t *config) {
    driver_config = config;

    (void)ina219Init(config);
    led_Init(config);
    servo_init(config);
    (void)ts0224Init(config);
    (void)szhSenInit(config);
}

// servo 잠금 각도 이동
void driverLockerLock(void) {
    servo_Ang0();
}

// servo 해제 각도 이동
void driverLockerUnlock(void) {
    servo_Ang90();
}

// locker LED indicator 설정
void driverLockerSetIndicator(driver_locker_indicator_t indicator) {
    led_ALL_Off();

    switch (indicator) {
        case DRIVER_LOCKER_INDICATOR_LOCKED:
            led_On((driver_config != 0) ? driver_config->led_green_pin : PB2);
            break;

        case DRIVER_LOCKER_INDICATOR_UNLOCKED:
            led_On((driver_config != 0) ? driver_config->led_yellow_pin : PB1);
            break;

        case DRIVER_LOCKER_INDICATOR_ALERT:
        default:
            led_On((driver_config != 0) ? driver_config->led_red_pin : PB0);
            break;
    }
}

// door/item sensor 갱신 및 cache
bool driverLockerUpdate(void) {
    bool door_ok;
    bool item_ok;

    door_ok = ts0224Update();
    item_ok = szhSenUpdate();

    driver_door_closed = ts0224IsDetectedDigital();
    driver_item_detected = szhSenIsDetected();
    driver_pressure_value = szhSenGetRaw();
    driver_pressure_diff = szhSenGetDiff();

    if ((door_ok == true) && (item_ok == true)) {
        return true;
    }

    return false;
}

// 최근 door closed 상태 반환
bool driverLockerIsDoorClosed(void) {
    return driver_door_closed;
}

// 최근 item detected 상태 반환
bool driverLockerIsItemDetected(void) {
    return driver_item_detected;
}

// door sensor 기준값 보정
bool driverLockerCalibrateDoor(uint8_t samples) {
    return ts0224Calibrate(samples);
}

// item sensor 기준값 보정
bool driverLockerCalibrateItem(uint8_t samples) {
    return szhSenCalibrateBaseline(samples);
}

// 최근 pressure raw 반환
uint16_t driverLockerGetPressureValue(void) {
    return driver_pressure_value;
}

// 최근 pressure diff 반환
uint16_t driverLockerGetPressureDiff(void) {
    return driver_pressure_diff;
}

// INA219 current 측정 및 ampere 변환
float driverLockerGetCurrentAmp(void) {
    return ina219ReadCurrent_mA() / 1000.0f;
}
