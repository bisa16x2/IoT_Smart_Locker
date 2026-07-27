#ifndef __INCLUDE_DRIVER__SZH_SEN_H__
#define __INCLUDE_DRIVER__SZH_SEN_H__

#include "common.h"
#include "access_config.h"

typedef enum {
    SZH_SEN_MODE_ABS_DIFF = 0,
    SZH_SEN_MODE_RAW_HIGH,
    SZH_SEN_MODE_RAW_LOW
} szh_sen_detect_mode_t;

bool szhSenInit(const access_config_t *config);
bool szhSenUpdate(void);

bool szhSenCalibrateBaseline(uint8_t samples);
bool szhSenCalibrateLoaded(uint8_t samples);

void szhSenSetThreshold(uint16_t threshold);
void szhSenSetDetectMode(szh_sen_detect_mode_t mode);

uint16_t szhSenGetRaw(void);
uint16_t szhSenGetBaseline(void);
uint16_t szhSenGetLoadedValue(void);
uint16_t szhSenGetDiff(void);
uint8_t szhSenGetPercent(void);

bool szhSenIsDetected(void);

#endif //__INCLUDE_DRIVER__SZH_SEN_H__
