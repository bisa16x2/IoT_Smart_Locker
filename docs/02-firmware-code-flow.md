# Firmware 코드 흐름

## Locker Controller

기본 PlatformIO build 진입점은 `firmware/Locker-control/src/main.cpp`입니다.

### 초기화

```text
setup()
├── Hall sensor input 설정
├── Servo 잠금 위치 설정
├── RC522 SPI 초기화
├── INA219 I2C 초기화
├── Flex sensor baseline 보정
├── Door·item 초기 상태 확인
├── ESP8266 Wi-Fi 초기화
└── BOOT 상태 전송
```

### 반복 처리

```text
loop()
├── 500ms: Wi-Fi 수신 polling 및 reconnect
├── 300ms: Hall·flex·current 상태 갱신
├── RFID card 확인
├── Door open timeout 검사
├── 10초: 주기 상태 전송
└── 요청된 sensor report 전송
```

`socketEvent()`는 server message를 `[`와 `@` 기준으로 분리하고 `handleCommand()`에 전달합니다.

주요 command:

- `KIOSK_AUTH`: Kiosk 인증 완료 후 대상 locker 개방
- `RFID_AUTH`: DB 인증 완료 후 RFID를 기억하고 개방
- `RFID_DENY`: 인증 거부 alert
- `UNLOCK`, `LOCK`, `DOOR`: 잠금 제어
- `GETSTATE`: 현재 상태 전송
- `GETSENSOR`: sensor report 설정
- `CALFLEX`: flex baseline 재보정

### 계층형 AVR 구현

`firmware/Locker-control/AVR_C`에는 보관함 제어 기능을 다음 계층으로 분리한 구현이 있습니다.

```text
main.c
└── AccessLock.c
    └── bsp.c / bspWifi.c
        └── driver.c
            ├── servo.c
            ├── led.c
            ├── ts0224.c
            ├── szh_sen.c
            ├── ina219.c
            ├── mfrc522.c
            └── esp8266.c
                └── hw.c / my_adc.c / my_i2c.c / my_uart.c
```

현재 `platformio.ini`의 기본 build는 `src/main.cpp`를 대상으로 하므로 두 구현을 동일 build 대상으로 혼동하지 않습니다.

## Kiosk Controller

Kiosk는 STM32CubeMX 생성 code와 `app/` source를 CMake로 함께 build합니다.

```text
main.c
└── MX_FREERTOS_Init()
    └── StartDefaultTask()
        ├── apInit()
        │   └── hwInit()
        │       ├── lcdInit()
        │       └── kioskInit()
        └── apMain()
            └── hwMain()
                └── kioskMain()
```

`kioskMain()`은 keypad 입력과 server 응답을 FSM으로 처리합니다.

```text
IDLE
├── LOGIN_INPUT → AUTH_WAIT → AUTH_SUCCESS / AUTH_FAIL
└── REGISTER_INPUT → REGISTER_WAIT → REGISTER_SUCCESS / REGISTER_FAIL
```

- Keypad로 locker 번호와 4자리 PIN 입력
- UART1로 Bluetooth module에 인증·등록 요청 송신
- `AUTH_*`, `REGISTER_*` 응답 수신
- LCD에 진행 및 결과 표시
- 3초 timeout 또는 결과 표시 후 초기 화면 복귀
