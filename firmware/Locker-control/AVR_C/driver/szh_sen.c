#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include "driver/szh_sen.h"
#include "hw/my_adc.h"

#define SZH_SEN_ADC_CH MY_ADC_CH_SZH_SEN

static const access_config_t *szh_sen_config = 0;
static uint16_t szh_sen_raw = 0;
static uint16_t szh_sen_baseline = 0;
static uint16_t szh_sen_loaded_value = 0;
static uint16_t szh_sen_diff = 0;
static uint16_t szh_sen_threshold = 50U;

static bool szh_sen_detected = false;

static szh_sen_detect_mode_t szh_sen_mode = SZH_SEN_MODE_ABS_DIFF;

// 두 ADC 값의 절대 차이 계산
static uint16_t szhSenAbsDiff(uint16_t a, uint16_t b) {
    if (a >= b) {
        return a - b;
    }

    return b - a;
}

// flex pressure ADC 읽기
static bool szhSenReadAnalog(uint16_t *p_data) {
    if (p_data == 0) {
        return false;
    }

    return adcRead(SZH_SEN_ADC_CH, p_data);
}

// flex pressure sensor 초기화
bool szhSenInit(const access_config_t *config) {
    uint8_t samples;

    szh_sen_config = config;
    szh_sen_threshold = (config != 0) ? config->flex_threshold : 50U;
    samples = (config != 0) ? config->flex_calibration_samples : 16U;

    adcInit(config);

    /*
     * 빈 상태에서 baseline 선행 보정 필요
     */
    if (szhSenCalibrateBaseline(samples) != true) {
        return false;
    }

    szh_sen_loaded_value = szh_sen_baseline;

    if (szhSenUpdate() != true) {
        return false;
    }

    return true;
}

// flex pressure 상태 갱신
bool szhSenUpdate(void) {
    if (szhSenReadAnalog(&szh_sen_raw) != true) {
        return false;
    }

    szh_sen_diff = szhSenAbsDiff(szh_sen_raw, szh_sen_baseline);

    switch (szh_sen_mode) {
        case SZH_SEN_MODE_ABS_DIFF:
            if (szh_sen_diff >= szh_sen_threshold) {
                szh_sen_detected = true;
            }
            else {
                szh_sen_detected = false;
            }
            break;

        case SZH_SEN_MODE_RAW_HIGH:
            if (szh_sen_raw >= (szh_sen_baseline + szh_sen_threshold)) {
                szh_sen_detected = true;
            }
            else {
                szh_sen_detected = false;
            }
            break;

        case SZH_SEN_MODE_RAW_LOW:
            if ((szh_sen_baseline >= szh_sen_threshold) &&
                (szh_sen_raw <= (szh_sen_baseline - szh_sen_threshold))) {
                szh_sen_detected = true;
            }
            else {
                szh_sen_detected = false;
            }
            break;

        default:
            szh_sen_detected = false;
            break;
    }

    return true;
}

// 빈 상태 baseline 보정
bool szhSenCalibrateBaseline(uint8_t samples) {
    uint32_t sum = 0;
    uint16_t adc_value = 0;
    uint8_t i;

    if (samples == 0) {
        return false;
    }

    for (i = 0; i < samples; i++) {
        if (szhSenReadAnalog(&adc_value) != true) {
            return false;
        }

        sum += adc_value;
    }

    szh_sen_baseline = (uint16_t)(sum / samples);
    szh_sen_raw = szh_sen_baseline;
    szh_sen_diff = 0;
    szh_sen_detected = false;

    return true;
}

// 적재 상태 기준값 보정
bool szhSenCalibrateLoaded(uint8_t samples) {
    uint32_t sum = 0;
    uint16_t adc_value = 0;
    uint8_t i;

    if (samples == 0) {
        return false;
    }

    /*
     * 물체 적재 후 호출
     * percent 계산 기준값으로 사용
     */
    for (i = 0; i < samples; i++) {
        if (szhSenReadAnalog(&adc_value) != true) {
            return false;
        }

        sum += adc_value;
    }

    szh_sen_loaded_value = (uint16_t)(sum / samples);

    return true;
}

// 감지 threshold 설정
void szhSenSetThreshold(uint16_t threshold) {
    szh_sen_threshold = threshold;
}

// 감지 mode 설정
void szhSenSetDetectMode(szh_sen_detect_mode_t mode) {
    szh_sen_mode = mode;
}

// raw 값 반환
uint16_t szhSenGetRaw(void) {
    return szh_sen_raw;
}

// baseline 반환
uint16_t szhSenGetBaseline(void) {
    return szh_sen_baseline;
}

// 적재 기준값 반환
uint16_t szhSenGetLoadedValue(void) {
    return szh_sen_loaded_value;
}

// baseline 대비 차이 반환
uint16_t szhSenGetDiff(void) {
    return szh_sen_diff;
}

// pressure 백분율 반환
uint8_t szhSenGetPercent(void) {
    uint16_t range;
    uint16_t value;
    uint32_t percent;

    /*
     * loaded calibration을 하지 않았다면
     * diff 기준 단순 계산
     */
    if (szh_sen_loaded_value == szh_sen_baseline) {
        percent = ((uint32_t)szh_sen_diff * 100UL) / ((szh_sen_config != 0) ? szh_sen_config->adc_max_value : 1023U);

        if (percent > 100UL) {
            percent = 100UL;
        }

        return (uint8_t)percent;
    }

    /*
     * baseline과 loaded_value 사이를 0~100%로 환산
     */
    if (szh_sen_loaded_value > szh_sen_baseline) {
        range = szh_sen_loaded_value - szh_sen_baseline;

        if (szh_sen_raw <= szh_sen_baseline) {
            value = 0;
        }
        else if (szh_sen_raw >= szh_sen_loaded_value) {
            value = range;
        }
        else {
            value = szh_sen_raw - szh_sen_baseline;
        }
    }
    else {
        range = szh_sen_baseline - szh_sen_loaded_value;

        if (szh_sen_raw >= szh_sen_baseline) {
            value = 0;
        }
        else if (szh_sen_raw <= szh_sen_loaded_value) {
            value = range;
        }
        else {
            value = szh_sen_baseline - szh_sen_raw;
        }
    }

    if (range == 0) {
        return 0;
    }

    percent = ((uint32_t)value * 100UL) / range;

    if (percent > 100UL) {
        percent = 100UL;
    }

    return (uint8_t)percent;
}

// 물체 감지 여부 반환
bool szhSenIsDetected(void) {
    return szh_sen_detected;
}
