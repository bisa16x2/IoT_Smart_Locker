# ap Layer

`ap` layer는 smart locker의 application logic을 담당한다. 현재 구현 파일은 `AccessLock.c`이다.

## 파일 역할

`AccessLock.c`는 locker의 상태 머신, command parser, sensor 상태 해석, DB 저장용 메시지 생성을 처리한다. 이 파일은 동작에 필요한 pin, threshold, Wi-Fi/server 설정을 `static const access_config_t`로 가지고 있으며, 초기화 시 `bspInit(&access_config)`로 설정 포인터를 아래 layer에 전달한다.

`ap` layer는 `driver`나 `hw`의 개별 함수를 직접 호출하지 않는다. 실제 locker 제어와 통신은 `bsp` API를 통해 수행한다.

## 주요 상태

```c
ACCESS_LOCK_STATE_LOCKED
ACCESS_LOCK_STATE_UNLOCKED
ACCESS_LOCK_STATE_ALERT
```

`LOCKED`는 잠긴 상태, `UNLOCKED`는 사용자가 문을 열 수 있는 상태, `ALERT`는 문 열림 timeout 같은 비정상 상태를 의미한다.

## 입력 Command

`AccessLock.c`는 UART로 들어온 한 줄 command를 처리한다. 실제 byte 수신은 `bspComAvailable()`과 `bspComRead()`를 통해 수행한다.

```text
PING
STATUS?
UNLOCK:1
LOCK:1
CAL:DOOR
CAL:ITEM
```

`UNLOCK:1`은 locker 1번을 해제하고, `LOCK:1`은 다시 잠근다. `CAL:DOOR`와 `CAL:ITEM`은 sensor baseline 보정용이다.

## 출력 메시지

주기적으로 locker 상태를 DB 저장용 메시지로 전송한다.

```text
[JBC_SQL]LOCKER_STATE@L01@NONE@CLOSED@LOCKED@EXIST@512@70@0.123@NORMAL
```

필드 순서는 다음과 같다.

```text
locker_no, user_id, door_state, lock_state, item_state, pressure_value, pressure_diff, current_amp, alarm_state
```

비정상 상태가 발생하면 alert 메시지를 별도로 보낸다.

```text
[JBC_SQL]LOCKER_ALERT@L01@DOOR_TIMEOUT@MANAGER@Locker door timeout detected
```

## 하위 Layer 연결

`ap` layer는 설정값을 소유하지만, 하위 layer의 개별 구현에는 직접 접근하지 않는다. 초기화 흐름은 아래와 같다.

```text
AccessLock.c
  -> bspInit(&access_config)
  -> hwInit(config)
  -> driverInit(config)
```

초기화 이후 application logic은 아래 API만 통해 `bsp` layer로 요청한다.

```c
bspLockerLock();
bspLockerUnlock();
bspLockerUpdate();
bspLockerIsDoorClosed();
bspLockerIsItemDetected();
bspLockerGetPressureValue();
bspLockerGetPressureDiff();
bspLockerGetCurrentAmp();
bspComPrint();
```

이 구조에서는 아래 layer가 `AccessLock.h`를 include하지 않는다. 설정값은 위에서 아래로 전달되므로 하위 layer가 상위 layer를 바라보는 의존성이 생기지 않는다.
