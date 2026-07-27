#ifndef __INCLUDE_DRIVER__MFRC522_H__
#define __INCLUDE_DRIVER__MFRC522_H__

#include "common.h"
#include "access_config.h"

#define MI_OK      0
#define MI_ERR     1
#define MI_TIMEOUT 2

#define PICC_REQIDL    0x26
#define PICC_REQALL    0x52
#define PICC_ANTICOLL  0x93
#define PICC_SELECTTAG 0x93
#define PICC_HALT      0x50

void mfrc522_init(const access_config_t *config);
void mfrc522_reset(void);

uint8_t mfrc522_request(uint8_t reqMode, uint8_t *tagType);
uint8_t mfrc522_anticoll(uint8_t *serialNum);
uint8_t mfrc522_select_tag(const uint8_t *serialNum, uint8_t *sak);
void mfrc522_halt(void);

#endif //__INCLUDE_DRIVER__MFRC522_H__
