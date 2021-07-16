/*
clankVirtualMachine.js

vm for Clank-FXY / all things physically attached (end effector not included)

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the open systems assembly protocol (OSAP) project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

import { PK, TS, VT, EP, TIMES } from '../../osapjs/core/ts.js'
import MotionVM from './motionVirtualMachine.js'
import MotorVM from './motorVirtualMachine.js'

/* cz-head
0: serialport 
1: bus head
2: add to move queue 
3: set position 
4: get position 
*/

export default function ClankVM(osap) {

  // ------------------------------------------------------ NET LOCATION OF BUS HEAD 

  let headRoute = PK.route().sib(0).pfwd().sib(1).pfwd().end()

  // ------------------------------------------------------ MOTION
  // with base route -> embedded smoothie instance 
  this.motion = new MotionVM(osap, headRoute)

  // .settings() for rates and accels, 
  this.motion.settings({
    accel: {  // mm/sec^2 
      X: 1000,  // 1500
      Y: 1000,  // 1500
      Z: 150,   // 300, 
      E: 150    // 500 
    },
    maxRate: {
      X: 100,   // 100
      Y: 100,   // 100 
      Z: 50,    // 50 
      E: 50    // 100 
    }
  })

  // ------------------------------------------------------ MOTORS

  // clank fxy:
  // AXIS   SPU     INVERT    BUS
  // X:     320     false     1
  // YL:    320     true      2
  // YR:    320     false     3
  // ZLF:   x       x         4 0b100
  // ZLR:   x                 5 0b101
  // ZRF:   x                 6 0b110
  // ZRR:   x                 7 0b111
  // E: Claystack:            
  //        x       x         8
  // E: included here (?) shouldn't be 
  // E:     550     true 
  // per bondtech, for BMG on 16 microsteps, do 415: we are 32 microsteps 
  // https://www.bondtech.se/en/customer-service/faq/ 
  // however, this is measured & calibrated: 830 was extruding 75mm for a 50mm request 

  this.motors = {
    X: new MotorVM(osap, PK.route(headRoute).sib(1).bfwd(1).end()),
    YL: new MotorVM(osap, PK.route(headRoute).sib(1).bfwd(2).end()),
    YR: new MotorVM(osap, PK.route(headRoute).sib(1).bfwd(3).end()),
    ZLF: new MotorVM(osap, PK.route(headRoute).sib(1).bfwd(4).end()),
    ZLR: new MotorVM(osap, PK.route(headRoute).sib(1).bfwd(5).end()),
    ZRF: new MotorVM(osap, PK.route(headRoute).sib(1).bfwd(6).end()),
    ZRR: new MotorVM(osap, PK.route(headRoute).sib(1).bfwd(7).end()),
    E: new MotorVM(osap, PK.route(headRoute).sib(1).bfwd(9).end())
  }

  // .settings() just preps for the .init() or whatever other call, 
  this.motors.X.settings({
    axisPick: 0,
    axisInversion: false,
    microstep: 16,
    SPU: 80,
    currentScale: 0.4,
    homeRate: 20, // units / sec
    homeOffset: 5, // units 
  })

  this.motors.YL.settings({
    axisPick: 1,
    axisInversion: true,
    microstep: 16,
    SPU: 80,
    currentScale: 0.4,
    homeRate: 20,
    homeOffset: 5,
  })

  this.motors.YR.settings({
    axisPick: 1,
    axisInversion: false,
    microstep: 16,
    SPU: 80,
    currentScale: 0.4,
    homeRate: 20,
    homeOffset: 5
  })

  let zMotorMicrostep = 16
  let zMotorSPU = 228.571428575
  let zMotorCurrent = 0.5

  this.motors.ZLF.settings({
    axisPick: 2,
    axisInversion: false,
    microstep: zMotorMicrostep,
    SPU: zMotorSPU,
    currentScale: zMotorCurrent,
    homeRate: 10,
    homeOffset: 5
  })

  this.motors.ZLR.settings({
    axisPick: 2,
    axisInversion: false,
    microstep: zMotorMicrostep,
    SPU: zMotorSPU,
    currentScale: zMotorCurrent,
    homeRate: 10,
    homeOffset: 5
  })

  this.motors.ZRF.settings({
    axisPick: 2,
    axisInversion: false,
    microstep: zMotorMicrostep,
    SPU: zMotorSPU,
    currentScale: zMotorCurrent,
    homeRate: 10,
    homeOffset: 5
  })

  this.motors.ZRR.settings({
    axisPick: 2,
    axisInversion: false,
    microstep: zMotorMicrostep,
    SPU: zMotorSPU,
    currentScale: zMotorCurrent,
    homeRate: 10,
    homeOffset: 5
  })

  this.motors.E.settings({
    axisPick: 3,
    axisInversion: true, 
    microstep: 4,
    SPU: 1200,  // 1200 
    currentScale: 0.5, // 0.5, 0.2 for nathan plunger
  })

  // ------------------------------------------------------ setup / handle motor group

  this.setupMotors = async () => {
    for (let mot in this.motors) {
      try {
        await this.motors[mot].setup()
      } catch (err) {
        console.error(`failed to setup ${mot}`)
        throw err
      }
    }
  }

  this.enableMotors = async () => {
    for (let mot in this.motors) {
      try {
        await this.motors[mot].enable()
      } catch (err) {
        console.error(`failed to enable ${mot}`)
        throw err
      }
    }
  }

  this.disableMotors = async () => {
    for (let mot in this.motors) {
      try {
        await this.motors[mot].disable()
      } catch (err) {
        console.error(`failed to disable ${mot}`)
        throw err
      }
    }
  }

  // ------------------------------------------------------ HOMING 

  this.homeZ = async () => {
    // don't home if no z motors,  
    if (!this.motors.ZLF) return
    try {
      await this.motion.awaitMotionEnd()
    } catch (err) { throw err }
    // runs twice: define and then... 
    let oneZHome = async () => {
      try {
        // have to start homing routines "~ synchronously"
        this.motors.ZRF.home()
        this.motors.ZRR.home()
        this.motors.ZLF.home()
        this.motors.ZLR.home()
        // and await each of their completions, 
        await this.motors.ZLF.awaitHomeComplete()
        await this.motors.ZLR.awaitHomeComplete()
        await this.motors.ZRF.awaitHomeComplete()
        await this.motors.ZRR.awaitHomeComplete()
      } catch (err) { 
        console.error(err)
        //throw err 
      }
    }
    try {
      await oneZHome()
      await oneZHome()
    } catch (err) { throw err }
  }

  this.homeXY = async () => {
    try {
      await this.motion.awaitMotionEnd()
      if (this.motors.X) await this.motors.X.home()
      if (this.motors.YL) await this.motors.YL.home()
      if (this.motors.YR) await this.motors.YR.home()
      if (this.motors.X) await this.motors.X.awaitHomeComplete()
      if (this.motors.YL) await this.motors.YL.awaitHomeComplete()
      if (this.motors.YR) await this.motors.YR.awaitHomeComplete()
    } catch (err) { throw err }
  }

  // ------------------------------------------------------ TOOLCHANGER

  let tcServoEP = osap.endpoint()
  tcServoEP.addRoute(PK.route(headRoute).sib(1).bfwd(8).sib(2).end())

  this.setTCServo = (micros) => {
    let wptr = 0
    let datagram = new Uint8Array(4)
    // write micros 
    wptr += TS.write('uint32', micros, datagram, wptr, true)
    // do the shipment
    return new Promise((resolve, reject) => {
      tcServoEP.write(datagram, "acked").then(() => {
        console.warn('tc set', micros)
        resolve()
      }).catch((err) => {
        reject(err)
      })
    })
  }

  this.openTC = () => {
    return this.setTCServo(2000)
  }

  this.closeTC = () => {
    return this.setTCServo(840)
  }

  /*

  // ------------------------------------------------------ TOOL CHANGING

  // tool localization for put-down & pickup, tool statefulness, 
  // from back left 0,0 
  // put-down HE at (23.8, -177) -> (23.8, -222.6) -> release -> (-17.8, -208.6) clear -> (-17.8, -183)
  // { position: {X: num, Y: num, Z: num}, rate: num }

  this.goto = async (pos, rate) => {
    try {
      if (!rate) rate = 6000
      // set remote queue-wait-time 
      await this.setWaitTime(1)
      await delay(5)
      // wait for the stop 
      await this.awaitMotionEnd()
      await this.addMoveToQueue({
        position: { X: pos[0], Y: pos[1], Z: pos[2], E: 0 },
        rate: rate
      })
      await delay(5)
      await this.awaitMotionEnd()
      await this.setWaitTime(100)
    } catch (err) {
      console.error('during goto')
      throw err
    }
  }

  let tools = [{
    pickX: 16.8,
    pickY: -177,
    plunge: -45.6
  }]

  this.dropTool = async (num) => {
    try {
      await this.awaitMotionEnd()
      await this.closeTC()
      let cp = await this.getPos()
      await this.goto([tools[num].pickX, tools[num].pickY, cp.Z])
      console.warn('done setup')
      await this.delta([0, tools[num].plunge, 0])
      await this.openTC()
      await delay(250)
      console.warn('tc open')
      await this.delta([-6, 10, 0])
      await this.delta([0, 50, 0])
      await this.goto([tools[num].pickX, tools[num].pickY, cp.Z])
    } catch (err) {
      console.error(`at T${num} drop`)
      throw err
    }
  }

  this.pickTool = async (num) => {
    try {
      await this.awaitMotionEnd()
      await this.openTC()
      let cp = await this.getPos()
      await this.goto([tools[num].pickX, tools[num].pickY, cp.Z])
      await this.delta([-6, 0, 0])
      await this.delta([0, tools[num].plunge + 10, 0])
      await this.delta([6, -10, 0])
      await this.closeTC()
      await delay(250)
      await this.delta([0, -tools[num].plunge, 0])
      await delay(250)
    } catch (err) {
      console.error(`at T${num} pick`)
      throw err
    }
  }
  */
} // end clank vm 