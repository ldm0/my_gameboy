#include"cpu.h"
#include"../cart/cart.h"
#include"ram.h"         // get offset of each ram chunk
#include"input.h"       // get joypad IO registers
#include"sound.h"       // get sound IO registers
#include"screen.h"      // get screen IO registers

#include<stdio.h>	    // for debug information output
#include<stdlib.h>
#include<stdint.h>


static uint8_t *internal_ram;
static uint8_t internal_rom[256] = {
    0x31, 0xfe, 0xff, 0xaf, 0x21, 0xff, 0x9f, 0x32, 0xcb, 0x7c, 0x20, 0xfb,
    0x21, 0x26, 0xff, 0x0e, 0x11, 0x3e, 0x80, 0x32, 0xe2, 0x0c, 0x3e, 0xf3,
    0xe2, 0x32, 0x3e, 0x77, 0x77, 0x3e, 0xfc, 0xe0, 0x47, 0x11, 0x04, 0x01,
    0x21, 0x10, 0x80, 0x1a, 0xcd, 0x95, 0x00, 0xcd, 0x96, 0x00, 0x13, 0x7b,
    0xfe, 0x34, 0x20, 0xf3, 0x11, 0xd8, 0x00, 0x06, 0x08, 0x1a, 0x13, 0x22,
    0x23, 0x05, 0x20, 0xf9, 0x3e, 0x19, 0xea, 0x10, 0x99, 0x21, 0x2f, 0x99,
    0x0e, 0x0c, 0x3d, 0x28, 0x08, 0x32, 0x0d, 0x20, 0xf9, 0x2e, 0x0f, 0x18,
    0xf3, 0x67, 0x3e, 0x64, 0x57, 0xe0, 0x42, 0x3e, 0x91, 0xe0, 0x40, 0x04,
    0x1e, 0x02, 0x0e, 0x0c, 0xf0, 0x44, 0xfe, 0x90, 0x20, 0xfa, 0x0d, 0x20,
    0xf7, 0x1d, 0x20, 0xf2, 0x0e, 0x13, 0x24, 0x7c, 0x1e, 0x83, 0xfe, 0x62,
    0x28, 0x06, 0x1e, 0xc1, 0xfe, 0x64, 0x20, 0x06, 0x7b, 0xe2, 0x0c, 0x3e,
    0x87, 0xe2, 0xf0, 0x42, 0x90, 0xe0, 0x42, 0x15, 0x20, 0xd2, 0x05, 0x20,
    0x4f, 0x16, 0x20, 0x18, 0xcb, 0x4f, 0x06, 0x04, 0xc5, 0xcb, 0x11, 0x17,
    0xc1, 0xcb, 0x11, 0x17, 0x05, 0x20, 0xf5, 0x22, 0x23, 0x22, 0x23, 0xc9,
    0xce, 0xed, 0x66, 0x66, 0xcc, 0x0d, 0x00, 0x0b, 0x03, 0x73, 0x00, 0x83,
    0x00, 0x0c, 0x00, 0x0d, 0x00, 0x08, 0x11, 0x1f, 0x88, 0x89, 0x00, 0x0e,
    0xdc, 0xcc, 0x6e, 0xe6, 0xdd, 0xdd, 0xd9, 0x99, 0xbb, 0xbb, 0x67, 0x63,
    0x6e, 0x0e, 0xec, 0xcc, 0xdd, 0xdc, 0x99, 0x9f, 0xbb, 0xb9, 0x33, 0x3e,
    0x3c, 0x42, 0xb9, 0xa5, 0xb9, 0xa5, 0x42, 0x3c, 0x21, 0x04, 0x01, 0x11,
    0xa8, 0x00, 0x1a, 0x13, 0xbe, 0x20, 0xfe, 0x23, 0x7d, 0xfe, 0x34, 0x20,
    0xf5, 0x06, 0x19, 0x78, 0x86, 0x23, 0x05, 0x20, 0xfb, 0x86, 0x20, 0xfe,
    0x3e, 0x01, 0xe0, 0x50
};

static uint32_t is_stopped;

static uint16_t AF;
static uint16_t BC;
static uint16_t DE;
static uint16_t HL;
static uint16_t SP;
static uint16_t PC;

// bit mask of the interruptions
// The bigger the value, the higher the priority
#define INT_VBLANK_MASK 0x1
#define INT_LCDC_STATUS_MASK 0x2
#define INT_TIMER_OVERFLOW_MASK 0x4
#define INT_SERIAL_TRANSFER_COMPLETION_MASK 0x8
#define INT_BUTTON_PRESS_MASK 0x10

#define INT_VBLANK_ADDRESS 0x0040
#define INT_LCDC_STATUS_ADDRESS 0x0048
#define INT_TIMER_OVERFLOW_ADDRESS 0x0050
#define INT_SERIAL_TRANSFER_COMPLETION_ADDRESS 0x0058
#define INT_BUTTON_PRESS_ADDRESS 0x0060

// Currently we ensure only valid bits of the result can be 0 or 1,
// invalid bit can only be 0.
// Maybe we can do something to get rid of these duplicate defensive measure,
// to squeeze the last drop of cpu performance.
#define get_A ((uint8_t)(AF >> 8))
#define get_B ((uint8_t)(BC >> 8))
#define get_C ((uint8_t)(BC & (uint16_t)0x00FF))
#define get_D ((uint8_t)(DE >> 8))
#define get_E ((uint8_t)(DE & (uint16_t)0x00FF))
#define get_H ((uint8_t)(HL >> 8))
#define get_L ((uint8_t)(HL & (uint16_t)0x00FF))
#define get_flag_Z (((uint8_t)AF >> 7) & (uint8_t)0x1)
#define get_flag_N (((uint8_t)AF >> 6) & (uint8_t)0x1)
#define get_flag_H (((uint8_t)AF >> 5) & (uint8_t)0x1)
#define get_flag_C (((uint8_t)AF >> 4) & (uint8_t)0x1)

#define set_A(a) do { AF = (AF & (uint16_t)0x00FF) | ((uint16_t)(a) << 8); } while(0)
#define set_B(b) do { BC = (BC & (uint16_t)0x00FF) | ((uint16_t)(b) << 8); } while(0)
#define set_C(c) do { BC = (BC & (uint16_t)0xFF00) | ((uint16_t)(c) & (uint16_t)0x00FF); } while(0)
#define set_D(d) do { DE = (DE & (uint16_t)0x00FF) | ((uint16_t)(d) << 8); } while(0)
#define set_E(e) do { DE = (DE & (uint16_t)0xFF00) | ((uint16_t)(e) & (uint16_t)0x00FF); } while(0)
#define set_H(h) do { HL = (HL & (uint16_t)0x00FF) | ((uint16_t)(h) << 8); } while(0)
#define set_L(l) do { HL = (HL & (uint16_t)0xFF00) | ((uint16_t)(l) & (uint16_t)0x00FF); } while(0)
#define set_flag_Z(b) do { AF = (AF & ~((uint16_t)0x1 << 7)) | (((uint16_t)(b) & (uint16_t)0x1) << 7); } while(0)
#define set_flag_N(b) do { AF = (AF & ~((uint16_t)0x1 << 6)) | (((uint16_t)(b) & (uint16_t)0x1) << 6); } while(0)
#define set_flag_H(b) do { AF = (AF & ~((uint16_t)0x1 << 5)) | (((uint16_t)(b) & (uint16_t)0x1) << 5); } while(0)
#define set_flag_C(b) do { AF = (AF & ~((uint16_t)0x1 << 4)) | (((uint16_t)(b) & (uint16_t)0x1) << 4); } while(0)

// IO registers

/*
(ATTENTION: CURRENTLY NOT IMPLEMENTED!!!)
FF01
   Name     - SB
   Contents - Serial transfer data (R/W)

              8 Bits of data to be read/written

(ATTENTION: CURRENTLY NOT IMPLEMENTED!!!)
FF02
   Name     - SC
   Contents - SIO control  (R/W)

              Bit 7 - Transfer Start Flag
                      0: Non transfer
                      1: Start transfer

              Bit 0 - Shift Clock
                      0: External Clock (500KHz Max.)
                      1: Internal Clock (8192Hz)

               Transfer is initiated by setting the
              Transfer Start Flag. This bit may be read
              and is automatically set to 0 at the end of
              Transfer.

               Transmitting and receiving serial data is
              done simultaneously. The received data is
              automatically stored in SB.

FF04
   Name     - DIV
   Contents - Divider Register (R/W)

              This register is incremented 16384 (~16779
              on SGB) times a second. Writing any value
              sets it to $00.
FF05
   Name     - TIMA
   Contents - Timer counter (R/W)

              This timer is incremented by a clock frequency
              specified by the TAC register ($FF07). The timer
              generates an interrupt when it overflows.

FF06
   Name     - TMA
   Contents - Timer Modulo (R/W)

              When the TIMA overflows, this data will be loaded.

FF07
   Name     - TAC
   Contents - Timer Control (R/W)

              Bit 2 - Timer Stop
                      0: Stop Timer
                      1: Start Timer

              Bits 1+0 - Input Clock Select
                         00: 4.096 KHz    (~4.194 KHz SGB)
                         01: 262.144 KHz  (~268.4 KHz SGB)
                         10: 65.536 KHz   (~67.11 KHz SGB)
                         11: 16.384 KHz   (~16.78 KHz SGB)

FF0F
   Name     - IF
   Contents - Interrupt Flag (R/W)

              Bit 4: Transition from High to Low of Pin number P10-P13
              Bit 3: Serial I/O transfer complete
              Bit 2: Timer Overflow
              Bit 1: LCDC (see STAT)
              Bit 0: V-Blank

   The priority and jump address for the above 5 interrupts are:

    Interrupt        Priority        Start Address

    V-Blank             1              $0040
    LCDC Status         2              $0048 - Modes 0, 1, 2
                                               LYC=LY coincide (selectable)
    Timer Overflow      3              $0050
    Serial Transfer     4              $0058 - when transfer is complete
    Hi-Lo of P10-P13    5              $0060

    * When more than 1 interrupts occur at the same time
      only the interrupt with the highest priority can be
      acknowledged. When an interrupt is used a '0' should
      be stored in the IF register before the IE register
      is set.

FF50
   NAME     - BOOT
   Contents - Turn off boot rom (R?/W)

FFFF
   Name     - IE
   Contents - Interrupt Enable (R/W)

              Bit 4: Transition from High to Low of Pin
                     number P10-P13.
              Bit 3: Serial I/O transfer complete
              Bit 2: Timer Overflow
              Bit 1: LCDC (see STAT)
              Bit 0: V-Blank

              0: disable
              1: enable

*/
uint8_t SB;
uint8_t SC;
uint8_t DIV;
uint8_t TIMA;
uint8_t TMA;
uint8_t TAC;
uint8_t IF;
uint8_t BOOT;
uint8_t IE;

// Interrupt Master Enable flag
// This register is not mapped to gameboy address space
// 1. There are only two instruction we can add it
// 2. This is a boolean value so only the LSB is used.
uint8_t IME;


