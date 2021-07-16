/*
tempVirtualMachine.js

vm for stepper motors 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the open systems assembly protocol (OSAP) project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

import { PK, TS, VT, EP, TIMES } from '../../osapjs/core/ts.js'

export default function MotorVM(osap, route) {

  // ------------------------------------------------------ OSAP Interfaces, 

  // 0: usb interface
  // 1: bus interface 

  // -------------------------------------------- 2: axis pick 
  let axisPickEP = osap.endpoint()
  axisPickEP.addRoute(PK.route(route).sib(2).end())
  this.setAxisPick = (pick) => {
    let datagram = new Uint8Array(1)
    TS.write('uint8', pick, datagram, 0, true)
    return new Promise((resolve, reject) => {
      axisPickEP.write(datagram, "acked").then(() => {
        resolve()
      }).catch((err) => { reject(err) })  
    })
  }
  
  // -------------------------------------------- 3: axis inversion 
  let axisInvertEP = osap.endpoint()
  axisInvertEP.addRoute(PK.route(route).sib(3).end())
  this.setAxisInversion = (invert) => {
    let datagram = new Uint8Array(1)
    TS.write('boolean', invert, datagram, 0, true)
    return new Promise((resolve, reject) => {
      axisInvertEP.write(datagram, "acked").then(() => {
        resolve()
      }).catch((err) => { reject(err) })  
    })
  }

  // -------------------------------------------- 4: microstep 
  let microstepEP = osap.endpoint()
  microstepEP.addRoute(PK.route(route).sib(4).end())
  this.setMicrostep = (micro) => {
    let datagram = new Uint8Array(1)
    datagram[0] = micro 
    //console.log(datagram[0])
    return new Promise((resolve, reject) => {
      microstepEP.write(datagram, "acked").then(() => {
        resolve()
      }).catch((err) => { reject(err) })
    })
  }

  // -------------------------------------------- 5: steps per unit 
  let spuEP = osap.endpoint()
  spuEP.addRoute(PK.route(route).sib(5).end())
  this.setSPU = (spu) => {
    let datagram = new Uint8Array(4)
    TS.write('float32', spu, datagram, 0, true)
    return new Promise((resolve, reject) => {
      spuEP.write(datagram, "acked").then(() => {
        resolve()
      }).catch((err) => { reject(err) })  
    })
  }

  // -------------------------------------------- 6: active current scaling 
  let cscaleEP = osap.endpoint()
  cscaleEP.addRoute(PK.route(route).sib(6).end())
  this.setCScale = (cscale) => {
    let datagram = new Uint8Array(4)
    TS.write('float32', cscale, datagram, 0, true)
    return new Promise((resolve, reject) => {
      cscaleEP.write(datagram, "acked").then(() => {
        resolve()
      }).catch((err) => { reject(err) })  
    })
  }

  // -------------------------------------------- 7: homing 
  let homeEP = osap.endpoint()
  homeEP.addRoute(PK.route(route).sib(7).end())
  this.home = () => {
    let rate = config.homeRate
    let offset = config.homeOffset
    let datagram = new Uint8Array(8)
    TS.write('float32', rate, datagram, 0, true)
    TS.write('float32', offset, datagram, 4, true)
    return new Promise((resolve, reject) => {
      homeEP.write(datagram, "acked").then(() => {
        resolve()
      }).catch((err) => { reject(err) })
    })
  }

  // -------------------------------------------- 8: homing state 
  let homeStateQuery = osap.query(PK.route(route).sib(8).end())
  this.getHomeState = () => {
    return new Promise((resolve, reject) => {
      homeStateQuery.pull().then((data) => {
        if(data[0] > 0){
          resolve(true)
        } else {
          resolve(false)
        }
      }).catch((err) => { reject(err) })
    })
  }

  this.awaitHomeComplete = () => {
    return new Promise((resolve, reject) => {
      let check = () => {
        this.getHomeState().then((homing) => {
          if(homing){
            setTimeout(check, 50)
          } else {
            resolve()
          }
        }).catch((err) => { reject(err) })
      } // end 'check' def 
      setTimeout(check, 50)
    })
  }

  // ------------------------------------------------------ JS API

  // default config, 
  let config = {
    axisPick: 0, 
    axisInversion: false, 
    SPU: 320,
    microstep: 16, // 1, 4, 8, 16, 32, 64 
    currentScale: 0.2,
    homeRate: 10,
    homeOffset: 10
  }

  // update config 
  this.settings = (settings, publish) => {
    // could do: on each setup change, if flag 'publish' set, do network work here 
    // would mean this becomes async... 
    // also, check microstep is allowed 
    if(settings.microstep){
      switch(settings.microstep){
        case 1:
        case 4:
        case 8:
        case 16:
        case 32:
        case 64:
          break;
        default:
          throw new Error("microstep motor setting must be 1 or power of 2, max 64")
          break;
      }
    }
    for(let key in settings){
      if(key in config){
        config[key] = settings[key]
      } else {
        console.warn(`motor settings spec key '${key}', it doesn't exist!`)
      }
    }
  }

  // publish settings to motors 
  this.setup = async () => {
    try {
      await this.setAxisPick(config.axisPick)
      await this.setAxisInversion(config.axisInversion)
      await this.setMicrostep(config.microstep)
      await this.setSPU(config.SPU)
      await this.setCScale(0.0) // default: off 
    } catch (err) { throw err }
  }

  // enable the thing,
  this.enable = async () => {
    try {
      await this.setCScale(config.currentScale)
    } catch (err) { throw err }
  }

  this.disable = async () => {
    try {
      await this.setCScale(0.0)
    } catch (err) { throw err }
  }

}