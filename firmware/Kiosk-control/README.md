# STM32 Kiosk Controller Firmware

STM32F411RE 기반 사용자 인증 Kiosk Firmware입니다.

## Responsibilities

- Keypad 기반 Locker 번호 입력
- 4자리 PIN 입력 및 등록
- LCD 화면 상태 표시
- UART를 통한 인증·등록 요청 전송
- 인증 결과 수신
- FreeRTOS Task에서 Kiosk FSM 실행

## Main Flow

```text
main()
└── MX_FREERTOS_Init()
    └── StartDefaultTask()
        ├── apInit()
        └── apMain()
            └── kioskMain()
