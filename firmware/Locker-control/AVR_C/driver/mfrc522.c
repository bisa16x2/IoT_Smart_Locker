#include "driver/mfrc522.h"

static const access_config_t *mfrc522_config = 0;

static volatile uint8_t *mfrc522CsDdr(void) {
    return (mfrc522_config != 0) ? mfrc522_config->rfid_cs_ddr : &DDRB;
}

static volatile uint8_t *mfrc522CsPort(void) {
    return (mfrc522_config != 0) ? mfrc522_config->rfid_cs_port : &PORTB;
}

static uint8_t mfrc522CsPin(void) {
    return (mfrc522_config != 0) ? mfrc522_config->rfid_cs_pin : PB2;
}

static volatile uint8_t *mfrc522RstDdr(void) {
    return (mfrc522_config != 0) ? mfrc522_config->rfid_rst_ddr : &DDRB;
}

static volatile uint8_t *mfrc522RstPort(void) {
    return (mfrc522_config != 0) ? mfrc522_config->rfid_rst_port : &PORTB;
}

static uint8_t mfrc522RstPin(void) {
    return (mfrc522_config != 0) ? mfrc522_config->rfid_rst_pin : PB1;
}

static uint8_t mfrc522MosiPin(void) {
    return (mfrc522_config != 0) ? mfrc522_config->rfid_mosi_pin : PB3;
}

static uint8_t mfrc522MisoPin(void) {
    return (mfrc522_config != 0) ? mfrc522_config->rfid_miso_pin : PB4;
}

static uint8_t mfrc522SckPin(void) {
    return (mfrc522_config != 0) ? mfrc522_config->rfid_sck_pin : PB5;
}

// ================================
// MFRC522 Register Map
// ================================

#define COMMAND_REG 0x01
#define COM_IEN_REG 0x02
#define DIV_IEN_REG 0x03
#define COMM_IRQ_REG 0x04
#define DIV_IRQ_REG 0x05
#define ERROR_REG 0x06
#define STATUS1_REG 0x07
#define STATUS2_REG 0x08
#define FIFO_DATA_REG 0x09
#define FIFO_LEVEL_REG 0x0A
#define CONTROL_REG 0x0C
#define BIT_FRAMING_REG 0x0D
#define COLL_REG 0x0E

#define MODE_REG 0x11
#define TX_MODE_REG 0x12
#define RX_MODE_REG 0x13
#define TX_CONTROL_REG 0x14
#define TX_ASK_REG 0x15

#define CRC_RESULT_REG_H 0x21
#define CRC_RESULT_REG_L 0x22

#define T_MODE_REG 0x2A
#define T_PRESCALER_REG 0x2B
#define T_RELOAD_REG_H 0x2C
#define T_RELOAD_REG_L 0x2D

// ================================
// MFRC522 Command
// ================================

#define PCD_IDLE 0x00
#define PCD_AUTHENT 0x0E
#define PCD_TRANSCEIVE 0x0C
#define PCD_RESETPHASE 0x0F
#define PCD_CALC_CRC 0x03

// ================================
// SPI / CS 제어
// ================================

static void rc522_cs_low(void) {
    *mfrc522CsPort() &= ~(1 << mfrc522CsPin());
}

// RC522 CS HIGH
static void rc522_cs_high(void) {
    *mfrc522CsPort() |= (1 << mfrc522CsPin());
}

// SPI master 초기화
static void spi_master_init(void) {
    // PB2: SS, PB3: MOSI, PB5: SCK 출력
    *mfrc522CsDdr() |= (1 << mfrc522CsPin()) | (1 << mfrc522MosiPin()) | (1 << mfrc522SckPin());

    // PB4: MISO 입력
    *mfrc522CsDdr() &= ~(1 << mfrc522MisoPin());

    // SS 기본 HIGH
    rc522_cs_high();

    /*
     * SPE  : SPI Enable
     * MSTR : Master Mode
     * SPR0 : fosc / 16
     */
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR0);
    SPSR = 0x00;
}

// SPI 1-byte 송수신
static uint8_t spi_transfer(uint8_t data) {
    SPDR = data;

    while (!(SPSR & (1 << SPIF))) {
        ;
    }

    return SPDR;
}

// ================================
// RC522 Register 접근
// ================================

static void mfrc522_write_register(uint8_t reg, uint8_t value) {
    rc522_cs_low();

    /*
     * Write format:
     * bit7 = 0
     * bit6~1 = address
     * bit0 = 0
     */
    spi_transfer((reg << 1) & 0x7E);
    spi_transfer(value);

    rc522_cs_high();
}

// RC522 register 읽기
static uint8_t mfrc522_read_register(uint8_t reg) {
    uint8_t value;

    rc522_cs_low();

    /*
     * Read format:
     * bit7 = 1
     * bit6~1 = address
     * bit0 = 0
     */
    spi_transfer(((reg << 1) & 0x7E) | 0x80);
    value = spi_transfer(0x00);

    rc522_cs_high();

    return value;
}

