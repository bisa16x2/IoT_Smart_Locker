#ifndef __INCLUDE_DRIVER__INA219_H__
#define __INCLUDE_DRIVER__INA219_H__

#include "common.h"
#include "access_config.h"

#define INA219_REG_CONFIG      0x00
#define INA219_REG_SHUNTVOLT   0x01
#define INA219_REG_BUSVOLT     0x02
#define INA219_REG_POWER       0x03
#define INA219_REG_CURRENT     0x04
#define INA219_REG_CALIBRATION 0x05

bool ina219Init(const access_config_t *config);

float ina219ReadCurrent_mA(void);
float ina219ReadBusVoltage_V(void);
float ina219ReadShuntVoltage_mV(void);
float ina219ReadCurrentAvg_mA(uint8_t samples);

uint32_t ina219GetI2cErrorCount(void);

#endif //__INCLUDE_DRIVER__INA219_H__
