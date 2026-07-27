# IoT Smart Locker System

Arduino Uno 보관함 제어기와 STM32F411 Kiosk를 연동해 사용자 인증, 잠금 제어, 물품 감지 및 이상 상태 보고를 수행한 팀 프로젝트입니다.

두 firmware와 Raspberry Pi software의 build 및 전체 장비 동작을 확인했습니다. 공개 source에서는 credential을 placeholder로 교체했으며, 개인·팀 기여 범위를 구분해 설명합니다.

## 개인 기여 핵심

- Arduino 보관함 제어부 구현
- Hall·flex·current sensor 입력을 이용한 상태 분류
- RFID 인증 요청과 보관함 상태·event·alert message 구성
- 장치와 server 사이의 message protocol 정의
- DB 저장 항목 협의
- 장치·network·server 통합 debugging

STM32 Kiosk와 server·MariaDB 구축 및 운영은 팀 프로젝트 범위에 포함되지만 개인 직접 구현으로 표시하지 않습니다. 세부 구분은 [CONTRIBUTIONS.md](CONTRIBUTIONS.md)를 참고하세요.

## Firmware 구성

| 구분 | Platform | 역할 | Build 상태 |
| --- | --- | --- | --- |
| Locker controller | Arduino Uno | RFID, servo, Hall, flex, INA219, ESP8266 연동 | 정상 build 확인 |
| Kiosk controller | STM32F411 | Keypad 입력, LCD 화면, PIN 인증·등록 요청, UART 통신 | 정상 build 확인 |

Firmware build와 전체 연결 동작은 사용자가 각 개발 환경과 실제 장비에서 확인했습니다.

## 저장소 구조

```text
.
├── firmware/
│   ├── Locker-control/
│   │   ├── src/main.cpp
│   │   ├── AVR_C/
│   │   └── platformio.ini
│   └── Kiosk-control/
│       ├── Core/
│       ├── Drivers/
│       ├── Middlewares/
│       ├── app/
│       ├── CMakeLists.txt
│       └── STM32_Kiosk.ioc
├── software/
│   └── TCP Socket/
│       ├── iot_locker_client.c
│       ├── iot_server.c
│       └── iot_kiosk_client.c
├── docs/
├── CONTRIBUTIONS.md
├── ATTRIBUTION.md
└── CHANGELOG.md
```

`Locker-control/src/main.cpp`는 현재 PlatformIO build 대상입니다. `Locker-control/AVR_C`는 같은 보관함 제어 영역을 `Application → BSP → Driver → Hardware` 구조로 분리한 계층형 구현 자료입니다.

## Build

### Locker controller

요구 사항:

- PlatformIO
- Arduino Uno

```bash
cd firmware/Locker-control
pio run
```

### Kiosk controller

요구 사항:

- CMake 3.22 이상
- Ninja
- GNU Arm Embedded Toolchain

```bash
cd firmware/Kiosk-control
cmake --preset Debug
cmake --build --preset Debug
```

## Credential

공개 source에는 실제 Wi-Fi, server, 인증값과 RFID UID 대신 다음 placeholder를 사용합니다.

```text
(Wi-Fi ID)
(Wi-Fi PASSWORD)
(SERVER IP)
(DEVICE ID)
(PASSWORD)
(RFID UID)
```

실제 장비에서 실행할 때는 각 값을 로컬 환경에 맞게 설정해야 합니다.

## 문서

- [Firmware 시스템 구조](docs/01-system-architecture.md)
- [Firmware 코드 흐름](docs/02-firmware-code-flow.md)
- [Sensor 상태 분류](docs/03-sensor-classification.md)
- [Firmware 통신 protocol](docs/04-communication-protocol.md)
- [Module 책임](docs/05-module-responsibilities.md)
- [설계 결정](docs/06-design-decisions.md)
- [시험 및 debugging](docs/07-test-and-debug.md)
- [문제 해결](docs/08-troubleshooting.md)
- [개인 기여 코드 안내](docs/09-contribution-code-guide.md)
- [Software 구조와 Routing](docs/10-software-architecture.md)
- [Software Build와 실행](docs/11-software-build-and-run.md)

## 전체 검증 상태

- Locker controller 정상 build
- Kiosk controller 정상 build
- Raspberry Pi server·client 정상 build
- STM32 Kiosk → Bluetooth bridge → TCP server 연결
- Arduino Locker → Wi-Fi → TCP server·DB 연동
- PIN 및 RFID 인증, 잠금·해제, 상태·alert 전체 동작 확인

## 출처와 기여 범위

원본과 재구성 원칙은 [ATTRIBUTION.md](ATTRIBUTION.md), 개인·공동·팀원 기여 범위는 [CONTRIBUTIONS.md](CONTRIBUTIONS.md)에서 관리합니다.

## License

현재 별도 license를 부여하지 않았습니다. 팀 code는 개인 구현 범위를 명시하는 조건으로 portfolio 공개 허용을 확인했지만, 이는 제3자의 복제·수정·재배포를 허용하는 open-source license를 의미하지 않습니다.
