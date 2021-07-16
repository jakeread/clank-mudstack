/*
      This file is part of Smoothie (http://smoothieware.org/). The motion control part is heavily based on Grbl (https://github.com/simen/grbl).
      Smoothie is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
      Smoothie is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
      You should have received a copy of the GNU General Public License along with Smoothie. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef PLANNER_H
#define PLANNER_H

#include <Arduino.h>
#include "../../../indicators.h"

#include "../../SmoothieConfig.h"
#include "../../SmoothieRoll.h"
#include "Block.h"
#include "Conveyor.h"

class Block;

class Planner
{
public:
    Planner();
    static Planner* getInstance(void);
    float max_allowable_speed( float acceleration, float target_velocity, float distance);
    bool append_block(ActuatorCoordinates &target, uint8_t n_motors, float rate_mm_s, float distance, float unit_vec[], float accleration, float s_value, bool g123);
    void append_move(float *target, uint8_t n_motors, float rate, float delta_e);
    void set_position(float *target, uint8_t n_motors);
    volatile boolean do_set_position = false;

    // track for increments 
    float last_position[SR_NUM_MOTORS] = {0,0,0,0};

    friend class Robot; // for acceleration, junction deviation, minimum_planner_speed

private:
    static Planner* instance;
    void recalculate();
    void config_load();
    float previous_unit_vec[SR_NUM_MOTION_AXES];
    float junction_deviation;    // Setting
    float z_junction_deviation;  // Setting
    float minimum_planner_speed; // Setting
};

extern Planner* planner;

#endif
