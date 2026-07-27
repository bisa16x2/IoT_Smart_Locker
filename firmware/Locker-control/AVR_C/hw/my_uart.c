#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#include "hw/my_uart.h"

#define UART_WIFI_CH 1U

static uint8_t wifi_rx_pin = PD6;
static uint8_t wifi_tx_pin = PD7;
static uint32_t wifi_baud = 38400UL;

static void wifiSoftUartInit(const access_config_t *config);
static void wifiSoftUartBitDelay(void);
static void wifiSoftUartHalfBitDelay(void);
static void wifiSoftUartWriteByte(uint8_t data);
static uint8_t wifiSoftUartReadByte(void);
static uint8_t wifiSoftUartAvailable(void);

/*
    uart baudrate register >> 통신 주파수 설정
    control state register  A >> Rx
                            B >> Tx/Rx 활성화
                            C >> data frame 설정 몇 bit씩 보낼건지
    uart data register >> 데이터를 저장해 두는 레지스터 Tx면 1개 씩 보내고, Rx면 전부 비울 때까지 작동
*/

void uart_Init(const access_config_t *config) {
    uint32_t baud = 9600UL;
    uint16_t ubrr_val;

    if ((config != 0) && (config->uart_baud > 0UL)) {
        baud = config->uart_baud;
    }

    ubrr_val = (uint16_t)((F_CPU / 8UL / baud) - 1UL);

    // baudrate register
    UBRR0H = (uint8_t)(ubrr_val >> 8);
    UBRR0L = (uint8_t)(ubrr_val);
    // control state register
    UCSR0B = (1 << TXEN0) | (1 << RXEN0);   // TX/Rx 활성화
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // data frame 설정 8bit

    wifiSoftUartInit(config);
}

// UART 1-byte 송신
void uart_putchar(char c) {
    while (!(UCSR0A & (1 << UDRE0))); // 송신 register 빌 때까지 대기
    UDR0 = (uint8_t)c;                // c를 data register에 저장
}

// UART 문자열 송신
void uart_print(const char *str) {
    while (*str) {
        if (*str == '\n') uart_putchar('\r'); // newline \n 앞에 \r 추가
        uart_putchar(*str++);                 // str을 한 글자씩 전송
    }
}

// UART 8-bit 정수 송신
void uart_print_num(uint8_t n) {
    uart_print_u16(n);
}

// UART 16-bit 정수 송신
void uart_print_u16(uint16_t n) {
    char numString[6] = "0";
    uint8_t i = 0;

    if (n == 0) {
        uart_putchar('0');
        return;
    }

    while (n != 0) {
        numString[i++] = (n % 10U) + '0';
        n /= 10U;
    }

    while (i > 0) {
        uart_putchar(numString[--i]);
    }
}

// UART 1-byte 수신
char uart_getchar(void) {
    while (!(UCSR0A & (1 << RXC0))); // data register로 Rx 완료 대기
    return (char)UDR0;               // data register 값 읽기
}

// UART 수신 가능 여부 확인
uint8_t uartAvailable(uint8_t ch) {
    if (ch == UART_WIFI_CH) {
        return wifiSoftUartAvailable();
    }

    if (UCSR0A & (1 << RXC0)) {
        return 1;
    }

    return 0;
}

// UART channel별 1-byte 수신
uint8_t uartRead(uint8_t ch) {
    if (ch == UART_WIFI_CH) {
        return wifiSoftUartReadByte();
    }

    if (uartAvailable(0) == 0) {
        return 0;
    }

    return UDR0;
}

// UART channel별 문자열 송신
uint32_t uartWrite(uint8_t ch, const uint8_t *data, uint32_t len) {
    uint32_t i;

    if (data == 0) {
        return 0;
    }

    for (i = 0; i < len; i++) {
        if (ch == UART_WIFI_CH) {
            wifiSoftUartWriteByte(data[i]);
        }
        else {
            uart_putchar((char)data[i]);
        }
    }

    return len;
}
// D6/D7 Wi-Fi software UART 초기화
static void wifiSoftUartInit(const access_config_t *config) {
    if (config != 0) {
        wifi_rx_pin = config->wifi_rx_pin;
        wifi_tx_pin = config->wifi_tx_pin;
        if (config->uart_baud > 0UL) {
            wifi_baud = config->uart_baud;
        }
    }

    DDRD &= (uint8_t)~(1U << wifi_rx_pin);
    PORTD |= (uint8_t)(1U << wifi_rx_pin);

    DDRD |= (uint8_t)(1U << wifi_tx_pin);
    PORTD |= (uint8_t)(1U << wifi_tx_pin);
}

static void wifiSoftUartBitDelay(void) {
    if (wifi_baud <= 9600UL) {
        _delay_us(104.0);
    }
    else if (wifi_baud <= 19200UL) {
        _delay_us(52.0);
    }
    else {
        _delay_us(26.0);
    }
}

static void wifiSoftUartHalfBitDelay(void) {
    if (wifi_baud <= 9600UL) {
        _delay_us(52.0);
    }
    else if (wifi_baud <= 19200UL) {
        _delay_us(26.0);
    }
    else {
        _delay_us(13.0);
    }
}

static void wifiSoftUartWriteByte(uint8_t data) {
    uint8_t sreg = SREG;

    cli();
    PORTD &= (uint8_t)~(1U << wifi_tx_pin);
    wifiSoftUartBitDelay();

    for (uint8_t i = 0; i < 8U; i++) {
        if ((data & 0x01U) != 0U) {
            PORTD |= (uint8_t)(1U << wifi_tx_pin);
        }
        else {
            PORTD &= (uint8_t)~(1U << wifi_tx_pin);
        }

        wifiSoftUartBitDelay();
        data >>= 1;
    }

    PORTD |= (uint8_t)(1U << wifi_tx_pin);
    wifiSoftUartBitDelay();
    SREG = sreg;
}

static uint8_t wifiSoftUartReadByte(void) {
    uint8_t data = 0U;
    uint8_t sreg;

    if (wifiSoftUartAvailable() == 0U) {
        return 0U;
    }

    sreg = SREG;
    cli();

    wifiSoftUartHalfBitDelay();
    wifiSoftUartBitDelay();

    for (uint8_t i = 0; i < 8U; i++) {
        if ((PIND & (1U << wifi_rx_pin)) != 0U) {
            data |= (uint8_t)(1U << i);
        }
        wifiSoftUartBitDelay();
    }

    wifiSoftUartBitDelay();
    SREG = sreg;

    return data;
}

static uint8_t wifiSoftUartAvailable(void) {
    if ((PIND & (1U << wifi_rx_pin)) == 0U) {
        return 1U;
    }

    return 0U;
}
