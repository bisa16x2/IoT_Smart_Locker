#ifndef __INCLUDE_DRIVER__DRIVER_H__
#define __INCLUDE_DRIVER__DRIVER_H__

#include "common.h"
#include "access_config.h"

typedef enum {
    DRIVER_LOCKER_INDICATOR_LOCKED = 0,
    DRIVER_LOCKER_INDICATOR_UNLOCKED,
    DRIVER_LOCKER_INDICATOR_ALERT
} driver_locker_indicator_t;

void driverInit(const access_config_t *config);

void driverLockerLock(void);
void driverLockerUnlock(void);
void driverLockerSetIndicator(driver_locker_indicator_t indicator);

bool driverLockerUpdate(void);

bool driverLockerIsDoorClosed(void);
bool driverLockerIsItemDetected(void);

bool driverLockerCalibrateDoor(uint8_t samples);
bool driverLockerCalibrateItem(uint8_t samples);

uint16_t driverLockerGetPressureValue(void);
uint16_t driverLockerGetPressureDiff(void);
float driverLockerGetCurrentAmp(void);

#endif // __INCLUDE_DRIVER__DRIVER_H__
