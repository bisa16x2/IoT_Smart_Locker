#include "ap/AccessLock.h"

// application 초기화 및 main loop 실행
int main(void) {
    // Init functions
    AccessLockInit();

    while (1) {
        // Main loop code
        AccessLockMain();
    }

    return 0;
}
