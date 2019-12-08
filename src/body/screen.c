#include"screen.h"
#include"cpu.h"
#include"ram.h"
#include"../../dep/SCG/scg.h"
#include<stdint.h>

// scale between real window size and gameboy screen size
#define SCALE_RATIO 2

// virtual gameboy underlaying screen buffer size
#define SCREEN_BUFFER_SQUARE_WIDTH 256

#define TILE_SQUARE_WIDTH 8
#define SIZEOF_TILE 16
#define SIZEOF_TILE_LINE 2

// virtual gameboy screen buffer size(not directly seen buffer) 
// screen buffer which will be seen is scg_back_buffer 
#define GAMEBOY_SCREEN_WIDTH 160
#define GAMEBOY_SCREEN_HEIGHT 144

#define OAM_SEARCH_CYCLES 20
#define PIXEL_TRANSFER_CYCLES 43
#define HBLANK_CYCLES 51
// We split V blank period into 10 small parts
// because LY need to be changed during it.
#define VBLANK_CYCLES 114
#define VBLANK_TIMES 10

//#define MDEBUG

uint8_t LCDC;
uint8_t STAT;
uint8_t SCY;
uint8_t SCX;
uint8_t LY;
uint8_t LYC;
uint8_t DMA;
uint8_t BGP;
uint8_t OBP0;
uint8_t OBP1;
uint8_t WY;
uint8_t WX;

static uint8_t * ram;
static uint32_t screen_buffer[SCREEN_BUFFER_SQUARE_WIDTH * SCREEN_BUFFER_SQUARE_WIDTH];
static struct {
    enum {
        SCREEN_STATE_HBLANK,
        SCREEN_STATE_VBLANK,
        SCREEN_STATE_OAM_SEARCH,
        SCREEN_STATE_PIXEL_TRANSFER,
    } state_next;
    int cycles_remain;
    // do a copy here in case they changes between pixel transfer and screen mapping
    // we do this compensation because our screen is not implemented like GB screen and draw line by line
    uint8_t SCX_pixel_transfer;            
    uint8_t SCY_pixel_transfer;
    uint8_t line_current;   // between 0 and 153
} screen_context;

int my_gb_screen_construct(WNDPROC callback)
{
    screen_context.state_next = SCREEN_STATE_OAM_SEARCH;
    screen_context.cycles_remain = 0;
    screen_context.line_current = 0;
	if (scg_create_window(
#ifdef MDEBUG
		SCREEN_BUFFER_SQUARE_WIDTH * SCALE_RATIO,
		SCREEN_BUFFER_SQUARE_WIDTH * SCALE_RATIO,
#else
		GAMEBOY_SCREEN_WIDTH * SCALE_RATIO,
		GAMEBOY_SCREEN_HEIGHT * SCALE_RATIO,
#endif
		_T("My Game Boy"),
		callback) == -1) {
		return -1;
	}
	return 0;
}

void my_gb_screen_destruct(void)
{
	scg_close_window();
}

int my_gb_screen_link_ram(uint8_t* _ram)
{
	ram = _ram;
	return 0;
}

void my_gb_cpu_link_screen(void)
{
	// pretend to link cpu
    // we can use cpu functions directly
}

static inline uint32_t color_translate(uint16_t tile_line, uint8_t bit_index)
{
    // to get color of bit 2
    //      |-|
    // 11000|1|10 -> $C6 (low byte of tile line)
    // 00000|0|00 -> $00 (high byte of tile line)
    //      |-|
    // so color is 0b01 = 1
    uint16_t c = ((tile_line >> bit_index) & 0x1) | ((tile_line >> (bit_index + 7)) & 0x2);
    switch (c) {
    case 0x0:
        return 0xE0F8CF;
    case 0x1:
        return 0x86C06C;
    case 0x2:
        return 0x306850;
    case 0x3:
        return 0x072821;
    }
    return 0;
}

/*
 * Gameboy lcdc mode changing sequence is like this:
 *   2233330000022333300000....111111
 * 2: OAM search        (20 clocks)
 * 3: pixel transfer    (43 clocks)
 * 0: H blank           (51 clocks)
 * 1: V blank           (10 lines 10 * 114 clocks)
 *
 *  Total clock count equals to 114 * 154 = 17556 clocks.
 *  So refresh rate is 1024 * 1024 / 17556 = 59.7275.
 */

static void _oam_search()
{
	// Change LCDC STAT mode to 2
	STAT = (STAT & (~0x3)) | 0x2;

	// Don't need really search for OAM

	// If LY == LYC
	if (screen_context.line_current == LYC) {
		// Set bit 2(coincidence flag)
		STAT |= 0x1 << 2;
		// if STAT register bit 6(coincidence interruption enable) is set
        // trying to cause interruption
		if ((STAT << 1) >> 6)
			my_gb_cpu_on_interruption(INT_LCDC_STATUS);
	} else {
		// Reset bit 2(coincidence flag)
		STAT &= ~(0x1 << 2);
	}

	// If bit 5(enable OAM search interruption) is enabled
	if ((STAT << 2) >> 5)
		my_gb_cpu_on_interruption(INT_LCDC_STATUS);

}

