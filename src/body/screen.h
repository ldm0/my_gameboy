#pragma once
#ifndef _MY_GB_SCREEN_H_
#define _MY_GB_SCREEN_H_

#include<stdint.h>
#include<Windows.h>

/*
FF40
   Name     - LCDC  (value $91 at reset)
   Contents - LCD Control (R/W)

              Bit 7 - LCD Control Operation *
                      0: Stop completely (no picture on screen)
                      1: operation

              Bit 6 - Window Tile Map Display Select
                      0: $9800-$9BFF
                      1: $9C00-$9FFF

              Bit 5 - Window Display
                      0: off
                      1: on

              Bit 4 - BG & Window Tile Data Select
                      0: $8800-$97FF
                      1: $8000-$8FFF <- Same area as OBJ

              Bit 3 - BG Tile Map Display Select
                      0: $9800-$9BFF
                      1: $9C00-$9FFF

              Bit 2 - OBJ (Sprite) Size
                      0: 8*8
                      1: 8*16 (width*height)

              Bit 1 - OBJ (Sprite) Display
                      0: off
                      1: on

              Bit 0 - BG Display
                      0: off
                      1: on

       * - Stopping LCD operation (bit 7 from 1 to 0)
           must be performed during V-blank to work
           properly. V-blank can be confirmed when the
           value of LY is greater than or equal to 144.

FF41
   Name     - STAT
   Contents - LCDC Status   (R/W)

              Bits 6-3 - Interrupt Selection By LCDC Status

              Bit 6 - LYC=LY Coincidence (Selectable)
              Bit 5 - Mode 10
              Bit 4 - Mode 01
              Bit 3 - Mode 00
                      0: Non Selection
                      1: Selection

              Bit 2 - Coincidence Flag
                      0: LYC not equal to LCDC LY
                      1: LYC = LCDC LY

              Bit 1-0 - Mode Flag
                        00: During H-Blank
                        01: During V-Blank
                        10: During Searching OAM-RAM
                        11: During Transfering Data to LCD Driver

     STAT shows the current status of the LCD controller.
     Mode 00: When the flag is 00 it is the H-Blank period
              and the CPU can access the display RAM
              ($8000-$9FFF).

     Mode 01: When the flag is 01 it is the V-Blank period
              and the CPU can access the display RAM
              ($8000-$9FFF).

     Mode 10: When the flag is 10 then the OAM is being
              used ($FE00-$FE9F). The CPU cannot access
              the OAM during this period

     Mode 11: When the flag is 11 both the OAM and display
              RAM are being used. The CPU cannot access
              either during this period.


     The following are typical when the display is enabled:

Mode 0  000___000___000___000___000___000___000________________
Mode 1  _______________________________________11111111111111__
Mode 2  ___2_____2_____2_____2_____2_____2___________________2_
Mode 3  ____33____33____33____33____33____33__________________3


       The Mode Flag goes through the values 0, 2,
      and 3 at a cycle of about 109uS. 0 is present
      about 48.6uS, 2 about 19uS, and 3 about 41uS. This
      is interrupted every 16.6ms by the VBlank (1).
      The mode flag stays set at 1 for about 1.08 ms.
      (Mode 0 is present between 201-207 clks, 2 about
       77-83 clks, and 3 about 169-175 clks. A complete
       cycle through these states takes 456 clks.
       VBlank lasts 4560 clks. A complete screen refresh
       occurs every 70224 clks.)

FF42
   Name     - SCY
   Contents - Scroll Y   (R/W)

              8 Bit value $00-$FF to scroll BG Y screen
              position.

FF43
   Name     - SCX
   Contents - Scroll X   (R/W)

              8 Bit value $00-$FF to scroll BG X screen
              position.

FF44
   Name     - LY
   Contents - LCDC Y-Coordinate (R)

            The LY indicates the vertical line to which
            the present data is transferred to the LCD
            Driver. The LY can take on any value between
            0 through 153. The values between 144 and 153
            indicate the V-Blank period. Writing will
            reset the counter.

FF45
   Name     - LYC
   Contents - LY Compare  (R/W)

            The LYC compares itself with the LY. If the
            values are the same it causes the STAT to set
            the coincident flag.

FF46
   Name     - DMA
   Contents - DMA Transfer and Start Address (W)

   The DMA Transfer (40*28 bit) from internal ROM or RAM
   ($0000-$F19F) to the OAM (address $FE00-$FE9F) can be
   performed. It takes 160 microseconds for the transfer.

   40*28 bit = #140  or #$8C.  As you can see, it only
   transfers $8C bytes of data. OAM data is $A0 bytes
   long, from $0-$9F.

   But if you examine the OAM data you see that 4 bits are
   not in use.

   40*32 bit = #$A0, but since 4 bits for each OAM is not
   used it's 40*28 bit.

   It transfers all the OAM data to OAM RAM.

   The DMA transfer start address can be designated every
   $100 from address $0000-$F100. That means $0000, $0100,
   $0200, $0300....

    As can be seen by looking at register $FF41 Sprite RAM
   ($FE00 - $FE9F) is not always available. A simple routine
   that many games use to write data to Sprite memory is shown
   below. Since it copies data to the sprite RAM at the appro-
   priate times it removes that responsibility from the main
   program.
    All of the memory space, except high ram ($FF80-$FFFE),
   is not accessible during DMA. Because of this, the routine
   below must be copied & executed in high ram. It is usually
   called from a V-blank Interrupt.

   Example program:

      org $40
      jp VBlank

      org $ff80
VBlank:
      push af        <- Save A reg & flags
      ld a,BASE_ADRS <- transfer data from BASE_ADRS
      ld ($ff46),a   <- put A into DMA registers
      ld a,28h       <- loop length
Wait:                <- We need to wait 160 microseconds.
      dec a          <-  4 cycles - decrease A by 1
      jr nz,Wait     <- 12 cycles - branch if Not Zero to Wait
      pop af         <- Restore A reg & flags
      reti           <- Return from interrupt


FF47
   Name     - BGP
   Contents - BG & Window Palette Data  (R/W)

              Bit 7-6 - Data for Dot Data 11 (Normally darkest color)
              Bit 5-4 - Data for Dot Data 10
              Bit 3-2 - Data for Dot Data 01
              Bit 1-0 - Data for Dot Data 00 (Normally lightest color)

              This selects the shade of grays to use for
              the background (BG) & window pixels. Since
              each pixel uses 2 bits, the corresponding
              shade will be selected from here.

FF48
   Name     - OBP0
   Contents - Object Palette 0 Data (R/W)

              This selects the colors for sprite palette 0.
              It works exactly as BGP ($FF47) except each
              each value of 0 is transparent.

FF49
   Name     - OBP1
   Contents - Object Palette 1 Data (R/W)

              This Selects the colors for sprite palette 1.
              It works exactly as OBP0 ($FF48).
              See BGP for details.

FF4A
   Name     - WY
   Contents - Window Y Position  (R/W)

              0 <= WY <= 143

              WY must be greater than or equal to 0 and
              must be less than or equal to 143 for
              window to be visible.

FF4B
   Name     - WX
   Contents - Window X Position  (R/W)

              0 <= WX <= 166

              WX must be greater than or equal to 0 and
              must be less than or equal to 166 for
              window to be visible.

              WX is offset from absolute screen coordinates
              by 7. Setting the window to WX=7, WY=0 will
              put the upper left corner of the window at
              absolute screen coordinates 0,0.


              Lets say WY = 70 and WX = 87.
              The window would be positioned as so:

               0                  80               159
               ______________________________________
            0 |                                      |
              |                   |                  |
              |                                      |
              |         Background Display           |
              |               Here                   |
              |                                      |
              |                                      |
           70 |         -         +------------------|
              |                   | 80,70            |
              |                   |                  |
              |                   |  Window Display  |
              |                   |       Here       |
              |                   |                  |
              |                   |                  |
          143 |___________________|__________________|


          OBJ Characters (Sprites) can still enter the
          window. None of the window colors are
          transparent so any background tiles under the
          window are hidden.
*/

extern uint8_t LCDC;
extern uint8_t STAT;
extern uint8_t SCY;
extern uint8_t SCX;
extern uint8_t LY;
extern uint8_t LYC;
extern uint8_t DMA;
extern uint8_t BGP;
extern uint8_t OBP0;
extern uint8_t OBP1;
extern uint8_t WY;
extern uint8_t WX;

// Currently fix resolution
// Future will enable 2 real pixel per pixel and 3 real pixel per pixel
int my_gb_screen_construct(WNDPROC callback);

void my_gb_screen_destruct(void);

// Link the VRAM 
int my_gb_screen_link_ram(uint8_t* ram);

// Pretend to link cpu
// then include"cpu.h" to use static functions in it.
void my_gb_cpu_link_screen(void);

// return exceed cycles
uint32_t my_gb_screen_run(uint32_t cycles_want);


#endif 