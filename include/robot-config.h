#pragma once
#include "vex.h"
#include "core.h"
// #define COMP_BOT


extern vex::brain Brain;
extern vex::controller con;
extern vex::inertial imu;
#ifdef COMP_BOT
extern vex::motor_group cata_motors;
#endif

// ================ INPUTS ================
// Digital sensors

// Analog sensors


// ================ OUTPUTS ================
// Motors

// ================ SUBSYSTEMS ================
extern robot_specs_t robot_cfg;
extern OdometryTank odom;
extern TankDrive drive_sys;

extern double motor_volts;
extern PID::pid_config_t cfg;
extern GraphDrawer target_graph;
extern GraphDrawer pos_graph;
extern Serializer *serializer;

// ================ UTILS ================

void robot_init();