static void _pixel_transfer()
{

    screen_context.SCX_pixel_transfer = SCX;
    screen_context.SCY_pixel_transfer = SCY;
	// change lcdc mode to 3
	STAT = (STAT & (~0x3)) | 0x3;

	// draw a line by accessing V-RAM and OAM
    uint16_t sprite_tile_data_pt;
    uint16_t bg_wnd_tile_data_pt;
    uint16_t bg_tile_map_pt;
    uint16_t wnd_tile_map_pt;
    uint16_t sprite_tile_map_pt;

    // sprite map and tile data address is fixed
    sprite_tile_data_pt = 0x8000;
    sprite_tile_map_pt = 0xFE00;

    // extract this bit because bit 4 will be used in determining offset
    uint8_t lcdc_bit0 = (LCDC) & 0x1;
    uint8_t lcdc_bit1 = (LCDC >> 1) & 0x1;
    uint8_t lcdc_bit2 = (LCDC >> 2) & 0x1;
    uint8_t lcdc_bit3 = (LCDC >> 3) & 0x1;
    uint8_t lcdc_bit4 = (LCDC >> 4) & 0x1;
    uint8_t lcdc_bit5 = (LCDC >> 5) & 0x1;
    uint8_t lcdc_bit6 = (LCDC >> 6) & 0x1;

	if (lcdc_bit4) {
		bg_wnd_tile_data_pt = 0x8000;
	} else {
		bg_wnd_tile_data_pt = 0x9000;
	}
	if (lcdc_bit3) {
        bg_tile_map_pt = 0x9C00;    // 0x9C00~0x9FFF
    } else {
        bg_tile_map_pt = 0x9800;    // 0x9800~0x9BFF
    }
	if (lcdc_bit6) {
        wnd_tile_map_pt = 0x9C00;   // same with above
    } else {
        wnd_tile_map_pt = 0x9800;
    }

    // drawing below is drawing to mapped line of 256 * 256 gameboy virtual screen buffer 
    // rather than drawing to 160 * 144 buffer
    // draw background 
    if (lcdc_bit0) {
        uint8_t y_real = (screen_context.line_current + SCY);       // real y index in screen buffer
        uint8_t y_tile = y_real / TILE_SQUARE_WIDTH;                // line of tile data 
        uint8_t y_tile_offset = y_real % TILE_SQUARE_WIDTH;         // offset of tile data 
        for (uint8_t x_tile = 0; x_tile < (SCREEN_BUFFER_SQUARE_WIDTH / TILE_SQUARE_WIDTH); ++x_tile) {
            // draw a chunk
            uint8_t tile_index = ram[bg_tile_map_pt - 0x8000 + RAM_OFFSET_VRAM + x_tile + y_tile * (SCREEN_BUFFER_SQUARE_WIDTH / TILE_SQUARE_WIDTH)];
            uint16_t tile_address;
            // With different background window tile data address,
            // different addressing method are used.
            tile_address = bg_wnd_tile_data_pt - 0x8000 + RAM_OFFSET_VRAM + y_tile_offset * SIZEOF_TILE_LINE;
            if (lcdc_bit4 || tile_index <= 127)
                tile_address += tile_index * SIZEOF_TILE;
            else
                tile_address -= tile_index * SIZEOF_TILE;
            uint16_t line_data = ram[tile_address];
            for (uint8_t px = 0; px < TILE_SQUARE_WIDTH; ++px) {
                uint8_t x_real = px + x_tile * TILE_SQUARE_WIDTH;
                screen_buffer[x_real + y_real * SCREEN_BUFFER_SQUARE_WIDTH] = color_translate(line_data, TILE_SQUARE_WIDTH - 1 - px);
            }
        }
    }
    /*
     * I decided to not do branching between draw background or window,
     * just draw the background and then draw the window on the top
     * we will achieve the same result 
     * (which can be proved because no interruption need to be triggered during scanline FIFO)
     */
    // draw window
    if (lcdc_bit5) {
        for (uint8_t x_tile = 0; x_tile < (SCREEN_BUFFER_SQUARE_WIDTH / TILE_SQUARE_WIDTH); ++x_tile) {
            // draw a chunk
            // attention windows x need to -7
        }
    }
    // draw sprite
    if (lcdc_bit1) {
        for (uint8_t i = 0; i < 0xA0; ++i) {
            // get sprite size
            if (lcdc_bit2);
        }
    }

    // Here all the data was transferred to LCD driver,
	// so set LY to current line(use this logic according to documentation)
	LY = screen_context.line_current;
}