static uint8_t _address_read(uint16_t address)
{
    // return 0xFF when invalid
    uint8_t read_result;

    if (address <= 0x00FF) {
        //|------------------------------------------------------
        //| (0x0000-0x00FF) internal ROM / non-switchable ROM BANK
        //|------------------------------------------------------
        if (_address_read(0xFF50)) {	// if bootstrap have been executed
            read_result = my_gb_cart_address_read(address);
        } else {
            read_result = internal_rom[address];
        }
    } else if (address <= 0x3FFF) {
        //|------------------------------------------------------
        //| (0x00FF-0x3FFF) non-switchable ROM BANK
        //|------------------------------------------------------
        read_result = my_gb_cart_address_read(address);
    } else if (address <= 0x7FFF) {
        //|------------------------------------------------------
        //| (0x4000-0x7FFF) switchable ROM BANK
        //|------------------------------------------------------
        read_result = my_gb_cart_address_read(address);
    } else if (address <= 0x9FFF) {
        //|------------------------------------------------------
        //| (0x8000-0x9FFF)	Video RAM BANK
        //|------------------------------------------------------
        uint8_t lcd_state = _address_read(0xFF41) & 0x3;
        if (lcd_state == 3) {
            fprintf(stderr, "LCD state: %d and VRAM cannot be accessed.\n", lcd_state);
            read_result = 0xFF;
        } else {
            read_result = internal_ram[address - 0x8000 + RAM_OFFSET_VRAM];
        }
    } else if (address <= 0xBFFF) {
        //|------------------------------------------------------
        //| (0xA000-0xBFFF)	switchable Cartridge RAM BANK
        //|------------------------------------------------------
        read_result = my_gb_cart_address_read(address);
    } else if (address <= 0xDFFF) {
        //|------------------------------------------------------
        //| (0xC000-0xDFFF) Game Boy internal RAM BANK
        //|------------------------------------------------------
        read_result = internal_ram[address - 0xC000 + RAM_OFFSET_INTERNAL_RAM];
    } else if (address <= 0xFDFF) {
        //|------------------------------------------------------
        //| (0xE000-0xFDFF) echo of Game Boy internal RAM BANK
        //|------------------------------------------------------
        read_result = internal_ram[address - 0xE000 + RAM_OFFSET_INTERNAL_RAM];
    } else if (address <= 0xFE9F) {
        //|------------------------------------------------------
        //| (0xFE00-0xFE9F) Object Attribute Memory(Sprite information table)
        //|------------------------------------------------------
        read_result = internal_ram[address - 0xFE00 + RAM_OFFSET_OAM];
    } else if (address <= 0xFEFF) {
        //|------------------------------------------------------
        //| (0xFEA0-0xFEFF) Unused memory
        //|------------------------------------------------------
        fprintf(stderr, "Access to unused memory(%16X) was denied.\n", address);
        read_result = 0xFF;
    } else if (address <= 0xFF7F) {
        //|------------------------------------------------------
        //| (0xFF00-0xFF7F) Game Boy I/O registers
        //|------------------------------------------------------

        // Currently doesn't care about the register readability and writability
        // We assume cart rom writer is smart and don't touch UB of gameboy.
        // If necessary, UB behavior will be added.
        if (address == 0xFF00) {
            read_result = P1;
        } else if (address == 0xFF01) {
            read_result = SB;
        } else if (address == 0xFF02) {
            read_result = SC;
        } else if (address == 0xFF03) {
            read_result = DIV;
        } else if (address == 0xFF04) {
            read_result = TIMA;
        } else if (address == 0xFF05) {
            read_result = TMA;
        } else if (address == 0xFF06) {
            read_result = TAC;
        } else if (address == 0xFF0F) {
            read_result = IF;
        } else if (address == 0xFF10) {
            read_result = NR10;
        } else if (address == 0xFF11) {
            read_result = NR11;
        } else if (address == 0xFF12) {
            read_result = NR12;
        } else if (address == 0xFF13) {
            read_result = NR13;
        } else if (address == 0xFF14) {
            read_result = NR14;
        } else if (address == 0xFF16) {
            read_result = NR21;
        } else if (address == 0xFF17) {
            read_result = NR22;
        } else if (address == 0xFF18) {
            read_result = NR23;
        } else if (address == 0xFF19) {
            read_result = NR24;
        } else if (address == 0xFF1A) {
            read_result = NR30;
        } else if (address == 0xFF1B) {
            read_result = NR31;
        } else if (address == 0xFF1C) {
            read_result = NR32;
        } else if (address == 0xFF1D) {
            read_result = NR33;
        } else if (address == 0xFF1E) {
            read_result = NR34;
        } else if (address == 0xFF20) {
            read_result = NR41;
        } else if (address == 0xFF21) {
            read_result = NR42;
        } else if (address == 0xFF22) {
            read_result = NR43;
        } else if (address == 0xFF23) {
            read_result = NR44;
        } else if (address == 0xFF24) {
            read_result = NR50;
        } else if (address == 0xFF25) {
            read_result = NR51;
        } else if (address == 0xFF26) {
            read_result = NR52;
        } else if (address >= 0xFF30 && address < 0xFF40) {
            read_result = W[address - 0xFF30];
        } else if (address == 0xFF40) {
            read_result = LCDC;
        } else if (address == 0xFF41) {
            read_result = STAT;
        } else if (address == 0xFF42) {
            read_result = SCY;
        } else if (address == 0xFF43) {
            read_result = SCX;
        } else if (address == 0xFF44) {
            read_result = LY;
        } else if (address == 0xFF45) {
            read_result = LYC;
        } else if (address == 0xFF46) {
            read_result = DMA;
        } else if (address == 0xFF47) {
            read_result = BGP;
        } else if (address == 0xFF48) {
            read_result = OBP0;
        } else if (address == 0xFF49) {
            read_result = OBP1;
        } else if (address == 0xFF4A) {
            read_result = WY;
        } else if (address == 0xFF4B) {
            read_result = WX;
        } else if (address == 0xFF50) {
            read_result = BOOT;
        } else {
            fprintf(stderr, "Cart want to touch the unused IO register area(%16X).\n", address);
            read_result = 0xFF;
        }
    } else if (address <= 0xFFFE) {
        //|------------------------------------------------------
        //| (0xFF80-0xFFFE) Internal RAM(Heap RAM) in Game Boy(often used as stack)
        //|------------------------------------------------------
        read_result = internal_ram[address - 0xFF80 + RAM_OFFSET_HRAM];
    } else { //if (address <= 0xFFFF) {
        //|------------------------------------------------------
        //| (0xFFFF-0xFFFF) Interrupt enable flag
        //|------------------------------------------------------
        read_result = IE;
    }
    return read_result;
}

static uint16_t _address_read_16(uint16_t address)
{
    uint16_t data = (uint16_t)_address_read(address);
    data |= ((uint16_t)_address_read(address + 1)) << 8;
    return data;
}

static void _address_write(uint16_t address, uint8_t data)
{
    if (address <= 0x7FFF) {
        //|------------------------------------------------------
        //| (0x0000-0x3FFF) non-switchable ROM BANK
        //|------------------------------------------------------
        //|------------------------------------------------------
        //| (0x4000-0x7FFF) switchable ROM BANK
        //|------------------------------------------------------
        fprintf(stderr, "Failed to write to ROM area(%16X).\n", address);
    } else if (address <= 0x9FFF) {
        //|------------------------------------------------------
        //| (0x8000-0x9FFF)	Video RAM BANK
        //|------------------------------------------------------
        internal_ram[address - 0x8000 + RAM_OFFSET_VRAM] = data;
    } else if (address <= 0xBFFF) {
        //|------------------------------------------------------
        //| (0xA000-0xBFFF)	switchable Cartridge RAM BANK
        //|------------------------------------------------------
        my_gb_cart_address_write(address, data);
    } else if (address <= 0xDFFF) {
        //|------------------------------------------------------
        //| (0xC000-0xDFFF) Game Boy internal RAM BANK
        //|------------------------------------------------------
        internal_ram[address - 0xC000 + RAM_OFFSET_INTERNAL_RAM] = data;
    } else if (address <= 0xFDFF) {
        //|------------------------------------------------------
        //| (0xE000-0xFDFF) echo of Game Boy internal RAM BANK
        //|------------------------------------------------------
        internal_ram[address - 0xE000 + RAM_OFFSET_INTERNAL_RAM] = data;
    } else if (address <= 0xFE9F) {
        //|------------------------------------------------------
        //| (0xFE00-0xFE9F) Object Attribute Memory(Sprite information table)
        //|------------------------------------------------------
        internal_ram[address - 0xFE00 + RAM_OFFSET_OAM] = data;
    } else if (address <= 0xFEFF) {
        //|------------------------------------------------------
        //| (0xFEA0-0xFEFF) Unused memory
        //|------------------------------------------------------
        fprintf(stderr, "Failed to write to unused memory(%16X).\n", address);
    } else if (address <= 0xFF7F) {
        //|------------------------------------------------------
        //| (0xFF00-0xFF7F) Game Boy I/O registers
        //|------------------------------------------------------
        // Implementations needed.
        // Should refer to the documents for each I/O registers to check if it is mutable
        // and also implement the UB if needed
        if (address == 0xFF00) {
            P1 = data;
        } else if (address == 0xFF01) {
            SB = data;
        } else if (address == 0xFF02) {
            SC = data;
        } else if (address == 0xFF03) {
            DIV = data;
        } else if (address == 0xFF04) {
            TIMA = data;
        } else if (address == 0xFF05) {
            TMA = data;
        } else if (address == 0xFF06) {
            TAC = data;
        } else if (address == 0xFF0F) {
            IF = data;
        } else if (address == 0xFF10) {
            NR10 = data;
        } else if (address == 0xFF11) {
            NR11 = data;
        } else if (address == 0xFF12) {
            NR12 = data;
        } else if (address == 0xFF13) {
            NR13 = data;
        } else if (address == 0xFF14) {
            NR14 = data;
        } else if (address == 0xFF16) {
            NR21 = data;
        } else if (address == 0xFF17) {
            NR22 = data;
        } else if (address == 0xFF18) {
            NR23 = data;
        } else if (address == 0xFF19) {
            NR24 = data;
        } else if (address == 0xFF1A) {
            NR30 = data;
        } else if (address == 0xFF1B) {
            NR31 = data;
        } else if (address == 0xFF1C) {
            NR32 = data;
        } else if (address == 0xFF1D) {
            NR33 = data;
        } else if (address == 0xFF1E) {
            NR34 = data;
        } else if (address == 0xFF20) {
            NR41 = data;
        } else if (address == 0xFF21) {
            NR42 = data;
        } else if (address == 0xFF22) {
            NR43 = data;
        } else if (address == 0xFF23) {
            NR44 = data;
        } else if (address == 0xFF24) {
            NR50 = data;
        } else if (address == 0xFF25) {
            NR51 = data;
        } else if (address == 0xFF26) {
            NR52 = data;
        } else if (address >= 0xFF30 && address < 0xFF40) {
            W[address - 0xFF30] = data;
        } else if (address == 0xFF40) {
            LCDC = data;
        } else if (address == 0xFF41) {
            STAT = data;
        } else if (address == 0xFF42) {
            SCY = data;
        } else if (address == 0xFF43) {
            SCX = data;
        } else if (address == 0xFF44) {
            LY = data;
        } else if (address == 0xFF45) {
            LYC = data;
        } else if (address == 0xFF46) {
            DMA = data;
        } else if (address == 0xFF47) {
            BGP = data;
        } else if (address == 0xFF48) {
            OBP0 = data;
        } else if (address == 0xFF49) {
            OBP1 = data;
        } else if (address == 0xFF4A) {
            WY = data;
        } else if (address == 0xFF4B) {
            WX = data;
        } else if (address == 0xFF50) {
            BOOT = data;
        } else {
            fprintf(stderr, "Cart want to touch the unused IO register area(%16X).\n", address);
        }
    } else if (address <= 0xFFFE) {
        //|------------------------------------------------------
        //| (0xFF80-0xFFFE) Internal RAM in Game Boy(often used as stack)
        //|------------------------------------------------------
        internal_ram[address - 0xFF80 + RAM_OFFSET_HRAM] = data;
    } else /*if (address <= 0xFFFF)*/ {
        //|------------------------------------------------------
        //| (0xFFFF-0xFFFF) Interrupt enable flag
        //|------------------------------------------------------
        IE = data;
    }
}

