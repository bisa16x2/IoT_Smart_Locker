# driver Layer

`driver` layer는 실제 장치 단위 driver를 관리한다. 현재 locker 목적의 집계 파일은 `driver.c`이다.

## 파일 역할

`driver.c`는 개별 driver를 include하고, `bsp`가 사용할 locker 목적 API로 묶는다.

```c
#include "driver/ina219.h"
#include "driver/led.h"
#include "driver/servo.h"
#include "driver/szh_sen.h"
#include "driver/ts0224.h"
```

`driverInit(config)`는 AP에서 BSP를 거쳐 내려온 설정 포인터를 개별 driver 초기화 함수에 전달한다. 각 driver는 필요한 pin, threshold, I2C 주소 값을 이 설정에서 읽는다. `driver`는 `AccessLock.h`를 include하지 않는다.

## 개별 Driver 역할

`servo.c`는 servo motor 각도를 바꿔 locker의 잠금/해제 동작을 만든다.

`led.c`는 locker 상태를 LED로 표시한다.

`ts0224.c`는 hall sensor 값을 읽어 문 열림/닫힘 상태를 판단한다.

`szh_sen.c`는 flex pressure sensor 값을 읽어 보관물 유무와 pressure 값을 판단한다.

`ina219.c`는 current sensor 값을 읽어 `current_amp`로 상위 layer에 전달한다.

`mfrc522.c`와 `esp8266.c`는 RFID와 ESP8266 장치 driver이다. `esp8266.c`는 AT 명령 단위 동작만 제공하고, Wi-Fi AP 접속 순서와 TCP server login 같은 board 목적 흐름은 `bspWifi.c`가 담당한다.

## Driver 집계 API

`driver.c`는 아래 API를 제공한다.

```c
driverLockerLock();
driverLockerUnlock();
driverLockerSetIndicator();
driverLockerUpdate();
driverLockerIsDoorClosed();
driverLockerIsItemDetected();
driverLockerCalibrateDoor();
driverLockerCalibrateItem();
driverLockerGetPressureValue();
driverLockerGetPressureDiff();
driverLockerGetCurrentAmp();
```

`driverLockerUpdate()`는 `ts0224Update()`와 `szhSenUpdate()`를 호출한 뒤, door/item/pressure 상태를 내부 cache에 저장한다. 이후 getter 함수들은 마지막으로 갱신된 값을 반환한다.

## 상위 Layer로 이어지는 방식

`driver` layer는 `bsp` layer로 올라간다. `bsp.c`는 `driver/driver.h`만 include하고, locker 목적 API를 그대로 board API로 감싼다.

```text
servo/led/sensor/current driver
  -> driver.c
  -> bsp.c
  -> ap/AccessLock.c
```
