#include <Arduino.h>

#include "indicators.h"
#include "drivers/step_a4950.h"
#include "osape-d51/osape/osap/osap.h"
#include "osape-d51/vertices/vt_usbSerial.h"
#include "osape-d51/vertices/vt_ucBusDrop.h"
#include "osape-d51/vertices/ucBusDrop.h"

// bare defaults: use vm / bus id to set on startup 
uint8_t axis_pick = 0;
float spu = 400.0F;
float old_spu = 400.0F;
volatile boolean spu_was_set = false;
float c_scale = 0.00F;
#define TICKS_PER_PACKET 25.0F
#define TICKS_PER_SECOND 50000.0F

// -------------------------------------------------------- AXIS PICK EP 

boolean onAxisPickData(uint8_t* data, uint16_t len){
  if(data[0] > 3){
    axis_pick = 0;
  } else {
    axis_pick = data[0];
  }
  return true;
}

vertex_t* axisPickEp = osapBuildEndpoint("axisPick", onAxisPickData, nullptr);

// -------------------------------------------------------- AXIS INVERSION EP

boolean onAxisInvertData(uint8_t* data, uint16_t len){
  if(data[0] > 0){
    stepper_hw->setInversion(true);
  } else {
    stepper_hw->setInversion(false);
  }
  return true;
}

vertex_t* axisInvertEp = osapBuildEndpoint("axisInvert", onAxisInvertData, nullptr);

// -------------------------------------------------------- MICROSTEP EP 

boolean onMicrostepData(uint8_t* data, uint16_t len){
  stepper_hw->setMicrostep(data[0]);
  return true;
}

vertex_t* microstepEp = osapBuildEndpoint("microstep", onMicrostepData, nullptr);

// -------------------------------------------------------- SPU EP

boolean onSPUData(uint8_t* data, uint16_t len){
  chunk_float32 spuc = { .bytes = { data[0], data[1], data[2], data[3] } };
  old_spu = spu;
  spu = fabsf(spuc.f);
  spu_was_set = true;
  return true;
}

vertex_t* spuEp = osapBuildEndpoint("SPU", onSPUData, nullptr);

// -------------------------------------------------------- CSCALE DATA

boolean onCScaleData(uint8_t* data, uint16_t len){
  chunk_float32 cscalec = { .bytes = { data[0], data[1], data[2], data[3] } };
  if(cscalec.f > 1.0F){
    cscalec.f = 1.0F;
  } else if (cscalec.f < 0.0F){
    cscalec.f = 0.0F;
  }
  stepper_hw->setCurrent(cscalec.f);
  return true;
}

vertex_t* cScaleEp = osapBuildEndpoint("CScale", onCScaleData, nullptr);

// -------------------------------------------------------- HOME ROUTINE

// some homeing globals, 
#define HOME_NOT 0
#define HOME_FIRST 1
#define HOME_BACKOFF 2

uint8_t homing = 0;           // statemachine 
float homeStepCounter = 0.0F; // step-float-counter
float homePos = 0.0F;         // position (units)
float homeStepRate = 0.0F;    // rate (steps/tick)
float homePosRate = 0.0F;     // rate (units/tick)
boolean homeDir = false;      // direction 
float homeOffset = 0.0F;      // after-home offset 

boolean onHomeData(uint8_t* data, uint16_t len){
  chunk_float32 rate = { .bytes = { data[0], data[1], data[2], data[3] } };
  chunk_float32 offset = { .bytes = { data[4], data[5], data[6], data[7] } };
  homing = HOME_FIRST;
  homeStepCounter = 0.0F;
  if(rate.f > 0){
    homeDir = true;
    stepper_hw->dir(true);
  } else {
    homeDir = false;
    stepper_hw->dir(false);
  }
  homeStepRate = abs(rate.f * spu) / TICKS_PER_SECOND;
  homePosRate = abs(rate.f) / TICKS_PER_SECOND;
  homeOffset = offset.f;
  return true;
}

vertex_t* homeEp = osapBuildEndpoint("Home", onHomeData, nullptr);

// -------------------------------------------------------- HOME STATE 

boolean beforeHomeStateQuery(void);

vertex_t* homeStateEp = osapBuildEndpoint("HomeState", nullptr, beforeHomeStateQuery);

boolean beforeHomeStateQuery(void){
  homeStateEp->ep->data[0] = homing;
  homeStateEp->ep->dataLen = 1;
  return true;
}

// -------------------------------------------------------- LIMIT SETUP 

#define LIMIT_PORT PORT->Group[0]
#define LIMIT_PIN 23 
#define LIMIT_BM ((uint32_t)(1 << LIMIT_PIN))

void limitSetup(void){
  // not-an-output 
  LIMIT_PORT.DIRCLR.reg = LIMIT_BM;
  // enable input 
  LIMIT_PORT.PINCFG[LIMIT_PIN].bit.INEN = 1;
  // enable pull 
  LIMIT_PORT.PINCFG[LIMIT_PIN].bit.PULLEN = 1;
  // 'pull' references direction from 'out' register, so we set hi to pull up (switch pulls to gnd)
  LIMIT_PORT.OUTSET.reg = LIMIT_BM;
}

boolean limitIsMade(void){
  // return true if switch is hit 
  return (LIMIT_PORT.IN.reg & LIMIT_BM);
}

