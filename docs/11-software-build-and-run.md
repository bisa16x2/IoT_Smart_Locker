# Software Build와 실행

세 프로그램은 Linux/Raspberry Pi 환경의 C application입니다.

## 요구 사항

- GCC
- POSIX socket
- pthread
- BlueZ development header·library
- MariaDB Connector/C development header·library

Package 이름은 배포판에 따라 다를 수 있습니다. Debian/Raspberry Pi OS 계열에서는 일반적으로 `libbluetooth-dev`, `libmariadb-dev`가 필요합니다.

## Build

다음 명령은 `software/TCP Socket` directory에서 실행하는 예시입니다.

```bash
gcc iot_server.c -o iot_server \
    $(pkg-config --cflags --libs mariadb) -lpthread

gcc iot_locker_client.c -o iot_locker_client \
    $(pkg-config --cflags --libs mariadb) -lpthread

gcc iot_kiosk_client.c -o iot_kiosk_client \
    -lbluetooth -lpthread
```

MariaDB package가 다른 `pkg-config` 이름을 제공하는 환경에서는 해당 개발 환경의 compiler·linker option을 사용해야 합니다.

## 실행 전 설정

공개 placeholder를 실제 로컬 환경 값으로 설정합니다.

```text
(DB HOST)
(DB USER)
(DB PASSWORD)
(DB NAME)
(BLUETOOTH MAC)
(PASSWORD)
```

실제 client 인증 파일 생성:

```bash
cp idpasswd.txt.example idpasswd.txt
```

`idpasswd.txt`의 ID와 password는 각 client 실행 인자 및 source의 login 값과 일치해야 합니다.

## 실행 순서

### 1. TCP Server

`iot_server`는 현재 working directory에서 `idpasswd.txt`를 찾습니다.

```bash
./iot_server <PORT>
```

### 2. Locker Client

```bash
./iot_locker_client <SERVER IP> <PORT> <LOCKER CLIENT ID>
```

### 3. Kiosk Client

```bash
./iot_kiosk_client <SERVER IP> <PORT> <KIOSK CLIENT ID>
```

Kiosk client는 TCP server에 로그인한 뒤 RFCOMM channel 1로 STM32 Kiosk Bluetooth module에 연결합니다.

### 4. Firmware

- Arduino Locker의 ESP8266을 server IP·port에 연결
- STM32 Kiosk의 Bluetooth module 연결 확인

## 종료 및 Runtime File

- Client terminal에서 `quit` 입력 시 client 종료
- Server log와 runtime `idpasswd.txt`는 공개 credential을 포함하지 않도록 관리
- Build 결과와 실제 credential file은 Git 추적 대상에서 제외

## 검증 상태

두 firmware와 세 software의 정상 build 및 전체 장비 연결 동작을 사용자가 확인했습니다. 공개 source는 placeholder 상태이므로 실제 실행 전 로컬 값을 복원해야 합니다.
