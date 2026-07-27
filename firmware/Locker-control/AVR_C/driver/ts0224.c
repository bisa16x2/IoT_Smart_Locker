#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include "driver/ts0224.h"
#include "hw/my_adc.h"

#define TS0224_ADC_CH MY_ADC_CH_TS0224

// 내부 상태
static const access_config_t *ts0224_config = 0;
static uint16_t ts0224_adc_raw = 0;
static uint16_t ts0224_adc_baseline = 0;
static uint16_t ts0224_adc_diff = 0;
static uint16_t ts0224_adc_threshold = 50U;

static bool ts0224_d0_raw = false;
static bool ts0224_detected_digital = false;
static bool ts0224_detected_analog = false;

static ts0224_d0_active_t ts0224_d0_active = TS0224_D0_ACTIVE_LOW;

// 두 ADC 값의 절대 차이 계산
static uint16_t ts0224AbsDiff(uint16_t a, uint16_t b) {
    if (a >= b) {
        return a - b;
    }

    return b - a;
}

// TS0224 digital output pull-up 초기화
static void ts0224D0Init(void) {
    volatile uint8_t *d0_ddr = (ts0224_config != 0) ? ts0224_config->hall_d0_ddr : &DDRD;
    volatile uint8_t *d0_port = (ts0224_config != 0) ? ts0224_config->hall_d0_port : &PORTD;
    uint8_t d0_pin = (ts0224_config != 0) ? ts0224_config->hall_d0_pin : PD2;

    /*
     * D0 입력 설정
     */
    *d0_ddr &= ~(1 << d0_pin);

    /*
     * 내부 Pull-up 활성화
     * LM393 비교기 출력 모듈이면 pull-up이 도움이 됨.
     * 모듈 자체에 pull-up이 있어도 보통 문제 없음.
     */
    *d0_port |= (1 << d0_pin);
}

// TS0224 analog ADC 읽기
static bool ts0224ReadAnalog(uint16_t *p_data) {
    if (p_data == 0) {
        return false;
    }

    return adcRead(TS0224_ADC_CH, p_data);
}

// TS0224 digital output 읽기
static bool ts0224ReadDigital(void) {
    volatile uint8_t *d0_pinr = (ts0224_config != 0) ? ts0224_config->hall_d0_pinr : &PIND;
    uint8_t d0_pin = (ts0224_config != 0) ? ts0224_config->hall_d0_pin : PD2;

    if (*d0_pinr & (1 << d0_pin)) {
        return true;
    }

    return false;
}

// TS0224 driver 초기화
bool ts0224Init(const access_config_t *config) {
    uint8_t samples;

    ts0224_config = config;
    ts0224_adc_threshold = (config != 0) ? config->hall_threshold : 50U;
    samples = (config != 0) ? config->hall_calibration_samples : 16U;
    /*
     * ADC 초기화
     */
    adcInit(config);

    /*
     * D0 GPIO 초기화
     */
    ts0224D0Init();

    /*
     * 자석이 없는 상태에서 init을 호출해야 baseline이 정상적으로 잡힘
     */
    if (ts0224Calibrate(samples) != true) {
        return false;
    }

    if (ts0224Update() != true) {
        return false;
    }

    return true;
}

// TS0224 감지 상태 갱신
bool ts0224Update(void) {
    if (ts0224ReadAnalog(&ts0224_adc_raw) != true) {
        return false;
    }

    ts0224_adc_diff = ts0224AbsDiff(ts0224_adc_raw, ts0224_adc_baseline);

    if (ts0224_adc_diff >= ts0224_adc_threshold) {
        ts0224_detected_analog = true;
    }
    else {
        ts0224_detected_analog = false;
    }

    ts0224_d0_raw = ts0224ReadDigital();

    if (ts0224_d0_active == TS0224_D0_ACTIVE_HIGH) {
        ts0224_detected_digital = ts0224_d0_raw;
    }
    else {
        ts0224_detected_digital = !ts0224_d0_raw;
    }

    return true;
}

// TS0224 baseline 보정
bool ts0224Calibrate(uint8_t samples) {
    uint32_t sum = 0;
    uint16_t adc_value = 0;
    uint8_t i;

    if (samples == 0) {
        return false;
    }

    for (i = 0; i < samples; i++) {
        if (ts0224ReadAnalog(&adc_value) != true) {
            return false;
        }

        sum += adc_value;
    }

    ts0224_adc_baseline = (uint16_t)(sum / samples);
    ts0224_adc_raw = ts0224_adc_baseline;
    ts0224_adc_diff = 0;
    ts0224_detected_analog = false;

    return true;
}

// 최근 analog raw 반환
uint16_t ts0224GetAnalogRaw(void) {
    return ts0224_adc_raw;
}

// analog raw 백분율 반환
uint8_t ts0224GetAnalogPercent(void) {
    return (uint8_t)((ts0224_adc_raw * 100U) / ((ts0224_config != 0) ? ts0224_config->adc_max_value : 1023U));
}

// baseline 반환
uint16_t ts0224GetBaseline(void) {
    return ts0224_adc_baseline;
}

// baseline 대비 차이 반환
uint16_t ts0224GetDiff(void) {
    return ts0224_adc_diff;
}

// digital output raw 반환
bool ts0224GetD0Raw(void) {
    return ts0224_d0_raw;
}

// digital 감지 여부 반환
bool ts0224IsDetectedDigital(void) {
    return ts0224_detected_digital;
}

// analog 감지 여부 반환
bool ts0224IsDetectedAnalog(void) {
    return ts0224_detected_analog;
}

// digital active level 설정
void ts0224SetD0Active(ts0224_d0_active_t active) {
    ts0224_d0_active = active;
}

// analog threshold 설정
void ts0224SetAnalogThreshold(uint16_t threshold) {
    ts0224_adc_threshold = threshold;
}
