#pragma once

#ifndef _MY_GB_INPUT_H_
#define _MY_GB_INPUT_H_

#include<stdint.h>

enum BUTTON_TYPE {
    BUTTON_OTHERS,
    BUTTON_A,
    BUTTON_B,
    BUTTON_SELECT,
    BUTTON_START,
    BUTTON_RIGHT,
    BUTTON_LEFT,
    BUTTON_UP,
    BUTTON_DOWN,
};

enum EDGE_TYPE {
    EDGE_OTHERS,
    EDGE_DOWN,
    EDGE_UP,
};

/*
FF00
   Name     - P1
   Contents - Register for reading joy pad info
              and determining system type.    (R/W)

           Bit 7 - Not used
           Bit 6 - Not used
           Bit 5 - P15 out port
           Bit 4 - P14 out port
           Bit 3 - P13 in port
           Bit 2 - P12 in port
           Bit 1 - P11 in port
           Bit 0 - P10 in port

         This is the matrix layout for register $FF00:


                 P14        P15
                  |          |
        P10-------O-Right----O-A
                  |          |
        P11-------O-Left-----O-B
                  |          |
        P12-------O-Up-------O-Select
                  |          |
        P13-------O-Down-----O-Start
                  |          |
*/
extern uint8_t P1;

int my_gb_input_construct(void);

void my_gb_input_destruct(void);

void my_gb_cpu_link_input(void);

// just for interruption check
// check button if is pressed after last call
uint32_t my_gb_input_button_pressed(void);

// edge_type:
// 0 for down 
// 1 for up
void my_gb_input_callback(enum BUTTON_TYPE button, uint32_t edge_type);

#endif
