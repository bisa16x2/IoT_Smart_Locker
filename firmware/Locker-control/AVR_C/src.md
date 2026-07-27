# src Layer Overview

`src/`는 Arduino Uno firmware의 실행 코드를 담는 최상위 source 영역이다. 전체 흐름은 `main.c`에서 시작해 `ap`, `bsp`, `driver`, `hw` 순서로 내려간다. 상위 layer는 하위 layer의 세부 구현을 직접 알지 않도록 구성한다.

## 실행 흐름

```text
main.c
  -> ap/AccessLock.c
  -> bsp/bsp.c
  -> driver/driver.c
  -> hw/hw.c
```

`main.c`는 `AccessLockInit()`을 한 번 호출한 뒤, 무한 반복문에서 `AccessLockMain()`을 계속 호출한다. 실제 smart locker 정책과 상태 전송은 `ap` layer에서 처리한다.

## Layer 책임

`ap` layer는 locker 상태 머신, UART command 처리, DB 저장용 상태 메시지 생성을 담당한다.

`bsp` layer는 `ap`가 사용할 목적 중심 API를 제공한다. `ap`는 sensor, servo, UART의 실제 구현을 직접 호출하지 않는다.

`driver` layer는 servo, LED, hall sensor, flex pressure sensor, current sensor 같은 장치 단위 driver를 locker 목적에 맞게 묶는다.

`hw` layer는 UART, timer, delay 같은 MCU peripheral 기능을 제공한다.

## 상위 Layer로 이어지는 방식

`hw`와 `driver`는 각각 `hw.c`, `driver.c`에서 집계 API를 만든다. `bsp.c`는 이 두 집계 API만 사용한다. `ap`는 `bsp.h`만 포함하고, `driver`나 `hw`의 개별 모듈을 직접 include하지 않는다.

## DB 메시지 흐름

Arduino는 상태를 문자열 메시지로 만들어 상위 시스템으로 보낸다. Raspberry Pi 쪽 SQL client가 이 메시지를 MariaDB query로 변환해 `lockers`, `locker_event_log`, `alert_log`에 저장한다.

```text
[JBC_SQL]LOCKER_STATE@L01@NONE@CLOSED@LOCKED@EXIST@512@70@0.123@NORMAL
[JBC_SQL]LOCKER_ALERT@L01@DOOR_TIMEOUT@MANAGER@Locker door timeout detected
```