void setup() {
  ERRLIGHT_SETUP;
  CLKLIGHT_SETUP;
  // limit switch 
  limitSetup();
  // osap
  osapSetup();
  // ports 
  vt_usbSerial_setup();
  osapAddVertex(vt_usbSerial);    // 0
  vt_ucBusDrop_setup();
  osapAddVertex(vt_ucBusDrop);    // 1
  // axis pick 
  osapAddVertex(axisPickEp);      // 2
  // axis invert
  osapAddVertex(axisInvertEp);    // 3
  // microstep 
  osapAddVertex(microstepEp);     // 4
  // SPU 
  osapAddVertex(spuEp);           // 5
  // cscale 
  osapAddVertex(cScaleEp);        // 6
  // homing 
  osapAddVertex(homeEp);          // 7 
  osapAddVertex(homeStateEp);     // 8 
  // stepper init 
  stepper_hw->init(false, c_scale);
}

// have available, 
// stepper_hw->setCurrent(currentChunks[AXIS_PICK].f);

void loop() {
  osapLoop();
  stepper_hw->dacRefresh();
  if(limitIsMade()){
    ERRLIGHT_ON;
  } else {
    ERRLIGHT_OFF;
  }
} // end loop 


volatile float current_floating_pos = 0.0F;
volatile int32_t current_step_pos = 0;
volatile uint32_t delta_steps = 0;

volatile float vel = 0.0F; 
volatile float move_counter = 0.0F;

volatile boolean setBlock = false;


void ucBusDrop_onPacketARx(uint8_t* inBufferA, volatile uint16_t len){
  // don't execute when we have been given a set-position block 
  if(setBlock) return;
  // don't execute if we are currently homing
  if(homing) return;
  //DEBUG2PIN_TOGGLE;
  // last move is done, convert back steps -> float,
  if(spu_was_set){
    current_floating_pos = current_step_pos / old_spu;
    current_step_pos = lroundf(current_floating_pos * spu);
    spu_was_set = false;
  } else {
    current_floating_pos = current_step_pos / spu;
  }
  vel = 0.0F; // reset zero in case packet is not move 
  uint8_t bptr = 0;
  // switch bus packet types 
  switch(inBufferA[0]){
    case UB_AK_GOTOPOS:
      {
        bptr = axis_pick * 4 + 1;
        chunk_float32 target = {
          .bytes = { inBufferA[bptr], inBufferA[bptr + 1], inBufferA[bptr + 2], inBufferA[bptr + 3] }
        };
        /*
        chunk_float64 target = { 
          .bytes = { inBuffer[bptr], inBuffer[bptr + 1], inBuffer[bptr + 2], inBuffer[bptr + 3],
                    inBuffer[bptr + 4], inBuffer[bptr + 5], inBuffer[bptr + 6], inBuffer[bptr + 7] }};
                    */
        float delta = target.f - current_floating_pos;
        // update,
        //move_counter = 0.0F; // shouldn't need this ? try deleting 
        // direction swop,
        if(delta > 0){
          stepper_hw->dir(true);
        } else {
          stepper_hw->dir(false);
        }
        // how many steps, 
        delta_steps = lroundf(abs(delta * spu));
        // what speed 
        vel = abs(delta * spu) / TICKS_PER_PACKET;
        // for str8 r8 
        /*
        if(delta_steps == 0){
          vel = 0.0F;
        } else {
          vel = abs(delta * SPU) / TICKS_PER_PACKET;
        }
        */
        break; // end gotopos 
      }
    case UB_AK_SETPOS:
      {
        // reqest is to set position, not go to it... 
        bptr = axis_pick * 4 + 1;
        chunk_float32 target = {
          .bytes = { inBufferA[bptr], inBufferA[bptr + 1], inBufferA[bptr + 2], inBufferA[bptr + 3] }
        };
        float target_current_pos = target.f;
        int32_t target_current_steps = lroundf(target_current_pos * spu);
        setBlock = true; // don't do step work while these are modified 
        current_floating_pos = target_current_pos;
        current_step_pos = target_current_steps;
        vel = 0;
        delta_steps = 0;
        setBlock = false;
        break;
      }
    default:
      break;
  }  
  //DEBUG2PIN_OFF;
}

void ucBusDrop_onRxISR(void){
  // no-op when given a set block, 
  if(setBlock) return;
  // incremental motion if is homing 
  if(homing != 0){
    switch(homing){
      case HOME_FIRST:
        if(limitIsMade()){
          // traaaaaaansition -> backoff 
          stepper_hw->dir(!homeDir);
          homeStepCounter = 0.0F;
          homePos = 0.0F;
          homing = HOME_BACKOFF;
        } else {
          homeStepCounter += homeStepRate;
          if(homeStepCounter >= 1.0F){
            homeStepCounter -= 1.0F;
            stepper_hw->step();
          }
        }
        break;
      case HOME_BACKOFF:
        homeStepCounter += homeStepRate;
        homePos += homePosRate;
        if(homeStepCounter >= 1.0F){  // backoff motion 
          homeStepCounter -= 1.0F;
          stepper_hw->step();
        }
        if(homePos >= homeOffset){ // until more than 2mm away 
          // traaaaaaaaaaaaaansition -> end 
          homing = 0;
        }
        break;
      default:
        homing = 0;
    }
    return;
  }
  // normal step operation 
  //DEBUG2PIN_TOGGLE;
  move_counter += vel;
  boolean move_check = (move_counter > 1.0F);
  //DEBUG2PIN_TOGGLE;
  if(move_check){
    move_counter -= 1.0F;
    if(delta_steps == 0){
      // nothing 
    } else {
      //DEBUG1PIN_TOGGLE;
      stepper_hw->step();
      delta_steps --;
      if(stepper_hw->getDir()){
        current_step_pos ++;
      } else {
        current_step_pos --;
      }
    }
  }
}

