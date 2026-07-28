# AVR C Layered Firmware Reference

Arduino Uno 보관함 제어 코드를 **Application → BSP → Driver → Hardware** 계층으로 분리해 정리한 AVR C 참고 구현입니다.

> [!IMPORTANT]
> 현재 `Locker-control`의 PlatformIO 기본 빌드 대상은 [`../src/main.cpp`](../src/main.cpp)입니다.  
> 이 디렉터리는 실제 통합 동작 코드의 구조를 계층형 Firmware Architecture로 재구성하고, 각 모듈의 책임과 의존 관계를 설명하기 위한 참고 구현입니다.

## 1. 목적

초기 통합 구현은 센서 입력, 장치 제어, 통신 및 상태 판단 로직이 하나의 실행 파일에 집중되어 있었습니다.

이 디렉터리에서는 다음 목적에 따라 코드를 분리했습니다.

- Application 정책과 하드웨어 제어 코드 분리
- 장치별 Driver 책임 명확화
- MCU Peripheral 의존성의 Hardware 계층 한정
- 상위 계층에서 하위 구현 세부사항 은닉
- 센서 또는 통신 모듈 변경 시 수정 범위 최소화
- 모듈 단위 테스트와 코드 재사용 가능성 확보

## 2. 실행 구조

```text
main.c
  └── AccessLockInit()
      └── BSP / Driver / Hardware 초기화

main loop
  └── AccessLockMain()
      ├── 명령 수신 및 처리
      ├── RFID 인증 상태 처리
      ├── Door / Item / Current 상태 확인
      ├── Servo 및 LED 제어
      └── 상태·Event·Alert 메시지 전송
```

`main.c`는 Application 초기화와 반복 실행만 담당합니다.

```c
int main(void) {
    AccessLockInit();

    while (1) {
        AccessLockMain();
    }

    return 0;
}
```

## 3. Firmware Architecture

```text
┌──────────────────────────────────────────┐
│ Application                              │
│ ap/AccessLock.c                          │
│                                          │
│ Locker 정책, 상태 판단, 명령 처리,       │
│ 상태·Event·Alert 메시지 구성             │
└─────────────────────┬────────────────────┘
                      │ 목적 중심 API
                      ▼
┌──────────────────────────────────────────┐
│ Board Support Package                    │
│ bsp/bsp.c, bsp/bspWifi.c                 │
│                                          │
│ Application이 사용할 보드 기능을         │
│ 하나의 인터페이스로 제공                 │
└─────────────────────┬────────────────────┘
                      │ 장치 제어 API
                      ▼
┌──────────────────────────────────────────┐
│ Device Driver                            │
│ driver/                                  │
│                                          │
│ RFID, Servo, Hall, Flex, Current,        │
│ ESP8266, LED 장치별 동작 구현            │
└─────────────────────┬────────────────────┘
                      │ Peripheral API
                      ▼
┌──────────────────────────────────────────┐
│ Hardware Abstraction                     │
│ hw/                                      │
│                                          │
│ ADC, I2C, UART 등 MCU Peripheral 제어    │
└──────────────────────────────────────────┘
```

의존 방향은 항상 상위 계층에서 하위 계층으로만 향하도록 구성합니다.

```text
Application → BSP → Driver → Hardware
```

Application은 개별 Driver 또는 Hardware 모듈을 직접 호출하지 않고 `bsp.h`를 통해 보관함 기능을 사용합니다.

## 4. 디렉터리 구조

```text
AVR_C/
├── main.c
│
├── ap/
│   ├── AccessLock.c
│   ├── AccessLock.h
│   └── ap.md
│
├── bsp/
│   ├── bsp.c
│   ├── bsp.h
│   ├── bspWifi.c
│   ├── bspWifi.h
│   └── bsp.md
│
├── driver/
│   ├── driver.c
│   ├── driver.h
│   ├── esp8266.c
│   ├── esp8266.h
│   ├── ina219.c
│   ├── ina219.h
│   ├── led.c
│   ├── led.h
│   ├── mfrc522.c
│   ├── mfrc522.h
│   ├── servo.c
│   ├── servo.h
│   ├── szh_sen.c
│   ├── szh_sen.h
│   ├── ts0224.c
│   ├── ts0224.h
│   └── driver.md
│
└── hw/
    ├── hw.c
    ├── hw.h
    ├── my_adc.c
    ├── my_adc.h
    ├── my_i2c.c
    ├── my_i2c.h
    ├── my_uart.c
    ├── my_uart.h
    └── hw.md
```

