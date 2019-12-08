#pragma once;
#ifndef _MY_GB_CPU_H_
#define _MY_GB_CPU_H_

#include"../cart/cart.h"
#include<stdint.h>

#define LOAD_BOOTSTRAP

enum INTERRUPTION_TYPE {
	INT_VBLANK,
	INT_LCDC_STATUS,
	INT_TIMER_OVERFLOW,
	INT_SERIAL_TRANSFER_COMPLETION,
	INT_BUTTON_PRESS,
};

int my_gb_cpu_construct(void);

void my_gb_cpu_destruct(void);

void my_gb_cpu_link_ram(uint8_t* _internal_ram);

void my_gb_cpu_link_cart(void);

// currently not implemented
// process only one instruction
// return number of machine cycles
uint32_t my_gb_cpu_run(uint32_t cycles_want);

// currently not implemented
// need to check IME(interrupt master enable)
void my_gb_cpu_on_interruption(enum INTERRUPTION_TYPE type);

#endif