static void _address_write_16(uint16_t address, uint16_t data_16)
{
    // consider situation that write a word across two memory bank into consideration
    // so split data_16 to write
    _address_write(address, (uint8_t)data_16);
    _address_write(address + 1, (uint8_t)(data_16 >> 8));
}

static int _RLC(int i)
{
    int cycles;

    set_flag_N(0);
    set_flag_H(0);

    uint8_t data_8;
    switch (i) {
    case 0:		// RLC B
        cycles = 2;
        set_flag_C(get_B >> 7);
        set_B(get_B << 1);
        set_flag_Z(!get_B);
        break;
    case 1:		// RLC C
        cycles = 2;
        set_flag_C(get_C >> 7);
        set_C(get_C << 1);
        set_flag_Z(!get_C);
        break;
    case 2:		// RLC D
        cycles = 2;
        set_flag_C(get_D >> 7);
        set_D(get_D << 1);
        set_flag_Z(!get_D);
        break;
    case 3:		// RLC E
        cycles = 2;
        set_flag_C(get_E >> 7);
        set_E(get_E << 1);
        set_flag_Z(!get_E);
        break;
    case 4:		// RLC H
        cycles = 2;
        set_flag_C(get_H >> 7);
        set_H(get_H << 1);
        set_flag_Z(!get_H);
        break;
    case 5:		// RLC L
        cycles = 2;
        set_flag_C(get_L >> 7);
        set_L(get_L << 1);
        set_flag_Z(!get_L);
        break;
    case 6:		// RLC (HL)
        cycles = 4;
        data_8 = _address_read(HL);
        set_flag_C(data_8 >> 7);
        data_8 <<= 1;
        _address_write(HL, data_8);
        set_flag_Z(!data_8);
        break;
    case 7:		// RLC A
        cycles = 2;
        set_flag_C(get_A >> 7);
        set_A(get_A << 1);
        set_flag_Z(!get_A);
        break;
    default:
        cycles = 0xFFFF;
        fprintf(stderr, "Illegal parameter for RLC.\n");
        break;
    }
    return cycles;
}

static int _RRC(int i)
{
    int cycles;

    set_flag_N(0);
    set_flag_H(0);

    uint8_t data_8;
    switch (i) {
    case 0:		// RRC B
        cycles = 2;
        set_flag_C(get_B & (uint8_t)0x01);
        set_B(get_B >> 1);
        set_flag_Z(!get_B);
        break;
    case 1:		// RRC C
        cycles = 2;
        set_flag_C(get_C & (uint8_t)0x01);
        set_C(get_C >> 1);
        set_flag_Z(!get_C);
        break;
    case 2:		// RRC D
        cycles = 2;
        set_flag_C(get_D & (uint8_t)0x01);
        set_D(get_D >> 1);
        set_flag_Z(!get_D);
        break;
    case 3:		// RRC E
        cycles = 2;
        set_flag_C(get_E & (uint8_t)0x01);
        set_E(get_E >> 1);
        set_flag_Z(!get_E);
        break;
    case 4:		// RRC H
        cycles = 2;
        set_flag_C(get_H & (uint8_t)0x01);
        set_H(get_H >> 1);
        set_flag_Z(!get_H);
        break;
    case 5:		// RRC L
        cycles = 2;
        set_flag_C(get_L & (uint8_t)0x01);
        set_L(get_L >> 1);
        set_flag_Z(!get_L);
        break;
    case 6:		// RRC (HL)
        cycles = 4;
        data_8 = _address_read(HL);
        set_flag_C(data_8 & (uint8_t)0x01);
        data_8 >>= 1;
        _address_write(HL, data_8);
        set_flag_Z(!data_8);
        break;
    case 7:		// RRC A
        cycles = 2;
        set_flag_C(get_A & (uint8_t)0x01);
        set_A(get_A >> 1);
        set_flag_Z(!get_A);
        break;
    default:
        cycles = 0xFFFF;
        fprintf(stderr, "Illegal parameter for RRC.\n");
        break;
    }
    return cycles;
}

static int _RL(int i)
{
    int cycles;

    set_flag_N(0);
    set_flag_H(0);

    uint8_t data_8;
    switch (i) {
    case 0:		// RL B
        cycles = 2;
        data_8 = (get_B << 1) | get_flag_C;
        set_flag_C(get_B >> 7);
        set_B(data_8);
        set_flag_Z(!data_8);
        break;
    case 1:		// RL C
        cycles = 2;
        data_8 = (get_C << 1) | get_flag_C;
        set_flag_C(get_C >> 7);
        set_C(data_8);
        set_flag_Z(!data_8);
        break;
    case 2:		// RL D
        cycles = 2;
        data_8 = (get_D << 1) | get_flag_C;
        set_flag_C(get_D >> 7);
        set_D(data_8);
        set_flag_Z(!data_8);
        break;
    case 3:		// RL E
        cycles = 2;
        data_8 = (get_E << 1) | get_flag_C;
        set_flag_C(get_E >> 7);
        set_E(data_8);
        set_flag_Z(!data_8);
        break;
    case 4:		// RL H
        cycles = 2;
        data_8 = (get_H << 1) | get_flag_C;
        set_flag_C(get_H >> 7);
        set_H(data_8);
        set_flag_Z(!data_8);
        break;
    case 5:		// RL L
        cycles = 2;
        data_8 = (get_L << 1) | get_flag_C;
        set_flag_C(get_L >> 7);
        set_L(data_8);
        set_flag_Z(!data_8);
        break;
    case 6:		// RL (HL)
        cycles = 4;
        data_8 = _address_read(HL) | get_flag_C;
        set_flag_C(_address_read(HL) >> 7);
        _address_write(HL, data_8);
        set_flag_Z(!data_8);
        break;
    case 7:		// RL A
        cycles = 2;
        data_8 = (get_A << 1) | get_flag_C;
        set_flag_C(get_A >> 7);
        set_A(data_8);
        set_flag_Z(!data_8);
        break;
    default:
        cycles = 0xFFFF;
        fprintf(stderr, "Illegal parameter for RL.\n");
        break;
    }
    return cycles;
}

static int _RR(int i)
{
    int cycles;

    set_flag_N(0);
    set_flag_H(0);

    uint8_t data_8;
    switch (i) {
    case 0:		// RR B
        cycles = 2;
        data_8 = (get_B >> 1) | (get_flag_C << 7);
        set_flag_C(get_B & (uint8_t)0x01);
        set_B(data_8);
        set_flag_Z(!data_8);
        break;
    case 1:		// RR C
        cycles = 2;
        data_8 = (get_C >> 1) | (get_flag_C << 7);
        set_flag_C(get_C & (uint8_t)0x01);
        set_C(data_8);
        set_flag_Z(!data_8);
        break;
    case 2:		// RR D
        cycles = 2;
        data_8 = (get_D >> 1) | (get_flag_C << 7);
        set_flag_C(get_D & (uint8_t)0x01);
        set_D(data_8);
        set_flag_Z(!data_8);
        break;
    case 3:		// RR E
        cycles = 2;
        data_8 = (get_E >> 1) | (get_flag_C << 7);
        set_flag_C(get_E & (uint8_t)0x01);
        set_E(data_8);
        set_flag_Z(!data_8);
        break;
    case 4:		// RR H
        cycles = 2;
        data_8 = (get_H >> 1) | (get_flag_C << 7);
        set_flag_C(get_H & (uint8_t)0x01);
        set_H(data_8);
        set_flag_Z(!data_8);
        break;
    case 5:		// RR L
        cycles = 2;
        data_8 = (get_L >> 1) | (get_flag_C << 7);
        set_flag_C(get_L & (uint8_t)0x01);
        set_L(data_8);
        set_flag_Z(!data_8);
        break;
    case 6:		// RR (HL)
        cycles = 4;
        data_8 = (_address_read(HL) >> 1) | (get_flag_C << 7);
        set_flag_C(_address_read(HL) & (uint8_t)0x01);
        _address_write(HL, data_8);
        set_flag_Z(!data_8);
        break;
    case 7:		// RR A
        cycles = 2;
        data_8 = (get_A >> 1) | (get_flag_C << 7);
        set_flag_C(get_A & (uint8_t)0x01);
        set_A(data_8);
        set_flag_Z(!data_8);
        break;
    default:
        cycles = 0xFFFF;
        fprintf(stderr, "Illegal parameter for RR.\n");
        break;
    }
    return cycles;
}

static int _SLA(int i)
{
    int cycles;

    set_flag_N(0);
    set_flag_H(0);

    uint8_t data_8;
    switch (i) {
    case 0:		// SLA A
        cycles = 2;
        set_flag_C(get_A >> 7);
        set_A(get_A << 1);
        set_flag_Z(!get_A);
        break;
    case 1:		// SLA B
        cycles = 2;
        set_flag_C(get_B >> 7);
        set_B(get_B << 1);
        set_flag_Z(!get_B);
        break;
    case 2:		// SLA C
        cycles = 2;
        set_flag_C(get_C >> 7);
        set_C(get_C << 1);
        set_flag_Z(!get_C);
        break;
    case 3:		// SLA D
        cycles = 2;
        set_flag_C(get_D >> 7);
        set_D(get_D << 1);
        set_flag_Z(!get_D);
        break;
    case 4:		// SLA H
        cycles = 2;
        set_flag_C(get_H >> 7);
        set_H(get_H << 1);
        set_flag_Z(!get_H);
        break;
    case 5:		// SLA L
        cycles = 2;
        set_flag_C(get_L >> 7);
        set_L(get_L << 1);
        set_flag_Z(!get_L);
        break;
    case 6:		// SLA (HL)
        cycles = 4;
        data_8 = _address_read(HL);
        set_flag_C(data_8 >> 7);
        data_8 <<= 1;
        _address_write(HL, data_8);
        set_flag_Z(!data_8);
        break;
    case 7:		// SLA A
        cycles = 2;
        set_flag_C(get_A >> 7);
        set_A(get_A << 1);
        set_flag_Z(!get_A);
        break;
    default:
        cycles = 0xFFFF;
        fprintf(stderr, "Illegal instruction in SLA");
        break;
    }
    return cycles;
}

static int _SRA(int i)
{
    int cycles;

    set_flag_N(0);
    set_flag_H(0);

    uint8_t data_8;
    switch (i) {
    case 0:		// SRA B
        cycles = 2;
        set_flag_C(get_B & 0x1);
        set_B((get_B >> 1) | (get_B & 0x80));
        set_flag_Z(!get_B);
        break;
    case 1:		// SRA C
        cycles = 2;
        set_flag_C(get_C & 0x1);
        set_C((get_C >> 1) | (get_C & 0x80));
        set_flag_Z(!get_C);
        break;
    case 2:		// SRA D
        cycles = 2;
        set_flag_C(get_D & 0x1);
        set_D((get_D >> 1) | (get_D & 0x80));
        set_flag_Z(!get_D);
        break;
    case 3:		// SRA E
        cycles = 2;
        set_flag_C(get_E & 0x1);
        set_A((get_E >> 1) | (get_E & 0x80));
        set_flag_Z(!get_E);
        break;
    case 4:		// SRA H
        cycles = 2;
        set_flag_C(get_H & 0x1);
        set_H((get_H >> 1) | (get_H & 0x80));
        set_flag_Z(!get_H);
        break;
    case 5:		// SRA L
        cycles = 2;
        set_flag_C(get_L & 0x1);
        set_L((get_L >> 1) | (get_L & 0x80));
        set_flag_Z(!get_L);
        break;
    case 6:		// SRA (HL)
        cycles = 4;
        data_8 = _address_read(HL);
        set_flag_C(data_8 & 0x1);
        data_8 = (data_8 >> 1) | (data_8 & 0x80);
        _address_write(HL, data_8);
        set_flag_Z(!data_8);
        break;
    case 7:		// SRA A
        cycles = 2;
        set_flag_C(get_A & 0x1);
        set_A((get_A >> 1) | (get_A & 0x80));
        set_flag_Z(!get_A);
        break;
    default:
        cycles = 0xFFFF;
        fprintf(stderr, "Illegal instruction in SRA");
        break;
    }
    return cycles;
}

