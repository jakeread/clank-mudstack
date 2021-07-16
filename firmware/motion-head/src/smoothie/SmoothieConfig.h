#ifndef ACTUATORCOORDINATES_H_
#define ACTUATORCOORDINATES_H_

#include <array> 

#define SR_NUM_MOTORS 4
#define SR_NUM_MOTION_AXES 3

// default accel 
#define SR_DEFAULT_ACCEL 5000.0F

// Keep MAX_ROBOT_ACTUATORS as small as practical it impacts block size and therefore free memory.
const size_t k_max_actuators = SR_NUM_MOTORS;
typedef struct std::array<float, k_max_actuators> ActuatorCoordinates;

#endif 