#pragma once
#ifndef _MY_GB_SOUND_H_
#define _MY_GB_SOUND_H_

#include<stdint.h>

/*

FF10
   Name     - NR 10
   Contents - Sound Mode 1 register, Sweep register (R/W)

              Bit 6-4 - Sweep Time
              Bit 3   - Sweep Increase/Decrease
                         0: Addition    (frequency increases)
                         1: Subtraction (frequency decreases)
              Bit 2-0 - Number of sweep shift (n: 0-7)

              Sweep Time: 000: sweep off - no freq change
                          001: 7.8 ms  (1/128Hz)
                          010: 15.6 ms (2/128Hz)
                          011: 23.4 ms (3/128Hz)
                          100: 31.3 ms (4/128Hz)
                          101: 39.1 ms (5/128Hz)
                          110: 46.9 ms (6/128Hz)
                          111: 54.7 ms (7/128Hz)

              The change of frequency (NR13,NR14) at each shift
              is calculated by the following formula where
              X(0) is initial freq & X(t-1) is last freq:

               X(t) = X(t-1) +/- X(t-1)/2^n

FF11
   Name     - NR 11
   Contents - Sound Mode 1 register, Sound length/Wave pattern duty (R/W)

              Only Bits 7-6 can be read.

              Bit 7-6 - Wave Pattern Duty
              Bit 5-0 - Sound length data (t1: 0-63)

              Wave Duty: 00: 12.5% ( _--------_--------_-------- )
                         01: 25%   ( __-------__-------__------- )
                         10: 50%   ( ____-----____-----____----- ) (default)
                         11: 75%   ( ______---______---______--- )

              Sound Length = (64-t1)*(1/256) seconds
FF12
   Name     - NR 12
   Contents - Sound Mode 1 register, Envelope (R/W)

              Bit 7-4 - Initial volume of envelope
              Bit 3 -   Envelope UP/DOWN
                         0: Attenuate
                         1: Amplify
              Bit 2-0 - Number of envelope sweep (n: 0-7)
                        (If zero, stop envelope operation.)

              Initial volume of envelope is from 0 to $F.
              Zero being no sound.

              Length of 1 step = n*(1/64) seconds

FF13
   Name     - NR 13
   Contents - Sound Mode 1 register, Frequency lo (W)

              Lower 8 bits of 11 bit frequency (x).
              Next 3 bit are in NR 14 ($FF14)

FF14
   Name     - NR 14
   Contents - Sound Mode 1 register, Frequency hi (R/W)

              Only Bit 6 can be read.

              Bit 7 - Initial (when set, sound restarts)
              Bit 6 - Counter/consecutive selection
              Bit 2-0 - Frequency's higher 3 bits (x)

              Frequency = 4194304/(32*(2048-x)) Hz
                        = 131072/(2048-x) Hz

              Counter/consecutive Selection
               0 = Regardless of the length data in NR11
                   sound can be produced consecutively.
               1 = Sound is generated during the time period
                   set by the length data in NR11. After this
                   period the sound 1 ON flag (bit 0 of NR52)
                   is reset.

FF16
   Name     - NR 21
   Contents - Sound Mode 2 register, Sound Length; Wave Pattern Duty (R/W)

              Only bits 7-6 can be read.

              Bit 7-6 - Wave pattern duty
              Bit 5-0 - Sound length data (t1: 0-63)

              Wave Duty: 00: 12.5% ( _--------_--------_-------- )
                         01: 25%   ( __-------__-------__------- )
                         10: 50%   ( ____-----____-----____----- ) (default)
                         11: 75%   ( ______---______---______--- )

              Sound Length = (64-t1)*(1/256) seconds

FF17
   Name     - NR 22
   Contents - Sound Mode 2 register, envelope (R/W)

              Bit 7-4 - Initial volume of envelope
              Bit 3 -   Envelope UP/DOWN
                         0: Attenuate
                         1: Amplify
              Bit 2-0 - Number of envelope sweep (n: 0-7)
                        (If zero, stop envelope operation.)

              Initial volume of envelope is from 0 to $F.
              Zero being no sound.

              Length of 1 step = n*(1/64) seconds

FF18
   Name     - NR 23
   Contents - Sound Mode 2 register, frequency lo data (W)

              Frequency's lower 8 bits of 11 bit data (x).
              Next 3 bits are in NR 14 ($FF19).

FF19
   Name     - NR 24
   Contents - Sound Mode 2 register, frequency hi data (R/W)

              Only bit 6 can be read.

              Bit 7 - Initial (when set, sound restarts)
              Bit 6 - Counter/consecutive selection
              Bit 2-0 - Frequency's higher 3 bits (x)

              Frequency = 4194304/(32*(2048-x)) Hz
                        = 131072/(2048-x) Hz

              Counter/consecutive Selection
               0 = Regardless of the length data in NR21
                   sound can be produced consecutively.
               1 = Sound is generated during the time period
                   set by the length data in NR21. After this
                   period the sound 2 ON flag (bit 1 of NR52)
                   is reset.

FF1A
   Name     - NR 30
   Contents - Sound Mode 3 register, Sound on/off (R/W)

              Only bit 7 can be read

              Bit 7 - Sound OFF
                      0: Sound 3 output stop
                      1: Sound 3 output OK

FF1B
   Name     - NR 31
   Contents - Sound Mode 3 register, sound length (R/W)

              Bit 7-0 - Sound length (t1: 0 - 255)

              Sound Length = (256-t1)*(1/2) seconds

FF1C
   Name     - NR 32
   Contents - Sound Mode 3 register, Select output level (R/W)

              Only bits 6-5 can be read

              Bit 6-5 - Select output level
                        00: Mute
                        01: Produce Wave Pattern RAM Data as it is
                            (4 bit length)
                        10: Produce Wave Pattern RAM data shifted once
                            to the RIGHT (1/2)  (4 bit length)
                        11: Produce Wave Pattern RAM data shifted twice
                            to the RIGHT (1/4)  (4 bit length)

       * - Wave Pattern RAM is located from $FF30-$FF3f.

FF1D
   Name     - NR 33
   Contents - Sound Mode 3 register, frequency's lower data (W)

              Lower 8 bits of an 11 bit frequency (x).

FF1E
   Name     - NR 34
   Contents - Sound Mode 3 register, frequency's higher data (R/W)

              Only bit 6 can be read.

              Bit 7 - Initial (when set, sound restarts)
              Bit 6 - Counter/consecutive flag
              Bit 2-0 - Frequency's higher 3 bits (x).

              Frequency = 4194304/(64*(2048-x)) Hz
                        = 65536/(2048-x) Hz

              Counter/consecutive Selection
               0 = Regardless of the length data in NR31
                   sound can be produced consecutively.
               1 = Sound is generated during the time period
                   set by the length data in NR31. After this
                   period the sound 3 ON flag (bit 2 of NR52)
                   is reset.

FF20
   Name     - NR 41
   Contents - Sound Mode 4 register, sound length (R/W)

              Bit 5-0 - Sound length data (t1: 0-63)

              Sound Length = (64-t1)*(1/256) seconds

FF21
   Name     - NR 42
   Contents - Sound Mode 4 register, envelope (R/W)

              Bit 7-4 - Initial volume of envelope
              Bit 3 -   Envelope UP/DOWN
                         0: Attenuate
                         1: Amplify
              Bit 2-0 - Number of envelope sweep (n: 0-7)
                        (If zero, stop envelope operation.)

              Initial volume of envelope is from 0 to $F.
              Zero being no sound.

              Length of 1 step = n*(1/64) seconds

FF22
   Name     - NR 43
   Contents - Sound Mode 4 register, polynomial counter (R/W)

              Bit 7-4 - Selection of the shift clock frequency of the
                        polynomial counter
              Bit 3   - Selection of the polynomial counter's step
              Bit 2-0 - Selection of the dividing ratio of frequencies

              Selection of the dividing ratio of frequencies:
              000: f * 1/2^3 * 2
              001: f * 1/2^3 * 1
              010: f * 1/2^3 * 1/2
              011: f * 1/2^3 * 1/3
              100: f * 1/2^3 * 1/4
              101: f * 1/2^3 * 1/5
              110: f * 1/2^3 * 1/6
              111: f * 1/2^3 * 1/7           f = 4.194304 Mhz

              Selection of the polynomial counter step:
              0: 15 steps
              1: 7 steps

              Selection of the shift clock frequency of the polynomial
              counter:

              0000: dividing ratio of frequencies * 1/2
              0001: dividing ratio of frequencies * 1/2^2
              0010: dividing ratio of frequencies * 1/2^3
              0011: dividing ratio of frequencies * 1/2^4
                    :                          :
                    :                          :
                    :                          :
              0101: dividing ratio of frequencies * 1/2^14
              1110: prohibited code
              1111: prohibited code

FF23
   Name     - NR 44
   Contents - Sound Mode 4 register, counter/consecutive; inital (R/W)

              Only bit 6 can be read.

              Bit 7 - Initial (when set, sound restarts)
              Bit 6 - Counter/consecutive selection

              Counter/consecutive Selection
               0 = Regardless of the length data in NR41
                   sound can be produced consecutively.
               1 = Sound is generated during the time period
                   set by the length data in NR41. After this
                   period the sound 4 ON flag (bit 3 of NR52)
                   is reset.

FF24
   Name     - NR 50
   Contents - Channel control / ON-OFF / Volume (R/W)

              Bit 7 - Vin->SO2 ON/OFF
              Bit 6-4 - SO2 output level (volume) (# 0-7)
              Bit 3 - Vin->SO1 ON/OFF
              Bit 2-0 - SO1 output level (volume) (# 0-7)

              Vin->SO1 (Vin->SO2)

              By synthesizing the sound from sound 1
              through 4, the voice input from Vin
              terminal is put out.
              0: no output
              1: output OK

FF25
    Name     - NR 51
    Contents - Selection of Sound output terminal (R/W)

               Bit 7 - Output sound 4 to SO2 terminal
               Bit 6 - Output sound 3 to SO2 terminal
               Bit 5 - Output sound 2 to SO2 terminal
               Bit 4 - Output sound 1 to SO2 terminal
               Bit 3 - Output sound 4 to SO1 terminal
               Bit 2 - Output sound 3 to SO1 terminal
               Bit 1 - Output sound 2 to SO1 terminal
               Bit 0 - Output sound 1 to SO1 terminal

FF26
    Name     - NR 52  (Value at reset: $F1-GB, $F0-SGB)
    Contents - Sound on/off (R/W)

               Bit 7 - All sound on/off
                       0: stop all sound circuits
                       1: operate all sound circuits
               Bit 3 - Sound 4 ON flag
               Bit 2 - Sound 3 ON flag
               Bit 1 - Sound 2 ON flag
               Bit 0 - Sound 1 ON flag

                Bits 0 - 3 of this register are meant to
               be status bits to be read. Writing to these
               bits does NOT enable/disable sound.

                If your GB programs don't use sound then
               write $00 to this register to save 16% or
               more on GB power consumption.
FF30 - FF3F
   Name     - Wave Pattern RAM
   Contents - Waveform storage for arbitrary sound data

              This storage area holds 32 4-bit samples
              that are played back upper 4 bits first.
*/

extern uint8_t NR10;
extern uint8_t NR11;
extern uint8_t NR12;
extern uint8_t NR13;
extern uint8_t NR14;
extern uint8_t NR21;
extern uint8_t NR22;
extern uint8_t NR23;
extern uint8_t NR24;
extern uint8_t NR30;
extern uint8_t NR31;
extern uint8_t NR32;
extern uint8_t NR33;
extern uint8_t NR34;
extern uint8_t NR41;
extern uint8_t NR42;
extern uint8_t NR43;
extern uint8_t NR44;
extern uint8_t NR50;
extern uint8_t NR51;
extern uint8_t NR52;
extern uint8_t W[16];

int my_gb_sound_construct(void);

void my_gb_sound_destruct(void);

uint32_t my_gb_sound_run(uint32_t cycles_want);

#endif 