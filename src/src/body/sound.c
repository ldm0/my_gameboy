#include "sound.h"

uint8_t NR10;
uint8_t NR11;
uint8_t NR12;
uint8_t NR13;
uint8_t NR14;
uint8_t NR21;
uint8_t NR22;
uint8_t NR23;
uint8_t NR24;
uint8_t NR30;
uint8_t NR31;
uint8_t NR32;
uint8_t NR33;
uint8_t NR34;
uint8_t NR41;
uint8_t NR42;
uint8_t NR43;
uint8_t NR44;
uint8_t NR50;
uint8_t NR51;
uint8_t NR52;
uint8_t W[0x10];

int my_gb_sound_construct(void)
{
    NR10 = 0;
    NR11 = 0;
    NR12 = 0;
    NR13 = 0;
    NR14 = 0;
    NR21 = 0;
    NR22 = 0;
    NR23 = 0;
    NR24 = 0;
    NR30 = 0;
    NR31 = 0;
    NR32 = 0;
    NR33 = 0;
    NR34 = 0;
    NR41 = 0;
    NR42 = 0;
    NR43 = 0;
    NR44 = 0;
    NR50 = 0;
    NR51 = 0;
    NR52 = 0;
    for (int i = 0; i < 0x10; ++i)
        W[i] = 0;
    return 0;
}

void my_gb_sound_destruct(void)
{
}

uint32_t my_gb_sound_run(uint32_t cycles_want)
{
    uint32_t cycles = 0;
    return 0;
}

