#include "lcd.h"

#define LCD_I2C_ADDR   (0x27 << 1)   // 안 나오면 (0x3F << 1)로 변경
#define LCD_BACKLIGHT  0x08
#define LCD_ENABLE     0x04
#define LCD_RS         0x01

#define LCD_CMD        0x00
#define LCD_DATA       LCD_RS

static uint8_t lcd_backlight = LCD_BACKLIGHT;

static void lcdWriteI2C(uint8_t data)
{
  HAL_I2C_Master_Transmit(&hi2c1, LCD_I2C_ADDR, &data, 1, 10);
}

static void lcdPulseEnable(uint8_t data)
{
  lcdWriteI2C(data | LCD_ENABLE | lcd_backlight);
  HAL_Delay(1);

  lcdWriteI2C((data & ~LCD_ENABLE) | lcd_backlight);
  HAL_Delay(1);
}

static void lcdWrite4Bits(uint8_t data)
{
  lcdWriteI2C(data | lcd_backlight);
  lcdPulseEnable(data);
}

static void lcdSend(uint8_t value, uint8_t mode)
{
  uint8_t high_nibble;
  uint8_t low_nibble;

  high_nibble = value & 0xF0;
  low_nibble  = (value << 4) & 0xF0;

  lcdWrite4Bits(high_nibble | mode);
  lcdWrite4Bits(low_nibble | mode);
}

static void lcdCommand(uint8_t cmd)
{
  lcdSend(cmd, LCD_CMD);
}

static void lcdData(uint8_t data)
{
  lcdSend(data, LCD_DATA);
}

void lcdInit(void)
{
  HAL_Delay(50);

  lcdWrite4Bits(0x30);
  HAL_Delay(5);

  lcdWrite4Bits(0x30);
  HAL_Delay(5);

  lcdWrite4Bits(0x30);
  HAL_Delay(5);

  lcdWrite4Bits(0x20);
  HAL_Delay(5);

  lcdCommand(0x28);  // 4-bit, 2 line, 5x8 font
  lcdCommand(0x0C);  // Display ON, Cursor OFF
  lcdCommand(0x06);  // Entry mode
  lcdClear();
}

void lcdClear(void)
{
  lcdCommand(0x01);
  HAL_Delay(2);
}

void lcdSetCursor(uint8_t row, uint8_t col)
{
  uint8_t address;

  if (row == 0)
  {
    address = 0x00 + col;
  }
  else
  {
    address = 0x40 + col;
  }

  lcdCommand(0x80 | address);
}

void lcdPrint(const char *str)
{
  while (*str)
  {
    lcdData((uint8_t)*str);
    str++;
  }
}

void lcdPrintf(const char *fmt, ...)
{
  char buf[32];

  va_list args;

  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  lcdPrint(buf);
}

void lcdClearLine(uint8_t row)
{
  lcdSetCursor(row, 0);
  lcdPrint("                ");
  lcdSetCursor(row, 0);
}