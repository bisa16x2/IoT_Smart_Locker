# Software 구조와 Routing

Raspberry Pi software는 TCP server를 중심으로 Arduino Locker와 STM32 Kiosk의 message를 연결합니다.

## Process 구성

| Source | 역할 |
| --- | --- |
| `software/TCP Socket/iot_server.c` | TCP client 인증, message routing, PIN·RFID 인증, 상태·event·alert DB 처리 |
| `software/TCP Socket/iot_locker_client.c` | Locker 관련 message parsing, MariaDB 조회·저장, 인증 결과 전달 |
| `software/TCP Socket/iot_kiosk_client.c` | STM32 Kiosk Bluetooth RFCOMM과 TCP socket 사이의 양방향 bridge |

## 연결 구조

```text
STM32F411 Kiosk
      ↕ Bluetooth RFCOMM
iot_kiosk_client.c
      ↕ TCP/IP
  iot_server.c
      ↕ TCP/IP
iot_locker_client.c
      ↕ MariaDB
 Arduino Locker / DB
```

Arduino Locker는 ESP8266을 통해 TCP server에 접속하고 `LOCKER` ID로 routing됩니다.

## Client 인증

Server는 실행 directory의 `idpasswd.txt`를 읽어 허용된 client ID와 password를 구성합니다.

Client의 첫 message:

```text
[<CLIENT ID>:<PASSWORD>]
```

공개 저장소에는 `idpasswd.txt.example`만 포함하며 실제 `idpasswd.txt`는 Git에서 제외합니다.

## Kiosk 인증 흐름

```text
STM32 Kiosk
  → AUTH:<locker_no>:<pin>
  → iot_kiosk_client
  → iot_server
  → users PIN 확인
  → [LOCKER]KIOSK_AUTH@<locker_no>
  → Arduino Locker 개방
  → AUTH_SUCCESS
  → STM32 Kiosk 결과 표시
```

등록 요청은 `REGISTER:<locker_no>:<pin>` 형식이며 server는 `users.pin_hash`를 갱신합니다.

## RFID 인증 흐름

```text
Arduino Locker
  → [LOCKER_SQL]RFID_AUTH@<locker_no>@<rfid_uid>
  → Server / Locker client
  → users.rfid_tag 확인
  → [LOCKER]RFID_AUTH 또는 RFID_DENY
  → Arduino Locker 잠금 제어
```

RFID 비교 시 공백, `:`, `-`를 제거하고 대문자로 정규화합니다.

## DB 반영 영역

Code에서 확인되는 주요 table:

| Table | 용도 |
| --- | --- |
| `users` | Locker 번호, PIN hash, RFID tag |
| `lockers` | Door·lock·item 최신 상태 |
| `kiosk_auth_log` | Kiosk·RFID 인증 결과 |
| `locker_event_log` | 잠금·문·물품 event |
| `alert_log` | 강제 개방과 상태 이상 |

DB 구축·query·운영은 팀원 구현 영역이며, 개인 기여는 device data와 protocol field 협의 및 integration debugging으로 구분합니다.
