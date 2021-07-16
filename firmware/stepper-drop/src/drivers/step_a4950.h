/*
osap/drivers/step_a4950.h

stepper code for two A4950s

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo
projects. Copyright is retained and must be preserved. The work is provided as
is; no warranty is provided, and users accept all liability.
*/

#ifndef STEP_A4950_H_
#define STEP_A4950_H_

#include <Arduino.h>

#include "dacs.h"
#include "indicators.h"
#include "../syserror.h"

// C_SCALE 
// 1: DACs go 0->512 (of 4096, peak current is 1.6A at 4096): 0.2A
// 2: DACs go 0->1024,
// ...
// 8: DACs go full width 
//#define C_SCALE 8 // on init 
// MICROSTEP_COUNT 
// 1:   do 1 tick of 256 table, for full resolution, this is 64 'microsteps'
// 2:   32 microsteps
// 4:   16 microsteps
// 8:   8 microsteps
// 16:  4 microsteps
// 32:  full steps 
#define MICROSTEP_COUNT 1

#define MICROSTEP_64_COUNT 1
#define MICROSTEP_32_COUNT 2
#define MICROSTEP_16_COUNT 4
#define MICROSTEP_8_COUNT 8 
#define MICROSTEP_4_COUNT 16
#define MICROSTEP_1_COUNT 32 

// AIN1 PB06
// AIN2 PA04 
// BIN1 PA07 
// BIN2 PA06 
#define AIN1_PIN 6
#define AIN1_PORT PORT->Group[1]
#define AIN1_BM (uint32_t)(1 << AIN1_PIN)
#define AIN2_PIN 4 
#define AIN2_PORT PORT->Group[0]
#define AIN2_BM (uint32_t)(1 << AIN2_PIN)
#define BIN1_PIN 7 
#define BIN1_PORT PORT->Group[0]
#define BIN1_BM (uint32_t)(1 << BIN1_PIN)
#define BIN2_PIN 6 
#define BIN2_PORT PORT->Group[0] 
#define BIN2_BM (uint32_t)(1 << BIN2_PIN)

// handles
#define AIN1_HI AIN1_PORT.OUTSET.reg = AIN1_BM
#define AIN1_LO AIN1_PORT.OUTCLR.reg = AIN1_BM
#define AIN2_HI AIN2_PORT.OUTSET.reg = AIN2_BM
#define AIN2_LO AIN2_PORT.OUTCLR.reg = AIN2_BM 
#define BIN1_HI BIN1_PORT.OUTSET.reg = BIN1_BM
#define BIN1_LO BIN1_PORT.OUTCLR.reg = BIN1_BM
#define BIN2_HI BIN2_PORT.OUTSET.reg = BIN2_BM
#define BIN2_LO BIN2_PORT.OUTCLR.reg = BIN2_BM

// set a phase up or down direction
// transition low first, avoid brake condition for however many ns 
#define A_UP AIN2_LO; AIN1_HI
#define A_OFF AIN2_LO; AIN1_LO
#define A_DOWN AIN1_LO; AIN2_HI
#define B_UP BIN2_LO; BIN1_HI 
#define B_OFF BIN2_LO; BIN1_LO
#define B_DOWN BIN1_LO; BIN2_HI

class STEP_A4950 {
   private:
    // is driver, is singleton, 
    static STEP_A4950* instance;
    volatile uint8_t _aStep = 0;    // 0 of 256 micros, 
    volatile uint8_t _bStep = 63;   // of the same table, startup 90' out of phase 
    volatile boolean _dir = false;
    boolean _dir_invert = false;
    uint8_t _microstep_count = 1;
    // try single scalar
    float _cscale = 0.1;

   public:
    STEP_A4950();
    static STEP_A4950* getInstance(void);
    // do like 
    void init(boolean invert, float cscale);
    void step(void);
    void dir(boolean val);
    boolean getDir(void);
    // microstep setting 
    void setMicrostep(uint8_t microstep);
    // current settings 
    void setCurrent(float cscale);
    void setInversion(boolean inv);
    // for the dacs 
    void dacRefresh(void);
};

extern STEP_A4950* stepper_hw;

#endif