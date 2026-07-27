# 설계 결정

## Kiosk와 Locker 책임 분리

Kiosk는 사용자 입력과 인증 요청을 담당하고 Locker는 실제 sensor와 actuator를 제어합니다.

```text
Kiosk: locker 번호·PIN 입력, 요청 송신, 결과 표시
Locker: RFID, 잠금, 문·물품 상태, alert
```

두 firmware는 외부 통신 계층을 경계로 독립 build와 시험이 가능합니다.

## Locker의 주기 작업 분리

`loop()`에서 작업별 주기를 독립적으로 관리합니다.

| 작업 | 주기 |
| --- | ---: |
| Sensor 상태 갱신 | `300ms` |
| Wi-Fi socket polling | `500ms` |
| Wi-Fi reconnect | `5s` |
| 상태 report | `10s` |

각 작업은 `millis()` 차이를 사용하므로 지속적인 main loop 안에서 서로 다른 주기로 실행됩니다.

## Flex Sensor Hysteresis

물품 감지와 해제에 서로 다른 threshold를 적용합니다.

```text
EMPTY → EXIST: diff >= 8
EXIST 유지: diff >= 5
```

경계값 부근의 sensor noise로 상태가 반복 전환되는 현상을 줄이기 위한 구성입니다.

## RFID 상태 기억

Locker가 열린 동안 인증에 사용한 RFID UID를 `activeRfidUid`에 저장합니다. 닫기 요청에서는 같은 UID인지 확인하고, 일치하지 않으면 `RFID_MISMATCH` alert를 생성합니다.

Kiosk 인증처럼 UID 없이 열린 경우에는 공개용 `RFID_TAG` fallback 경로가 함께 존재합니다. 이 값은 실제 장비 설정 시 교체해야 합니다.

## 상태와 Event 분리

Locker는 현재 상태와 상태 변화 원인을 별도 message로 전송합니다.

- `LOCKER_STATE`: 현재 door·lock·item 상태
- `LOCKER_LOG`: event와 처리 결과
- `ALERT`: 이상 원인과 당시 상태

이를 통해 외부 영역에서 최신 상태와 이력 data를 구분할 수 있습니다.

## 공개용 Credential 분리

실제 credential을 별도 구성 파일로 분리하는 구조는 현재 적용되어 있지 않습니다. 공개 source에서는 실값을 placeholder로 교체했습니다. 실행 전에 로컬 환경에 맞는 값으로 설정해야 합니다.

## Software 역할 분리

Main 연결 구성은 `iot_locker_client.c ↔ iot_server.c ↔ iot_kiosk_client.c`입니다.

- `iot_server.c`: 접속 인증, client routing, 공통 인증·DB 처리
- `iot_locker_client.c`: Locker message와 SQL/DB 처리
- `iot_kiosk_client.c`: STM32 Bluetooth와 TCP server 사이의 bridge

각 process를 분리해 장치 통신, routing 및 DB 처리를 독립적으로 실행·확인할 수 있습니다.
