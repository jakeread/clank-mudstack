#include <Arduino.h>

#include "indicators.h"
#include "syserror.h"
#include "osape-d51/d51ClockBoss.h"

#include "smoothie/SmoothieRoll.h"

#include "osape-d51/osape/osap/osap.h"
#include "osape-d51/vertices/vt_usbSerial.h"
#include "osape-d51/vertices/vt_ucBusHead.h"
#include "osape-d51/vertices/ucBusHead.h"
// -------------------------------------------------------- SMOOTHIE HANDLES 
// *should* go to smoothieRoll.h, duh 

boolean smoothie_is_queue_empty(void){
  return conveyor->queue.is_empty();
}

boolean smoothie_is_moving(void){
  return (smoothieRoll->actuators[0]->is_moving() 
        || smoothieRoll->actuators[1]->is_moving() 
        || smoothieRoll->actuators[2]->is_moving()
        || smoothieRoll->actuators[3]->is_moving()
        || !smoothie_is_queue_empty());
}

// -------------------------------------------------------- OSAP ENDPOINTS SETUP

// -------------------------------------------------------- MOVE QUEUE ENDPOINT 

EP_ONDATA_RESPONSES onMoveData(uint8_t* data, uint16_t len){
  // can we load it?
  if(!conveyor->is_queue_full()){
    // read from head, 
    uint16_t ptr = 0;
    // feedrate is 1st, 
    chunk_float32 feedrateChunk = { .bytes = { data[ptr ++], data[ptr ++], data[ptr ++], data[ptr ++] } };
    // get positions XYZE
    chunk_float32 targetChunks[4];
    targetChunks[0] = { .bytes = { data[ptr ++], data[ptr ++], data[ptr ++], data[ptr ++] } };
    targetChunks[1] = { .bytes = { data[ptr ++], data[ptr ++], data[ptr ++], data[ptr ++] } };
    targetChunks[2] = { .bytes = { data[ptr ++], data[ptr ++], data[ptr ++], data[ptr ++] } };
    targetChunks[3] = { .bytes = { data[ptr ++], data[ptr ++], data[ptr ++], data[ptr ++] } };
    // check and load, 
    if(feedrateChunk.f < 0.01){
      sysError("ZERO FR");
      return EP_ONDATA_ACCEPT; // ignore this & ack 
    } else {
      // do load 
      float target[3] = {targetChunks[0].f, targetChunks[1].f, targetChunks[2].f };
      //sysError("targets, rate: " + String(target[0], 6) + ", " + String(target[1], 6) + ", " + String(target[2], 6) + ", " + String(feedrateChunk.f, 6));
      planner->append_move(target, SR_NUM_MOTORS, feedrateChunk.f, targetChunks[3].f); // mm/min -> mm/sec 
      return EP_ONDATA_ACCEPT; 
    }
  } else {
    // await, try again next loop 
    return EP_ONDATA_WAIT;
  }
}

vertex_t* moveQueueEp = osapBuildEndpoint("moveQueue", onMoveData, nullptr);  // 2

// -------------------------------------------------------- POSITION ENDPOINT 

EP_ONDATA_RESPONSES onPositionSet(uint8_t* data, uint16_t len);
boolean beforePositionQuery(void);

vertex_t* positionEp = osapBuildEndpoint("position", onPositionSet, beforePositionQuery); // 3

EP_ONDATA_RESPONSES onPositionSet(uint8_t* data, uint16_t len){
  // only if it's not moving, 
  if(smoothie_is_moving()){
    return EP_ONDATA_REJECT;
  } else {
    uint16_t ptr = 0;
    chunk_float32 targetChunks[4];
    targetChunks[0] = { .bytes = { data[ptr ++], data[ptr ++], data[ptr ++], data[ptr ++] } };
    targetChunks[1] = { .bytes = { data[ptr ++], data[ptr ++], data[ptr ++], data[ptr ++] } };
    targetChunks[2] = { .bytes = { data[ptr ++], data[ptr ++], data[ptr ++], data[ptr ++] } };
    targetChunks[3] = { .bytes = { data[ptr ++], data[ptr ++], data[ptr ++], data[ptr ++] } };
    // 
    float set[4] = { targetChunks[0].f, targetChunks[1].f, targetChunks[2].f, targetChunks[3].f };
    // ...
    planner->set_position(set, 4);
    return EP_ONDATA_ACCEPT;
  }
}

