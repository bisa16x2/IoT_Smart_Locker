# 시험 및 Debugging

## Build 확인

사용자가 현재 정리된 두 firmware의 정상 build를 확인했습니다.

| 대상 | Build 방식 | 결과 |
| --- | --- | --- |
| Locker controller | PlatformIO, Arduino Uno | 성공 |
| Kiosk controller | CMake·Ninja, GNU Arm Embedded Toolchain | 성공 |
| Raspberry Pi software | GCC, BlueZ, MariaDB Connector | 성공 |

### Locker

```bash
cd firmware/Locker-control
pio run
```

### Kiosk

```bash
cd firmware/Kiosk-control
cmake --preset Debug
cmake --build --preset Debug
```

## Firmware별 확인 지점

### Locker Controller

- Boot 시 servo 잠금 위치 이동
- RC522 RFID 초기화 및 UID 문자열 변환
- Hall sensor의 door 상태 변화
- Flex sensor baseline과 물품 상태 변화
- INA219의 servo 구동 current peak
- ESP8266 AP 및 TCP 연결
- `KIOSK_AUTH`, `RFID_AUTH`, `RFID_DENY` 처리
- `LOCKER_STATE`, `LOCKER_LOG`, `ALERT` 생성

### Kiosk Controller

- Keypad의 locker 번호와 PIN 입력
- Login·register FSM 전환
- UART의 `AUTH`, `REGISTER` message 송신
- `AUTH_*`, `REGISTER_*` 응답 처리
- LCD의 진행·성공·실패·timeout 화면

## 전체 동작 확인

- STM32 Kiosk keypad의 locker 번호·PIN 입력
- Bluetooth RFCOMM을 통한 Kiosk request 전달
- TCP client 인증과 server routing
- PIN 등록 및 인증 성공·실패
- Arduino Locker의 RFID 인증과 servo 잠금·해제
- Hall·flex·INA219 상태 확인
- 상태·event·alert의 MariaDB 반영
- 강제 개방 및 잠금 중 물품 변경 alert

## 공개 Source 실행 제한

- 두 firmware의 build 성공은 사용자 확인 사항입니다.
- Software build와 전체 장비 동작도 사용자 확인 사항입니다.
- 현재 문서 정리 과정에서는 장비 시험을 다시 실행하지 않았습니다.
- 실제 credential이 placeholder로 바뀌어 있으므로 공개 source 그대로는 network 인증이 완료되지 않습니다.
