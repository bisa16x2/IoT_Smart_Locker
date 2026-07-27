#include "driver/servo.h"

static const access_config_t *servo_config = 0;

static uint8_t servoPin(void) {
    return (servo_config != 0) ? servo_config->servo_pin : PD3;
}

static uint8_t servoUnlockValue(void) {
    return (servo_config != 0) ? servo_config->servo_unlock_value : 24U;
}

static uint8_t servoLockValue(void) {
    return (servo_config != 0) ? servo_config->servo_lock_value : 7U;
}

// servo PWM 초기화
void servo_init(const access_config_t *config) {
    servo_config = config;

    DDRD |= (1 << servoPin());

    // Fast PWM, OC2B 비반전
    TCCR2A = (1 << COM2B1) | (1 << WGM21) | (1 << WGM20);
    TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20);

    OCR2B = servoLockValue(); // 시작은 닫힘
}

// 창문 열기
void servo_Ang90(void) {
    OCR2B = servoUnlockValue();
}

// 창문 닫기
void servo_Ang0(void) {
    OCR2B = servoLockValue();
}
