
#pragma once

#define PULSES_PER_LITER                        660

#define ZONE1                                   "e00fce68db61e483e6b7a085"
#define ZONE2                                   "e00fce687edca63b21266dfc"
#define ZONE3                                   "e00fce68195036200d338fb6"

#define USBSerial   Serial

// State tracking
enum IrrigationState
{
    IDLE,
    RUNNING
};


//Hardware Revision
//#define MAX_RELAY   7
#define MAX_RELAY   14