static int _SWAP(int i)
{
    int cycles;

    set_flag_N(0);
    set_flag_H(0);
    set_flag_C(0);

    uint8_t data_8;
    switch (i) {
    case 0:		// SWAP B
        cycles = 2;
        set_B(((get_B & 0x0F) << 4) | (get_B >> 4));
        set_flag_Z(!get_B);
        break;
    case 1:		// SWAP C
        cycles = 2;
        set_C(((get_C & 0x0F) << 4) | (get_C >> 4));
        set_flag_Z(!get_C);
        break;
    case 2:		// SWAP D
        cycles = 2;
        set_D(((get_D & 0x0F) << 4) | (get_D >> 4));
        set_flag_Z(!get_D);
        break;
    case 3:		// SWAP E
        cycles = 2;
        set_E(((get_E & 0x0F) << 4) | (get_E >> 4));
        set_flag_Z(!get_E);
        break;
    case 4:		// SWAP H
        cycles = 2;
        set_H(((get_H & 0x0F) << 4) | (get_H >> 4));
        set_flag_Z(!get_H);
        break;
    case 5:		// SWAP L
        cycles = 2;
        set_L(((get_L & 0x0F) << 4) | (get_L >> 4));
        set_flag_Z(!get_L);
        break;
    case 6:		// SWAP (HL)
        cycles = 4;
        data_8 = _address_read(HL);
        data_8 = ((data_8 & 0x0F) << 4) | (data_8 >> 4);
        _address_write(HL, data_8);
        set_flag_Z(!data_8);
        break;
    case 7:		// SWAP A
        cycles = 2;
        set_A(((get_A & 0x0F) << 4) | (get_A >> 4));
        set_flag_Z(!get_A);
        break;
    default:
        cycles = 0xFFFF;
        fprintf(stderr, "Illegal SWAP command");
        break;
    }
    return cycles;
}

static int _SRL(int i)
{
    int cycles;

    set_flag_N(0);
    set_flag_H(0);

    uint8_t data_8;
    switch (i) {
    case 0:		// SRL B
        cycles = 2;
        set_flag_C(get_B & 0x1);
        set_B(get_B >> 1);
        set_flag_Z(!get_B);
        break;
    case 1:		// SRL C
        cycles = 2;
        set_flag_C(get_C & 0x1);
        set_C(get_C >> 1);
        set_flag_Z(!get_C);
        break;
    case 2:		// SRL D
        cycles = 2;
        set_flag_C(get_D & 0x1);
        set_D(get_D >> 1);
        set_flag_Z(!get_D);
        break;
    case 3:		// SRL E
        cycles = 2;
        set_flag_C(get_E & 0x1);
        set_A(get_E >> 1);
        set_flag_Z(!get_E);
        break;
    case 4:		// SRL H
        cycles = 2;
        set_flag_C(get_H & 0x1);
        set_H(get_H >> 1);
        set_flag_Z(!get_H);
        break;
    case 5:		// SRL L
        cycles = 2;
        set_flag_C(get_L & 0x1);
        set_L(get_L >> 1);
        set_flag_Z(!get_L);
        break;
    case 6:		// SRL (HL)
        cycles = 4;
        data_8 = _address_read(HL);
        set_flag_C(data_8 & 0x1);
        data_8 = data_8 >> 1;
        _address_write(HL, data_8);
        set_flag_Z(!data_8);
        break;
    case 7:		// SRL A
        cycles = 2;
        set_flag_C(get_A & 0x1);
        set_A(get_A >> 1);
        set_flag_Z(!get_A);
        break;
    default:
        cycles = 0xFFFF;
        fprintf(stderr, "Illegal instruction in SRL");
        break;
    }
    return cycles;
}

static int _BIT(int bit, int i)
{
    int cycles;

    set_flag_N(0);
    set_flag_H(0);

    uint8_t data_8;
    switch (i) {
    case 0:		// BIT bit, B
        cycles = 2;
        set_flag_Z(!((get_B >> bit) & 0x1));
        break;
    case 1:		// BIT bit, C
        cycles = 2;
        set_flag_Z(!((get_C >> bit) & 0x1));
        break;
    case 2:		// BIT bit, D
        cycles = 2;
        set_flag_Z(!((get_D >> bit) & 0x1));
        break;
    case 3:		// BIT bit, E
        cycles = 2;
        set_flag_Z(!((get_E >> bit) & 0x1));
        break;
    case 4:		// BIT bit, H
        cycles = 2;
        set_flag_Z(!((get_H >> bit) & 0x1));
        break;
    case 5:		// BIT bit, L
        cycles = 2;
        set_flag_Z(!((get_L >> bit) & 0x1));
        break;
    case 6:		// BIT bit, (HL)
        cycles = 4;
        data_8 = _address_read(HL);
        set_flag_Z(!((data_8 >> bit) & 0x1));
        break;
    case 7:		// BIT bit, A
        cycles = 2;
        set_flag_Z(!((get_A >> bit) & 0x1));
        break;
    default:
        cycles = 0xFFFF;
        fprintf(stderr, "Illegal instruction in BIT.\n");
    }
    return cycles;
}

static int _RES(int bit, int i)
{
    int cycles;

    uint8_t data_8;
    switch (i) {
    case 0:		// RES bit, B
        cycles = 2;
        set_B(get_B & ~(0x1 << bit));
        break;
    case 1:		// RES bit, C
        cycles = 2;
        set_C(get_C & ~(0x1 << bit));
        break;
    case 2:		// RES bit, D
        cycles = 2;
        set_D(get_D & ~(0x1 << bit));
        break;
    case 3:		// RES bit, E
        cycles = 2;
        set_E(get_E & ~(0x1 << bit));
        break;
    case 4:		// RES bit, H
        cycles = 2;
        set_H(get_H & ~(0x1 << bit));
        break;
    case 5:		// RES bit, L
        cycles = 2;
        set_L(get_L & ~(0x1 << bit));
        break;
    case 6:		// RES bit, (HL)
        cycles = 4;
        data_8 = _address_read(HL) & ~(0x1 << bit);
        _address_write(HL, data_8);
        break;
    case 7:		// RES bit, A
        cycles = 2;
        set_A(get_A & ~(0x1 << bit));
        break;
    default:
        cycles = 0xFFFF;
        fprintf(stderr, "Illegal instruction in RES");
        break;
    }
    return cycles;
}

static int _SET(int bit, int i)
{
    int cycles;

    uint8_t data_8;
    switch (i) {
    case 0:		// SET bit, B
        cycles = 2;
        set_B(get_B | (0x1 << bit));
        break;
    case 1:		// SET bit, C
        cycles = 2;
        set_C(get_C | (0x1 << bit));
        break;
    case 2:		// SET bit, D
        cycles = 2;
        set_D(get_D | (0x1 << bit));
        break;
    case 3:		// SET bit, E
        cycles = 2;
        set_E(get_E | (0x1 << bit));
        break;
    case 4:		// SET bit, H
        cycles = 2;
        set_H(get_H | (0x1 << bit));
        break;
    case 5:		// SET bit, L
        cycles = 2;
        set_L(get_L | (0x1 << bit));
        break;
    case 6:		// SET bit, (HL)
        cycles = 4;
        data_8 = _address_read(HL) | (0x1 << bit);
        _address_write(HL, data_8);
        break;
    case 7:		// SET bit, A
        cycles = 2;
        set_A(get_A | (0x1 << bit));
        break;
    default:
        cycles = 0xFFFF;
        fprintf(stderr, "Illegal tail in SET.\n");
        break;
    }
    return cycles;
}

static uint32_t _cpu_interruption(void)
{
    // If no interrupting, do nothing.
    // If interrupting, do interruption.
    /*
     * 1. If IME flag is set and IE corresponding to current IF is set,
          following 3 steps are executed:
     * 2. Reset IME to disable all interruptions
     * 3. Push current PC to the stack
     * 4. Jump to address corresponding to current interruption type
     */

    uint32_t cycles;

    uint8_t _int = (IE & IF);
    if (IME && _int) {
        IME = 0;    // reset IME
        IF = 0;     // reset IF

        // push PC to the stack
        SP -= 2;
        _address_write_16(SP, PC);

        switch (_int) {
        case INT_VBLANK_MASK:
            PC = INT_VBLANK_ADDRESS;
            printf("VBLANK interruption.\n");
            break;
        case INT_LCDC_STATUS_MASK:
            printf("LCDC status interruption.\n");
            PC = INT_LCDC_STATUS_ADDRESS;
            break;
        case INT_TIMER_OVERFLOW_MASK:
            printf("Timer overflow interruption.\n");
            PC = INT_TIMER_OVERFLOW_ADDRESS;
            break;
        case INT_SERIAL_TRANSFER_COMPLETION_MASK:
            printf("Serial transfer interruption.\n");
            PC = INT_SERIAL_TRANSFER_COMPLETION_ADDRESS;
            break;
        case INT_BUTTON_PRESS_MASK:
            printf("Button press interruption.\n");
            PC = INT_BUTTON_PRESS_ADDRESS;
            break;
        }
        // currently we assume interruption doesn't consume any cycles.
        // I know it's not realistic, but I also don't know how many cycles an interruption will consume
        // and I don't want to put a wrong number here.
        cycles = 0;
    }
    cycles = 0;

    return cycles;
}

static uint32_t _cpu_execute_cb(void)
{
    uint32_t cycles;
    uint8_t command = _address_read(PC);
    uint8_t low = command % 8;
    command >>= 3;
    ++PC;
    switch (command) {
    case 0x00:		// RLC
        cycles = _RLC(low);
        break;
    case 0x01:		// RRC
        cycles = _RRC(low);
        break;
    case 0x02:		// RL
        cycles = _RL(low);
        break;
    case 0x03:		// RR
        cycles = _RR(low);
        break;
    case 0x04:		// SLA
        cycles = _SLA(low);
        break;
    case 0x05:		// SRA
        cycles = _SRA(low);
        break;
    case 0x06:		// SWAP
        cycles = _SWAP(low);
        break;
    case 0x07:		// SRL
        cycles = _SRL(low);
        break;
    case 0x08:		// BIT 0
        cycles = _BIT(0, low);
        break;
    case 0x09:		// BIT 1
        cycles = _BIT(1, low);
        break;
    case 0x0A:		// BIT 2
        cycles = _BIT(2, low);
        break;
    case 0x0B:		// BIT 3
        cycles = _BIT(3, low);
        break;
    case 0x0C:		// BIT 4
        cycles = _BIT(4, low);
        break;
    case 0x0D:		// BIT 5
        cycles = _BIT(5, low);
        break;
    case 0x0E:		// BIT 6
        cycles = _BIT(6, low);
        break;
    case 0x0F:		// BIT 8
        cycles = _BIT(7, low);
        break;
    case 0x10:		// RES 0
        cycles = _RES(0, low);
        break;
    case 0x11:		// RES 1
        cycles = _RES(1, low);
        break;
    case 0x12:		// RES 2
        cycles = _RES(2, low);
        break;
    case 0x13:		// RES 3
        cycles = _RES(3, low);
        break;
    case 0x14:		// RES 4
        cycles = _RES(4, low);
        break;
    case 0x15:		// RES 5
        cycles = _RES(5, low);
        break;
    case 0x16:		// RES 6
        cycles = _RES(6, low);
        break;
    case 0x17:		// RES 7
        cycles = _RES(7, low);
        break;
    case 0x18:		// SET 0
        cycles = _SET(0, low);
        break;
    case 0x19:		// SET 1
        cycles = _SET(1, low);
        break;
    case 0x1A:		// SET 2
        cycles = _SET(2, low);
        break;
    case 0x1B:		// SET 3
        cycles = _SET(3, low);
        break;
    case 0x1C:		// SET 4
        cycles = _SET(4, low);
        break;
    case 0x1D:		// SET 5
        cycles = _SET(5, low);
        break;
    case 0x1E:		// SET 6
        cycles = _SET(6, low);
        break;
    case 0x1F:		// SET 7
        cycles = _SET(7, low);
        break;
    default:
        cycles = 0xFFFF;
        fprintf(stderr, "Illegal prefix CB instruction.\n");
        break;
    }
    return cycles;
}

