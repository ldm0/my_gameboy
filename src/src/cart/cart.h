#pragma once
#ifndef _MY_GB_CART_H_
#define _MY_GB_CART_H_

#include<stdint.h> // for uintN_t

int my_gb_cart_construct(const char *filename);

void my_gb_cart_destruct(void);

uint8_t my_gb_cart_address_read(uint16_t address);

void my_gb_cart_address_write(uint16_t address, uint8_t data);

#endif 
