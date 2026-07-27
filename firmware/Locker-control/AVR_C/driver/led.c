#include "driver/led.h"

static const access_config_t *led_config = 0;

static uint8_t ledRedPin(void) {
    return (led_config != 0) ? led_config->led_red_pin : PB0;
}

static uint8_t ledYellowPin(void) {
    return (led_config != 0) ? led_config->led_yellow_pin : PB1;
}

static uint8_t ledGreenPin(void) {
    return (led_config != 0) ? led_config->led_green_pin : PB2;
}

/*
    set DDRx, output PORTx >> LED control
*/

void led_Init(const access_config_t *config) {
    led_config = config;
    DDRB |= (1 << ledRedPin()) | (1 << ledYellowPin()) | (1 << ledGreenPin());
}
// 전체 LED ON
void led_ALL_ON(void) {
    PORTB |= (1 << ledRedPin()) | (1 << ledYellowPin()) | (1 << ledGreenPin());
}
// 전체 LED OFF
void led_ALL_Off(void) {
    PORTB &= ~((1 << ledRedPin()) | (1 << ledYellowPin()) | (1 << ledGreenPin()));
}

// 지정 LED ON
void led_On(uint8_t color) {
    PORTB |= (1 << color);
}

// 지정 LED OFF
void led_Off(uint8_t color) {
    PORTB &= ~(1 << color);
}

// 지정 LED toggle
void led_Toggle(uint8_t color) {
    PORTB ^= (1 << color);
}
