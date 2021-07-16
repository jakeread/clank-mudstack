/*
SmoothieRoll.h

bottle & state container for the SmoothieWare instance running here

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo projects.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef SMOOTHIEROLL_H_
#define SMOOTHIEROLL_H_

#include <Arduino.h>
// get top level stepTicker, conveyor...
#include "smoothie/libs/StepTicker.h"         // has singleton 'stepTicker'
#include "smoothie/modules/robot/Conveyor.h"  // has singleton 'conveyor' 
#include "smoothie/modules/robot/Planner.h"   // has singleton 'planner'
// motor state-trackers
#include "modules/robot/StepInterface.h"
// smoothie globals ?
#include "SmoothieConfig.h"

class SmoothieRoll{
    public:
        SmoothieRoll(void);
        static SmoothieRoll* getInstance(void);
        void init(float frequency);
        void step_tick(void);

        StepInterface* actuators[SR_NUM_MOTORS];

    private:
        static SmoothieRoll* instance;
};

extern SmoothieRoll* smoothieRoll;

#endif 