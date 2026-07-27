# 출처

이 저장소는 기존 팀 프로젝트를 코드 포트폴리오 형태로 재구성한 작업본입니다.

## 현재 포함 범위

- Arduino Uno 보관함 제어 firmware
- STM32F411 Kiosk firmware
- Raspberry Pi TCP server·client source
- Firmware와 software 구조를 설명하는 portfolio 문서

Software는 `iot_locker_client.c`, `iot_server.c`, `iot_kiosk_client.c`를 main source로 정리했으며 build와 전체 연결 동작을 확인했습니다.

## 원본 Repository

- 원본 repository: [bisa16x2-IoT_mini_SmartLocker](https://github.com/bisa16x2/bisa16x2-IoT_mini_SmartLocker)
- 재구성 기준 commit: `e1ddb36380dbb8f3ada3667b75a120b8a00236fa`

현재 저장소는 원본 source를 공개 portfolio 구조로 재배치하고 민감정보·build 산출물·개인 IDE 설정을 제외한 구성입니다.

## 재구성 원칙

- 원본 repository의 중첩된 Git metadata를 포함하지 않습니다.
- build 산출물과 개인 IDE 설정을 공개 대상에서 제외합니다.
- Wi-Fi, server, DB, PIN, UID 및 인증 관련 실제 값을 공개하지 않습니다.
- 개인·공동·팀원 구현 범위를 문서로 구분합니다.
- 동작하지 않은 이전 software 파생본은 공개 대상에서 제외합니다.

## 공개 조건

팀 code는 개인 구현 범위를 명시하는 조건으로 portfolio 공개 허용을 확인했습니다. 별도 license가 없으므로 제3자에게 복제·수정·재배포 권한을 부여하지 않습니다.
