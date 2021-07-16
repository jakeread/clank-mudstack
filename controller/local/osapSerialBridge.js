/*
osap-usb-bridge.js

osap bridge to firmwarelandia

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2020

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the open systems assembly protocol (OSAP) project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

// big s/o to https://github.com/standard-things/esm for allowing this
import OSAP from '../osapjs/core/osapRoot.js'
import { TS, PK, TIMES } from '../osapjs/core/ts.js'

import WSSPipe from './utes/wssPipe.js'

import SerialPort from 'serialport'
import Delimiter from '@serialport/parser-delimiter'
import ByteLength from '@serialport/parser-byte-length'

import COBS from './utes/cobs.js'

// temporary...
import { reverseRoute } from '../osapjs/core/osapLoop.js'

let LOGPHY = false

// we include an osap object - a node
let osap = new OSAP()
osap.name = "local-usb-bridge"
osap.description = "node featuring wss to client and usbserial cobs connection to hardware"

// -------------------------------------------------------- WSS VPort

let wssVPort = osap.vPort("wssVPort")   // 0
let serVPort = osap.vPort("serVPort");  // 1

// test endpoint, 

let ep2 = osap.endpoint()

ep2.onData = (buffer) => {
  return new Promise((resolve, reject) => {
    TIMES.delay(250).then(() => {
      console.log('EP2 clear')
      resolve()  
    })
  })
}

// then resolves with the connected webSocketServer to us 
let LOGWSSPHY = false 
wssVPort.maxSegLength = 16384
WSSPipe.start().then((ws) => {
  // no loop or init code, 
  // implement status 
  let status = "open"
  wssVPort.cts = () => {
    if (status == "open") {
      return true
    } else {
      return false
    }
  }
  // implement rx,
  ws.onmessage = (msg) => {
    if (LOGWSSPHY) console.log('PHY WSS Recv')
    if (LOGWSSPHY) TS.logPacket(msg.data)
    wssVPort.receive(msg.data)
  }
  // implement transmit 
  wssVPort.send = (buffer) => {
    if (LOGWSSPHY) console.log('PHY WSS Send')
    if (LOGWSSPHY) PK.logPacket(buffer)
    ws.send(buffer)
  }
  // local to us, 
  ws.onerror = (err) => {
    status = "closed"
    console.log('wss error', err)
  }
  ws.onclose = (evt) => {
    status = "closed"
    // because this local script is remote-kicked,
    // we shutdown when the connection is gone
    console.log('wss closes, exiting')
    process.exit()
    // were this a standalone network node, this would not be true
  }
})

// -------------------------------------------------------- USB Serial VPort

serVPort.maxSegLength = 512 // lettuce do this for embedded expectations
let LOGSER = false
let LOGSERTX = false
let LOGSERRX = false 
let findSerialPort = (pid) => {
  if (LOGSER) console.log(`SERPORT hunt for productId: ${pid}`)
  return new Promise((resolve, reject) => {
    SerialPort.list().then((ports) => {
      let found = false
      for (let port of ports) {
        if (port.productId === pid) {
          found = true
          resolve(port.path)
          break
        }
      }
      if (!found) reject(`serialport w/ productId: ${pid} not found`)
    }).catch((err) => {
      reject(err)
    })
  })
}

let opencount = 0

// options: passthrough for node-serialport API
let startSerialPort = (pid, options) => {
  // implement status
  let status = "opening"
  let flowCondition = () => { return false }
  serVPort.cts = () => { 
    if(status = "open" && flowCondition()){
      return true
    } else {
      return false 
    }
  }
  // open up,
  findSerialPort(pid).then((com) => {
    if (true) console.log(`SERPORT contact at ${com}, opening`)
    let port = new SerialPort(com, options)
    let pcount = opencount
    opencount++
    port.on('open', () => {
      // we track remote open spaces, this is stateful per link... 
      let rcrxb = 4 // see vt_usbSerial.h for how many remote spaces we can push to, and use that # - 1,  
      console.log(`SERPORT at ${com} #${pcount} OPEN`)
      // is now open,
      status = "open"
      // to get, use delimiter
      let parser = port.pipe(new Delimiter({ delimiter: [0] }))
      //let parser = port.pipe(new ByteLength({ length: 1 }))
      flowCondition = () => {
        return (rcrxb > 0)
      }
      // implement rx
      parser.on('data', (buf) => {
        let decoded = COBS.decode(buf)
        //if(decoded[0] == 77) console.log('rx 77', decoded[1])
        if (LOGSERRX) {
          console.log('SERPORT Rx')
          PK.logPacket(decoded)
        }
        // 1st byte is count of how many acks this loop, 
        rcrxb += decoded[0] 
        //console.log('rcrxb', rcrxb)
        // hotswitch low level escapes 
        if(decoded[2] == PK.LLESCAPE.KEY){
          let str = TS.read('string', decoded, 2, true).value
          console.log('LL:', str)
        } else {
          if(decoded.length > 1) serVPort.receive(decoded.slice(1))
        }
      })
      // implement tx
      serVPort.send = (buffer) => {
        rcrxb -= 1 
        //console.log('rcrxb', rcrxb)
        port.write(COBS.encode(buffer))
        if (LOGSERTX) {
          console.log('SERPORT Tx')
          PK.logPacket(buffer)
        }
      }
      // phy handle to close,
      serVPort.requestClose = () => {
        console.log(`CLOSING #${pcount}`)
        status = "closing"
        port.close(() => { // await close callback, add 1s buffer
          console.log(`SERPORT #${pcount} closed`)
          status = "closed"
        })
      }
    }) // end on-open
    port.on('error', (err) => {
      status = "closing"
      console.log(`SERPORT #${pcount} ERR`, err)
      port.close(() => { // await close callback, add 1s buffer
        console.log(`SERPORT #${pcount} CLOSED`)
        status = "closed"
      })
    })
    port.on('close', (evt) => {
      console.log('FERME LA')
      status = "closing"
      console.log(`SERPORT #${pcount} closed`)
    })
  }).catch((err) => {
    if (LOGSER) console.log(`SERPORT cannot find device at ${pid}`, err)
    status = "closed"
  })
} // end start serial

startSerialPort('8031')

serVPort.requestOpen = () => {
  startSerialPort('8031')
}