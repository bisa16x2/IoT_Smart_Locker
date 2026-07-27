
#ifndef KIOSK_KEYPAD_H
#define KIOSK_KEYPAD_H

#include "hw_def.h"

// Row pin define
#define ROW1_PORT GPIOC
#define ROW1_PIN  GPIO_PIN_0

#define ROW2_PORT GPIOC
#define ROW2_PIN  GPIO_PIN_1

#define ROW3_PORT GPIOC
#define ROW3_PIN  GPIO_PIN_2

#define ROW4_PORT GPIOC
#define ROW4_PIN  GPIO_PIN_3


// Column pin define
#define COL1_PORT GPIOC
#define COL1_PIN  GPIO_PIN_4

#define COL2_PORT GPIOC
#define COL2_PIN  GPIO_PIN_5

#define COL3_PORT GPIOB
#define COL3_PIN  GPIO_PIN_0

#define COL4_PORT GPIOA
#define COL4_PIN  GPIO_PIN_4


char get_key(void);

#endif
