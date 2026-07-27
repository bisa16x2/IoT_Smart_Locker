# bsp Layer

`bsp` layer는 application이 사용할 board 목적 API를 제공한다. 현재 구현 파일은 `bsp.c`이다.

## 파일 역할

`bsp.c`는 `hw/hw.h`, `driver/driver.h`, `bsp/bspWifi.h`를 include한다. 개별 UART, servo, LED, sensor header를 직접 include하지 않는다.

```c
#include "bsp/bspWifi.h"
#include "hw/hw.h"
#include "driver/driver.h"
```

이 layer는 `ap`의 요청을 `hw`와 `driver`의 집계 API, 그리고 `bspWifi.c`의 Wi-Fi 목적 API로 연결한다.

## 제공 API 성격

초기화와 시간 API는 `hw` 쪽으로 이어진다.

```c
bspInit(&access_config);
bspMillis();
bspDelay();
```

locker 제어와 sensor 상태 API는 `driver` 쪽으로 이어진다.

```c
bspLockerLock();
bspLockerUnlock();
bspLockerSetIndicator();
bspLockerUpdate();
bspLockerIsDoorClosed();
bspLockerIsItemDetected();
bspLockerGetPressureValue();
bspLockerGetPressureDiff();
bspLockerGetCurrentAmp();
```

일반 통신 API는 `hw` 쪽 UART 집계 API로 이어진다.

```c
bspComAvailable();
bspComRead();
bspComPrint();
bspComPrintNum();
bspComPrintU16();
```

## Wi-Fi API

Wi-Fi 목적 API는 `bspWifi.c`가 담당한다. `bspWifiInit(config)`는 설정 포인터를 저장하고 ESP8266 context를 초기화한다. `bspWifiConnect()`는 저장된 설정으로 AP 접속, TCP 연결, server login을 수행한다. 실제 AT 명령 단위 처리는 `driver/esp8266.c`에 위임한다.

```c
bspWifiConnect();
bspWifiSend(data);
bspWifiIsConnected();
```

## 상위 Layer로 이어지는 방식

`ap` layer는 `bsp.h`를 통해 board 목적 API를 사용한다. `bsp`는 application과 실제 board 구현 사이의 boundary 역할을 한다.

## 하위 Layer로 이어지는 방식

`bspInit()`은 AP에서 받은 설정 포인터를 `hwInit()`, `driverInit()`, `bspWifiInit()`으로 전달한다. `bsp`가 pin 값을 직접 결정하지 않기 때문에 하위 layer가 상위 header를 include할 필요가 없다.

```text
bspInit(config)
  -> hwInit(config)
  -> driverInit(config)
  -> bspWifiInit(config)
```

그 외 API도 같은 방식으로 `hw` 또는 `driver` 집계 함수에 위임한다.
