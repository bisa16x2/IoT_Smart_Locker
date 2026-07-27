# 개인 기여 코드 안내

Portfolio 검토자가 개인의 직접 구현 영역을 빠르게 확인할 수 있도록 경로와 핵심 함수를 연결합니다.

## 우선 탐색 경로

```text
firmware/Locker-control/src/main.cpp
```

현재 PlatformIO build에서 보관함의 sensor, actuator, RFID, Wi-Fi 및 protocol integration을 확인할 수 있는 중심 file입니다.

## Sensor와 상태 분류

| 기능 | 주요 함수 |
| --- | --- |
| Door 상태 | `readDoorClosed()` |
| Item 상태와 hysteresis | `readItemPresent()` |
| Flex baseline | `calibrateFlex()` |
| Sensor 주기 처리 | `readSensors()` |
| Servo current | `driveServoAndReadCurrent()` |

## 인증과 잠금 제어

| 기능 | 주요 함수 |
| --- | --- |
| Server command 처리 | `handleCommand()` |
| RFID scan | `checkRfid()`, `handleRfidScan()` |
| RFID UID 비교 | `processRfidAuth()`, `isSameActiveRfid()` |
| 잠금 해제 | `openLocker()` |
| 잠금 | `closeLocker()` |

## Protocol Integration

| 기능 | 주요 함수 |
| --- | --- |
| Socket 수신 parsing | `socketEvent()` |
| 상태 전송 | `sendState()` |
| Event log | `sendLockerLog()` |
| Alert | `sendAlert()`, `sendClientAlert()` |
| RFID 인증 요청 | `sendRfidAuthRequest()` |
| Wi-Fi 초기화·연결 | `wifi_Setup()`, `wifi_Init()`, `server_Connect()` |

## 계층형 구조

동일한 보관함 제어 영역을 계층별로 탐색하려면 다음 경로를 참고합니다.

```text
firmware/Locker-control/AVR_C/
├── ap/AccessLock.c
├── bsp/bsp.c
├── bsp/bspWifi.c
├── driver/
└── hw/
```

- `Application`: 상태와 command
- `BSP`: board 목적 interface
- `Driver`: sensor·actuator·ESP8266
- `Hardware`: ADC·I2C·UART·timer

## 팀 프로젝트 Context

다음 영역은 전체 동작을 이해하기 위해 포함했지만 개인 단독 구현으로 표시하지 않습니다.

```text
firmware/Kiosk-control/
software/
```

Kiosk는 사용자 입력과 인증 요청을 담당합니다. Software의 실행 흐름은 `iot_locker_client.c`, `iot_server.c`, `iot_kiosk_client.c`를 기준으로 설명하며, 개인 직접 구현과 팀원 구현은 `CONTRIBUTIONS.md`에서 구분합니다.
