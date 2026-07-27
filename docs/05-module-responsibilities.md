# Module 책임

## Locker Controller

### PlatformIO 구현

| 경로 | 책임 |
| --- | --- |
| `firmware/Locker-control/src/main.cpp` | 초기화, main loop, 인증 command, sensor 판정, 잠금 제어, protocol message |
| `firmware/Locker-control/platformio.ini` | Arduino Uno build 환경과 library 의존성 |

사용 library:

- `WiFiEsp`
- `Servo`
- `MFRC522`
- `Adafruit INA219`

### 계층형 AVR 구현

| 계층 | 주요 경로 | 책임 |
| --- | --- | --- |
| Application | `AVR_C/ap/AccessLock.c` | 상태 관리, command 처리, 상태 전송 |
| BSP | `AVR_C/bsp/` | Locker 목적 API와 Wi-Fi 연결 절차 |
| Driver | `AVR_C/driver/` | Servo, LED, Hall, flex, current, RFID, ESP8266 |
| Hardware | `AVR_C/hw/` | Timer, ADC, I2C, UART |

## Kiosk Controller

| 경로 | 책임 |
| --- | --- |
| `Core/Src/main.c` | STM32 HAL과 peripheral 초기화 |
| `Core/Src/freertos.c` | `defaultTask` 생성과 application 진입 |
| `app/ap/ap.c` | Application 초기화·주기 실행 |
| `app/hw/hw.c` | LCD와 Kiosk module 집계 |
| `app/driver/kiosk.c` | Keypad 입력, FSM, UART message, LCD 화면 |
| `app/driver/keypad.c` | 4×4 keypad scan |
| `app/driver/lcd.c` | I2C LCD 제어 |
| `app/driver/flex_sensor.c` | Flex sensor driver code |

현재 `hwInit()`은 `lcdInit()`과 `kioskInit()`을 호출합니다. `flex_sensor.c`는 source에 포함되어 있지만 현재 Kiosk 실행 흐름에서는 초기화되지 않습니다.

## 기여 구분

| 영역 | 구분 |
| --- | --- |
| Locker controller firmware | 개인 직접 구현 |
| Sensor 상태 분류와 protocol message | 개인 직접 구현 |
| Firmware·network·server 통합 debugging | 개인 통합 기여 |
| Kiosk controller firmware | 팀 프로젝트 영역 |
| Server 및 MariaDB | 팀원 구현 영역 |
