/*
      This file is part of Smoothie (http://smoothieware.org/). The motion control part is heavily based on Grbl (https://github.com/simen/grbl).
      Smoothie is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
      Smoothie is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
      You should have received a copy of the GNU General Public License along with Smoothie. If not, see <http://www.gnu.org/licenses/>.
*/



#pragma once

// smoothie includes 
#include <stdint.h>
#include <array>
#include <bitset>
#include <functional>
#include <atomic>

#include "../SmoothieRoll.h"
#include "../modules/robot/Conveyor.h"
// step state interface hack 
#include "../modules/robot/StepInterface.h"
/*
#include "TSRingBuffer.h"
*/
class Block;

// osap includes 
#include <Arduino.h>
#include "../../indicators.h"

// handle 2.30 Fixed point
#define STEPTICKER_FPSCALE (1<<30)
#define STEPTICKER_TOFP(x) ((int32_t)roundf((float)(x)*STEPTICKER_FPSCALE))
#define STEPTICKER_FROMFP(x) ((float)(x)/STEPTICKER_FPSCALE)

class StepTicker{
    private:
        static StepTicker* instance;
        bool start_next_block();

        float frequency;
        uint32_t period;
        // not using motor refs, direct stepping std::array<StepperMotor*, k_max_actuators> motor;
        // do adhoc hack 
        StepInterface* motor[SR_NUM_MOTORS];

        Block *current_block;
        uint32_t current_tick{0};

        struct {
            volatile bool running:1;
            uint8_t num_motors:4;
        };
    public:
        StepTicker();
        static StepTicker* getInstance();
        void init(float frequency);
        // go 
        void set_frequency( float frequency );
        // deleted this void set_unstep_time( float microseconds );
        // deleted this int register_motor(StepperMotor* motor);
        float get_frequency() const { return frequency; }
        void unstep_tick();
        const Block *get_current_block() const { return current_block; }

        void step_tick (void);
        void handle_finish (void);
        void start();

        // whatever setup the block should register this to know when it is done
        std::function<void()> finished_fnc{nullptr};
};

extern StepTicker* stepTicker;
