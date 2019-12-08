#include"ram.h"
#include<stdlib.h>
#include<stdint.h>
#include<string.h>

static uint8_t* ram = 0;

uint8_t* my_gb_ram_get(void)
{
    return ram;
}

int my_gb_ram_construct(void)
{
    /* 8KB video RAM from 0x8000 ~ 0x9FFF
     * 8KB internal RAM from 0xC000 ~ 0xDFFF
     * 160B Object Attribute Memory(Sprite information table) 0xFE00 ~ 0xFE9F
     * 127B Internal stack RAM(HRAM) 0xFF80 ~ 0xFFFE
     */
    size_t ram_size = 8 * 1024 + 8 * 1024 + 0xA0 + 0x7F;
    ram = (uint8_t *)malloc(ram_size);
    if (!ram)
        return -1;
    memset(ram, 0x55, ram_size);
    return 0;
}

void my_gb_ram_destruct(void)
{
    if (ram) {
        free(ram);
        ram = 0;
    }
}
