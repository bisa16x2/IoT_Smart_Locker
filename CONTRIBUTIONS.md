# 기여 범위

이 문서는 팀 프로젝트의 전체 기능과 개인의 직접 구현·통합 영역을 구분합니다.

## 직접 설계·구현

- Arduino Uno 기반 보관함 제어부
- RFID 입력과 servo 잠금·해제 동작
- Hall sensor 기반 문 상태 판정
- Flex sensor 기반 물품 유무 판정과 threshold 적용
- INA219 기반 servo 구동 current 확인
- Wi-Fi 연결과 device message 송수신
- 보관함 상태, event, RFID 인증 요청 및 alert message 구성
- 장치와 server 사이의 message protocol 정의

주요 탐색 경로:

```text
firmware/Locker-control/src/main.cpp
firmware/Locker-control/AVR_C/ap/
firmware/Locker-control/AVR_C/bsp/
firmware/Locker-control/AVR_C/driver/
firmware/Locker-control/AVR_C/hw/
```

## 통합·Debugging

- DB에 저장할 상태 및 event 항목 협의
- Arduino·network·server·DB 사이의 message 연동
- Kiosk 인증 결과와 보관함 개방 동작 통합 확인
- Sensor 판정 결과와 server 수신 결과 대조
- RFID 인증 및 잠금·해제 시나리오 debugging

## 팀 프로젝트 영역

- STM32F411 Kiosk firmware
- Raspberry Pi TCP server
- Bluetooth–TCP bridge
- MariaDB 구축, query 및 운영

현재 저장소에는 전체 동작 이해와 재현을 위해 Kiosk firmware와 Raspberry Pi software가 포함되어 있습니다. 해당 source를 개인 단독 구현으로 표현하지 않습니다.

Software 역할:

| Source | 역할 | 기여 구분 |
| --- | --- | --- |
| `iot_locker_client.c` | Arduino Locker message와 SQL/DB 처리 연결 | 팀 구현 및 integration |
| `iot_server.c` | TCP client 인증·routing과 DB 연동 | 팀원 구현 |
| `iot_kiosk_client.c` | STM32 Kiosk Bluetooth–TCP bridge | 팀 구현 및 integration |

## 해석 기준

- Project 전체 source와 개인 직접 구현 source를 구분합니다.
- Protocol·data field 협의와 DB 구축·운영을 구분합니다.
- 팀원 source의 최초 구현과 개인의 integration·debugging 기여를 구분합니다.
- 확인되지 않은 역할은 개인 기여로 추정하지 않습니다.
