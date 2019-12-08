#include "input.h"
#include "cpu.h"

uint8_t P1;

int my_gb_input_construct(void)
{
    P1 = 0;
    return 0;
}

void my_gb_input_destruct(void)
{
    // do nothing
}

void my_gb_input_callback(enum BUTTON_TYPE button, uint32_t edge_type)
{
    uint8_t button_mask;
    switch (button) {
    case BUTTON_A:
        button_mask = (1 << 0) | (1 << 5);
        break;
    case BUTTON_B:
        button_mask = (1 << 1) | (1 << 5);
        break;
    case BUTTON_SELECT:
        button_mask = (1 << 2) | (1 << 5);
        break;
    case BUTTON_START:
        button_mask = (1 << 3) | (1 << 5);
        break;
    case BUTTON_RIGHT:
        button_mask = (1 << 0) | (1 << 4);
        break;
    case BUTTON_LEFT:
        button_mask = (1 << 1) | (1 << 4);
        break;
    case BUTTON_UP:
        button_mask = (1 << 2) | (1 << 4);
        break;
    case BUTTON_DOWN:
        button_mask = (1 << 3) | (1 << 4);
        break;

    case BUTTON_OTHERS:
    default:
        button_mask = 0;
        break;
    }

    if (button_mask) {  // if button is valid gameboy button
        switch (edge_type) {
        case EDGE_DOWN:
            P1 |= button_mask;
            my_gb_cpu_on_interruption(INT_BUTTON_PRESS);
            break;
        case EDGE_UP:
            P1 &= ~button_mask;
            break;
        }
    }
}

void my_gb_cpu_link_input(void)
{
    // do nothing
    // Finally! We already have permission to use cpu functions! XD
}

uint32_t my_gb_input_button_pressed(void)
{
    uint32_t result = !!P1;
    P1 = 0;
    return result;
}
