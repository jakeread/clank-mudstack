/*
filamentExperiment.js

data generator for NIST 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the open systems assembly protocol (OSAP) project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

'use strict'

import { Button, TextInput } from '../../osapjs/client/interface/basics.js'
import { AutoPlot } from '../../osapjs/client/components/autoPlot.js'
import { SaveFile } from '../../osapjs/client/utes/saveFile.js'

export default function FilamentExperiment(clank, hotendVm, loadcellVm, xPlace, yPlace) {
  let title = new Button(xPlace, yPlace, 104, 34, 'NIST')

  let temp = 180
  let speeds = [0.25, 0.5, 0.75, 1, 1.25, 1.5, 1.75, 2, 2.25, 2.5, 3, 3.5, 4, 4.5, 5, 5.5, 6, 6.5, 7]
  let lengths = [1.25, 2.5, 3.75, 5, 6.25, 7.5, 8.75, 10, 11.25, 12.5, 15, 17.5, 20, 22.5, 25, 27.5, 30, 32.5, 35]

  if (speeds.length != lengths.length) throw new Error("bad speed / length set for experiment")

  let tempPlot = new AutoPlot(xPlace + 120, yPlace, 420, 250, 'temp')
  tempPlot.setHoldCount(1500)
  tempPlot.redraw()

  let loadPlot = new AutoPlot(xPlace + 120, yPlace + 270, 420, 250, 'load')
  loadPlot.setHoldCount(1500)
  loadPlot.redraw()

  let speedPlot = new AutoPlot(xPlace + 120, yPlace + 540, 420, 250, 'speed')
  speedPlot.setHoldCount(1500)
  speedPlot.redraw()

  let tempSetBtn = new Button(xPlace, yPlace + 60, 104, 14, 'set temp')
  tempSetBtn.onClick(() => {
    hotendVm.setExtruderTemp(temp).then(() => {
      tempSetBtn.good("ok", 500)
    }).catch((err) => {
      console.error(err)
      tempSetBtn.bad("err", 1000)
    })
  })

  let tempCoolBtn = new Button(xPlace, yPlace + 90, 104, 14, 'cooldown')
  tempCoolBtn.onClick(() => {
    hotendVm.setExtruderTemp(0).then(() => {
      tempCoolBtn.good("ok", 500)
    }).catch((err) => {
      console.error(err)
      tempCoolBtn.bad("err", 500)
    })
  })

  let pullExtruderTest = () => {
    return new Promise((resolve, reject) => {
      let ts = "txd"  // temp state
      let ls = "txd"  // load state
      let ss = "txd"  // speed state
      let res = {
        temp: undefined,
        speed: undefined,
        load: undefined,
      }
      let check = () => {
        if (ts == "rxd" && ls == "rxd" && ss == "rxd") {
          resolve(res)
        } else if (ts == "error" || ls == "error" || ss == "error") {
          reject(`temp: ${ts}, load: ${ls}, speed: ${ss}`)
        }
      }
      hotendVm.getExtruderTemp().then((temp) => {
        res.temp = temp
        ts = "rxd"
        check()
      }).catch((err) => {
        console.log("hotend pull err")
        console.error(err)
        ts = "error"
        check()
      })
      loadcellVm.getReading().then((load) => {
        res.load = load
        ls = "rxd"
        check()
      }).catch((err) => {
        console.log("loadcell pull err")
        console.error(err)
        ls = "error"
        check()
      })
      clank.motion.getSpeeds().then((speeds) => {
        $(speedOutput.elem).html(`<pre>X: ${speeds.X.toFixed(3)} \nY: ${speeds.Y.toFixed(3)} \nZ: ${speeds.Z.toFixed(3)} \nE: ${speeds.E.toFixed(3)} </pre>`)
        res.speed = speeds.E
        ss = "rxd"
        check()
      }).catch((err) => {
        console.log("speeds pull err")
        console.error(err)
        ss = "error"
        check()
      })
    })
  }

  let plotLpBtn = new Button(xPlace, yPlace + 120, 104, 14, 'run plots')
  let plotLp = false
  let plotLpCount = 0
  plotLpBtn.onClick(() => {
    if (plotLp) {
      plotLp = false
      plotLpBtn.good('stopped', 500)
    } else {
      let poll = () => {
        if (!plotLp) {
          plotLpBtn.good('stopped')
          return;
        }
        pullExtruderTest().then((res) => {
          plotLpCount++
          tempPlot.pushPt([plotLpCount, res.temp])
          tempPlot.redraw()
          loadPlot.pushPt([plotLpCount, res.load])
          loadPlot.redraw()
          speedPlot.pushPt([plotLpCount, res.speed])
          speedPlot.redraw()
          setTimeout(poll, 50)
        }).catch((err) => {
          plotLp = false
          plotLpBtn.bad("err", 500)
        })
      }
      plotLp = true
      poll()
    }
  })

  let testLpBtn = new Button(xPlace, yPlace + 150, 104, 14, 'run test')
  let testIndice = new TextInput(xPlace, yPlace + 180, 110, 20, '0')
  let speedOutput = new Button(xPlace, yPlace + 210, 104, 84, 'speeds')
  let testing = false
  let testLpCount = 0

  testLpBtn.onClick(() => {
    if (testing) {
      testing = false
      testLpBtn.good('stopped', 500)
    } else {
      // condition to start test, turn off plot if currently operating
      plotLp = false
      // testing is on 
      testing = true
      // collect some move, 
      let indice = parseInt(testIndice.value)
      let move = {
        position: {
          X: 0, 
          Y: 0, 
          Z: 0,
          E: lengths[indice]
        },
        rate: speeds[indice] * 60 
      }
      console.warn('rate', move.rate)
      // error escape, 
      let badness = (err) => {
        testing = false
        console.error(err)
        testLpBtn.bad("err", 1000)
      }
      // setup / start move pusher
      let push = async () => {
        try {
          await loadcellVm.tare()
          await clank.motion.addMoveToQueue(move)
        } catch (err) {
          console.log("push err")
          badness(err)
        }
        console.warn('adding test moves: complete')
      }
      push()
      // reset plots 
      tempPlot.reset()
      loadPlot.reset()
      speedPlot.reset()
      // pull extruder data on 50ms, and gather 
      let data = []
      let poll = () => {
        if (!testing) return;
        pullExtruderTest().then((res) => {
          // add to data, 
          data.push(res)
          // plot, 
          testLpCount++
          tempPlot.pushPt([testLpCount, res.temp])
          tempPlot.redraw()
          loadPlot.pushPt([testLpCount, res.load])
          loadPlot.redraw()
          speedPlot.pushPt([testLpCount, res.speed])
          speedPlot.redraw()
          setTimeout(poll, 50)
        }).catch((err) => {
          console.log("exp pull err")
          badness(err)
        })
      }
      // startup 
      setTimeout(poll, 100)
      // when motion ends, complete 
      let checkMotion = () => {
        if (data[data.length - 1].speed == 0) {
          testLpBtn.good("complete", 1000)
          console.warn('test complete')
          setTimeout(() => {
            testing = false 
            testLpCount = 0 
            console.log(data)
            let indice = parseInt(testIndice.value)
            testIndice.value = indice + 1
            SaveFile(data, 'json', `extruderTest-${temp}_${indice}-${move.rate/60}-${move.position.E}`)
          }, 90000)
        } else {
          setTimeout(checkMotion, 100)
        }
      }
      setTimeout(checkMotion, 3000)
    }
  })
}