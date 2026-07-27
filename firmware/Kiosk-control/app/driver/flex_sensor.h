/*
#ifndef __FLEX_SENSOR_H__
#define __FLEX_SENSOR_H__

#include "hw_def.h"

#define FLEX_SENSOR_ADC_MAX             4095U
#define FLEX_SENSOR_VREF_MV             3300U
#define FLEX_SENSOR_SAMPLE_TIME_MS      500U
#define FLEX_SENSOR_ADC_TIMEOUT_MS      10U

typedef struct
{
    bool is_valid;

    uint16_t raw;
    uint16_t raw_filtered;
    uint16_t voltage_mv;

    uint32_t sample_count;
    uint32_t error_count;
    uint32_t last_update_time;
} flex_sensor_t;

bool flexSensorInit(void);
bool flexSensorReadOnce(flex_sensor_t *p_data);

void flexSensorTask(void);
bool flexSensorAvailable(void);

const flex_sensor_t *flexSensorGetData(void);

#endif
*/