## 5. 계층별 책임

| 계층 | 주요 파일 | 책임 |
|---|---|---|
| Entry Point | `main.c` | 초기화 함수 호출 및 Application 반복 실행 |
| Application | `ap/AccessLock.c` | Locker 상태 머신, 명령 처리, 상태 분류 및 메시지 생성 |
| BSP | `bsp/bsp.c` | Application에서 사용할 목적 중심 보드 API 제공 |
| Wi-Fi BSP | `bsp/bspWifi.c` | Network 연결과 송수신 기능을 Application 관점으로 추상화 |
| Driver | `driver/*.c` | 센서·액추에이터·통신 모듈별 장치 제어 |
| Hardware | `hw/*.c` | ADC, I2C, UART 등 MCU Peripheral 접근 |

## 6. 주요 모듈

### Application

| 파일 | 역할 |
|---|---|
| `ap/AccessLock.c` | 보관함 제어 정책과 주 실행 흐름 |
| `ap/AccessLock.h` | `AccessLockInit()`, `AccessLockMain()` 등 Application 공개 API |

Application 계층은 다음 기능을 조정합니다.

- 보관함 잠금 및 해제 흐름
- RFID 인증 요청 처리
- 문 열림·닫힘 상태 판단
- 물품 존재 상태 판단
- Servo 전류 및 이상 상태 확인
- Server 명령 처리
- 상태·Event·Alert 메시지 생성

### BSP

| 파일 | 역할 |
|---|---|
| `bsp/bsp.c` | Sensor, Servo, LED 및 통신 기능의 통합 인터페이스 |
| `bsp/bspWifi.c` | Wi-Fi 연결, Server 연결 및 메시지 송수신 인터페이스 |

BSP는 Application이 장치별 구현을 직접 알지 않도록 다음과 같은 목적 중심 인터페이스를 제공합니다.

```text
문 상태 읽기
물품 상태 읽기
잠금 또는 해제
RFID UID 읽기
Network 상태 확인
Server 메시지 송수신
```

### Driver

| 모듈 | 대상 장치 | 역할 |
|---|---|---|
| `esp8266` | ESP8266 | Wi-Fi 및 TCP 통신 제어 |
| `mfrc522` | RC522 | RFID UID 읽기 및 인증 데이터 획득 |
| `servo` | Servo Motor | 잠금 장치 위치 제어 |
| `ts0224` | Hall Sensor | Door open/closed 상태 입력 |
| `szh_sen` | Flex/Pressure Sensor | 보관 물품 존재 상태 입력 |
| `ina219` | Current Sensor | Servo 구동 전류 및 부하 상태 측정 |
| `led` | Status LED | 보관함 상태와 오류 상태 표시 |
| `driver.c` | Driver Aggregator | 장치 Driver 초기화 및 공통 진입점 제공 |

### Hardware

| 모듈 | 역할 |
|---|---|
| `my_adc` | Analog Sensor 값 획득 |
| `my_i2c` | INA219 등 I2C 장치 통신 |
| `my_uart` | ESP8266 및 상위 시스템과의 UART 송수신 |
| `hw.c` | Hardware 모듈 초기화와 집계 API 제공 |

## 7. 주요 데이터 흐름

### Sensor 상태 처리

```text
TS0224 / Flex Sensor / INA219
        ↓
Driver
        ↓
BSP 상태 조회 API
        ↓
AccessLock Application
        ↓
Door / Item / Current 상태 분류
        ↓
상태·Event·Alert 메시지 생성
```

### RFID 인증

```text
RC522 UID 감지
    ↓
MFRC522 Driver
    ↓
BSP
    ↓
AccessLock
    ↓
인증 요청 메시지 전송
    ↓
Server 응답 처리
    ├── 인증 성공 → Servo Unlock
    └── 인증 실패 → 잠금 유지 및 오류 표시
```

### 원격 명령 처리

```text
Server TCP Command
    ↓
ESP8266 / UART
    ↓
Wi-Fi BSP
    ↓
AccessLock Command Parser
    ├── LOCK
    ├── UNLOCK
    ├── STATUS
    └── 기타 관리 명령
```

## 8. 메시지 구성

