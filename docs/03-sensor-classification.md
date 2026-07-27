# Sensor 상태 분류

Sensor 판정 기준은 `firmware/Locker-control/src/main.cpp`의 현재 상수를 기준으로 합니다.

## Door 상태

TS0224 Hall sensor는 Arduino `D2`의 digital input으로 사용합니다.

```c
doorClosed = (digitalRead(HALL_PIN) == LOW);
```

| 입력 | 상태 |
| --- | --- |
| `LOW` | `CLOSED` |
| `HIGH` | `OPEN` |

잠금 상태에서 문이 열리면 `FORCED_OPEN` alert를 생성합니다.

## Item 상태

Flex sensor는 Arduino `A1`에서 읽습니다.

1. Boot 시 50회 sampling해 `flexBase` 설정
2. 상태 갱신 시 10회 sampling 후 평균 계산
3. Baseline과 현재 값의 절대 차이인 `flexDiff` 계산
4. 서로 다른 감지·해제 threshold로 hysteresis 적용

| 조건 | 값 |
| --- | ---: |
| 감지 threshold | `FLEX_DETECT_DELTA = 8` |
| 해제 threshold | `FLEX_RELEASE_DELTA = 5` |
| 상태 sampling 주기 | `300ms` |

이미 물품이 감지된 상태에서는 해제 threshold를 적용해 경계값 부근의 반복 전환을 줄입니다. 잠금 상태에서 물품 상태가 바뀌면 `ITEM_CHANGED_WHILE_LOCKED` alert를 생성합니다.

## Servo Current

INA219는 servo 구동 중 current peak를 측정합니다.

| 항목 | 값 |
| --- | ---: |
| Servo 구동 측정 시간 | `700ms` |
| 동작 확인 기준 | `40mA` |

측정 peak가 `SERVO_CURRENT_ACTIVE_MA`보다 낮으면 다음 alert를 생성합니다.

- 잠금 해제: `UNLOCK_CURRENT_LOW`
- 잠금: `LOCK_CURRENT_LOW`

## 중복 Alert 방지

다음 flag로 같은 상태에서 alert가 반복 송신되는 것을 막습니다.

- `forcedOpenAlertSent`
- `itemTamperAlertSent`
- `alertSent`

이 값들은 잠금·해제 상태가 전환될 때 초기화됩니다.

## 적용 시 주의

현재 threshold는 이 project의 sensor와 기구 조건에 사용한 firmware 값입니다. 다른 sensor, 전원 또는 기구에 적용할 때는 실제 측정 후 다시 보정해야 합니다.
