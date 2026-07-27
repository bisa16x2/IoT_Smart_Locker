#include "keypad.h"

char get_key(void) {
    static const char key_map[4][4] = {
        {'1', '2', '3', 'A'},
        {'4', '5', '6', 'B'},
        {'7', '8', '9', 'C'},
        {'*', '0', '#', 'D'}
    };

    GPIO_TypeDef* row_ports[] = {ROW1_PORT, ROW2_PORT, ROW3_PORT, ROW4_PORT};
    uint16_t row_pins[] = {ROW1_PIN, ROW2_PIN, ROW3_PIN, ROW4_PIN};
    GPIO_TypeDef* col_ports[] = {COL1_PORT, COL2_PORT, COL3_PORT, COL4_PORT};
    uint16_t col_pins[] = {COL1_PIN, COL2_PIN, COL3_PIN, COL4_PIN};

    for (int i = 0; i < 4; i++) {
        // 모든 행 리셋 (SET = High)
        HAL_GPIO_WritePin(ROW1_PORT, ROW1_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(ROW2_PORT, ROW2_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(ROW3_PORT, ROW3_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(ROW4_PORT, ROW4_PIN, GPIO_PIN_SET);

        // 현재 행만 활성화 (RESET = Low)
        HAL_GPIO_WritePin(row_ports[i], row_pins[i], GPIO_PIN_RESET);

        for (int j = 0; j < 4; j++) {
            if (HAL_GPIO_ReadPin(col_ports[j], col_pins[j]) == GPIO_PIN_RESET) {
                // [핵심] FreeRTOS 환경이므로 HAL_Delay 대신 osDelay 사용! (태스크 멈춤 방지)
                osDelay(20);

                if (HAL_GPIO_ReadPin(col_ports[j], col_pins[j]) == GPIO_PIN_RESET) {
                    return key_map[i][j];
                }
            }
        }
    }
    return 0;
}