static void _h_blank(uint8_t line_current)
{
    // CPU just idling

	// change lcdc mode to 0
	STAT = (STAT & (~0x3)) | 0x0;

	// if bit 3(h blank interruption) is enabled
	if ((STAT << 4) >> 3)
		my_gb_cpu_on_interruption(INT_LCDC_STATUS);
}

static void _v_blank(uint8_t line_currrent)
{
    // CPU just idling

	// change lcdc mode to 1
	STAT = (STAT & (~0x3)) | 0x1;

	// if bit 4(v blank interruption is enabled)
	if ((STAT << 3) >> 4)
		my_gb_cpu_on_interruption(SCREEN_STATE_VBLANK);

    // change LY
    LY = line_currrent;
}

// put gameboy screen buffer to back buffer and swap buffer
// currently not refresh gameboy line by line 
// actually for reducing Bitblt count, 
// we use the strategy that only refresh screen only during V blank
// may try to use fresh line by line in the future
static void _screen_mapping(void)
{
	// copy screen buffer to windows dib buffer
    // (according to SCX and SCY map pixels from 256 * 256 to 160 * 144)
    for (uint32_t y = 0; y < GAMEBOY_SCREEN_HEIGHT; ++y) {
        for (uint32_t x = 0; x < GAMEBOY_SCREEN_WIDTH; ++x) {
            uint32_t buffer_x = (x + screen_context.SCX_pixel_transfer) % SCREEN_BUFFER_SQUARE_WIDTH;
            uint32_t buffer_y = (y + screen_context.SCY_pixel_transfer) % SCREEN_BUFFER_SQUARE_WIDTH;
            int buffer_index = buffer_x + buffer_y * SCREEN_BUFFER_SQUARE_WIDTH;
            uint32_t color = screen_buffer[buffer_index];
            for (uint32_t px_y = 0; px_y < SCALE_RATIO; ++px_y) {
                for (uint32_t px_x = 0; px_x < SCALE_RATIO; ++px_x) {
                    scg_back_buffer[px_x + x * SCALE_RATIO + (px_y + y * SCALE_RATIO) * (GAMEBOY_SCREEN_WIDTH * SCALE_RATIO)] = color;
                }
            }
        }
    }

	scg_refresh();
}

static void _debug_screen_mapping(void)
{
    for (uint32_t y = 0; y < SCREEN_BUFFER_SQUARE_WIDTH; ++y) {
        for (uint32_t x = 0; x < SCREEN_BUFFER_SQUARE_WIDTH; ++x) {
            uint32_t color = screen_buffer[x + y * SCREEN_BUFFER_SQUARE_WIDTH];
            for (uint32_t px_y = 0; px_y < SCALE_RATIO; ++px_y) {
                for (uint32_t px_x = 0; px_x < SCALE_RATIO; ++px_x) {
                    scg_back_buffer[px_x + x * SCALE_RATIO + px_y + (y * SCALE_RATIO) * (SCREEN_BUFFER_SQUARE_WIDTH * SCALE_RATIO)] = color;
                }
            }
        }
    }

    scg_refresh();
}

uint32_t my_gb_screen_run(uint32_t cycles_want)
{
    if (LCDC >> 7) {
        uint32_t cycles = 0;
        while (cycles < cycles_want) {
            switch (screen_context.state_next) {
            case SCREEN_STATE_HBLANK:
                cycles += HBLANK_CYCLES;
                _h_blank(screen_context.line_current);
                ++screen_context.line_current;
                if (screen_context.line_current > 143) {
                    screen_context.state_next = SCREEN_STATE_VBLANK;
                } else {
                    screen_context.state_next = SCREEN_STATE_OAM_SEARCH;
                }
                break;
            case SCREEN_STATE_VBLANK:
                cycles += VBLANK_CYCLES;
                _v_blank(screen_context.line_current);
                ++screen_context.line_current;
                if (screen_context.line_current > 153) {
                    screen_context.state_next = SCREEN_STATE_OAM_SEARCH;
                    screen_context.line_current = 0;
#ifdef MDEBUG
                    _debug_screen_mapping();
#else
                    _screen_mapping();
#endif
                } else {
                    screen_context.state_next = SCREEN_STATE_VBLANK;
                }
                break;
            case SCREEN_STATE_OAM_SEARCH:
                cycles += OAM_SEARCH_CYCLES;
                _oam_search(screen_context.line_current);
                screen_context.state_next = SCREEN_STATE_PIXEL_TRANSFER;
                break;
            case SCREEN_STATE_PIXEL_TRANSFER:
                cycles += PIXEL_TRANSFER_CYCLES;
                _pixel_transfer(screen_context.line_current);
                screen_context.state_next = SCREEN_STATE_HBLANK;
                break;
            }
        }
        return cycles - cycles_want;
    } else {
        return 0;
    }
}

