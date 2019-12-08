#include<stdio.h>
#include"./body/cpu.h"
#include"./body/ram.h"
#include"./body/screen.h"
#include"./body/input.h"
#include"./body/sound.h"
#include"./cart/cart.h"
#include<Windows.h>

#define NUM_SCANLINE_NON_VBLANK 144
#define NUM_SCANLINE_VBLANK 10

#define CYCLES_OAM_SEARCH 20
#define CYCLES_PIXEL_TRANSFER 43
#define CYCLES_H_BLANK 51
// 20 + 43 + 51 == 114
#define CYCLES_PER_SCANLINE 114

// 114 * 144 = 16416
#define CYCLES_NON_V_BLANK 16416
// 114 * 10 = 1140
#define CYCLES_V_BLANK 1140
// 16416 + 1140 = 17556
#define CYCLES_PER_FRAME 17556

#define CYCLES_PER_SECOND (1024 * 1024)

#define CYCLES_PER_TIME_SLICE 1

static const char *cart_location = "../assets/pacman.gb";

static enum BUTTON_TYPE kb2joypad(WPARAM vk)
{
    enum BUTTON_TYPE button;
    // Same keyboard mapping to BGB
    switch (vk) {
    case 'S':
        button = BUTTON_A;
        break;
    case 'A':
        button = BUTTON_B;
        break;
    case VK_SHIFT:
        button = BUTTON_SELECT;
        break;
    case VK_RETURN:
        button = BUTTON_START;
        break;
    case VK_LEFT:
        button = BUTTON_LEFT;
        break;
    case VK_RIGHT:
        button = BUTTON_RIGHT;
        break;
    case VK_UP:
        button = BUTTON_UP;
        break;
    case VK_DOWN:
        button = BUTTON_DOWN;
        break;
    default:
        button = BUTTON_OTHERS;
        break;
    }
    return button;
}

static LRESULT CALLBACK message_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
    case WM_KEYDOWN:
    case WM_KEYUP:
        {
            enum BUTTON_TYPE button = kb2joypad(wparam);
            enum EDGE_TYPE edge;
            if (msg == WM_KEYDOWN) {
                edge = EDGE_DOWN;
            } else if (msg == WM_KEYUP) {
                edge = EDGE_UP;
            } else {
                edge = EDGE_OTHERS;
            }
            my_gb_input_callback(button, edge);
        }
        break;
    case WM_QUIT:
    default:
        return DefWindowProc(hwnd, msg, wparam, lparam);
        break;
    }
    return 0;
}

static void message_dispatch(void)
{
    MSG msg;
    while (PeekMessage(&msg, 0, 0, 0, PM_NOREMOVE)) {
        if (GetMessage(&msg, 0, 0, 0) == 0)
            break;
        DispatchMessage(&msg);
    }
}

struct exceed_cycles {
    int cpu_cycles;
    int screen_cycles;
    int sound_cycles;
};

static LARGE_INTEGER timer_time_before;
static LARGE_INTEGER timer_time_freq;

int timer_init(void)
{
    if (QueryPerformanceFrequency(&timer_time_freq) == FALSE) {
        return -1;
    }
    if (QueryPerformanceCounter(&timer_time_before) == FALSE) {
        return -1;
    }
    return 0;
}

int timer_delta_cycles(void)
{
    LARGE_INTEGER timer_time_current;
    int dc = 0;
    QueryPerformanceCounter(&timer_time_current);
    dc = (int)((((timer_time_current.QuadPart - timer_time_before.QuadPart) * CYCLES_PER_SECOND) / timer_time_freq.QuadPart) + 0.5);
    timer_time_before = timer_time_current;
    return dc;
}

int main()
{
    // init internal ram
    if (my_gb_ram_construct() == -1) {
        fprintf(stderr, "internal ram construction failed.\n");
        return -1;
    }
    // init cpu
    if (my_gb_cpu_construct() == -1) {
        fprintf(stderr, "cpu construction failed.\n");
        return -1;
    }
    // init input
    if (my_gb_input_construct() == -1) {
        fprintf(stderr, "input construction failed.\n");
        return -1;
    }
    // init screen
    if (my_gb_screen_construct(message_callback) == -1) {
        fprintf(stderr, "screen construction failed.\n");
        return -1;
    }
    // init cart
    if (my_gb_cart_construct(cart_location) == -1) {
        fprintf(stderr, "cannot open cart in %s, please check.\n", cart_location);
        return -1;
    }

    my_gb_cpu_link_input();
    my_gb_cpu_link_screen();
    my_gb_cpu_link_ram(my_gb_ram_get());
    my_gb_screen_link_ram(my_gb_ram_get());
    my_gb_cpu_link_cart();

    struct exceed_cycles exc;
    exc.cpu_cycles = 0;
    exc.screen_cycles = 0;
    exc.sound_cycles = 0;

    if (timer_init() == -1) {
        fprintf(stderr, "Use high resolution timer failed.\n");
        return -1;
    }

    for (;;) {
        message_dispatch();
        int dc = timer_delta_cycles();
        // To get smaller time slice
        for (int i = 0; i < dc; i += CYCLES_PER_TIME_SLICE) {
            if (exc.cpu_cycles >= CYCLES_PER_TIME_SLICE) {
                exc.cpu_cycles -= CYCLES_PER_TIME_SLICE;
            } else {
                exc.cpu_cycles = my_gb_cpu_run(CYCLES_PER_TIME_SLICE - exc.cpu_cycles);
            }

            if (exc.screen_cycles >= CYCLES_PER_TIME_SLICE) {
                exc.screen_cycles -= CYCLES_PER_TIME_SLICE;
            } else {
                exc.screen_cycles = my_gb_screen_run(CYCLES_PER_TIME_SLICE - exc.screen_cycles);
            }

            if (exc.sound_cycles >= CYCLES_PER_TIME_SLICE) {
                exc.sound_cycles -= CYCLES_PER_TIME_SLICE;
            } else {
                exc.sound_cycles = my_gb_sound_run(CYCLES_PER_TIME_SLICE - exc.sound_cycles);
            }
        }
        Sleep(1);
    }

    my_gb_cart_destruct();
    my_gb_screen_destruct();
    my_gb_input_destruct();
    my_gb_cpu_destruct();
    my_gb_ram_destruct();
}