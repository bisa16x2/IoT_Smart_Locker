#include <Arduino.h>
  #include <SPI.h>
  #include <MFRC522.h>

  #define RFID_SDA_PIN 10
  #define RFID_RST_PIN 9

  MFRC522 rfid(RFID_SDA_PIN, RFID_RST_PIN);

  void setup() {
      Serial.begin(115200);
      while (!Serial) {}

      SPI.begin();
      rfid.PCD_Init();

      Serial.println(F("RFID ready"));
      rfid.PCD_DumpVersionToSerial();
  }

  void loop() {
      if (!rfid.PICC_IsNewCardPresent()) return;
      if (!rfid.PICC_ReadCardSerial()) return;

      Serial.print(F("UID="));
      for (byte i = 0; i < rfid.uid.size; i++) {
          if (rfid.uid.uidByte[i] < 0x10) Serial.print('0');
          Serial.print(rfid.uid.uidByte[i], HEX);
          if (i + 1 < rfid.uid.size) Serial.print(':');
      }
      Serial.println();

      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
  }
