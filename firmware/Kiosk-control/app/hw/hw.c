#include "hw.h"


void hwInit(void)
{
    lcdInit();
    kioskInit();
}

void hwMain(void)
{
    kioskMain();
}