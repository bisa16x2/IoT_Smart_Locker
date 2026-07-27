#ifndef __INCLUDE_DRIVER__SERVO_H__
#define __INCLUDE_DRIVER__SERVO_H__

#include "common.h"
#include "access_config.h"

void Timer0_Init(void);

void servo_init(const access_config_t *config);
void servo_Ang90(void);
void servo_Ang0(void);

#endif //__INCLUDE_DRIVER__SERVO_H__