// RC522 register bit 설정
static void mfrc522_set_bit_mask(uint8_t reg, uint8_t mask) {
    uint8_t tmp = mfrc522_read_register(reg);
    mfrc522_write_register(reg, tmp | mask);
}

// RC522 register bit 해제
static void mfrc522_clear_bit_mask(uint8_t reg, uint8_t mask) {
    uint8_t tmp = mfrc522_read_register(reg);
    mfrc522_write_register(reg, tmp & (~mask));
}

// ================================
// Antenna 제어
// ================================

static void mfrc522_antenna_on(void) {
    uint8_t temp = mfrc522_read_register(TX_CONTROL_REG);

    if ((temp & 0x03) != 0x03) {
        mfrc522_set_bit_mask(TX_CONTROL_REG, 0x03);
    }
}

// RC522 antenna 활성화
static void __attribute__((unused)) mfrc522_antenna_off(void) {
    mfrc522_clear_bit_mask(TX_CONTROL_REG, 0x03);
}

// ================================
// CRC 계산
// ================================

static uint8_t mfrc522_calculate_crc(const uint8_t *data, uint8_t len, uint8_t *result) {
    uint8_t i;
    uint8_t n;
    uint16_t timeout = 5000;

    mfrc522_clear_bit_mask(DIV_IRQ_REG, 0x04);
    mfrc522_set_bit_mask(FIFO_LEVEL_REG, 0x80);

    for (i = 0; i < len; i++) {
        mfrc522_write_register(FIFO_DATA_REG, data[i]);
    }

    mfrc522_write_register(COMMAND_REG, PCD_CALC_CRC);

    do {
        n = mfrc522_read_register(DIV_IRQ_REG);
        timeout--;
    } while ((timeout != 0) && !(n & 0x04));

    mfrc522_write_register(COMMAND_REG, PCD_IDLE);

    if (timeout == 0) {
        return MI_TIMEOUT;
    }

    result[0] = mfrc522_read_register(CRC_RESULT_REG_L);
    result[1] = mfrc522_read_register(CRC_RESULT_REG_H);

    return MI_OK;
}

// ================================
// RC522와 카드 사이의 송수신
// ================================

static uint8_t mfrc522_to_card(
    uint8_t command,
    const uint8_t *sendData,
    uint8_t sendLen,
    uint8_t *backData,
    uint8_t *backLen,
    uint16_t *backBits) {
    uint8_t status = MI_ERR;
    uint8_t irqEn = 0x00;
    uint8_t waitIRq = 0x00;
    uint8_t lastBits;
    uint8_t n;
    uint8_t fifoBytes;
    uint8_t i;
    uint16_t timeout = 2000;

    if (command == PCD_AUTHENT) {
        irqEn = 0x12;
        waitIRq = 0x10;
    }
    else if (command == PCD_TRANSCEIVE) {
        irqEn = 0x77;
        waitIRq = 0x30;
    }

    mfrc522_write_register(COM_IEN_REG, irqEn | 0x80);
    mfrc522_clear_bit_mask(COMM_IRQ_REG, 0x80);
    mfrc522_set_bit_mask(FIFO_LEVEL_REG, 0x80);

    mfrc522_write_register(COMMAND_REG, PCD_IDLE);

    for (i = 0; i < sendLen; i++) {
        mfrc522_write_register(FIFO_DATA_REG, sendData[i]);
    }

    mfrc522_write_register(COMMAND_REG, command);

    if (command == PCD_TRANSCEIVE) {
        mfrc522_set_bit_mask(BIT_FRAMING_REG, 0x80);
    }

    do {
        n = mfrc522_read_register(COMM_IRQ_REG);
        timeout--;
    } while ((timeout != 0) && !(n & 0x01) && !(n & waitIRq));

    mfrc522_clear_bit_mask(BIT_FRAMING_REG, 0x80);

    if (timeout == 0) {
        return MI_TIMEOUT;
    }

    if (mfrc522_read_register(ERROR_REG) & 0x1B) {
        return MI_ERR;
    }

    status = MI_OK;

    if (command == PCD_TRANSCEIVE) {
        fifoBytes = mfrc522_read_register(FIFO_LEVEL_REG);
        lastBits = mfrc522_read_register(CONTROL_REG) & 0x07;

        if (lastBits) {
            *backBits = ((uint16_t)(fifoBytes - 1) * 8) + lastBits;
        }
        else {
            *backBits = (uint16_t)fifoBytes * 8;
        }

        if (fifoBytes > *backLen) {
            fifoBytes = *backLen;
            status = MI_ERR;
        }

        *backLen = fifoBytes;

        for (i = 0; i < fifoBytes; i++) {
            backData[i] = mfrc522_read_register(FIFO_DATA_REG);
        }
    }

    return status;
}

