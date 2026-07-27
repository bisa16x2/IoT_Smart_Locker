# 스마트 개인 무인 보관함 관리 시스템

## 프로젝트 개요

본 프로젝트는 RFID 인증과 센서 기반 상태 감지를 이용하여 개인 보관함을 무인으로 관리하는 IoT 시스템입니다.

RFID-Based Smart Personal Locker Management System

## 핵심 기능

- RFID 사용자 인증
- 서보모터 기반 잠금 제어
- 문 열림/닫힘 감지
- 물품 수납 여부 감지
- 전류/전력 모니터링

## 시스템 구조

<img width="1448" height="1086" alt="무인보관함_구조도" src="https://github.com/user-attachments/assets/a6e8ea73-727a-4746-80d3-ea42b858651d" />

## 동작 시나리오

1. RFID 카드 태그
2. 인증 성공
3. 잠금 해제
4. 물품 보관
5. 문 닫힘 감지
6. 재태그 시 잠금
7. 상태 모니터링


## 사용 보드

| 보드 | 역할 |
|---|---|
| Raspberry Pi 4 | 중앙 관리 서버부 |
| Arduino Uno | 출입 인증·잠금 제어부 |
| STM32 F411RE | 보관 상태·도난 감시부 |

## 사용 부품

| 부품 | 역할 |
|---|---|
| RFID - RC522 | 사용자 인증 |
| Servo - SG90 | 잠금 제어 |
| Hall - TS0224 | 문 상태 감지 |
| Current - INA219 | 전류 측정 |
| Flex - SZH-SEN02 | 물품 감지 |