boolean beforePositionQuery(void){
  // write new pos data periodically, 
  uint8_t posData[16];
  uint16_t poswptr = 0;
  ts_writeFloat32(smoothieRoll->actuators[0]->floating_position, posData, &poswptr);
  ts_writeFloat32(smoothieRoll->actuators[1]->floating_position, posData, &poswptr);
  ts_writeFloat32(smoothieRoll->actuators[2]->floating_position, posData, &poswptr);
  ts_writeFloat32(smoothieRoll->actuators[3]->floating_position, posData, &poswptr);
  memcpy(positionEp->ep->data, posData, 16);
  positionEp->ep->dataLen = 16;
  return true; 
}

// -------------------------------------------------------- CURRENT SPEEDS

boolean beforeSpeedQuery(void);

vertex_t* speedEp = osapBuildEndpoint("speed", nullptr, beforeSpeedQuery);

boolean beforeSpeedQuery(void){
  // collect actuator speeds, 
  uint8_t speedData[16];
  uint16_t wptr = 0;
  ts_writeFloat32(smoothieRoll->actuators[0]->get_current_speed(), speedData, &wptr);
  ts_writeFloat32(smoothieRoll->actuators[1]->get_current_speed(), speedData, &wptr);
  ts_writeFloat32(smoothieRoll->actuators[2]->get_current_speed(), speedData, &wptr);
  ts_writeFloat32(smoothieRoll->actuators[3]->get_current_speed(), speedData, &wptr);
  memcpy(speedEp->ep->data, speedData, 16);
  speedEp->ep->dataLen = 16;
  return true;
}

// -------------------------------------------------------- MOTION STATE EP 

boolean beforeMotionStateQuery(void);

vertex_t* motionStateEp = osapBuildEndpoint("motionState", nullptr, beforeMotionStateQuery);  // 4

boolean beforeMotionStateQuery(void){
  uint8_t motion;
  if(smoothieRoll->actuators[0]->is_moving() || smoothieRoll->actuators[1]->is_moving() || smoothieRoll->actuators[2]->is_moving() || !smoothie_is_queue_empty()){
    motion = true;
  } else {
    motion = false;
  }
  motionStateEp->ep->data[0] = motion;
  motionStateEp->ep->dataLen = 1;
  //sysError("motion query " + String(motion));
  return true; 
}

// -------------------------------------------------------- WAIT TIME EP 

EP_ONDATA_RESPONSES onWaitTimeData(uint8_t* data, uint16_t len){
  // writes (in ms) how long to wait the queue before new moves are executed 
  // i.e. stack flow hysteresis 
  uint32_t ms;
  uint16_t ptr = 0;
  ts_readUint32(&ms, data, &ptr);
  conveyor->setWaitTime(ms);
  //sysError("set wait " + String(ms));
  return EP_ONDATA_ACCEPT;
}

vertex_t* waitTimeEp = osapBuildEndpoint("waitTime", onWaitTimeData, nullptr);  // 5 

// -------------------------------------------------------- ACCEL SETTTINGS 

EP_ONDATA_RESPONSES onAccelSettingsData(uint8_t* data, uint16_t len){
  // should be 4 floats: new accel values per-axis 
  uint16_t ptr = 0;
  chunk_float32 targetChunks[4];
  targetChunks[0] = { .bytes = { data[ptr ++], data[ptr ++], data[ptr ++], data[ptr ++] } };
  targetChunks[1] = { .bytes = { data[ptr ++], data[ptr ++], data[ptr ++], data[ptr ++] } };
  targetChunks[2] = { .bytes = { data[ptr ++], data[ptr ++], data[ptr ++], data[ptr ++] } };
  targetChunks[3] = { .bytes = { data[ptr ++], data[ptr ++], data[ptr ++], data[ptr ++] } };
  // they're in here 
  for(uint8_t m = 0; m < SR_NUM_MOTORS; m ++){
    smoothieRoll->actuators[m]->set_accel(targetChunks[m].f);
  }
  // assuming that went well, 
  return EP_ONDATA_ACCEPT;
}

vertex_t* accelSettingsEp = osapBuildEndpoint("accelSettings", onAccelSettingsData, nullptr);

// -------------------------------------------------------- RATES SETTINGS 

