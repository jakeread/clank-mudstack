/*
osap/drivers/step_a4950.cpp

stepper code for two A4950s

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo
projects. Copyright is retained and must be preserved. The work is provided as
is; no warranty is provided, and users accept all liability.
*/

#include "step_a4950.h"
//#include "ucbus_drop.h"

// sine, 0-8190, 4095 center / 'zero', 256 steps 
uint16_t LUT_8190[256] = {
    4095,4195,4296,4396,4496,4596,4696,4795,4894,4992,
    5090,5187,5284,5380,5475,5569,5662,5754,5846,5936,
    6025,6113,6200,6286,6370,6453,6534,6614,6693,6770,
    6845,6919,6991,7061,7129,7196,7260,7323,7384,7443,
    7500,7555,7607,7658,7706,7753,7797,7839,7878,7916,
    7951,7983,8014,8042,8067,8091,8111,8130,8146,8159,
    8170,8179,8185,8189,8190,8189,8185,8179,8170,8159,
    8146,8130,8111,8091,8067,8042,8014,7983,7951,7916,
    7878,7839,7797,7753,7706,7658,7607,7555,7500,7443,
    7384,7323,7260,7196,7129,7061,6991,6919,6845,6770,
    6693,6614,6534,6453,6370,6286,6200,6113,6025,5936,
    5846,5754,5662,5569,5475,5380,5284,5187,5090,4992,
    4894,4795,4696,4596,4496,4396,4296,4195,4095,3995,
    3894,3794,3694,3594,3494,3395,3296,3198,3100,3003,
    2906,2810,2715,2621,2528,2436,2344,2254,2165,2077,
    1990,1904,1820,1737,1656,1576,1497,1420,1345,1271,
    1199,1129,1061,994,930,867,806,747,690,635,
    583,532,484,437,393,351,312,274,239,207,
    176,148,123,99,79,60,44,31,20,11,
    5,1,0,1,5,11,20,31,44,60,
    79,99,123,148,176,207,239,274,312,351,
    393,437,484,532,583,635,690,747,806,867,
    930,994,1061,1129,1199,1271,1345,1420,1497,1576,
    1656,1737,1820,1904,1990,2077,2165,2254,2344,2436,
    2528,2621,2715,2810,2906,3003,3100,3198,3296,3395,
    3494,3594,3694,3794,3894,3995
};

// sine, 0-1022 (511 center / 'zero'), 256 steps 
uint16_t LUT_1022[256] = {
    511,524,536,549,561,574,586,598,611,623,635,647,659,671,683,695,
    707,718,729,741,752,763,774,784,795,805,815,825,835,845,854,863,
    872,881,890,898,906,914,921,929,936,943,949,956,962,967,973,978,
    983,988,992,996,1000,1003,1007,1010,1012,1014,1016,1018,1020,1021,1021,1022,
    1022,1022,1021,1021,1020,1018,1016,1014,1012,1010,1007,1003,1000,996,992,988,
    983,978,973,967,962,956,949,943,936,929,921,914,906,898,890,881,
    872,863,854,845,835,825,815,805,795,784,774,763,752,741,729,718,
    707,695,683,671,659,647,635,623,611,598,586,574,561,549,536,524,
    511,498,486,473,461,448,436,424,411,399,387,375,363,351,339,327,
    315,304,293,281,270,259,248,238,227,217,207,197,187,177,168,159,
    150,141,132,124,116,108,101,93,86,79,73,66,60,55,49,44,
    39,34,30,26,22,19,15,12,10,8,6,4,2,1,1,0,
    0,0,1,1,2,4,6,8,10,12,15,19,22,26,30,34,
    39,44,49,55,60,66,73,79,86,93,101,108,116,124,132,141,
    150,159,168,177,187,197,207,217,227,238,248,259,270,281,293,304,
    315,327,339,351,363,375,387,399,411,424,436,448,461,473,486,498,
};

uint16_t dacLUT[256];

STEP_A4950* STEP_A4950::instance = 0;

