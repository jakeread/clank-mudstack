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
import { TIMES } from '../../osapjs/core/ts.js'

export default function StiffnessMapper(clank, loadcellVm, xPlace, yPlace) {
  let title = new Button(xPlace, yPlace, 104, 34, 'Stiffness Mapper')

  let loadPlot = new AutoPlot(xPlace + 120, yPlace, 420, 250, 'load')
  loadPlot.setHoldCount(1500)
  loadPlot.redraw()

  let testLpBtn = new Button(xPlace, yPlace + 60, 104, 14, 'run test')
  let testIndice = new TextInput(xPlace, yPlace + 90, 110, 20, '0')
  let testing = false
  let testLpCount = 0

  testLpBtn.onClick(() => {
    if (testing) {
      testLpBtn.bad('already testing', 1000)
    } else {
      stiffnessTest()
      testLpBtn.good('...', 500)
    }
  })

  let startPos = [140, 60]
  let gridSpace = [25, 25]
  let gridSize = [4, 8]

  let stiffnessTest = async () => {
    testing = true
    let stash = new Array(gridSize[0])
    await clank.motion.setWaitTime(10)
    for(let x = 0; x < gridSize[0]; x ++){
      stash[x] = new Array(gridSize[1])
      for(let y = 0; y < gridSize[1]; y ++){
        let xPos = startPos[0] + gridSpace[0] * x
        let yPos = startPos[1] + gridSpace[1] * y
        await clank.motion.addMoveToQueue({
          position: {
            X: xPos,
            Y: yPos,
            Z: 5
          },
          rate: 2000
        })
        await clank.motion.awaitMotionEnd()
        console.warn(`hit ${xPos}, ${yPos}`)
        await clank.motion.addMoveToQueue({
          position:{
            X: xPos,
            Y: yPos,
            Z: 3.5
          },
          rate: 25
        })
        stash[x][y] = new Array()
        await loadcellVm.tare()
        while(await clank.motion.getMotionState()){
          let cp = await clank.motion.getPos()
          let load = await loadcellVm.getReading()
          stash[x][y].push([cp.Z, load])
          await TIMES.delay(10)
        }
        // in here... run loop? au-maunel with motion-end ? 
        await clank.motion.awaitMotionEnd()
        await clank.motion.addMoveToQueue({
          position:{
            X: xPos,
            Y: yPos,
            Z: 5
          },
          rate: 500
        })
        await clank.motion.awaitMotionEnd()
      }
    }
    await clank.motion.setWaitTime(750)
    console.log(stash)
    SaveFile(stash, 'json', `stiffnessTest`)
    console.warn('testing done')
    testing = false 
  }

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
            SaveFile(data, 'json', `extruderTest-${temp}_${indice}-${move.rate / 60}-${move.position.E}`)
          }, 90000)
        } else {
          setTimeout(checkMotion, 100)
        }
      }
      setTimeout(checkMotion, 3000)
    }
  })
}