// ================================
// Public Functions
// ================================

void mfrc522_reset(void) {
    mfrc522_write_register(COMMAND_REG, PCD_RESETPHASE);
    _delay_ms(50);
}

// RC522 초기화
void mfrc522_init(const access_config_t *config) {
    mfrc522_config = config;

    spi_master_init();

    *mfrc522RstDdr() |= (1 << mfrc522RstPin());

    *mfrc522RstPort() &= ~(1 << mfrc522RstPin());
    _delay_ms(10);
    *mfrc522RstPort() |= (1 << mfrc522RstPin());
    _delay_ms(50);

    mfrc522_reset();

    /*
     * Timer 설정
     * 카드 응답 대기 시간 관련 설정
     */
    mfrc522_write_register(T_MODE_REG, 0x8D);
    mfrc522_write_register(T_PRESCALER_REG, 0x3E);
    mfrc522_write_register(T_RELOAD_REG_L, 30);
    mfrc522_write_register(T_RELOAD_REG_H, 0);

    /*
     * 100% ASK
     */
    mfrc522_write_register(TX_ASK_REG, 0x40);

    /*
     * CRC preset 0x6363
     */
    mfrc522_write_register(MODE_REG, 0x3D);

    mfrc522_antenna_on();
}

// 카드 탐색
uint8_t mfrc522_request(uint8_t reqMode, uint8_t *tagType) {
    uint8_t status;
    uint8_t backLen = 2;
    uint16_t backBits = 0;

    mfrc522_write_register(BIT_FRAMING_REG, 0x07);

    tagType[0] = reqMode;

    status = mfrc522_to_card(
        PCD_TRANSCEIVE,
        tagType,
        1,
        tagType,
        &backLen,
        &backBits);

    if ((status != MI_OK) || (backBits != 0x10)) {
        status = MI_ERR;
    }

    return status;
}

// 카드 UID 충돌 방지 처리
uint8_t mfrc522_anticoll(uint8_t *serialNum) {
    uint8_t status;
    uint8_t i;
    uint8_t check = 0;
    uint8_t backLen = 5;
    uint16_t backBits = 0;
    uint8_t buffer[5];

    mfrc522_write_register(BIT_FRAMING_REG, 0x00);

    buffer[0] = PICC_ANTICOLL;
    buffer[1] = 0x20;

    status = mfrc522_to_card(
        PCD_TRANSCEIVE,
        buffer,
        2,
        buffer,
        &backLen,
        &backBits);

    if (status == MI_OK) {
        if (backLen != 5) {
            return MI_ERR;
        }

        for (i = 0; i < 4; i++) {
            serialNum[i] = buffer[i];
            check ^= buffer[i];
        }

        serialNum[4] = buffer[4];

        if (check != serialNum[4]) {
            return MI_ERR;
        }
    }

    return status;
}

// 카드 선택
uint8_t mfrc522_select_tag(const uint8_t *serialNum, uint8_t *sak) {
    uint8_t i;
    uint8_t status;
    uint8_t buffer[9];
    uint8_t crc[2];
    uint8_t backData[3];
    uint8_t backLen = sizeof(backData);
    uint16_t backBits = 0;

    buffer[0] = PICC_SELECTTAG;
    buffer[1] = 0x70;

    for (i = 0; i < 5; i++) {
        buffer[i + 2] = serialNum[i];
    }

    status = mfrc522_calculate_crc(buffer, 7, crc);

    if (status != MI_OK) {
        return status;
    }

    buffer[7] = crc[0];
    buffer[8] = crc[1];

    status = mfrc522_to_card(
        PCD_TRANSCEIVE,
        buffer,
        9,
        backData,
        &backLen,
        &backBits);

    if ((status == MI_OK) && (backBits == 0x18)) {
        *sak = backData[0];
        return MI_OK;
    }

    return MI_ERR;
}

// 카드 통신 종료
void mfrc522_halt(void) {
    uint8_t buffer[4];
    uint8_t crc[2];
    uint8_t backData[1];
    uint8_t backLen = sizeof(backData);
    uint16_t backBits = 0;

    buffer[0] = PICC_HALT;
    buffer[1] = 0x00;

    if (mfrc522_calculate_crc(buffer, 2, crc) != MI_OK) {
        return;
    }

    buffer[2] = crc[0];
    buffer[3] = crc[1];

    /*
     * HALT는 정상 동작 시 카드가 응답하지 않을 수 있음.
     * 반환값 미사용
     */
    (void)mfrc522_to_card(
        PCD_TRANSCEIVE,
        buffer,
        4,
        backData,
        &backLen,
        &backBits);
}