STEP_A4950* STEP_A4950::getInstance(void){
    if(instance == 0){
        instance = new STEP_A4950();
    }
    return instance;
}

STEP_A4950* stepper_hw = STEP_A4950::getInstance();

STEP_A4950::STEP_A4950() {}

void STEP_A4950::init(boolean invert, float cscale){
    // all of 'em, outputs 
    AIN1_PORT.DIRSET.reg = AIN1_BM;
    AIN2_PORT.DIRSET.reg = AIN2_BM;
    BIN1_PORT.DIRSET.reg = BIN1_BM;
    BIN2_PORT.DIRSET.reg = BIN2_BM;
    // floating cscale 
    if(cscale < 0){
        _cscale = 0;
    } else if (cscale > 1){
        _cscale = 1;
    } else {
        _cscale = cscale;
    }
    // write a rectified LUT for writing to DACs
    for(uint16_t i = 0; i < 256; i ++){
        if(LUT_8190[i] > 4095){
            dacLUT[i] = LUT_8190[i] - 4095;
        } else if (LUT_8190[i] < 4095){
            dacLUT[i] = abs(4095 - LUT_8190[i]);
        } else {
            dacLUT[i] = 0;
        }
    }
    // invert direction / not 
    _dir_invert = invert;
    // start the DAAAC
    dacs->init();
    // start condition, 
    step();
}

// sequence like
// S: 1 2 3 4 5 6 7 8 
// A: ^ ^ ^ x v v v x
// B: ^ x v v v x ^ ^
void STEP_A4950::step(void){
    // increment: wrapping comes for free with uint8_t, bless 
    if(_dir){
        if(_dir_invert){
            _aStep -= _microstep_count;
            _bStep -= _microstep_count;
        } else {
            _aStep += _microstep_count;
            _bStep += _microstep_count;
        }
    } else {
        if(_dir_invert){
            _aStep += _microstep_count;
            _bStep += _microstep_count;
        } else {
            _aStep -= _microstep_count;
            _bStep -= _microstep_count;
        }
    }
    // a phase, 
    if(LUT_8190[_aStep] > 4095){
        A_UP;
    } else if (LUT_8190[_aStep] < 4095){
        A_DOWN;
    } else {
        A_OFF;
    }
    // a DAC 
    // so that we can easily rewrite currents on the fly. will extend to servoing, yeah 
    dacs->writeDac0(dacLUT[_aStep] * _cscale);
    // b phase, 
    if(LUT_8190[_bStep] > 4095){
        B_UP;
    } else if (LUT_8190[_bStep] < 4095){
        B_DOWN;
    } else {
        B_OFF;
    }
    // b DAC
    dacs->writeDac1(dacLUT[_bStep] * _cscale);
}

void STEP_A4950::dir(boolean val){
    _dir = val;
}

boolean STEP_A4950::getDir(void){
    return _dir;
}

void STEP_A4950::setMicrostep(uint8_t microstep){
    switch(microstep){
        case 64:
            _microstep_count = MICROSTEP_64_COUNT;
            break;
        case 32:
            _microstep_count = MICROSTEP_32_COUNT;
            break;
        case 16:
            _microstep_count = MICROSTEP_16_COUNT;
            break;
        case 8:
            _microstep_count = MICROSTEP_8_COUNT;
            break;
        case 4: 
            _microstep_count = MICROSTEP_4_COUNT;
            break;
        case 1:
            _microstep_count = MICROSTEP_1_COUNT;
            break;
        default:
            _microstep_count = MICROSTEP_1_COUNT;
            break;
    }
}

void STEP_A4950::setCurrent(float cscale){
    if(cscale > 1){
        _cscale = 1;
    } else if(cscale < 0){
        _cscale = 0;
    } else {
        _cscale = cscale;
    }
    // do DAC re-writes 
    dacs->writeDac0(dacLUT[_aStep] * _cscale);
    dacs->writeDac1(dacLUT[_bStep] * _cscale);
}

void STEP_A4950::setInversion(boolean inv){
    _dir_invert = inv;
}

void STEP_A4950::dacRefresh(void){
    dacs->refresh();
}
