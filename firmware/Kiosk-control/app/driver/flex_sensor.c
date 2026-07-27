/*
#include "flex_sensor.h"

static flex_sensor_t flex_sensor;

static uint32_t pre_time = 0;
static bool is_updated = false;
static bool filter_init = false;

static bool flexSensorReadAdc(uint16_t *p_raw)
{
    if (p_raw == NULL)
    {
        return false;
    }

    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        return false;
    }

    if (HAL_ADC_PollForConversion(&hadc1, FLEX_SENSOR_ADC_TIMEOUT_MS) != HAL_OK)
    {
        HAL_ADC_Stop(&hadc1);
        return false;
    }

    *p_raw = (uint16_t)HAL_ADC_GetValue(&hadc1);

    HAL_ADC_Stop(&hadc1);

    return true;
}

bool flexSensorInit(void)
{
    memset(&flex_sensor, 0, sizeof(flex_sensor));

    pre_time = HAL_GetTick();
    is_updated = false;
    filter_init = false;

    return true;
}

bool flexSensorReadOnce(flex_sensor_t *p_data)
{
    uint16_t raw = 0;

    if (flexSensorReadAdc(&raw) != true)
    {
        flex_sensor.is_valid = false;
        flex_sensor.error_count++;

        if (p_data != NULL)
        {
            *p_data = flex_sensor;
        }

        return false;
    }

    flex_sensor.raw = raw;

    if (filter_init == false)
    {
        flex_sensor.raw_filtered = raw;
        filter_init = true;
    }
    else
    {
        flex_sensor.raw_filtered =
            (uint16_t)(((uint32_t)flex_sensor.raw_filtered * 7U + raw) / 8U);
    }

    flex_sensor.voltage_mv =
        (uint16_t)(((uint32_t)flex_sensor.raw_filtered * FLEX_SENSOR_VREF_MV)
                   / FLEX_SENSOR_ADC_MAX);

    flex_sensor.sample_count++;
    flex_sensor.last_update_time = HAL_GetTick();
    flex_sensor.is_valid = true;

    if (p_data != NULL)
    {
        *p_data = flex_sensor;
    }

    return true;
}

void flexSensorTask(void)
{
    uint32_t now = HAL_GetTick();

    if (now - pre_time >= FLEX_SENSOR_SAMPLE_TIME_MS)
    {
        pre_time = now;

        flexSensorReadOnce(&flex_sensor);
        is_updated = true;
    }
}

bool flexSensorAvailable(void)
{
    if (is_updated == true)
    {
        is_updated = false;
        return true;
    }

    return false;
}

const flex_sensor_t *flexSensorGetData(void)
{
    return &flex_sensor;
}
    */