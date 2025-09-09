
#pragma once

#define PULSES_PER_LITER                        660

#define FIRST                                   "e00fce68db61e483e6b7a085"
#define SECOND                                  "e00fce687edca63b21266dfc"
#define THIRD                                   "e00fce68195036200d338fb6"

#define USBSerial   Serial

// State tracking
enum IrrigationState
{ IDLE, RUNNING, WAITING };

//Job Handling
#define MAX_JOBS 8

//Hardware Revision
#define MAX_RELAY   7
//#define MAX_RELAY   14
//#define REV_C               //Enables compile of relay 8-14 command parser