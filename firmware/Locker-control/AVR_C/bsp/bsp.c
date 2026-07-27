#include "bsp/bsp.h"
#include "bsp/bspWifi.h"
#include "hw/hw.h"
#include "driver/driver.h"

// HW 및 Driver 계층 초기화
void bspInit(const access_config_t *config) {
    hwInit(config);
    driverInit(config);
    bspWifiInit(config);
}

// BSP 주기 작업
void bspMain(void) {
}

// system tick 반환
uint32_t bspMillis(void) {
    return hwMillis();
}

// blocking delay 제공
void bspDelay(uint32_t ms) {
    hwDelay(ms);
}

// locker 잠금
void bspLockerLock(void) {
    driverLockerLock();
}

// locker 잠금 해제
void bspLockerUnlock(void) {
    driverLockerUnlock();
}

// indicator 상태 변환 및 설정
void bspLockerSetIndicator(bsp_locker_indicator_t indicator) {
    driver_locker_indicator_t driver_indicator;

    switch (indicator) {
        case BSP_LOCKER_INDICATOR_LOCKED:
            driver_indicator = DRIVER_LOCKER_INDICATOR_LOCKED;
            break;

        case BSP_LOCKER_INDICATOR_UNLOCKED:
            driver_indicator = DRIVER_LOCKER_INDICATOR_UNLOCKED;
            break;

        case BSP_LOCKER_INDICATOR_ALERT:
        default:
            driver_indicator = DRIVER_LOCKER_INDICATOR_ALERT;
            break;
    }

    driverLockerSetIndicator(driver_indicator);
}

// locker sensor 갱신
bool bspLockerUpdate(void) {
    return driverLockerUpdate();
}

// door closed 상태 반환
bool bspLockerIsDoorClosed(void) {
    return driverLockerIsDoorClosed();
}

// item detected 상태 반환
bool bspLockerIsItemDetected(void) {
    return driverLockerIsItemDetected();
}

// door sensor 기준값 보정
bool bspLockerCalibrateDoor(uint8_t samples) {
    return driverLockerCalibrateDoor(samples);
}

// item sensor 기준값 보정
bool bspLockerCalibrateItem(uint8_t samples) {
    return driverLockerCalibrateItem(samples);
}

// pressure raw 반환
uint16_t bspLockerGetPressureValue(void) {
    return driverLockerGetPressureValue();
}

// pressure diff 반환
uint16_t bspLockerGetPressureDiff(void) {
    return driverLockerGetPressureDiff();
}

// current를 ampere 단위로 반환
float bspLockerGetCurrentAmp(void) {
    return driverLockerGetCurrentAmp();
}

// 통신 수신 가능 여부 반환
uint8_t bspComAvailable(void) {
    return hwComAvailable();
}

// 수신 byte 반환
uint8_t bspComRead(void) {
    return hwComRead();
}

// 문자열 송신
void bspComPrint(const char *str) {
    hwComPrint(str);
}

// 8-bit 정수 송신
void bspComPrintNum(uint8_t n) {
    hwComPrintNum(n);
}

// 16-bit 정수 송신
void bspComPrintU16(uint16_t n) {
    hwComPrintU16(n);
}