보관함 상태는 Raspberry Pi Server가 MariaDB에 저장할 수 있는 문자열 메시지로 전송합니다.

예시:

```text
[JBC_SQL]LOCKER_STATE@L01@NONE@CLOSED@LOCKED@EXIST@512@70@0.123@NORMAL
[JBC_SQL]LOCKER_ALERT@L01@DOOR_TIMEOUT@MANAGER@Locker door timeout detected
```

메시지는 목적에 따라 구분합니다.

| 구분 | 목적 |
|---|---|
| State | 현재 보관함 상태의 주기적 동기화 |
| Event | 인증, 개방, 폐쇄 등 단발성 동작 기록 |
| Alert | Door timeout, 비정상 전류 등 관리자 확인이 필요한 상태 |

전체 통신 형식은 [`../../../../docs/04-communication-protocol.md`](../../../../docs/04-communication-protocol.md)를 참고합니다.

## 9. 설계 결정

### Application에서 Driver를 직접 호출하지 않음

Application이 `servo.c`, `ts0224.c`, `my_uart.c` 같은 개별 구현을 직접 호출하면 하드웨어 변경 시 정책 코드까지 수정해야 합니다.

따라서 Application은 BSP 인터페이스만 사용하고, BSP가 필요한 Driver를 조합하도록 구성했습니다.

### 집계 초기화 함수 사용

`hw.c`, `driver.c`, `bsp.c`에서 각 계층의 초기화 진입점을 제공합니다.

```text
AccessLockInit()
    ↓
bspInit()
    ↓
driverInit()
    ↓
hwInit()
```

이를 통해 `main.c`가 개별 주변장치와 센서 초기화 순서를 직접 관리하지 않도록 했습니다.

### 상태와 Event 분리

현재 상태는 주기적으로 갱신할 수 있지만 인증, 문 개방, 경고 발생은 발생 시점이 중요합니다.

따라서 지속 상태와 단발성 Event·Alert를 별도 메시지로 구분했습니다.

## 10. Build 상태

이 디렉터리는 현재 독립적인 PlatformIO 빌드 대상으로 등록되어 있지 않습니다.

현재 검증된 Locker Firmware 빌드 대상은 다음입니다.

```text
firmware/Locker-control/src/main.cpp
```

빌드 명령:

```bash
cd firmware/Locker-control
pio run
```

`AVR_C`를 독립 빌드 대상으로 전환하려면 다음 작업이 추가로 필요합니다.

- `platformio.ini`의 별도 Environment 또는 `src_filter` 구성
- AVR Header 및 Toolchain Include 정리
- Arduino Library 의존 코드의 AVR C 인터페이스 정리
- Pin map과 Peripheral 초기화 구현 확인
- 각 계층 `.c` 파일을 Build Source에 등록
- 통합 Hardware Test 재수행

따라서 현재 이 디렉터리는 **Architecture Reference**로 사용합니다.

## 11. 코드 탐색 순서

처음 코드를 확인할 때는 다음 순서를 권장합니다.

1. [`main.c`](main.c)
2. [`ap/AccessLock.c`](ap/AccessLock.c)
3. [`bsp/bsp.c`](bsp/bsp.c)
4. [`bsp/bspWifi.c`](bsp/bspWifi.c)
5. [`driver/driver.c`](driver/driver.c)
6. 장치별 Driver
7. [`hw/hw.c`](hw/hw.c)
8. Peripheral별 Hardware 모듈

## 12. 관련 문서

- [프로젝트 README](../../../README.md)
- [Firmware Code Flow](../../../../docs/02-firmware-code-flow.md)
- [Sensor Classification](../../../../docs/03-sensor-classification.md)
- [Communication Protocol](../../../../docs/04-communication-protocol.md)
- [Design Decisions](../../../../docs/06-design-decisions.md)
- [Contribution Code Guide](../../../../docs/09-contribution-code-guide.md)
- [Contribution Scope](../../../../CONTRIBUTIONS.md)

## 13. 문서 정리 권장사항

현재 각 계층의 설명 파일은 다음과 같습니다.

```text
ap/ap.md
bsp/bsp.md
driver/driver.md
hw/hw.md
```

GitHub에서 각 디렉터리에 들어갔을 때 설명이 자동으로 표시되도록, 이후 다음과 같이 변경하는 것을 권장합니다.

```text
ap/README.md
bsp/README.md
driver/README.md
hw/README.md
```