EP_ONDATA_RESPONSES onRateSettingsData(uint8_t* data, uint16_t len){
  // should be 4 floats: new accel values per-axis 
  uint16_t ptr = 0;
  chunk_float32 targetChunks[4];
  targetChunks[0] = { .bytes = { data[ptr ++], data[ptr ++], data[ptr ++], data[ptr ++] } };
  targetChunks[1] = { .bytes = { data[ptr ++], data[ptr ++], data[ptr ++], data[ptr ++] } };
  targetChunks[2] = { .bytes = { data[ptr ++], data[ptr ++], data[ptr ++], data[ptr ++] } };
  targetChunks[3] = { .bytes = { data[ptr ++], data[ptr ++], data[ptr ++], data[ptr ++] } };
  // they're in here 
  for(uint8_t m = 0; m < SR_NUM_MOTORS; m ++){
    smoothieRoll->actuators[m]->set_max_rate(targetChunks[m].f);
  }
  // assuming that went well, 
  return EP_ONDATA_ACCEPT;
}

vertex_t* rateSettingsEp = osapBuildEndpoint("rateSettings", onRateSettingsData, nullptr);

// -------------------------------------------------------- SETUP 

void setup() {
  ERRLIGHT_SETUP;
  CLKLIGHT_SETUP;
  DEBUG1PIN_SETUP;
  DEBUG2PIN_SETUP;
  DEBUG3PIN_SETUP;
  DEBUG4PIN_SETUP;
  // osap
  osapSetup();
  // ports 
  vt_usbSerial_setup();
  osapAddVertex(vt_usbSerial);    // 0
  vt_ucBusHead_setup();
  osapAddVertex(vt_ucBusHead);    // 1
  // move to queue 
  osapAddVertex(moveQueueEp);     // 2
  // position 
  osapAddVertex(positionEp);      // 3
  // speed 
  osapAddVertex(speedEp);         // 4 
  // motion state 
  osapAddVertex(motionStateEp);   // 5
  // set wait time (ms)
  osapAddVertex(waitTimeEp);      // 6
  // acceler8 settings
  osapAddVertex(accelSettingsEp); // 7 
  // r8 settings 
  osapAddVertex(rateSettingsEp);  // 8 
  // smoothie (and frequency of loop below)
  smoothieRoll->init(50000);
  // 100kHz base (10us period)
  // 25kHz base (40us period)
  d51ClockBoss->start_ticker_a(20);
  // l i g h t s 
  ERRLIGHT_ON;
  CLKLIGHT_ON;
}

uint32_t ledTickCount = 0;
void loop() {
  // write ~ every second, transmit on chb to drop 1 
  // check indices on the way down / up ... was shifting, are not anymore 
  osapLoop();
  conveyor->on_idle(nullptr);
  // blink
  ledTickCount ++;
  if(ledTickCount > 1024){
    // blink 
    DEBUG1PIN_TOGGLE;
    ledTickCount = 0;
  }
} // end loop 

// runs on period defined by timer_a setup: 
volatile uint8_t tick_count = 0;
uint8_t motion_packet[64]; // three floats bb, space 

void TC0_Handler(void){
  // runs at 100KHz (10us period), eats about 2.5us, or 5 if the transmit occurs 
  TC0->COUNT32.INTFLAG.bit.MC0 = 1;
  TC0->COUNT32.INTFLAG.bit.MC1 = 1;
  tick_count ++;
  // do bus action first: want downstream clocks to be deterministic-ish
  ucBusHead_timerISR();
  // do step tick 
  smoothieRoll->step_tick();
  // every n ticks, ship position? 
  // was / 25...
  if(tick_count > 25){
    tick_count = 0;
    uint16_t mpptr = 0; // motion packet pointer 
    if(planner->do_set_position){
      motion_packet[mpptr ++] = UB_AK_SETPOS;
      planner->do_set_position = false;
    } else {
      motion_packet[mpptr ++] = UB_AK_GOTOPOS;
    }
    // XYZE 
    ts_writeFloat32(smoothieRoll->actuators[0]->floating_position, motion_packet, &mpptr);
    ts_writeFloat32(smoothieRoll->actuators[1]->floating_position, motion_packet, &mpptr);
    ts_writeFloat32(smoothieRoll->actuators[2]->floating_position, motion_packet, &mpptr);
    ts_writeFloat32(smoothieRoll->actuators[3]->floating_position, motion_packet, &mpptr);
    // write packet, put on ucbus
    //DEBUG3PIN_ON;
    ucBusHead_transmitA(motion_packet, 17);
    //DEBUG3PIN_OFF;
  }
}
