#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include "hw/my_adc.h"

static const access_config_t *adc_config = 0;

// ADC 초기화
void adcInit(const access_config_t *config) {
    adc_config = config;
    /*
     * 기준 전압: AVCC
     * ADC 채널: 초기값 ADC0
     */
    ADMUX = (1 << REFS0);

    /*
     * ADEN  : ADC Enable
     * ADPS2:ADPS0 = 111
     * 16 MHz / 128 = 125 kHz
     * AVR ADC 권장 클럭 범위에 맞춤
     */
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

// ADC channel 읽기
bool adcRead(my_adc_ch_t ch, uint16_t *p_data) {
    uint8_t adc_channel;

    if (p_data == 0) {
        return false;
    }

    if (ch >= MY_ADC_CH_MAX) {
        return false;
    }

    switch (ch) {
        case MY_ADC_CH_TS0224:
            adc_channel = (adc_config != 0) ? adc_config->hall_adc_channel : 0U;
            break;

        case MY_ADC_CH_SZH_SEN:
            adc_channel = (adc_config != 0) ? adc_config->flex_adc_channel : 1U;
            break;

        default:
            return false;
    }

    /*
     * REFS0는 유지하고, 하위 4비트만 ADC 채널로 설정
     */
    ADMUX = (ADMUX & 0xF0) | (adc_channel & 0x0F);

    /*
     * ADC 변환 시작
     */
    ADCSRA |= (1 << ADSC);

    /*
     * 변환 완료 대기
     */
    while (ADCSRA & (1 << ADSC)) {
        ;
    }

    /*
     * ADC는 10비트 값, 범위 0~1023
     */
    *p_data = ADC;

    return true;
}

// ADC raw 값 읽기
uint16_t adcReadRaw(my_adc_ch_t ch) {
    uint16_t adc_value = 0;

    (void)adcRead(ch, &adc_value);

    return adc_value;
}
