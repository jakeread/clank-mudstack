/*
      This file is part of Smoothie (http://smoothieware.org/). The motion control part is heavily based on Grbl (https://github.com/simen/grbl).
      Smoothie is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
      Smoothie is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
      You should have received a copy of the GNU General Public License along with Smoothie. If not, see <http://www.gnu.org/licenses/>.
*/


#include "StepTicker.h"
#include "../modules/robot/Block.h"
//#include "../modules/robot/Conveyor.h"
/*
#include "libs/nuts_bolts.h"
#include "libs/Module.h"
#include "libs/Kernel.h"
#include "StepperMotor.h"
#include "StreamOutputPool.h"
#include "Block.h"
#include "Conveyor.h"

#include "system_LPC17xx.h" // mbed.h lib
#include <math.h>
#include <mri.h>
*/

StepTicker* StepTicker::instance = 0;

StepTicker* StepTicker::getInstance(void){
    if(instance == 0){
        instance = new StepTicker();
    }
    return instance;
}

StepTicker* stepTicker = StepTicker::getInstance();

StepTicker::StepTicker(){
}

void StepTicker::init(float frequency){
    // Default start values
    this->set_frequency(frequency);

    this->num_motors = SR_NUM_MOTORS;

    for(uint8_t m = 0; m < SR_NUM_MOTORS; m ++){
        motor[m] = smoothieRoll->actuators[m];
    }

    this->running = false;
    this->current_block = nullptr;
}

//called when everything is setup and interrupts can start
void StepTicker::start(){
    //NVIC_EnableIRQ(TIMER0_IRQn);     // Enable interrupt handler
    //NVIC_EnableIRQ(TIMER1_IRQn);     // Enable interrupt handler
    current_tick= 0;
}

// Set the base stepping frequency
#warning This leaves config as is, see startup to 'make proper'.
void StepTicker::set_frequency( float frequency ){
    this->frequency = frequency;
    this->period = 80; // set manually above, floorf((SystemCoreClock / 4.0F) / frequency); // SystemCoreClock/4 = Timer increments in a second
}

// step clock
void StepTicker::step_tick (void){
    if(running){
        //DEBUG3PIN_ON;
    } else {
        //DEBUG3PIN_OFF;
    }
    // if nothing has been setup we ignore the ticks
    if(!running){
        // check if anything new available
        if(conveyor->get_next_block(&current_block)) { // returns false if no new block is available
            running = start_next_block(); // returns true if there is at least one motor with steps to issue
            if(!running) return;
        } else {
            return;
        }
    }

    /*
    eliminated this, but here's a halting case 
    if(THEKERNEL->is_halted()) {
        running= false;
        current_tick = 0;
        current_block= nullptr;
        return;
    }
    */

    bool still_moving = false;
    // foreach motor, if it is active see if time to issue a step to that motor
    for (uint8_t m = 0; m < num_motors; m++) {

        if(current_block->tick_info[m].steps_to_move == 0) continue; // not active

        current_block->tick_info[m].steps_per_tick += current_block->tick_info[m].acceleration_change;

        if(current_tick == current_block->tick_info[m].next_accel_event) {
            if(current_tick == current_block->accelerate_until) { // We are done accelerating, deceleration becomes 0 : plateau
                current_block->tick_info[m].acceleration_change = 0;
                if(current_block->decelerate_after < current_block->total_move_ticks) {
                    current_block->tick_info[m].next_accel_event = current_block->decelerate_after;
                    if(current_tick != current_block->decelerate_after) { // We are plateauing
                        // steps/sec / tick frequency to get steps per tick
                        current_block->tick_info[m].steps_per_tick = current_block->tick_info[m].plateau_rate;
                    }
                }
            }

            if(current_tick == current_block->decelerate_after) { // We start decelerating
                current_block->tick_info[m].acceleration_change = current_block->tick_info[m].deceleration_change;
            }
        }

        // protect against rounding errors and such
        if(current_block->tick_info[m].steps_per_tick <= 0) {
            current_block->tick_info[m].counter = STEPTICKER_FPSCALE; // we force completion this step by setting to 1.0
            current_block->tick_info[m].steps_per_tick = 0;
        }

        current_block->tick_info[m].counter += current_block->tick_info[m].steps_per_tick;

        if(current_block->tick_info[m].counter >= STEPTICKER_FPSCALE) { // >= 1.0 step time
            current_block->tick_info[m].counter -= STEPTICKER_FPSCALE; // -= 1.0F;
            ++current_block->tick_info[m].step_count;

            // step the motor
            bool ismoving = motor[m]->step(); // returns false if the moving flag was set to false externally (probes, endstops etc)

            if(!ismoving || current_block->tick_info[m].step_count == current_block->tick_info[m].steps_to_move) {
                // done
                current_block->tick_info[m].steps_to_move = 0;
                motor[m]->stop_moving(); // let motor know it is no longer moving
            }
        }

        // upd8 speed for queries 
        //motor[m]->set_ticks_per_step(current_block->tick_info[m].steps_per_tick);

        // see if any motors are still moving after this tick
        if(motor[m]->is_moving()) still_moving = true;
    }

    // do this after so we start at tick 0
    current_tick++; // count number of ticks

    // see if any motors are still moving
    if(!still_moving) {
        // block transition 
        //DEBUG3PIN_TOGGLE;

        // all moves finished
        current_tick = 0;

        // get next block
        // do it here so there is no delay in ticks
        conveyor->block_finished();

        if(conveyor->get_next_block(&current_block)) { // returns false if no new block is available
            running = start_next_block(); // returns true if there is at least one motor with steps to issue
        } else {
            current_block = nullptr;
            running = false;
        }
        // all moves finished
        // queue needs to be incremented, that happens on the conveyor's idle cycle 
    }
} // end step_tick 

// only called from the step tick ISR (single consumer)
bool StepTicker::start_next_block()
{
    if(current_block == nullptr){
        return false;
    }
    bool ok = false;
    // need to prepare each active motor
    for (uint8_t m = 0; m < num_motors; m++) {
        if(current_block->tick_info[m].steps_to_move == 0) continue;
        ok = true; // mark at least one motor is moving
        // set direction bit here
        // NOTE this would be at least 10us before first step pulse.
        // TODO does this need to be done sooner, if so how without delaying next tick
        motor[m]->set_direction(current_block->direction_bits[m]);
        motor[m]->start_moving(); // also let motor know it is moving now
    }

    current_tick= 0;

    if(ok) {
        //SET_STEPTICKER_DEBUG_PIN(1);
        return true;
    } else {
        // this is an edge condition that should never happen, but we need to discard this block if it ever does
        // basically it is a block that has zero steps for all motors
        conveyor->block_finished();
    }

    return false;
}