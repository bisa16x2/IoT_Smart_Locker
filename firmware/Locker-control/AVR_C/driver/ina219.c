#include "driver/ina219.h"
#include "hw/my_i2c.h"

static const access_config_t *ina219_config = 0;
static uint32_t g_ina219_i2c_error_count = 0;

static uint8_t ina219Addr(void) {
    return (ina219_config != 0) ? ina219_config->ina219_addr_7bit : 0x40U;
}

static uint16_t ina219CalibrationValue(void) {
    return (ina219_config != 0) ? ina219_config->ina219_calibration_value : 4096U;
}

static uint16_t ina219ConfigValue(void) {
    return (ina219_config != 0) ? ina219_config->ina219_config_value : 0x399FU;
}

// INA219 register 쓰기
static uint8_t ina219WriteRegister(uint8_t reg, uint16_t value) {
    uint8_t result;

    result = i2cWriteReg16(ina219Addr(), reg, value);

    if (result != I2C_OK) {
        g_ina219_i2c_error_count++;
        return I2C_ERROR;
    }

    return I2C_OK;
}

// INA219 register 읽기
static uint8_t ina219ReadRegister(uint8_t reg, uint16_t *out_value) {
    uint8_t result;

    if (out_value == 0) {
        return I2C_ERROR;
    }

    result = i2cReadReg16(ina219Addr(), reg, out_value);

    if (result != I2C_OK) {
        g_ina219_i2c_error_count++;
        return I2C_ERROR;
    }

    return I2C_OK;
}

// INA219 calibration 및 config 복구
static void ina219Reconfigure(void) {
    /*
     * INA219가 리셋되면 Calibration 레지스터가 초기화되어
     * Current 레지스터가 0으로만 나올 수 있으므로 재설정
     */
    (void)ina219WriteRegister(INA219_REG_CALIBRATION, ina219CalibrationValue());
    (void)ina219WriteRegister(INA219_REG_CONFIG, ina219ConfigValue());
}

// INA219 초기화
bool ina219Init(const access_config_t *config) {
    ina219_config = config;

    i2cInit(config);

    _delay_ms(50);

    if (i2cIsDeviceReady(ina219Addr()) != true) {
        g_ina219_i2c_error_count++;
        return false;
    }

    if (ina219WriteRegister(INA219_REG_CALIBRATION, ina219CalibrationValue()) != I2C_OK) {
        return false;
    }

    if (ina219WriteRegister(INA219_REG_CONFIG, ina219ConfigValue()) != I2C_OK) {
        return false;
    }

    return true;
}

// current 측정(mA)
float ina219ReadCurrent_mA(void) {
    static int16_t last_valid_raw_current = 0;
    static uint8_t zero_read_count = 0;

    uint16_t raw_u16 = 0;
    int16_t raw_i16;

    if (ina219ReadRegister(INA219_REG_CURRENT, &raw_u16) == I2C_OK) {
        raw_i16 = (int16_t)raw_u16;

        /*
         * 첨부 STM32 코드의 자동 복구 로직 반영:
         * current raw가 계속 0이면 calibration/config 재설정
         */
        if (raw_i16 == 0) {
            if (zero_read_count < 10U) {
                zero_read_count++;
            }

            if (zero_read_count >= 3U) {
                ina219Reconfigure();
            }
        }
        else {
            zero_read_count = 0;
        }

        last_valid_raw_current = raw_i16;
    }

    /*
     * 기존 STM32 코드와 동일하게 1 LSB = 0.1mA로 계산
     * mA = raw / 10.0
     */
    return (float)last_valid_raw_current / 10.0f;
}

// bus voltage 측정(V)
float ina219ReadBusVoltage_V(void) {
    static uint16_t last_valid_raw_bus = 0;

    uint16_t raw = 0;
    uint16_t bus_raw;

    if (ina219ReadRegister(INA219_REG_BUSVOLT, &raw) == I2C_OK) {
        last_valid_raw_bus = raw;
    }
    else {
        raw = last_valid_raw_bus;
    }

    /*
     * Bus Voltage Register:
     * bit 15:3 = 전압 데이터
     * 1 LSB = 4mV
     */
    bus_raw = raw >> 3;

    return ((float)bus_raw * 4.0f) / 1000.0f;
}

// shunt voltage 측정(mV)
float ina219ReadShuntVoltage_mV(void) {
    uint16_t raw_u16 = 0;
    int16_t raw_i16;

    if (ina219ReadRegister(INA219_REG_SHUNTVOLT, &raw_u16) == I2C_OK) {
        raw_i16 = (int16_t)raw_u16;

        /*
         * Shunt Voltage Register:
         * 1 LSB = 10uV = 0.01mV
         */
        return (float)raw_i16 * 0.01f;
    }

    return 0.0f;
}

// 평균 current 측정(mA)
float ina219ReadCurrentAvg_mA(uint8_t samples) {
    float sum = 0.0f;
    uint8_t i;

    if (samples == 0) {
        return 0.0f;
    }

    for (i = 0; i < samples; i++) {
        sum += ina219ReadCurrent_mA();
        _delay_ms(5);
    }

    return sum / (float)samples;
}

// I2C 오류 횟수 반환
uint32_t ina219GetI2cErrorCount(void) {
    return g_ina219_i2c_error_count;
}
