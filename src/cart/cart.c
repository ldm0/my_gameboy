#include"cart.h"
#include<stdio.h>		// for debug info
#include<stdlib.h>		// for malloc
#include<stdint.h>		// for uint*_t
#include<string.h>

static uint8_t *rom = 0;
static uint8_t *ram = 0;


// MBC2 have built-in 512x4 bit ram.
// Only when cart MBC type is MBC2, this chunk of ram is used 
// For convenience, use lower nibble of 8bit to simulate the 4bit,
// higher nibbles are left untouched.
static uint8_t MBC2_ram[512];

// MBC read and write function pointer
static uint8_t(*MBC_read)(uint16_t address) = 0;
static void (*MBC_write)(uint16_t address, uint8_t data) = 0;

static uint8_t MBC1_read(uint16_t address);
static uint8_t MBC2_read(uint16_t address);
static uint8_t MBC3_read(uint16_t address);
static uint8_t MBC5_read(uint16_t address);

static void MBC1_write(uint16_t address, uint8_t data);
static void MBC2_write(uint16_t address, uint8_t data);
static void MBC3_write(uint16_t address, uint8_t data);
static void MBC5_write(uint16_t address, uint8_t data);

int my_gb_cart_construct(const char *filename)
{
    // open ROM file
    FILE *cart_file_fs = NULL;      // ROM file file stream
    uint8_t *cart_file = NULL;      // ROM file memory map

    uint32_t cart_type = 0;
    uint32_t rom_type = 0;
    uint32_t ram_type = 0;

    uint32_t rom_size = 0;
    uint32_t ram_size = 0;
    // store data locally, move to static or global when necessary


    cart_file_fs = fopen(filename, "rb");
    if (cart_file_fs == NULL) {
        fprintf(stderr, "Open rom file %s failed.\n", filename);
        goto error;
    }
    if (fseek(cart_file_fs, 0, SEEK_END) == -1)
        goto error;

    // get ROM file size for read
    int rom_file_size = ftell(cart_file_fs);
    if (rom_file_size <= 0x150) {
        fprintf(stderr, "Rom file %s too small to check cartridge properties.\n", filename);
        goto error;
    }

    // copy ROM to computer memory
    cart_file = (uint8_t *)malloc(rom_file_size * sizeof(uint8_t));
    if (!cart_file)
        goto error;
    rewind(cart_file_fs);
    int read_size = fread(cart_file, 1, rom_file_size, cart_file_fs);
    if (read_size != rom_file_size) {
        fprintf(stderr, "Open rom file(%s) successful, but read failed.\n", filename);
        goto error;
    }
    fclose(cart_file_fs);
    cart_file_fs = 0;

    printf("ROM %15s loaded.\n", cart_file + 0x134);

    // check cartridge hardware type
    cart_type = cart_file[0x147];
    switch (cart_type) {
    case 0x0:		// ROM ONLY
    case 0x8:		// ROM + RAM
    case 0x9:		// ROM + RAM + BATTERY
        MBC_read = NULL;
        MBC_write = NULL;
    case 0x1:		// ROM + MBC1
    case 0x2:		// ROM + MBC1 + RAM
    case 0x3:		// ROM + MBC1 + RAM + BATT
        MBC_read = MBC1_read;
        MBC_write = MBC1_write;
        break;
    case 0x5:		// ROM + MBC2
    case 0x6:		// ROM + MBC2 + BATTERY
        MBC_read = MBC2_read;
        MBC_write = MBC2_write;
        break;
    case 0x12:		// ROM + MBC3 + RAM
    case 0x13:		// ROM + MBC3 + RAM + BATT
        MBC_read = MBC3_write;
        MBC_write = MBC3_write;
        break;
    case 0x19:		// ROM + MBC5
    case 0x1A:		// ROM + MBC5 + RAM
    case 0x1B:		// ROM + MBC5 + RAM + BATT
    case 0x1C:		// ROM + MBC5 + RUMBLE
    case 0x1D:		// ROM + MBC5 + RUMBLE + SRAM
    case 0x1E:		// ROM + MBC5 + RUMBLE + SRAM + BATT
        MBC_read = MBC5_write;
        MBC_write = MBC5_write;
        break;
    case 0xB:		// ROM + MMM01
    case 0xC:		// ROM + MMM01 + SRAM
    case 0xD:		// ROM + MMM01 + SRAM + BATT
    case 0x1F:		// Pocket Camera
    case 0xFD:		// Bandai TAMA5
    case 0xFE:		// Hudson HuC - 3
        printf("unsupported cart type: %X", cart_type);
        goto error;
    default:
        printf("never seen cart type: %X.\n", cart_type);
        goto error;
    }

    rom_type = cart_file[0x148];
    // ROM bank size: 16kb
    switch (rom_type) {
    case 0x0:
        rom_size = 2 * 16 * 1024;
        break;
    case 0x1:
        rom_size = 4 * 16 * 1024;
        break;
    case 0x2:
        rom_size = 8 * 16 * 1024;
        break;
    case 0x3:
        rom_size = 16 * 16 * 1024;
        break;
    case 0x4:
        rom_size = 32 * 16 * 1024;
        break;
    case 0x5:
        rom_size = 64 * 16 * 1024;
        break;
    case 0x6:
        rom_size = 128 * 16 * 1024;
        break;
    case 0x52:
        rom_size = 72 * 16 * 1024;
        break;
    case 0x53:
        rom_size = 80 * 16 * 1024;
        break;
    case 0x54:
        rom_size = 96 * 16 * 1024;
        break;
    default:
        rom_size = 0;
        printf("unrecognized ROM type %X. assume no ROM\n", rom_type);
        goto error;
    }

    if (rom_size) {
        rom = (uint8_t *)malloc(rom_size);
        if (!rom) {
            fprintf(stderr, "rom allocation failed!\n");
            goto error;
        }
        memcpy(rom, cart_file, rom_size);
    }

    // ATTENTION!!!!!!!!!!!!!!!!!!!!!
    // NEED to implement rom copy from rom_raw

    ram_type = cart_file[0x149];
    // RAM bank size: 8kb
    switch (ram_type) {
    case 0x0:
        ram_size = 0;
        break;
    case 0x1:
        ram_size = 2 * 1024;
        break;
    case 0x2:
        ram_size = 1 * 8 * 1024;
        break;
    case 0x3:
        ram_size = 4 * 8 * 1024;
        break;
    case 0x4:
        ram_size = 16 * 8 * 1024;
        break;
    case 0x5:
        ram_size = 8 * 8 * 1024;
        break;
    default:
        ram_size = 0;
        printf("unrecognized RAM type %X. assume no RAM\n", ram_type);
        goto error;
    }
    if (ram_size) {
        ram = (uint8_t *)malloc(ram_size);
        if (!ram) {
            fprintf(stderr, "ram allocation failed.\n");
            goto error;
        }
    }

    free(cart_file);
    return 0;
error:
    if (cart_file_fs)
        fclose(cart_file_fs);
    if (cart_file)
        free(cart_file);
    if (rom) {
        free(rom);
        rom = NULL;
    }
    if (ram) {
        free(ram);
        ram = NULL;
    }
    return -1;
}

