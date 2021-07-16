/*
tempVirtualMachine.js

vm for heater modules 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the open systems assembly protocol (OSAP) project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

import { PK, TS, VT, EP, TIMES } from '../../osapjs/core/ts.js'

export default function TempVM(osap, route) {
  // set a temp 
  let tempSetEP = osap.endpoint()
  tempSetEP.addRoute(PK.route(route).sib(2).end())
  this.setExtruderTemp = (temp) => {
    return new Promise((resolve, reject) => {
      let datagram = new Uint8Array(4)
      TS.write('float32', temp, datagram, 0, true)
      tempSetEP.write(datagram, "acked").then(() => {
        resolve()
      }).catch((err) => { reject(err) })
    })
  }

  // query current temp 
  let tempQuery = osap.query(PK.route(route).sib(3).end())
  this.getExtruderTemp = () => {
    return new Promise((resolve, reject) => {
      tempQuery.pull().then((data) => {
        let temp = TS.read('float32', data, 0, true)
        resolve(temp)
      }).catch((err) => { reject(err) })
    })
  }

  // query current heater effort 
  let outputQuery = osap.query(PK.route(route).sib(4).end())
  this.getExtruderTempOutput = () => {
    return new Promise((resolve, reject) => {
      outputQuery.pull().then((data) => {
        let effort = TS.read('float32', data, 0, true)
        resolve(effort)
      }).catch((err) => { reject(err) })
    })
  }

  // set PID terms 
  let tempPIDTermsEP = osap.endpoint()
  tempPIDTermsEP.addRoute(PK.route(route).sib(5).end())
  this.setPIDTerms = (vals) => {
    return new Promise((resolve, reject) => {
      let datagram = new Uint8Array(12)
      TS.write('float32', vals[0], datagram, 0, true)
      TS.write('float32', vals[1], datagram, 4, true)
      TS.write('float32', vals[2], datagram, 8, true)
      tempPIDTermsEP.write(datagram, "acked").then(() => {
        resolve()
      }).catch((err) => { reject(err) })
    })
  }
}