static uint32_t _cpu_execute(void)
{
    uint32_t cycles;	// number of CPU cycles

    uint8_t command = _address_read(PC);
    if (PC == 0xE6) {
        //printf("Logo comparing...\n");
    } else if (PC == 0x34) {
        printf("Logo data half loaded.\n");
    } else if (PC == 0x40) {
        printf("Logo data fully loaded.\n");
    } else if (PC == 0x6A) {
        //printf("Another frame.\n");
    } else if (PC == 0xF1) {
        printf("Logo is valid!\n");
    } else if (PC == 0x80) {
        printf("Play Init Sound.\n");
    } else if (PC == 0x100) {
        printf("Run through boot rom.\n");
        if (!(
            AF == 0x01B0 &&
            BC == 0x0013 &&
            DE == 0x00D8 &&
            HL == 0x014D &&
            SP == 0xFFFE &&
            PC == 0x0100)) {
            fprintf(stderr, "Boot ROM running badly!!!\n");
        } else {
            printf("Boot rom running correctly.\n");
        }
    }
    ++PC;

    // tmp values used when translating instructions
    uint8_t data_8;
    uint16_t data_16;
    uint32_t data_32;
    switch (command) {
    case 0x00:		// NOP
        cycles = 1;
        break;
    case 0x01:		// LD BC, d16
        cycles = 3;
        data_16 = _address_read_16(PC);
        PC += 2;
        break;
    case 0x02:		// LD (BC), A
        cycles = 2;
        _address_write(BC, get_A);
        break;
    case 0x03:		// INC BC
        cycles = 2;
        ++BC;
        break;
    case 0x04:		// INC B
        cycles = 1;
        set_flag_N(0);
        data_8 = get_B + 1;
        set_B(data_8);
        set_flag_H(!(data_8 & 0xF));
        set_flag_Z(!data_8);
        break;
    case 0x05:		// DEC B
        cycles = 1;
        set_flag_N(1);
        data_8 = get_B;
        set_flag_H(!(data_8 & 0xF));
        --data_8;
        set_B(data_8);
        set_flag_Z(!data_8);
        break;
    case 0x06:		// LD B, d8
        cycles = 2;
        data_8 = _address_read(PC);
        ++PC;
        set_B(data_8);
        break;
    case 0x07:		// RLCA
        cycles = 1;
        set_flag_Z(0);
        set_flag_N(0);
        set_flag_H(0);
        data_8 = (get_A & 0x80) >> 7;
        set_A((get_A << 1) | data_8);
        set_flag_C(data_8);
        break;
    case 0x08:		// LD (a16), SP
        cycles = 5;
        data_16 = _address_read_16(PC);
        PC += 2;
        _address_write_16(data_16, SP);
        break;
    case 0x09:		// ADD HL, BC
        cycles = 2;
        set_flag_N(0);
        data_32 = (uint32_t)HL + (uint32_t)BC;
        set_flag_H((data_32 & (uint32_t)0xFFF) < ((uint32_t)HL & (uint32_t)0xFFF));
        HL = (uint16_t)data_32;
        set_flag_C(data_32 >> 16);
        break;
    case 0x0A:		// LD A, (BC)
        cycles = 2;
        set_A(_address_read(BC));
        break;
    case 0x0B:		// DEC BC
        cycles = 2;
        --BC;
        break;
    case 0x0C:		// INC C
        cycles = 1;
        set_flag_N(0);
        data_8 = get_C + 1;
        set_C(data_8);
        set_flag_H(!(data_8 & 0xF));
        set_flag_Z(!data_8);
        break;
    case 0x0D:		// DEC C
        cycles = 1;
        set_flag_N(1);
        data_8 = get_C;
        set_flag_H(!(data_8 & 0xF));
        --data_8;
        set_C(data_8);
        set_flag_Z(!data_8);
        break;
    case 0x0E:		// LD C, d8
        cycles = 2;
        data_8 = _address_read(PC);
        ++PC;
        set_C(data_8);
        break;
    case 0x0F:		// RRCA
        cycles = 1;
        set_flag_Z(0);
        set_flag_N(0);
        set_flag_H(0);
        data_8 = get_A & 0x1;
        set_A((get_A >> 1) | (data_8 << 7));
        set_flag_C(data_8);
        break;
    case 0x10:		// STOP 0
        cycles = 0xFFFF;
        is_stopped = 1;
        if (_address_read(PC) == 0x00) {
            fprintf(stdout, "Entering stop mode. Halt CPU & LCD display until button pressed.\n");
        } else {
            fprintf(stdout, "Entering corrupted stop mode.\n");
        }
        ++PC;
        break;
    case 0x11:		// LD DE, d16
        cycles = 3;
        data_16 = _address_read_16(PC);
        PC += 2;
        DE = data_16;
        break;
    case 0x12:		// LD (DE), A
        cycles = 2;
        _address_write_16(DE, get_A);
        break;
    case 0x13:		// INC DE
        cycles = 2;
        ++DE;
        break;
    case 0x14:		// INC D
        cycles = 1;
        set_flag_N(0);
        data_8 = get_D + 1;
        set_D(data_8);
        set_flag_Z(!(data_8 & 0xF));
        set_flag_H(!data_8);
        break;
    case 0x15:		// DEC D
        cycles = 1;
        set_flag_N(1);
        data_8 = get_D;
        set_flag_H(!(data_8 & 0xF));
        --data_8;
        set_D(data_8);
        set_flag_Z(!data_8);
        break;
    case 0x16:		// LD D, d8
        cycles = 2;
        data_8 = _address_read(PC);
        ++PC;
        set_D(data_8);
        break;
    case 0x17:		// RLA
        cycles = 1;
        set_flag_Z(0);
        set_flag_N(0);
        set_flag_H(0);
        data_8 = get_A >> 7;
        set_A(get_A << 1 | get_flag_C);
        set_flag_C(data_8);
        break;
    case 0x18:		// JR r8
        cycles = 3;
        data_8 = _address_read(PC);
        ++PC;
        PC = (PC & 0xFF00) | ((PC + data_8) & 0x00FF);
        break;
    case 0x19:		// ADD HL, DE
        cycles = 2;
        set_flag_N(0);
        data_32 = (uint32_t)HL + (uint32_t)DE;
        set_flag_H((data_32 & (uint32_t)0xFFF) < ((uint32_t)HL & (uint32_t)0xFFF));
        HL = (uint16_t)data_32;
        set_flag_C(data_32 >> 16);
        break;
    case 0x1A:		// LD A, (DE)
        cycles = 2;
        set_A(_address_read(DE));
        break;
    case 0x1B:		// DEC DE
        cycles = 2;
        --DE;
        break;
    case 0x1C:		// INC E
        cycles = 1;
        set_flag_N(0);
        data_8 = get_E + 1;
        set_E(data_8);
        set_flag_H(!(data_8 & 0xF));
        set_flag_Z(!(data_8));
        break;
    case 0x1D:		// DEC E
        cycles = 1;
        set_flag_N(1);
        data_8 = get_E;
        set_flag_H(!(data_8 & 0xF));
        --data_8;
        set_E(data_8);
        set_flag_Z(!(data_8));
        break;
    case 0x1E:		// LD E, d8
        cycles = 2;
        data_8 = _address_read(PC);
        ++PC;
        set_E(data_8);
        break;
    case 0x1F:		// RRA
        cycles = 1;
        set_flag_Z(0);
        set_flag_N(0);
        set_flag_H(0);
        data_8 = get_A & 0x1;
        set_A((get_A >> 1) | (get_flag_C << 7));
        set_flag_C(data_8);
        break;
    case 0x20:		// JR NZ, r8
        data_8 = _address_read(PC);
        ++PC;
        if (get_flag_Z) {		// NOT JUMP
            cycles = 2;
        } else {				// JUMP
            cycles = 3;
            PC = (PC & 0xFF00) | ((PC + data_8) & 0x00FF);
        }
        break;
    case 0x21:		// LD HL, d16
        cycles = 3;
        HL = _address_read_16(PC);
        PC += 2;
        break;
    case 0x22:		// LD (HL+), A
        cycles = 2;
        _address_write(HL, get_A);
        ++HL;
        break;
    case 0x23:		// INC HL
        cycles = 2;
        ++HL;
        break;
    case 0x24:		// INC H
        cycles = 1;
        set_flag_N(0);
        data_8 = get_H + 1;
        set_H(data_8);
        set_flag_H(!(data_8 & 0xF));
        set_flag_Z(!data_8);
        break;
    case 0x25:		// DEC H
        cycles = 1;
        set_flag_N(1);
        data_8 = get_H;
        set_flag_H(!(data_8 & 0xF));
        --data_8;
        set_H(data_8);
        set_flag_Z(!data_8);
        break;
    case 0x26:		// LD H, d8
        cycles = 2;
        data_8 = _address_read(PC);
        ++PC;
        set_H(data_8);
        break;
    case 0x27:		// DAA
        cycles = 1;
        if (get_flag_N) {
            if (get_flag_H)
                set_A(get_A - 0x6);
            if (get_flag_C)
                set_A(get_A - 0x60);
        } else {
            if (get_flag_H || (get_A & 0xF) > 0x9) {
                set_A(get_A + 0x6);
            }
            if (get_flag_C || ((get_A >> 4) & 0xF) > 0x9) {
                set_A(get_A + 0x60);
            }
        }
        break;
    case 0x28:		// JR Z, r8 
        data_8 = _address_read(PC);
        ++PC;
        if (get_flag_Z) {
            cycles = 3;
            PC = (PC & 0xFF00) | ((PC + data_8) & 0x00FF);
        } else {
            cycles = 2;
        }
        break;
    case 0x29:		// ADD HL, HL
        cycles = 2;
        set_flag_N(0);
        data_32 = (uint32_t)HL + (uint32_t)HL;
        set_flag_H((data_32 & (uint32_t)0xFFF) < ((uint32_t)HL & (uint32_t)0xFFF));
        HL = (uint16_t)data_32;
        set_flag_C(data_32 >> 16);
        break;
    case 0x2A:		// LD A, (HL+)
        cycles = 2;
        data_8 = _address_read(HL);
        ++HL;
        set_A(data_8);
        break;
    case 0x2B:		// DEC HL
        cycles = 2;
        --HL;
        break;
    case 0x2C:		// INC L
        cycles = 1;
        set_flag_N(0);
        data_8 = get_L + 1;
        set_L(data_8);
        set_flag_H(!(data_8 & 0xF));
        set_flag_Z(!data_8);
        break;
    case 0x2D:		// DEC L
        cycles = 1;
        set_flag_N(1);
        data_8 = get_L;
        set_flag_H(!(data_8 & 0xF));
        --data_8;
        set_L(data_8);
        set_flag_Z(!data_8);
        break;
    case 0x2E:		// LD L, d8
        cycles = 2;
        data_8 = _address_read(PC);
        ++PC;
        set_L(data_8);
        break;
    case 0x2F:		// CPL
        cycles = 1;
        set_flag_N(1);
        set_flag_H(1);
        set_A(~get_A);
        break;
    case 0x30:		// JR NC, r8
        data_8 = _address_read(PC);
        ++PC;
        if (get_flag_C) {
            cycles = 2;
        } else {
            cycles = 3;
            PC = (PC & 0xFF00) | ((PC + data_8) & 0x00FF);
        }
        break;
    case 0x31:		// LD SP, s16
        cycles = 3;
        data_16 = _address_read_16(PC);
        PC += 2;
        SP = data_16;
        break;
    case 0x32:		// LD (HL-), A
        cycles = 2;
        _address_write(HL, get_A);
        --HL;
        break;
    case 0x33:		// INC SP
        cycles = 2;
        ++SP;
        break;
    case 0x34:		// INC (HL)
        cycles = 3;
        set_flag_N(0);
        data_8 = _address_read(HL);
        ++data_8;
        set_flag_H(!(data_8 & 0xF));
        set_flag_Z(!(data_8));
        _address_write(HL, data_8);
        break;
    case 0x35:		// DEC (HL)
        cycles = 3;
        set_flag_N(0);
        data_8 = _address_read(HL);
        set_flag_H(!(data_8 & 0xF));
        --data_8;
        set_flag_Z(!(data_8));
        _address_write(HL, data_8);
        break;
    case 0x36:		// LD (HL), d8
        cycles = 3;
        data_8 = _address_read(PC);
        ++PC;
        _address_write(HL, data_8);
        break;
    case 0x37:		// SCF
        cycles = 1;
        set_flag_N(0);
        set_flag_H(0);
        set_flag_C(1);
        break;
    case 0x38:		// JR C, r8
        data_8 = _address_read(PC);
        ++PC;
        if (get_flag_C) {
            cycles = 12;
            PC = (PC & 0xFF00) | ((PC + data_8) & 0x00FF);
        } else {
            cycles = 8;
        }
        break;
    case 0x39:		// ADD HL, SP
        cycles = 2;
        set_flag_N(0);
        data_32 = (uint32_t)HL + (uint32_t)SP;
        set_flag_H((data_32 & (uint32_t)0xFFF) < ((uint32_t)HL & (uint32_t)0xFFF));
        HL = (uint16_t)data_32;
        set_flag_C(data_32 >> 16);
        break;
    case 0x3A:		// LD A, (HL-)
        cycles = 2;
        data_8 = _address_read(HL);
        --HL;
        set_A(data_8);
        break;
    case 0x3B:		// DEC SP
        cycles = 2;
        --SP;
        break;
    case 0x3C:		// INC A
        cycles = 1;
        set_flag_N(0);
        data_8 = get_A + 1;
        set_A(data_8);
        set_flag_H(!(data_8 & 0xF));
        set_flag_Z(!data_8);
        break;
    case 0x3D:		// DEC A
        cycles = 1;
        set_flag_N(1);
        data_8 = get_A;
        set_flag_H(!(data_8 & 0xF));
        --data_8;
        set_A(data_8);
        set_flag_Z(!data_8);
        break;
    case 0x3E:		// LD A, d8
        cycles = 2;
        data_8 = _address_read(PC);
        ++PC;
        set_A(data_8);
        break;
    case 0x3F:		// CCF
        cycles = 1;
        set_flag_N(0);
        set_flag_H(0);
        set_flag_C(!get_flag_C);
        break;
    case 0x40:		// LD B, B
        cycles = 1;
        break;
    case 0x41:		// LD B, C
        cycles = 1;
        set_B(get_C);
        break;
    case 0x42:		// LD B, D
        cycles = 1;
        set_B(get_D);
        break;
    case 0x43:		// LD B, E
        cycles = 1;
        set_B(get_E);
        break;
    case 0x44:		// LD B, H
        cycles = 1;
        set_B(get_H);
        break;
    case 0x45:		// LD B, L
        cycles = 1;
        set_B(get_L);
        break;
    case 0x46:		// LD B, (HL)
        cycles = 2;
        data_8 = _address_read(HL);
        set_B(data_8);
        break;
    case 0x47:		// LD B, A
        cycles = 1;
        set_B(get_A);
        break;
    case 0x48:		// LD C, B
        cycles = 1;
        set_C(get_B);
        break;
    case 0x49:		// LD C, C
        cycles = 1;
        break;
    case 0x4A:		// LD C, D
        cycles = 1;
        set_C(get_D);
        break;
    case 0x4B:		// LD C, E
        cycles = 1;
        set_C(get_E);
        break;
    case 0x4C:		// LD C, H
        cycles = 1;
        set_C(get_H);
        break;
    case 0x4D:		// LD C, L
        cycles = 1;
        set_C(get_L);
        break;
    case 0x4E:		// LD C, (HL)
        cycles = 2;
        data_8 = _address_read(HL);
        set_C(data_8);
        break;
    case 0x4F:		// LD C, A
        cycles = 1;
        set_C(get_A);
        break;
    case 0x50:		// LD D, B
        cycles = 1;
        set_D(get_B);
        break;
    case 0x51:		// LD D, C
        cycles = 1;
        set_D(get_C);
        break;
    case 0x52:		// LD D, D
        cycles = 1;
        break;
    case 0x53:		// LD D, E
        cycles = 1;
        set_D(get_E);
        break;
    case 0x54:		// LD D, H
        cycles = 1;
        set_D(get_H);
        break;
    case 0x55:		// LD D, L
        cycles = 1;
        set_D(get_L);
        break;
    case 0x56:		// LD D, (HL)
        cycles = 2;
        data_8 = _address_read(HL);
        set_D(data_8);
        break;
    case 0x57:		// LD D, A
        cycles = 1;
        set_D(get_A);
        break;
    case 0x58:		// LD E, B
        cycles = 1;
        set_E(get_B);
        break;
    case 0x59:		// LD E, C
        cycles = 1;
        set_E(get_C);
        break;
    case 0x5A:		// LD E, D
        cycles = 1;
        set_E(get_D);
        break;
    case 0x5B:		// LD E, E
        cycles = 1;
        break;
    case 0x5C:		// LD E, H
        cycles = 1;
        set_E(get_H);
        break;
    case 0x5D:		// LD E, L
        cycles = 1;
        set_E(get_L);
        break;
    case 0x5E:		// LD E, (HL)
        cycles = 2;
        data_8 = _address_read(HL);
        set_E(data_8);
        break;
    case 0x5F:		// LD E, A
        cycles = 1;
        set_E(get_A);
        break;
    case 0x60:		// LD H, B
        cycles = 1;
        set_H(get_B);
        break;
    case 0x61:		// LD H, C
        cycles = 1;
        set_H(get_C);
        break;
    case 0x62:		// LD H, D
        cycles = 1;
        set_H(get_D);
        break;
    case 0x63:		// LD H, E
        cycles = 1;
        set_H(get_E);
        break;
    case 0x64:		// LD H, H
        cycles = 1;
        break;
    case 0x65:		// LD H, L
        cycles = 1;
        set_H(get_L);
        break;
    case 0x66:		// LD H, (HL)
        cycles = 2;
        data_8 = _address_read(HL);
        set_H(data_8);
        break;
    case 0x67:		// LD H, A
        cycles = 1;
        set_H(get_A);
        break;
    case 0x68:		// LD L, B
        cycles = 1;
        set_L(get_B);
        break;
    case 0x69:		// LD L, C
        cycles = 1;
        set_L(get_C);
        break;
    case 0x6A:		// LD L, D
        cycles = 1;
        set_L(get_D);
        break;
    case 0x6B:		// LD L, E
        cycles = 1;
        set_L(get_E);
        break;
    case 0x6C:		// LD L, H
        cycles = 1;
        set_L(get_H);
        break;
    case 0x6D:		// LD L, L
        cycles = 1;
        break;
    case 0x6E:		// LD L, (HL)
        cycles = 1;
        data_8 = _address_read(HL);
        set_L(data_8);
        break;
    case 0x6F:		// LD L, A
        cycles = 1;
        set_L(get_A);
        break;
    case 0x70:		// LD (HL), B
        cycles = 2;
        _address_write(HL, get_B);
        break;
    case 0x71:		// LD (HL), C
        cycles = 2;
        _address_write(HL, get_C);
        break;
    case 0x72:		// LD (HL), D
        cycles = 2;
        _address_write(HL, get_D);
        break;
    case 0x73:		// LD (HL), E
        cycles = 2;
        _address_write(HL, get_E);
        break;
    case 0x74:		// LD (HL), H
        cycles = 2;
        _address_write(HL, get_H);
        break;
    case 0x75:		// LD (HL), L
        cycles = 2;
        _address_write(HL, get_L);
        break;
    case 0x76:		// HALT
        cycles = 1;
        fprintf(stdout, "Entering halt mode.\n");
        break;
    case 0x77:		// LD (HL), A
        cycles = 2;
        _address_write(HL, get_A);
        break;
    case 0x78:		// LD A, B
        cycles = 1;
        set_A(get_B);
        break;
    case 0x79:		// LD A, C
        cycles = 1;
        set_A(get_C);
        break;
    case 0x7A:		// LD A, D
        cycles = 1;
        set_A(get_D);
        break;
    case 0x7B:		// LD A, E
        cycles = 1;
        set_A(get_E);
        break;
    case 0x7C:		// LD A, H
        cycles = 1;
        set_A(get_H);
        break;
    case 0x7D:		// LD A, L
        cycles = 1;
        set_A(get_L);
        break;
    case 0x7E:		// LD A, (HL)
        cycles = 1;
        data_8 = _address_read(HL);
        set_A(data_8);
        break;
    case 0x7F:		// LD A, A
        cycles = 1;
        break;
    case 0x80:		// ADD A, B
        cycles = 1;
        set_flag_N(0);
        data_8 = get_A + get_B;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) < (get_A & 0xF));
        set_flag_C(data_8 < get_A);
        set_A(data_8);
        break;
    case 0x81:		// ADD A, C
        cycles = 1;
        set_flag_N(0);
        data_8 = get_A + get_C;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) < (get_A & 0xF));
        set_flag_C(data_8 < get_A);
        set_A(data_8);
        break;
    case 0x82:		// ADD A, D
        cycles = 1;
        set_flag_N(0);
        data_8 = get_A + get_D;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) < (get_A & 0xF));
        set_flag_C(data_8 < get_A);
        set_A(data_8);
        break;
    case 0x83:		// ADD A, E
        cycles = 1;
        set_flag_N(0);
        data_8 = get_A + get_E;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) < (get_A & 0xF));
        set_flag_C(data_8 < get_A);
        set_A(data_8);
        break;
    case 0x84:		// ADD A, H
        cycles = 1;
        set_flag_N(0);
        data_8 = get_A + get_H;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) < (get_A & 0xF));
        set_flag_C(data_8 < get_A);
        set_A(data_8);
        break;
    case 0x85:		// ADD A, L
        cycles = 1;
        set_flag_N(0);
        data_8 = get_A + get_L;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) < (get_A & 0xF));
        set_flag_C(data_8 < get_A);
        set_A(data_8);
        break;
    case 0x86:		// ADD A, (HL)
        cycles = 2;
        set_flag_N(0);
        data_8 = get_A + _address_read(HL);
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) < (get_A & 0xF));
        set_flag_C(data_8 < get_A);
        set_A(data_8);
        break;
    case 0x87:		// ADD A, A
        cycles = 1;
        set_flag_N(0);
        data_8 = get_A + get_A;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) < (get_A & 0xF));
        set_flag_C(data_8 < get_A);
        set_A(data_8);
        break;
    case 0x88:		// ADC A, B
        cycles = 1;
        set_flag_N(0);
        data_8 = get_A + get_B + get_flag_C;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) < (get_A & 0xF));
        set_flag_C(data_8 < get_A);
        set_A(data_8);
        break;
    case 0x89:		// ADC A, C
        cycles = 1;
        set_flag_N(0);
        data_8 = get_A + get_C + get_flag_C;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) < (get_A & 0xF));
        set_flag_C(data_8 < get_A);
        set_A(data_8);
        break;
    case 0x8A:		// ADC A, D
        cycles = 1;
        set_flag_N(0);
        data_8 = get_A + get_D + get_flag_C;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) < (get_A & 0xF));
        set_flag_C(data_8 < get_A);
        set_A(data_8);
        break;
    case 0x8B:		// ADC A, E
        cycles = 1;
        set_flag_N(0);
        data_8 = get_A + get_E + get_flag_C;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) < (get_A & 0xF));
        set_flag_C(data_8 < get_A);
        set_A(data_8);
        break;
    case 0x8C:		// ADC A, H
        cycles = 1;
        set_flag_N(0);
        data_8 = get_A + get_H + get_flag_C;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) < (get_A & 0xF));
        set_flag_C(data_8 < get_A);
        set_A(data_8);
        break;
    case 0x8D:		// ADC A, L
        cycles = 1;
        set_flag_N(0);
        data_8 = get_A + get_L + get_flag_C;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) < (get_A & 0xF));
        set_flag_C(data_8 < get_A);
        set_A(data_8);
        break;
    case 0x8E:		// ADC A, (HL)
        cycles = 2;
        set_flag_N(0);
        data_8 = get_A + _address_read(HL) + get_flag_C;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) < (get_A & 0xF));
        set_flag_C(data_8 < get_A);
        set_A(data_8);
        break;
    case 0x8F:		// ADC A, A
        cycles = 1;
        set_flag_N(0);
        data_8 = get_A + get_A + get_flag_C;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) < (get_A & 0xF));
        set_flag_C(data_8 < get_A);
        set_A(data_8);
        break;
    case 0x90:		// SUB B
        cycles = 1;
        set_flag_N(1);
        data_8 = get_A - get_B;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        set_A(data_8);
        break;
    case 0x91:		// SUB C
        cycles = 1;
        set_flag_N(1);
        data_8 = get_A - get_C;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        set_A(data_8);
        break;
    case 0x92:		// SUB D
        cycles = 1;
        set_flag_N(1);
        data_8 = get_A - get_D;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        set_A(data_8);
        break;
    case 0x93:		// SUB E
        cycles = 1;
        set_flag_N(1);
        data_8 = get_A - get_E;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        set_A(data_8);
        break;
    case 0x94:		// SUB H
        cycles = 1;
        set_flag_N(1);
        data_8 = get_A - get_H;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        set_A(data_8);
        break;
    case 0x95:		// SUB L
        cycles = 1;
        set_flag_N(1);
        data_8 = get_A - get_L;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        set_A(data_8);
        break;
    case 0x96:		// SUB (HL)
        cycles = 2;
        set_flag_N(1);
        data_8 = get_A - _address_read(HL);
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        set_A(data_8);
        break;
    case 0x97:		// SUB A
        cycles = 1;
        set_flag_N(1);
        data_8 = 0;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        set_A(data_8);
        break;
    case 0x98:		// SBC A, B
        cycles = 1;
        set_flag_N(1);
        data_8 = get_A - get_B - get_flag_C;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        set_A(data_8);
        break;
    case 0x99:		// SBC A, C
        cycles = 1;
        set_flag_N(1);
        data_8 = get_A - get_C - get_flag_C;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        set_A(data_8);
        break;
    case 0x9A:		// SBC A, D
        cycles = 1;
        set_flag_N(1);
        data_8 = get_A - get_D - get_flag_C;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        set_A(data_8);
        break;
    case 0x9B:		// SBC A, E
        cycles = 1;
        set_flag_N(1);
        data_8 = get_A - get_E - get_flag_C;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        set_A(data_8);
        break;
    case 0x9C:		// SBC A, H
        cycles = 1;
        set_flag_N(1);
        data_8 = get_A - get_H - get_flag_C;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        set_A(data_8);
        break;
    case 0x9D:		// SBC A, L
        cycles = 1;
        set_flag_N(1);
        data_8 = get_A - get_L - get_flag_C;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        set_A(data_8);
        break;
    case 0x9E:		// SBC A, (HL)
        cycles = 2;
        set_flag_N(1);
        data_8 = get_A - _address_read(HL) - get_flag_C;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        set_A(data_8);
        break;
    case 0x9F:		// SBC A, A
        cycles = 1;
        set_flag_N(1);
        data_8 = -get_flag_C;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        set_A(data_8);
        break;
    case 0xA0:		// AND B
        cycles = 1;
        set_flag_N(0);
        set_flag_H(1);
        set_flag_C(0);
        set_A(get_A & get_B);
        set_flag_Z(!get_A);
        break;
    case 0xA1:		// AND C
        cycles = 1;
        set_flag_N(0);
        set_flag_H(1);
        set_flag_C(0);
        set_A(get_A & get_C);
        set_flag_Z(!get_A);
        break;
    case 0xA2:		// AND D
        cycles = 1;
        set_flag_N(0);
        set_flag_H(1);
        set_flag_C(0);
        set_A(get_A & get_D);
        set_flag_Z(!get_A);
        break;
    case 0xA3:		// AND E
        cycles = 1;
        set_flag_N(0);
        set_flag_H(1);
        set_flag_C(0);
        set_A(get_A & get_E);
        set_flag_Z(!get_A);
        break;
    case 0xA4:		// AND H
        cycles = 1;
        set_flag_N(0);
        set_flag_H(1);
        set_flag_C(0);
        set_A(get_A & get_H);
        set_flag_Z(!get_A);
        break;
    case 0xA5:		// AND L
        cycles = 1;
        set_flag_N(0);
        set_flag_H(1);
        set_flag_C(0);
        set_A(get_A & get_L);
        set_flag_Z(!get_A);
        break;
    case 0xA6:		// AND (HL)
        cycles = 2;
        set_flag_N(0);
        set_flag_H(1);
        set_flag_C(0);
        set_A(get_A & _address_read(HL));
        set_flag_Z(!get_A);
        break;
    case 0xA7:		// AND A
        cycles = 1;
        set_flag_N(0);
        set_flag_H(1);
        set_flag_C(0);
        set_flag_Z(!get_A);
        break;
    case 0xA8:		// XOR B
        cycles = 1;
        set_flag_N(0);
        set_flag_H(0);
        set_flag_C(0);
        set_A(get_A ^ get_B);
        set_flag_Z(!get_A);
        break;
    case 0xA9:		// XOR C
        cycles = 1;
        set_flag_N(0);
        set_flag_H(0);
        set_flag_C(0);
        set_A(get_A ^ get_C);
        set_flag_Z(!get_A);
        break;
    case 0xAA:		// XOR D
        cycles = 1;
        set_flag_N(0);
        set_flag_H(0);
        set_flag_C(0);
        set_A(get_A ^ get_D);
        set_flag_Z(!get_A);
        break;
    case 0xAB:		// XOR E
        cycles = 1;
        set_flag_N(0);
        set_flag_H(0);
        set_flag_C(0);
        set_A(get_A ^ get_E);
        set_flag_Z(!get_A);
        break;
    case 0xAC:		// XOR H
        cycles = 1;
        set_flag_N(0);
        set_flag_H(0);
        set_flag_C(0);
        set_A(get_A ^ get_H);
        set_flag_Z(!get_A);
        break;
    case 0xAD:		// XOR L
        cycles = 1;
        set_flag_N(0);
        set_flag_H(0);
        set_flag_C(0);
        set_A(get_A ^ get_L);
        set_flag_Z(!get_A);
        break;
    case 0xAE:		// XOR (HL)
        cycles = 2;
        set_flag_N(0);
        set_flag_H(0);
        set_flag_C(0);
        set_A(get_A ^ _address_read(HL));
        set_flag_Z(!get_A);
        break;
    case 0xAF:		// XOR A
        cycles = 1;
        set_flag_N(0);
        set_flag_H(0);
        set_flag_C(0);
        set_A(0);
        set_flag_Z(1);
        break;
    case 0xB0:		// OR B
        cycles = 1;
        set_flag_N(0);
        set_flag_H(0);
        set_flag_C(0);
        set_A(get_A | get_B);
        set_flag_Z(!get_A);
        break;
    case 0xB1:		// OR C
        cycles = 1;
        set_flag_N(0);
        set_flag_H(0);
        set_flag_C(0);
        set_A(get_A | get_C);
        set_flag_Z(!get_A);
        break;
    case 0xB2:		// OR D
        cycles = 1;
        set_flag_N(0);
        set_flag_H(0);
        set_flag_C(0);
        set_A(get_A | get_D);
        set_flag_Z(!get_A);
        break;
    case 0xB3:		// OR E
        cycles = 1;
        set_flag_N(0);
        set_flag_H(0);
        set_flag_C(0);
        set_A(get_A | get_E);
        set_flag_Z(!get_A);
        break;
    case 0xB4:		// OR H
        cycles = 1;
        set_flag_N(0);
        set_flag_H(0);
        set_flag_C(0);
        set_A(get_A | get_H);
        set_flag_Z(!get_A);
        break;
    case 0xB5:		// OR L
        cycles = 1;
        set_flag_N(0);
        set_flag_H(0);
        set_flag_C(0);
        set_A(get_A | get_L);
        set_flag_Z(!get_A);
        break;
    case 0xB6:		// OR (HL)
        cycles = 2;
        set_flag_N(0);
        set_flag_H(0);
        set_flag_C(0);
        set_A(get_A | _address_read(HL));
        set_flag_Z(!get_A);
        break;
    case 0xB7:		// OR A
        cycles = 1;
        set_flag_N(0);
        set_flag_H(0);
        set_flag_C(0);
        set_flag_Z(!get_A);
        break;
    case 0xB8:		// CP B
        cycles = 1;
        set_flag_N(1);
        data_8 = get_A - get_B;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        break;
    case 0xB9:		// CP C
        cycles = 1;
        set_flag_N(1);
        data_8 = get_A - get_C;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        break;
    case 0xBA:		// CP D
        cycles = 1;
        set_flag_N(1);
        data_8 = get_A - get_D;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        break;
    case 0xBB:		// CP E
        cycles = 1;
        set_flag_N(1);
        data_8 = get_A - get_E;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        break;
    case 0xBC:		// CP H
        cycles = 1;
        set_flag_N(1);
        data_8 = get_A - get_H;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        break;
    case 0xBD:		// CP L
        cycles = 1;
        set_flag_N(1);
        data_8 = get_A - get_L;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        break;
    case 0xBE:		// CP (HL)
        cycles = 2;
        set_flag_N(1);
        data_8 = get_A - _address_read(HL);
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        break;
    case 0xBF:		// CP A
        cycles = 1;
        set_flag_N(1);
        data_8 = 0;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        break;
    case 0xC0:		// RET NZ
        if (get_flag_Z) {
            cycles = 2;
        } else {
            cycles = 5;
            PC = _address_read_16(SP);
            SP += 2;
        }
        break;
    case 0xC1:		// POP BC
        cycles = 3;
        BC = _address_read_16(SP);
        SP += 2;
        break;
    case 0xC2:		// JP NZ, a16
        if (get_flag_Z) {
            cycles = 3;
            PC += 2;
        } else {
            cycles = 4;
            PC = _address_read_16(PC);
        }
        break;
    case 0xC3:		// JP a16
        cycles = 4;
        data_16 = _address_read_16(PC);
        PC = data_16;
        break;
    case 0xC4:		// CALL NZ, a16
        if (get_flag_Z) {
            cycles = 3;
            PC += 2;
        } else {
            cycles = 6;
            data_16 = _address_read_16(PC);
            PC += 2;
            SP -= 2;
            _address_write_16(SP, PC);
            PC = data_16;
        }
        break;
    case 0xC5:		// PUSH BC
        cycles = 4;
        SP -= 2;
        _address_write_16(SP, BC);
        break;
    case 0xC6:		// ADD A, d8
        cycles = 2;
        set_flag_N(0);
        data_8 = get_A + _address_read(PC);
        PC += 1;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) < (get_A & 0xF));
        set_flag_C(data_8 < get_A);
        break;
    case 0xC7:		// RST 00h
        cycles = 4;
        SP -= 2;
        _address_write_16(SP, PC);
        PC = 0;
        break;
    case 0xC8:		// RET Z
        if (get_flag_Z) {
            cycles = 5;
            PC = _address_read_16(SP);
            SP += 2;
        } else {
            cycles = 2;
        }
        break;
    case 0xC9:		// RET
        cycles = 4;
        PC = _address_read_16(SP);
        SP += 2;
        break;
    case 0xCA:		// JP Z, a16
        if (get_flag_Z) {
            cycles = 4;
            PC = _address_read_16(PC);
        } else {
            cycles = 3;
            PC += 2;
        }
        break;
    case 0xCB:		// PREFIX CB
        // CB needs 1 cycle itself
        cycles = _cpu_execute_cb() + 1;
        break;
    case 0xCC:		// CALL Z, a16
        if (get_flag_Z) {
            cycles = 6;
            data_16 = _address_read_16(PC);
            PC += 2;
            SP -= 2;
            _address_write_16(SP, PC);
            PC = data_16;
        } else {
            cycles = 3;
            PC += 2;
        }
        break;
    case 0xCD:		// CALL a16
        cycles = 6;
        data_16 = _address_read_16(PC);
        PC += 2;
        SP -= 2;    // might be right but it seems that the 0xFFFE is wasted
        _address_write_16(SP, PC);
        PC = data_16;
        break;
    case 0xCE:		// ADC A, d8
        cycles = 2;
        set_flag_N(0);
        data_8 = get_A + _address_read(PC) + get_flag_C;
        ++PC;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) < (get_A & 0xF));
        set_flag_C(data_8 < get_A);
        break;
    case 0xCF:		// RST 08H
        cycles = 4;
        SP -= 2;
        _address_write_16(SP, PC);
        PC = 8;
        break;
    case 0xD0:		// RET NC
        if (get_flag_C) {
            cycles = 2;
        } else {
            cycles = 5;
            PC = _address_read_16(SP);
            SP += 2;
        }
        break;
    case 0xD1:		// POP DE
        cycles = 3;
        DE = _address_read_16(SP);
        SP += 2;
        break;
    case 0xD2:		// JP NC, a16
        if (get_flag_C) {
            cycles = 3;
            PC += 2;
        } else {
            cycles = 4;
            PC = _address_read_16(PC);
        }
        break;
    case 0xD4:		// CALL NC, a16
        if (get_flag_C) {
            cycles = 3;
            PC += 2;
        } else {
            cycles = 6;
            data_16 = _address_read_16(PC);
            PC += 2;
            SP -= 2;
            _address_write_16(SP, PC);
            PC = data_16;
        }
        break;
    case 0xD5:		// PUSH DE
        cycles = 4;
        SP -= 2;
        _address_write_16(SP, DE);
        break;
    case 0xD6:		// SUB d8
        cycles = 2;
        set_flag_N(1);
        data_8 = get_A - _address_read(PC);
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        break;
    case 0xD7:		// RST 10H
        cycles = 4;
        SP -= 2;
        _address_write_16(SP, PC);
        PC = 0x10;
        break;
    case 0xD8:		// RET C
        if (get_flag_C) {
            cycles = 5;
            PC = _address_read_16(SP);
            SP += 2;
        } else {
            cycles = 2;
        }
        break;
    case 0xD9:		// RETI
        cycles = 4;
        PC = _address_read_16(SP);
        SP += 2;
        IME = 1;
        break;
    case 0xDA:		// JP C, a16
        if (get_flag_C) {
            cycles = 3;
            PC += 2;
        } else {
            cycles = 4;
            PC = _address_read_16(PC);
        }
        break;
    case 0xDC:		// CALL C, a16
        if (get_flag_C) {
            cycles = 6;
            data_16 = _address_read_16(PC);
            PC += 2;
            SP -= 2;
            _address_write_16(SP, PC);
            PC = data_16;
        } else {
            cycles = 3;
            PC += 2;
        }
        break;
    case 0xDE:		// SBC A, d8
        cycles = 2;
        set_flag_N(1);
        data_8 = get_A + _address_read(PC) + get_flag_C;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) > (get_A & 0xF));
        set_flag_C(data_8 > get_A);
        set_A(data_8);
        ++PC;
        break;
    case 0xDF:		// RST 18h
        cycles = 4;
        SP -= 2;
        _address_write_16(SP, PC);
        PC = 0x18;
        break;
    case 0xE0:		// LDH (a8), A
        cycles = 3;
        _address_write(0xFF00 | (uint16_t)_address_read(PC), get_A);
        ++PC;
        break;
    case 0xE1:		// POP HL
        cycles = 3;
        HL = _address_read_16(SP);
        SP += 2;
        break;
    case 0xE2:		// LD (C), A
        cycles = 2;
        _address_write((0xFF00 | (uint16_t)get_C), get_A);
        break;
    case 0xE5:		// PUSH HL
        cycles = 4;
        SP -= 2;
        _address_write_16(SP, HL);
        break;
    case 0xE6:		// AND d8
        cycles = 2;
        set_flag_N(0);
        set_flag_H(1);
        set_flag_C(0);
        set_A(get_A & _address_read(PC));
        set_flag_Z(get_A);
        break;
    case 0xE7:		// RST 20h
        cycles = 4;
        SP -= 2;
        _address_write_16(SP, PC);
        PC = 0x20;
    case 0xE8:		// ADD SP, r8
        cycles = 4;
        data_16 = SP + (uint16_t)_address_read(PC);
        ++PC;
        set_flag_Z(0);
        set_flag_N(0);
        set_flag_H((data_16 & 0xFFF) < (SP & 0xFFF));
        set_flag_C(data_16 < SP);
        SP = data_16;
        break;
    case 0xE9:		// JP (HL)
        cycles = 1;
        PC = _address_read_16(HL);
        break;
    case 0xEA:		// LD (a16), A
        cycles = 4;
        _address_write(_address_read_16(PC), get_A);
        PC += 2;
        break;
    case 0xEE:		// XOR d8
        cycles = 2;
        set_flag_N(0);
        set_flag_H(0);
        set_flag_C(0);
        set_A(get_A ^ _address_read(PC));
        ++PC;
        set_flag_Z(!get_A);
    case 0xEF:		// RST 28h
        cycles = 4;
        SP -= 2;
        _address_write_16(SP, PC);
        PC = 0x28;
        break;
    case 0xF0:		// LDH A, (d8)
        cycles = 3;
        set_A(_address_read(0xFF00 + _address_read(PC)));
        ++PC;
        break;
    case 0xF1:		// POP AF
        cycles = 3;
        AF = _address_read_16(SP);
        SP += 2;
        break;
    case 0xF2:		// LD A, (C)
        cycles = 2;
        set_A(_address_read(0xFF00 + get_C));
        break;
    case 0xF3:		// DI
        cycles = 1;
        IME = 0;
        break;
    case 0xF5:		// PUSH AF
        cycles = 4;
        SP -= 2;
        _address_write_16(SP, AF);
        break;
    case 0xF6:		// OR d8
        cycles = 2;
        set_flag_N(0);
        set_flag_H(0);
        set_flag_C(0);
        set_A(get_A & _address_read(PC));
        ++PC;
        set_flag_Z(!get_A);
        break;
    case 0xF7:		// RST 30h
        cycles = 4;
        SP -= 2;
        _address_write_16(SP, PC);
        PC = 0x30;
        break;
    case 0xF8:		// LD HL, SP + r8
        cycles = 3;
        set_flag_Z(0);
        set_flag_N(0);
        HL = SP + _address_read(PC);
        ++PC;
        set_flag_H((HL & 0xFFF) < (SP & 0xFFF));
        set_flag_C(HL < SP);
        break;
    case 0xF9:		// LD SP, HL
        cycles = 2;
        SP = HL;
        break;
    case 0xFA:		// LD A, (a16)
        cycles = 4;
        set_A(_address_read(_address_read(PC)));
        ++PC;
        break;
    case 0xFB:		// EI
        cycles = 1;
        IME = 1;
        break;
    case 0xFE:		// CP d8
        cycles = 2;
        set_flag_N(1);
        data_8 = get_A - _address_read(PC);
        PC += 1;
        set_flag_Z(!data_8);
        set_flag_H((data_8 & 0xF) < (get_A & 0xF));
        set_flag_C(data_8 < get_A);
        break;
    case 0xFF:		// RST 38h
        cycles = 4;
        SP -= 2;
        _address_write_16(SP, PC);
        PC = 0x38;
        break;
    default:
        cycles = 0xFFFF;
        fprintf(stderr, "Encounter illegal instruction.\n");
        break;
    }
    return cycles;
}

