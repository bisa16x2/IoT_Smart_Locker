#ifndef __INCLUDE_DRIVER__LED_H_
#define __INCLUDE_DRIVER__LED_H_

#include "common.h"
#include "access_config.h"

void led_Init(const access_config_t *config);

void led_ALL_ON(void);
void led_ALL_Off(void);

void led_On(uint8_t color);
void led_Off(uint8_t color);
void led_Toggle(uint8_t color);

#endif //__INCLUDE_DRIVER__LED_H_