void my_gb_cart_destruct(void)
{
    if (rom) {
        free(rom);
        rom = NULL;
    }
    if (ram) {
        free(ram);
        ram = NULL;
    }
}

uint8_t my_gb_cart_address_read(uint16_t address)
{
    uint8_t read_result = 0;
    read_result = MBC_read(address);
    return read_result;
}

void my_gb_cart_address_write(uint16_t address, uint8_t data)
{
    // implementation needed
    MBC_write(address, data);
}

/*
 * MBC1
 * max 2MByte ROM and/or 32KByte RAM
 * Partly From http://gbdev.gg8.se/wiki/articles/Memory_Bank_Controllers

 * 0000-3FFF - ROM Bank 00 (Read Only)
 * 4000-7FFF - ROM Bank 01-7F (Read Only)

 * 0000-1FFF - RAM Enable (Write Only)
 * 2000-3FFF - ROM Bank Number (Write Only)
 * 4000-5FFF - RAM Bank Number - or - Upper Bits of ROM Bank Number (Write Only)
 * 6000-7FFF - ROM/RAM Mode Select (Write Only)

 * A000-BFFF - RAM Bank 00-03, if any (Read/Write)
 */
uint8_t MBC1_read(uint16_t address)
{
    uint8_t result;
    if (address < 0x3FFF) {
        result = rom[address];
    } else if (address < 0x7FFF) {
        result = 0;
    } else if (address > 0xA000 && address < 0xBFFF) {
        result = 0;
    } else {
        result = 0;
        fprintf(stderr,
                "error: trying to read invalid location of cart.\n");
    }
    return result;
}
void MBC1_write(uint16_t address, uint8_t data)
{
    if (address < 0x1FFF) {
    } else if (address < 0x3FFF) {
    } else if (address < 0x5FFF) {
    } else if (address < 0x7FFF) {
    } else if (address > 0xA000 && address < 0xBFFF) {
    } else {
        fprintf(stderr,
                "error: trying to write invalid location of cart.\n");
    }
}


