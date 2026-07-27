#ifndef __LCD_H__
#define __LCD_H__

#include "hw_def.h"

void lcdInit(void);
void lcdClear(void);
void lcdSetCursor(uint8_t row, uint8_t col);
void lcdPrint(const char *str);
void lcdPrintf(const char *fmt, ...);
void lcdClearLine(uint8_t row);

#endif