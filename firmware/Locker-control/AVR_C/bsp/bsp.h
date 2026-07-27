#ifndef __INCLUDE_BSP__BSP_H__
#define __INCLUDE_BSP__BSP_H__

#include "common.h"
#include "access_config.h"

typedef enum {
    BSP_LOCKER_INDICATOR_LOCKED = 0,
    BSP_LOCKER_INDICATOR_UNLOCKED,
    BSP_LOCKER_INDICATOR_ALERT
} bsp_locker_indicator_t;

void bspInit(const access_config_t *config);
void bspMain(void);
uint32_t bspMillis(void);
void bspDelay(uint32_t ms);

void bspLockerLock(void);
void bspLockerUnlock(void);

void bspLockerSetIndicator(bsp_locker_indicator_t indicator);
bool bspLockerUpdate(void);

bool bspLockerIsDoorClosed(void);
bool bspLockerIsItemDetected(void);

bool bspLockerCalibrateDoor(uint8_t samples);
bool bspLockerCalibrateItem(uint8_t samples);

uint16_t bspLockerGetPressureValue(void);
uint16_t bspLockerGetPressureDiff(void);
float bspLockerGetCurrentAmp(void);

uint8_t bspComAvailable(void);
uint8_t bspComRead(void);
void bspComPrint(const char *str);
void bspComPrintNum(uint8_t n);
void bspComPrintU16(uint16_t n);

#endif //__INCLUDE_BSP__BSP_H__
