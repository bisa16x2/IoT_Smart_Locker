#include "hw/hw.h"
#include "hw/my_uart.h"
#include "hw/my_adc.h"
#include "hw/my_i2c.h"

// 1ms마다 증가하는 system tick counter
volatile uint32_t system_tick = 0;

// Timer0 compare interrupt: system tick 증가
ISR(TIMER0_COMPA_vect) {
    system_tick++;
}

// HW peripheral 초기화
void hwInit(const access_config_t *config) {
    uart_Init(config);
    adcInit(config);
    i2cInit(config);
    timer0_init();
}

// HW test 주기 작업
void hwMain(void) {
    hwComPrint("Hello, World!\r\n");
    hwDelay(10);
}

// Timer0 CTC 설정: 1ms tick
void timer0_init(void) {
    // CTC 모드
    TCCR0A = (1 << WGM01);

    // 64분주
    TCCR0B = (1 << CS01) | (1 << CS00);

    // 16MHz / 64 = 250kHz
    // 1ms마다 인터럽트 발생시키려면 250카운트 필요
    // OCR0A는 0부터 세므로 249
    OCR0A = 249;

    // Timer0 Compare Match A 인터럽트 허용
    TIMSK0 = (1 << OCIE0A);

    // 전역 인터럽트 허용
    sei();
}

// system tick 원자적 조회
uint32_t hwMillis(void) {
    uint32_t tick;

    cli();
    tick = system_tick;
    sei();

    return tick;
}

// blocking delay
void hwDelay(uint32_t ms) {
    _delay_ms(ms);
}

// UART 수신 가능 여부 반환
uint8_t hwComAvailable(void) {
    return uartAvailable(0);
}

// UART 1-byte 수신
uint8_t hwComRead(void) {
    return uartRead(0);
}

// UART 문자열 송신
void hwComPrint(const char *str) {
    uart_print(str);
}

// UART 8-bit 정수 송신
void hwComPrintNum(uint8_t n) {
    uart_print_num(n);
}

// UART 16-bit 정수 송신
void hwComPrintU16(uint16_t n) {
    uart_print_u16(n);
}
