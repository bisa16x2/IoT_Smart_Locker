# 문제 해결

## Socket Message Parsing

### 문제

Server 중계 message의 command 위치와 앞뒤 공백·개행에 따라 고정 index parsing이 실패할 수 있습니다.

### 현재 처리

`socketEvent()`는 token의 앞뒤 whitespace를 제거하고 모든 token 위치에서 다음 command를 찾습니다.

```text
KIOSK_AUTH
RFID_AUTH
RFID_DENY
```

Command를 찾으면 인접 token을 인자로 구성해 `handleCommand()`에 전달합니다.

관련 file:

```text
firmware/Locker-control/src/main.cpp
```

## RFID로 열린 Locker 닫기

### 문제

Locker를 연 RFID와 닫으려는 RFID가 다르면 잘못된 사용자가 잠글 수 있습니다. 반대로 Kiosk 인증은 RFID UID 없이 문을 열기 때문에 닫기 기준이 없을 수 있습니다.

### 현재 처리

- RFID 인증으로 열 때 UID를 `activeRfidUid`에 저장
- 닫을 때 동일 UID인지 비교
- 불일치 시 `RFID_MISMATCH`
- Kiosk 인증 경로에는 설정된 `RFID_TAG` fallback 적용

관련 함수:

```text
processRfidAuth()
isSameActiveRfid()
rememberActiveRfid()
```

## Flex Sensor 경계값 진동

### 문제

하나의 threshold만 사용하면 sensor 값이 경계 부근에서 변할 때 `EXIST`와 `EMPTY`가 반복될 수 있습니다.

### 현재 처리

- 감지 threshold: `8`
- 해제 threshold: `5`
- 10회 sampling 평균
- Boot 시 50회 baseline 보정

관련 함수:

```text
readItemPresent()
calibrateFlex()
```

## 이상 상태 Alert 반복

### 문제

문이 열린 상태나 물품 변경 상태가 유지되면 같은 alert가 주기적으로 반복될 수 있습니다.

### 현재 처리

상태별 flag로 최초 변화에서만 alert를 전송하고 잠금 상태 전환 시 flag를 초기화합니다.

```text
forcedOpenAlertSent
itemTamperAlertSent
alertSent
```

## Kiosk 응답 Timeout

Kiosk는 인증 또는 등록 요청 후 `3000ms` 동안 결과를 기다립니다. 시간 안에 성공 응답을 받지 못하면 실패 화면을 표시하고, 결과 화면도 `3000ms` 후 초기 상태로 복귀합니다.

관련 file:

```text
firmware/Kiosk-control/app/driver/kiosk.c
```

## 공개용 Placeholder

공개 source의 DB, Bluetooth 및 client 인증값은 placeholder입니다. 실제 실행 환경에서는 다음 값을 로컬 설정과 일치시켜야 합니다.

```text
(DB HOST)
(DB USER)
(DB PASSWORD)
(DB NAME)
(BLUETOOTH MAC)
(PASSWORD)
```

`idpasswd.txt`는 `idpasswd.txt.example`을 복사해 생성하며 Git 추적 대상에서 제외합니다.
