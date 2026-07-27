#ifndef __INCLUDE_HW__HW_H__
#define __INCLUDE_HW__HW_H__

#include "common.h"
#include "access_config.h"

void hwInit(const access_config_t *config);
void hwMain(void);

void timer0_init(void);
uint32_t hwMillis(void);
void hwDelay(uint32_t ms);

uint8_t hwComAvailable(void);
uint8_t hwComRead(void);
void hwComPrint(const char *str);
void hwComPrintNum(uint8_t n);
void hwComPrintU16(uint16_t n);

#endif //__INCLUDE_HW__HW_H__
