#ifndef __INCLUDE_HW__MY_ADC_H__
#define __INCLUDE_HW__MY_ADC_H__

#include "common.h"
#include "access_config.h"

typedef enum {
    MY_ADC_CH_TS0224 = 0,
    MY_ADC_CH_SZH_SEN,
    MY_ADC_CH_MAX
} my_adc_ch_t;

void adcInit(const access_config_t *config);

bool adcRead(my_adc_ch_t ch, uint16_t *p_data);
uint16_t adcReadRaw(my_adc_ch_t ch);

#endif // __INCLUDE_HW__MY_ADC_H__
