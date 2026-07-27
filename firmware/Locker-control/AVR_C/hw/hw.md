# hw Layer

`hw` layer는 ATmega328P peripheral과 low-level 기능을 담당한다. 현재 집계 파일은 `hw.c`이다.

## 파일 역할

`hw.c`는 UART, ADC, I2C, Timer0 기반 system tick을 상위 layer가 쓰기 쉬운 API로 묶는다. `bsp`는 `hw/hw.h`만 include하고 개별 UART/I2C/ADC 파일을 직접 보지 않는다.

## 주요 파일

`hw.c`는 HW 집계 API를 제공한다.

`my_uart.c`는 UART register 설정, 문자 송신, 문자열 송신, 수신 가능 여부 확인, byte read/write를 처리한다. Baudrate는 `hwInit(config)`로 받은 설정값을 사용한다.

`my_i2c.c`는 TWI/I2C start, stop, register read/write를 처리한다. I2C 주파수는 설정값을 사용하고, `ina219` driver가 이 기능을 사용한다.

`my_adc.c`는 ADC channel read를 처리한다. `ts0224`와 `szh_sen` driver가 sensor 값을 읽을 때 사용하며, 실제 ADC channel 번호는 설정값에서 가져온다.

## HW 집계 API

초기화 API는 설정 포인터를 받아 UART, ADC, I2C를 초기화한다.

```c
hwInit(config);
```

시간 관련 API는 Timer0 interrupt 기반이다.

```c
timer0_init();
hwMillis();
hwDelay();
```

통신 관련 API는 UART 기능을 감싼다.

```c
hwComAvailable();
hwComRead();
hwComPrint();
hwComPrintNum();
hwComPrintU16();
```

## 상위 Layer로 이어지는 방식

`hw` layer는 `bsp` layer로 올라간다. `bsp.c`는 UART나 Timer register를 직접 다루지 않고, `hwCom*`와 `hwMillis()` 같은 API만 사용한다.

```text
my_uart.c / my_i2c.c / my_adc.c
  -> hw.c
  -> bsp.c
  -> ap/AccessLock.c
```

## Driver와의 관계

일부 driver는 low-level peripheral이 필요하다. 예를 들어 `ina219.c`는 I2C를 사용하고, sensor driver는 ADC를 사용한다. 이런 경우 개별 driver가 필요한 `hw/my_i2c.h`나 `hw/my_adc.h`를 직접 사용한다. 다만 `bsp`와 `ap`는 이 의존성을 직접 알지 않는다.
