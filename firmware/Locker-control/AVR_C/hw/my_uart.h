#ifndef __INCLUDE_HW__MY_UART_H_
#define __INCLUDE_HW__MY_UART_H_

#include "common.h"
#include "access_config.h"

void uart_Init(const access_config_t *config);

void uart_putchar(char c);
void uart_print(const char *str);
void uart_print_num(uint8_t n);
void uart_print_u16(uint16_t n);

char uart_getchar(void);
uint8_t uartAvailable(uint8_t ch);
uint8_t uartRead(uint8_t ch);
uint32_t uartWrite(uint8_t ch, const uint8_t *data, uint32_t len);

#endif //__INCLUDE_HW__MY_UART_H_
