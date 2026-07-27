#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include "hw/my_i2c.h"
#include "common.h"

// I2C start 및 address 송신
static uint8_t i2cStart(uint8_t address_rw) {
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);

    while (!(TWCR & (1 << TWINT))) {
        ;
    }

    if ((TW_STATUS != TW_START) && (TW_STATUS != TW_REP_START)) {
        return I2C_ERROR;
    }

    TWDR = address_rw;
    TWCR = (1 << TWINT) | (1 << TWEN);

    while (!(TWCR & (1 << TWINT))) {
        ;
    }

    if ((TW_STATUS != TW_MT_SLA_ACK) && (TW_STATUS != TW_MR_SLA_ACK)) {
        return I2C_ERROR;
    }

    return I2C_OK;
}

// I2C stop
static void i2cStop(void) {
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);

    while (TWCR & (1 << TWSTO)) {
        ;
    }
}

// I2C 1-byte 송신
static uint8_t i2cWriteByte(uint8_t data) {
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);

    while (!(TWCR & (1 << TWINT))) {
        ;
    }

    if (TW_STATUS != TW_MT_DATA_ACK) {
        return I2C_ERROR;
    }

    return I2C_OK;
}

// I2C ACK 수신
static uint8_t i2cReadAck(void) {
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);

    while (!(TWCR & (1 << TWINT))) {
        ;
    }

    return TWDR;
}

// I2C NACK 수신
static uint8_t i2cReadNack(void) {
    TWCR = (1 << TWINT) | (1 << TWEN);

    while (!(TWCR & (1 << TWINT))) {
        ;
    }

    return TWDR;
}

// 공개 API
void i2cInit(const access_config_t *config) {
    uint32_t i2c_freq_hz = 100000UL;

    if ((config != 0) && (config->i2c_freq_hz > 0UL)) {
        i2c_freq_hz = config->i2c_freq_hz;
    }
    /*
     * TWI Prescaler = 1
     * SCL = F_CPU / (16 + 2 * TWBR * Prescaler)
     *
     * F_CPU = 16MHz, SCL = 100kHz일 때 TWBR = 72
     */
    TWSR = 0x00;
    TWBR = (uint8_t)(((F_CPU / i2c_freq_hz) - 16UL) / 2UL);

    TWCR = (1 << TWEN);
}

// I2C device 응답 확인
bool i2cIsDeviceReady(uint8_t dev_addr_7bit) {
    uint8_t result;

    result = i2cStart((uint8_t)((dev_addr_7bit << 1) | 0));

    i2cStop();

    if (result == I2C_OK) {
        return true;
    }

    return false;
}

// I2C 16-bit register 쓰기
uint8_t i2cWriteReg16(uint8_t dev_addr_7bit, uint8_t reg, uint16_t value) {
    uint8_t result;

    result = i2cStart((uint8_t)((dev_addr_7bit << 1) | 0));
    if (result != I2C_OK) {
        i2cStop();
        return I2C_ERROR;
    }

    if (i2cWriteByte(reg) != I2C_OK) {
        i2cStop();
        return I2C_ERROR;
    }

    /*
     * INA219는 16비트 레지스터를 MSB 먼저 전송
     */
    if (i2cWriteByte((uint8_t)(value >> 8)) != I2C_OK) {
        i2cStop();
        return I2C_ERROR;
    }

    if (i2cWriteByte((uint8_t)(value & 0xFF)) != I2C_OK) {
        i2cStop();
        return I2C_ERROR;
    }

    i2cStop();

    return I2C_OK;
}

// I2C 16-bit register 읽기
uint8_t i2cReadReg16(uint8_t dev_addr_7bit, uint8_t reg, uint16_t *out_value) {
    uint8_t msb;
    uint8_t lsb;

    if (out_value == 0) {
        return I2C_ERROR;
    }

    /*
     * 1단계: 읽을 레지스터 주소 지정
     */
    if (i2cStart((uint8_t)((dev_addr_7bit << 1) | 0)) != I2C_OK) {
        i2cStop();
        return I2C_ERROR;
    }

    if (i2cWriteByte(reg) != I2C_OK) {
        i2cStop();
        return I2C_ERROR;
    }

    /*
     * 2단계: Repeated Start 후 2바이트 읽기
     */
    if (i2cStart((uint8_t)((dev_addr_7bit << 1) | 1)) != I2C_OK) {
        i2cStop();
        return I2C_ERROR;
    }

    msb = i2cReadAck();
    lsb = i2cReadNack();

    i2cStop();

    *out_value = (uint16_t)(((uint16_t)msb << 8) | lsb);

    return I2C_OK;
}
