# 변경 이력

## Unreleased

- 공개용 repository 기본 구조 생성
- Locker controller와 Kiosk controller firmware 분리
- 두 firmware의 정상 build 확인 반영
- Wi-Fi, server, DB, Bluetooth, PIN 및 UID 관련 민감값 placeholder 처리
- Locker controller source 주석 정리
- Firmware 중심 README와 기술 문서 작성
- 동작하지 않던 이전 `iot_bt_kiosk_client.c` 제거
- `iot_user_client.c`를 `iot_kiosk_client.c`로 변경
- `iot_client_sql.c`를 `iot_locker_client.c`로 변경
- Software main 연결 구성을 Locker client → Raspberry Pi server → Kiosk client로 확정
- Firmware·software build 및 전체 장비 동작 확인
- Software 구조, build, 실행 순서와 message routing 문서 추가
- 임시 인수인계 문서와 중복 README 제거
- PlatformIO build cache와 개인 IDE 설정 제거
