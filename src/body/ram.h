#pragma once
#ifndef _MY_GB_RAM_H_
#define _MY_GB_RAM_H_

#include<stdint.h>

#define RAM_OFFSET_VRAM (0)
#define RAM_OFFSET_INTERNAL_RAM (8 * 1024)
#define RAM_OFFSET_OAM (8 * 1024 + 8 * 1024)
#define RAM_OFFSET_HRAM (8 * 1024 + 8 * 1024 + 0xA0)

uint8_t* my_gb_ram_get(void);

int my_gb_ram_construct(void);

void my_gb_ram_destruct(void);

#endif