/*
 * MBC2
 * max 256KByte ROM and 512x4 bits RAM
 * Partly From http://gbdev.gg8.se/wiki/articles/Memory_Bank_Controllers
 * 0000-3FFF - ROM Bank 00 (Read Only)
 * 4000-7FFF - ROM Bank 01-7F (Read Only)

 * 0000-1FFF - RAM Enable (Write Only)
 * 2000-3FFF - ROM Bank Number (Write Only)

 * A000-A1FF - 512x4bits RAM, built-in into the MBC2 chip (Read/Write)
 *    Hint: The MBC2 doesn't support external RAM,
            instead it includes 512x4 bits of built-in RAM.
            As the data consists of 4bit values,
            only the lower nibble of the "bytes" in this memory area are used.
 */

uint8_t MBC2_read(uint16_t address)
{
    uint8_t result;
    if (address < 0x3FFF) {
        result = 0;
    } else if (address < 0x7FFF) {
        result = 0;
    } else if (address > 0xA000 && address < 0xA1FF) {
        result = 0;
    } else {
        result = 0;
        fprintf(stderr,
                "error: trying to read invalid location of cart.\n");
    }
    return result;
}

void MBC2_write(uint16_t address, uint8_t data)
{
    if (address < 0x1FFF) {
    } else if (address < 0x3FFF) {
    } else if (address > 0xA000 && address < 0xA1FF) {
        MBC2_ram[address - 0xA000] = data & 0x0F;
    } else {
        fprintf(stderr,
                "error: trying to write invalid location of cart.\n");
    }
}


/*
 * MBC3
 * max 2MByte ROM and/or 64KByte RAM and Timer
 * Partly From http://gbdev.gg8.se/wiki/articles/Memory_Bank_Controllers
 * 0000-3FFF - ROM Bank 00 (Read Only)
 * 4000-7FFF - ROM Bank 01-7F (Read Only)

 * 0000-1FFF - RAM and Timer Enable (Write Only)
 * 2000-3FFF - ROM Bank Number (Write Only)
 * 4000-5FFF - RAM Bank Number - or - RTC Register Select (Write Only)
 * 6000-7FFF - Latch Clock Data (Write Only)

 * A000-BFFF - RAM Bank 00-03 - or - RTC Register 08-0C (Read/Write)
 */

uint8_t MBC3_read(uint16_t address)
{
    uint8_t result;
    if (address < 0x3FFF) {
        result = 0;
    } else if (address < 0x7FFF) {
        result = 0;
    } else if (address > 0xA000 && address < 0xBFFF) {
        result = 0;
    } else {
        result = 0;
        fprintf(stderr,
                "error: trying to read invalid location of cart.\n");
    }
    return result;
}

void MBC3_write(uint16_t address, uint8_t data)
{
    if (address < 0x1FFF) {
    } else if (address < 0x3FFF) {
    } else if (address < 0x5FFF) {
    } else if (address < 0x7FFF) {
    } else if (address > 0xA000 && address < 0xBFFF) {
    } else {
        fprintf(stderr,
                "error: trying to write invalid location of cart.\n");
    }
}

/*
 * MBC5
 * max 8MByte ROM and/or 128KByte RAM
 * Partly From http://gbdev.gg8.se/wiki/articles/Memory_Bank_Controllers

 * 0000-3FFF - ROM Bank 00 (Read Only)
 * 4000-7FFF - ROM Bank 00 - 1FF(Read Only)

 * 0000-1FFF - RAM Enable(Write Only)
 * 2000-2FFF - Low 8 bits of ROM Bank Number(Write Only)
 * 3000-3FFF - High bit of ROM Bank Number(Write Only)
 * 4000-5FFF - RAM Bank Number(Write Only)

 * A000-BFFF - RAM Bank 00 - 0F, if any(Read / Write)
 */

uint8_t MBC5_read(uint16_t address)
{
    uint8_t result;
    if (address < 0x3FFF) {
        result = 0;
    } else if (address < 0x7FFF) {
        result = 0;
    } else if (address > 0xA000 && address < 0xBFFF) {
        result = 0;
    } else {
        result = 0;
        fprintf(stderr,
                "error: trying to read invalid location of cart.\n");
    }
    return result;
}
void MBC5_write(uint16_t address, uint8_t data)
{
    if (address < 0x1FFF) {
    } else if (address < 0x2FFF) {
    } else if (address < 0x3FFF) {
    } else if (address < 0x5FFF) {
    } else if (address > 0xA000 && address < 0xBFFF) {
    } else {
        fprintf(stderr,
                "error: trying to write invalid location of cart.\n");
    }
}