int my_gb_cpu_construct(void)
{
    internal_ram = 0;
    is_stopped = 0;


    // bootstrap code(256Byte in gameboy) changes register to desired value
#ifdef LOAD_BOOTSTRAP
    AF = 0x0;
    BC = 0x0;
    DE = 0x0;
    HL = 0x0;
    SP = 0x0;
    PC = 0x0;
#else
    // set these values means jump over the bootstrap code
    AF = 0x01B0;
    BC = 0x0013;
    DE = 0x00D8;
    HL = 0x014D;
    SP = 0xFFFE;
    PC = 0x0100;
#endif
    SB = 0;
    SC = 0;
    DIV = 0;
    TIMA = 0;
    TMA = 0;
    TAC = 0;
    IF = 0;
    BOOT = 0;
    IE = 0;

    IME = 0;

    return 0;
}

void my_gb_cpu_destruct(void)
{
    // Do nothing because no heap memory allocation
}

void my_gb_cpu_link_ram(uint8_t * _internal_ram)
{
    internal_ram = _internal_ram;
}

void my_gb_cpu_link_cart(void)
{
}

uint32_t my_gb_cpu_run(uint32_t cycles_want)
{
    uint32_t cycles = 0;
    // check interruption
    // if interruption, interruption ^= interruption_type_will_do
    for (;;) {
        if (cycles >= cycles_want)
            break;
        cycles += _cpu_interruption();

        if (cycles >= cycles_want)
            break;
        cycles += _cpu_execute();
    }
    // check timer overflow if yes mark interruption
    return cycles - cycles_want;
}

void my_gb_cpu_on_interruption(enum INTERRUPTION_TYPE type)
{
    uint8_t _int = 0;
    switch (type) {
    case INT_VBLANK:
        _int = INT_VBLANK_MASK;
        break;
    case INT_LCDC_STATUS:
        _int = INT_LCDC_STATUS_MASK;
        break;
    case INT_TIMER_OVERFLOW:
        _int = INT_TIMER_OVERFLOW_MASK;
        break;
    case INT_SERIAL_TRANSFER_COMPLETION:
        _int = INT_SERIAL_TRANSFER_COMPLETION_MASK;
        break;
    case INT_BUTTON_PRESS:
        _int = INT_BUTTON_PRESS_MASK;
        break;
    default:
        break;
    }
    // We don't use IE to mask here because interruption type will be checked during interruption identify time.

    /*
        When more than 1 interrupts occur at the same
    time only the interrupt with the highest priority
    can be acknowledged. When an interrupt is used a
    '0' should be stored in the IF register before the
    IE register is set.
    */
    if (IF < _int) {
        IF = _int;
    }
}
