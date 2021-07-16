/*
osap/drivers/dacs.cpp

dacs on the d51

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo
projects. Copyright is retained and must be preserved. The work is provided as
is; no warranty is provided, and users accept all liability.
*/

#include "dacs.h"
//#include "ucbus_drop.h"

DACs* DACs::instance = 0;

DACs* DACs::getInstance(void){
    if(instance == 0){
        instance = new DACs();
    }
    return instance;
}

DACs* dacs = DACs::getInstance();

DACs::DACs() {}

void DACs::init(){
    /*
    // the below code was an attempt to scrape from 
    // scrape https://github.com/adafruit/ArduinoCore-samd/blob/master/cores/arduino/startup.c (clock)
    // scrape https://github.com/adafruit/ArduinoCore-samd/blob/master/cores/arduino/wiring.c (peripheral clock)
    // scrape https://github.com/adafruit/ArduinoCore-samd/blob/master/cores/arduino/wiring_analog.c
    // to setup the DAC 'from scratch' - of course it occurred to me later that this 
    // setup already happens in arduino's boot. so I omitted this and just used 
    // the messy per-analogWrite-call config below, and wrote small write-to-dac functions 
    // to operate under the assumption that this init happens once.

    // ... 
    // put the pins on the peripheral,
    // DAC0 is PA02, Peripheral B
    // DAC1 is PA05, Peripheral B
    //PORT->Group[0].DIRSET.reg = (uint32_t)(1 << 2);
    //PORT->Group[0].DIRCLR.reg = (uint32_t)(1 << 2);
    PORT->Group[0].PINCFG[2].bit.PMUXEN = 1;
    PORT->Group[0].PMUX[2 >> 1].reg |= PORT_PMUX_PMUXE(1);
    //PORT->Group[0].DIRSET.reg = (uint32_t)(1 << 5);
    //PORT->Group[0].DIRCLR.reg = (uint32_t)(1 << 5);
    PORT->Group[0].PINCFG[5].bit.PMUXEN = 1;
    PORT->Group[0].PMUX[5 >> 1].reg |= PORT_PMUX_PMUXO(1);

    // unmask the DAC peripheral
    MCLK->APBDMASK.bit.DAC_ = 1;

    // DAC needs a clock, 
    GCLK->GENCTRL[GENERIC_CLOCK_GENERATOR_12M].reg = GCLK_GENCTRL_SRC_DFLL | 
        GCLK_GENCTRL_IDC | 
        GCLK_GENCTRL_DIV(4) |
        GCLK_GENCTRL_GENEN;
    while(GCLK->SYNCBUSY.reg & GENERIC_CLOCK_GENERATOR_12M_SYNC);
    // feed that clock to the DAC,
    GCLK->PCHCTRL[DAC_GCLK_ID].reg = GCLK_PCHCTRL_CHEN | GCLK_PCHCTRL_GEN(GENERIC_CLOCK_GENERATOR_12M_SYNC);
    while(GCLK->PCHCTRL[DAC_GCLK_ID].bit.CHEN == 0);
    
    // software reset the DAC 
    while(DAC->SYNCBUSY.bit.SWRST == 1);
    DAC->CTRLA.bit.SWRST = 1;
    while(DAC->SYNCBUSY.bit.SWRST == 1);
    // and finally the DAC itself, 
    while(DAC->SYNCBUSY.bit.ENABLE || DAC->SYNCBUSY.bit.SWRST);
    DAC->CTRLA.bit.ENABLE = 0;
    // enable both channels 
    while(DAC->SYNCBUSY.bit.ENABLE || DAC->SYNCBUSY.bit.SWRST);
    DAC->DACCTRL[0].reg = DAC_DACCTRL_ENABLE | DAC_DACCTRL_REFRESH(2);
    while(DAC->SYNCBUSY.bit.ENABLE || DAC->SYNCBUSY.bit.SWRST);
    DAC->DACCTRL[1].reg = DAC_DACCTRL_ENABLE | DAC_DACCTRL_REFRESH(2);
    // voltage out, and select vref
    DAC->CTRLB.reg = DAC_CTRLB_REFSEL_VDDANA;
    // re-enable dac 
    while(DAC->SYNCBUSY.bit.ENABLE || DAC->SYNCBUSY.bit.SWRST);
    DAC->CTRLA.bit.ENABLE = 1;
    // await up, 
    while(!DAC->STATUS.bit.READY0);
    while(!DAC->STATUS.bit.READY1);
    */
   while(DAC->SYNCBUSY.bit.ENABLE || DAC->SYNCBUSY.bit.SWRST);
   DAC->CTRLA.bit.ENABLE = 0;
   while(DAC->SYNCBUSY.bit.ENABLE || DAC->SYNCBUSY.bit.SWRST);
   DAC->DACCTRL[0].bit.ENABLE = 1;
   DAC->DACCTRL[1].bit.ENABLE = 1;
   while(DAC->SYNCBUSY.bit.ENABLE || DAC->SYNCBUSY.bit.SWRST);
   DAC->CTRLA.bit.ENABLE = 1;
   while(!DAC->STATUS.bit.READY0);
   while(!DAC->STATUS.bit.READY1);
}

// 0 - 4095
void DACs::writeDac0(uint16_t val){
    //analogWrite(A0, val);
    while(DAC->SYNCBUSY.bit.DATA0);
    DAC->DATA[0].reg = val;//DAC_DATA_DATA(val);
    currentVal0 = val;
}

void DACs::writeDac1(uint16_t val){
    //analogWrite(A1, val);
    while(DAC->SYNCBUSY.bit.DATA1);
    DAC->DATA[1].reg = val;//DAC_DATA_DATA(val);
    currentVal1 = val;
}

void DACs::refresh(void){
    writeDac0(currentVal0);
    writeDac1(currentVal1);
    uint32_t now = micros();
    if(now > lastRefresh + 1000){
        lastRefresh = now;
    }
}
