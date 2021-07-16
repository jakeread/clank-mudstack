/*

StepInterface.cpp 

driver interface hack for smoothie port 

*/

#include "StepInterface.h"
#include "../../libs/StepTicker.h"

StepInterface::StepInterface(void){}

int32_t StepInterface::steps_to_target(float target){
    int32_t target_steps = lroundf(target * steps_per_mm);
    return target_steps - last_milestone_steps;
}

void StepInterface::update_last_milestones(float mm, int32_t steps){
    last_milestone_steps += steps;
    last_milestone_mm = mm;
}

void StepInterface::set_position(float mm){
    last_milestone_mm = mm;
    last_milestone_steps = lroundf(mm * steps_per_mm);
    // values *jake* tracks in rt 
    stepwise_position = last_milestone_steps;
    floating_position = last_milestone_mm;
}

float StepInterface::get_last_milestone_mm(void){
    return last_milestone_mm;
}

float StepInterface::get_max_rate(void){
    return max_rate;
}

void StepInterface::set_max_rate(float rate){
    // abs max is one step per step ticker frequency, 
    // so is frequency / spu
    float abs_max = stepTicker->get_frequency() / steps_per_mm;
    max_rate = fabsf(rate);
    if(max_rate > abs_max){
        max_rate = abs_max;
    }
}

float StepInterface::get_accel(void){
    return accel;
}

void StepInterface::set_accel(float acc){
    accel = fabsf(acc);
}

float StepInterface::get_current_speed(void){
    if(!moving){
        return 0.0F;
    } else {
        return current_speed;
    }
}

boolean StepInterface::step(void){
    // upd8 position, 
    if(direction){
        stepwise_position --;
    } else {
        stepwise_position ++;
    }
    floating_position = stepwise_position * mm_per_step;
    // track speed based on this tick (badness)
    now = micros();
    current_speed = (mm_per_step * 1000000) / (now - last_tick);
    last_tick = now;

    return moving;
}

void StepInterface::set_direction(boolean dir){
    direction = dir;
}

void StepInterface::start_moving(void){
    moving = true;
}

void StepInterface::stop_moving(void){
    moving = false;
}

boolean StepInterface::is_moving(void){
    return moving;
}