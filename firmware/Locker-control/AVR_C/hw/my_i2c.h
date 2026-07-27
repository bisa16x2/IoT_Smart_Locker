#ifndef __INCLUDE_HW__MY_I2C_H__
#define __INCLUDE_HW__MY_I2C_H__

#include "common.h"
#include "access_config.h"

#define I2C_OK 0
#define I2C_ERROR 1

void i2cInit(const access_config_t *config);

uint8_t i2cWriteReg16(uint8_t dev_addr_7bit, uint8_t reg, uint16_t value);
uint8_t i2cReadReg16(uint8_t dev_addr_7bit, uint8_t reg, uint16_t *out_value);
bool i2cIsDeviceReady(uint8_t dev_addr_7bit);

#endif //__INCLUDE_HW__MY_I2C_H__
