#include "ap.h"

void apInit(void)
{
    hwInit();
}

void apMain(void)
{
    hwMain();

    osDelay(10);
}