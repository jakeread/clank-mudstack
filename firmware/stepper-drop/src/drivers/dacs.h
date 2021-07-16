/*
osap/drivers/dacs.h

dacs on the d51

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo
projects. Copyright is retained and must be preserved. The work is provided as
is; no warranty is provided, and users accept all liability.
*/

#ifndef DACS_H_
#define DACS_H_

#include <arduino.h>

#include "indicators.h"
#include "../syserror.h"

// scrape https://github.com/adafruit/ArduinoCore-samd/blob/master/cores/arduino/wiring_analog.c
// scrape https://github.com/adafruit/ArduinoCore-samd/blob/master/cores/arduino/startup.c (clock)
// scrape https://github.com/adafruit/ArduinoCore-samd/blob/master/cores/arduino/wiring.c (peripheral clock)
// DAC0 is on PA02
// DAC1 is on PA05

// NOTE: the DAC must be refreshed manually to maintain voltage.
// there does appear to be a refresh register in DACCTRL band, 
// but it does *not* seem to work... 

#define GENERIC_CLOCK_GENERATOR_12M       (4u)
#define GENERIC_CLOCK_GENERATOR_12M_SYNC   GCLK_SYNCBUSY_GENCTRL4

class DACs {
   private:
    // is driver, is singleton, 
    static DACs* instance;
    volatile uint16_t currentVal0 = 0;
    volatile uint16_t currentVal1 = 0;
    volatile uint32_t lastRefresh = 0;

   public:
    DACs();
    static DACs* getInstance(void);
    void init(void);
    void writeDac0(uint16_t val);
    void writeDac1(uint16_t val);
    void refresh(void);
};

extern DACs* dacs;

#endif