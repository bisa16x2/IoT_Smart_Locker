# Firmware 시스템 구조

현재 문서는 정상 build가 확인된 두 firmware의 책임과 연결 경계를 설명합니다.

```text
사용자
  │ Keypad 입력 / LCD 표시
  ▼
STM32F411 Kiosk Controller
  │ UART1
  ▼
Bluetooth–TCP Bridge
  │
  │  AUTH:<locker_no>:<pin>
  │  REGISTER:<locker_no>:<pin>
  ▼
외부 인증·중계 영역
  │
  │  KIOSK_AUTH / RFID_AUTH / RFID_DENY
  ▼
Arduino Uno Locker Controller
  ├── RC522 RFID
  ├── SG90 Servo
  ├── TS0224 Hall sensor
  ├── Flex sensor
  ├── INA219 current sensor
  └── ESP8266 Wi-Fi
```

Bluetooth–TCP bridge, server 및 DB는 firmware 외부 경계입니다. Main source는 다음 구성으로 확정했습니다.

```text
iot_locker_client.c
        ↕
   iot_server.c
        ↕
iot_kiosk_client.c
```

세 software와 두 firmware를 연결한 전체 장비 동작을 확인했습니다. 세부 실행 순서는 [Software Build와 실행](11-software-build-and-run.md), routing은 [Software 구조와 Routing](10-software-architecture.md)을 참고하세요.

## Kiosk Controller

- Platform: STM32F411
- 실행 구조: FreeRTOS `defaultTask`
- 입력: 4×4 keypad
- 출력: I2C LCD
- 통신: UART 기반 Bluetooth
- 책임: locker 번호·PIN 입력, 인증·등록 요청, server 결과 표시

## Locker Controller

- Platform: Arduino Uno
- 입력: RFID, Hall, flex, INA219
- 출력: servo, 상태 message 및 alert
- 통신: ESP8266 Wi-Fi
- 책임: 잠금 제어, 문·물품 상태 판정, 인증 결과 실행, 상태·event 전송

## Firmware 책임 경계

| 기능 | Kiosk | Locker |
| --- | --- | --- |
| Locker 번호·PIN 입력 | O | - |
| 인증·등록 요청 생성 | O | - |
| RFID card 읽기 | - | O |
| 잠금·해제 | - | O |
| 문 상태 판정 | - | O |
| 물품 상태 판정 | - | O |
| 구동 current 확인 | - | O |
| 상태·alert message 생성 | - | O |
| DB 인증·저장 | 외부 영역 | 외부 영역 |
