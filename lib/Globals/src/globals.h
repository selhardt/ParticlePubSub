
#pragma once

#define PULSES_PER_LITER                        660
#define PULSES_PER_GALLON                       PULSES_PER_LITER * 3.78541

#define STEPPER_DELAY_US                        1200

//  Wondering if these should be configurable by NR at a dashboard level
#define NO_FLOW_TIMEOUT                         60  //AFter this amount of seconds error generated if water flow less than 
#define NO_FLOW_DISPENSE_AMOUNT                 xx  //If less that this in Liters is dispensed in 60 seconds - error out
#define DISPENSE_WATER_FLOW_TIMEOUT             yy  //Make some assumption (do I need to) around input flow rate - in other words - cant go on forever

// State tracking
enum IrrigationState
{ IDLE, RUNNING };

//Job Handling
#define MAX_JOBS 8

