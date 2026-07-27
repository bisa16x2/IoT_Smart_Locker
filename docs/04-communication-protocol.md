# 통신 Protocol

Firmware와 Raspberry Pi software가 교환하는 주요 message를 정리합니다.

## Kiosk 요청

Kiosk는 UART를 통해 줄바꿈으로 끝나는 message를 전송합니다.

| 방향 | Payload | 설명 |
| --- | --- | --- |
| Kiosk → 외부 | `AUTH:<locker_no>:<pin>\n` | PIN 인증 요청 |
| Kiosk → 외부 | `REGISTER:<locker_no>:<pin>\n` | Locker·PIN 등록 요청 |

Kiosk가 처리하는 응답:

```text
AUTH_SUCCESS
AUTH_FAIL
AUTH_ERROR
REGISTER_SUCCESS
REGISTER_FAIL
REGISTER_ERROR
```

## Locker 수신 Command

Locker는 `[`와 `@`를 delimiter로 사용해 message를 분리합니다.

| Command | 주요 인자 | 동작 |
| --- | --- | --- |
| `KIOSK_AUTH` | `locker_no`, 선택적 UID | Kiosk 인증 결과 확인 후 개방 |
| `RFID_AUTH` | `locker_no`, UID | RFID 인증 성공 처리 |
| `RFID_DENY` | - | RFID 인증 거부 alert |
| `UNLOCK` | - | 잠금 해제 |
| `LOCK` | - | 잠금 |
| `DOOR` | `OPEN` 또는 `CLOSE` | 문 제어 |
| `GETSTATE` | - | 현재 상태 전송 |
| `GETSENSOR` | report 주기 | Sensor 상태 전송 설정 |
| `CALFLEX` | - | Flex baseline 재보정 |

## Locker 송신 Message

### 상태

```text
[LOCKER_SQL]LOCKER_STATE@<locker_no>@<door>@<lock>@<item>
```

### Event log

```text
[LOCKER_SQL]LOCKER_LOG@<locker_no>@<event>@<rfid_uid>@<door>@<lock>@<item>@<result>
```

### Alert

```text
[LOCKER_SQL]ALERT@<locker_no>@<reason>@<door>@<lock>@<item>@<message>
```

### RFID 인증 요청

```text
[LOCKER_SQL]RFID_AUTH@<locker_no>@<rfid_uid>
```

### 특정 client alert

```text
[USER]ALERT@<locker_no>@<reason>@<door>@<lock>@<item>
[MANAGER]ALERT@<locker_no>@<reason>@<door>@<lock>@<item>
```

## 상태 값

| 구분 | 값 |
| --- | --- |
| Door | `OPEN`, `CLOSED` |
| Lock | `LOCKED`, `UNLOCKED` |
| Item | `EXIST`, `EMPTY` |

실제 Wi-Fi, server, password, PIN 및 UID는 protocol 문서와 공개 source에 포함하지 않습니다.

## Software Routing

Server는 client가 보낸 `[목적지]payload` 형식에서 목적지 ID를 찾아 해당 TCP socket으로 전달합니다.

```text
[LOCKER]KIOSK_AUTH@<locker_no>
[LOCKER]RFID_AUTH@<locker_no>@<rfid_uid>
[USER]ALERT@...
[MANAGER]ALERT@...
```

`iot_kiosk_client.c`는 인증·등록 결과와 `ALERT@` message만 Bluetooth로 STM32 Kiosk에 전달합니다. `iot_locker_client.c`는 상태·event·alert를 MariaDB에 반영하고 인증 결과를 Locker 대상으로 전송합니다.
