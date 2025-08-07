/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#line 1 "/Users/scottelhardt/Documents/TreeWateringProject/Particle_Pub_Sub/ParticlePubSub/src/ParticlePubSub.ino"
//  date       rev          comments
//  7/8/24      8           Removed Pressure and Altitude
//                          Adding more Debug Publish instead of serial prints
//  7/13/24     9           Changed steps and pulses to oz and gallons
//  7/14/25     10          Adding device_ID to all debug prints and publishes
//  7/15/25     11          Slowly implementing all check for device ID changes
//  7/15/25     12          Slowly implementing all check for device ID changes and fixing formatting of JSON's etc
//  7/15/25     13          Added reboot function to Particle Dashboard and deleted NR reboot
//  7/15/25     14          Stepper Particle Function
//  7/18/25     20          Rewrite for GCal 2.0 Events - working at functional level, does not include status dashboard or influx
//  7/22/25     20          Committed to main
//  7/24/25     21          Added a check for irrigationState == RUNNING before accepting a new irrigation event in handleIrrigationEvent
//  7/25/25     22          Working on bad status gallon values being reported to in-progress irrigation events
//                          Added debug print for pulsecount to see why formula is not working as expected
//                          TODO - debug further
//                          Added check for bme280 on address 0x77 if not found on 0x76 (#2 is erroring)
//                          Added irrigation state check = RUNNING for pulseCount increment
//  8/5/25      23          Adding and debugging device sequencing
//              24          Bugs in First Device Start
//  8/6/25      25          Fixing First Device Start
//  8/6/25      26          Commented out all Sequencing code - Think about publishing and listening on same node?
//  8/6/25      27          FIx conversino to gallons in status update - seems to work
//  *******************************************************************************************************************************************
//  Names for Devices at bottom of screen for direct flash
//  Name in VS Code             deviceID                        Relay Number            Relay Name
//  Pool_Fruit_Trees            e00fce68db61e483e6b7a085        1,2,3,4,5,6,7           Fig,Orange,LizLemon,MeyerLemon,3Bells,2Bells,Vitex
//  Pool_Palms_and_Flowers      e00fce68e12921ec4e2ff144        1,2,3,4,5,6,7           Palm1,Palm2,BirdParadise,3Lantana,SmallPalm,BigPalmSteps,RosataPalm
//  Garage_Flowers_and_Trees    e00fce687edca63b21266dfc        1,2,3,4,5,6,7           Tangelo,4Lantana,GarageMesquite,Ironwood,WestMesquite,MiddleMesquite,EastMesquite
//  softener_vavles             e00fce68f20c968df59370c6        
//  *******************************************************************************************************************************************
// Strikeout CmdK - release, then press delete
//  *******************************************************************************************************************************************
//TODO
//At the end of a dispense - return the Water quantity, pulses, MoonJoice Qty, rotations, water flow rate, moonjuice flow rate and elapsed time to dispense
//Check that the final status update is done prior to ending moving on to next job
//  *******************************************************************************************************************************************

#include "Particle.h"
#include "ArduinoJson.h"
#include "globals.h"
#include "functions.h"
#include <Wire.h>

//This must be directly programmed once prior to OTA updates
void pulseISR();
void setup();
void loop();
#line 45 "/Users/scottelhardt/Documents/TreeWateringProject/Particle_Pub_Sub/ParticlePubSub/src/ParticlePubSub.ino"
PRODUCT_VERSION(27); // Increment this with each new upload

// Designate GPIO pins
// GPIO pin for flow sensor
const int pulsePin = D2;
// GPIO pin map for water valve relays 1–7
int relayPins[7] = {D8, D7, D6, D5, A0, A1, A2};
// GPIO pin for moonjuice stepper motor driver
const int stepperPin = D4;
int stepperSleep = D3;

// Irrigation state and Tracking
int currentRelay = -1;
unsigned long requiredPulses = 0;
unsigned long pulseCount = 0;
unsigned long startTime = 0;
unsigned long timeoutDuration = 0;
unsigned long lastPulseTime = 0;
//bool pulseStarted = false;
bool fertEnabled = false;

IrrigationState irrigationState = IDLE;         //Goes to Running when dispensing Water
IrrigationState fertilizerState = IDLE;         //Goes to Running when dispensing Moon Juice

//Job Iteration
String jobDevice;
int jobRelay[MAX_JOBS];
int jobWaterQty[MAX_JOBS];
int jobFertQty[MAX_JOBS];
int totalJobs = 0;
int currentJobIndex = 0;
bool jobsPending = false;
bool jobJustStarted = false;

//Job Sequencing
// DevicePosition devicePosition;
// bool upstreamIdle;
// String upstreamDevice;

//Moon Juice state and tracking
int requiredSteps = 0;
unsigned long intervalMicros = 0; // microseconds between pulses
unsigned long lastPulseTimeStepper = 0;
int stepsSent = 0;

//Heartbeat
unsigned long lastHeartbeat = 0;
const unsigned long heartbeatInterval = 60000 * 30;     // 60 seconds * 30 minutes
//const unsigned long heartbeatInterval = 60000 * 5;    // 60 seconds * 5 minutes - for debugging

//Flow Variables
const unsigned long noFlowThreshold = 30000;            // 30 seconds without pulses to allow for valve to open fully
unsigned long lastPulseCount = 0;
float flow = 0.0;

// Temperature Sensor
bool bme280Error = false;
float tempF = 0.0;
float humidity = 0.0;

//Debug Print Enable or Disable
bool debugPrint = true;
bool particlePrint = true;

unsigned long lastStatusUpdate = 0;
const unsigned long statusInterval = 300000; // every 5 minutes

//Debug print array
char debugText[256];

void pulseISR() {
    //Only count pulses if the state is RUNNING
    //This flow sensor will run on 2nd system in series so don't count those
    //If it is counting and no other systems are irrigating - could be a leak
    if(irrigationState==RUNNING){
        pulseCount++;
        lastPulseTime = millis();
    }
}

void setup() {
    Wire.begin();
    Serial.begin(9600);
    waitFor(Serial.isConnected, 10000);

    Particle.connect();
    waitUntil(Particle.connected);
    //printDebugMessage("✅ Particle cloud connected");

//     //Job Sequencing Setup
//     String thisDevice = System.deviceID();
//     if (thisDevice == "e00fce68db61e483e6b7a085") {
//         devicePosition = FIRST;
//         upstreamDevice = "";  // None
//     } else if (thisDevice == "e00fce68e12921ec4e2ff144") {
//         devicePosition = SECOND;
//         upstreamDevice = "e00fce68db61e483e6b7a085";
//     } else if (thisDevice == "e00fce687edca63b21266dfc") {
//         devicePosition = THIRD;
//         upstreamDevice = "e00fce68e12921ec4e2ff144";
//     } else {
//     printDebugMessage("❌ Unknown device ID");
// }

    // Set relay pins as outputs and turn off initially
    for (int i = 0; i < 7; i++) {
        pinMode(relayPins[i], OUTPUT);
        digitalWrite(relayPins[i], LOW);
    }
    // Set up pulse input
    pinMode(pulsePin, INPUT_PULLUP);
    attachInterrupt(pulsePin, pulseISR, FALLING);

    // Set up stepper motor output
    pinMode(stepperPin, OUTPUT);
    digitalWrite(stepperPin, LOW);
    pinMode(stepperSleep, OUTPUT);    //Start with stepper driver disabled
    digitalWrite(stepperSleep, LOW);    

    memset(debugText, 0, sizeof(debugText));
      // Initialize BME280 sensor

    if (bme.begin(0x76)) {
        Serial.println("✅ BME280 found at 0x76");
        bme280Error = false;
        tempF = bme.readTemperature() * 9.0 / 5.0 + 32.0;
        humidity = bme.readHumidity();
        Serial.print("Temp = ");
        Serial.println(tempF);
    }
    else if (bme.begin(0x77))
    {
        Serial.println("✅ BME280 found at 0x77");
        bme280Error = false;
        tempF = bme.readTemperature() * 9.0 / 5.0 + 32.0;
        humidity = bme.readHumidity();
        Serial.print("Temp = ");
        Serial.println(tempF);
    }
    else
    {
        Serial.println("❌ Could not find BME280 sensor at 0x76 or 0x77");
        bme280Error = true;
    }

    //Particle Functions
    Particle.function("reboot", resetBoron);
    //if(success)printDebugMessage("✅ reboot function registered");
    Particle.function("stepperRotations", stepperRotations);

    // Subscribe to cloud messages
    Particle.subscribe("irrigate", handleIrrigationEvent);
    // Activates Temperature read from cloud
    Particle.subscribe("read_sensors", handleTempSensorRead);
    // Abort Irrigation
    Particle.subscribe("abortIrrigate", handleAbortEvent);
    //Announce being up
    printDebugMessage("✅ Ready for irrigation events :-)");

    Serial.print("firmware_version");
    Serial.println(String::format("version: %d", __system_product_version));
 
    // // Job Sequencin
    // Particle.subscribe("deviceComm", handleDeviceComm);

}

void loop() {

    unsigned long now = millis();
    Particle.process();

    // if (irrigationState != RUNNING && jobsPending) {
    //     startNextJob();     // Retry in case upstream just became idle
    // }

    if (irrigationState == RUNNING)
    {
        //fertilizer stepper logic
        //This needs to be here as can't inject moonjuice into high pressure lines
        //Needs irrigation water flowing to dispense
        if (fertilizerState == RUNNING) {
          unsigned long nowMicros = micros();
          if ((nowMicros - lastPulseTimeStepper) >= intervalMicros) {
            lastPulseTimeStepper = nowMicros;

            digitalWrite(stepperPin, HIGH);
            delayMicroseconds(STEPPER_DELAY_US);
            digitalWrite(stepperPin, LOW);
            delayMicroseconds(STEPPER_DELAY_US);

            stepsSent++;
            if (stepsSent >= requiredSteps) {

                if(debugPrint) Serial.print("Fertilizer Steps Sent");
                if(debugPrint) Serial.println(stepsSent);

                publishIrrigationStatus("fertilize_complete");
                stopFertilizer();
            }

          }
        }
        //Publish updates
        if (millis() - lastStatusUpdate >= statusInterval) {
        lastStatusUpdate = millis();
        publishIrrigationProgress();
        }
        // Handles no_flow on first relay - unshure why it trips as the math seems to be OK
        if (jobJustStarted) {
            jobJustStarted = false;
            return;  // skip this loop to allow startTime to settle
        }
        // timeout if < 1 liter flows during noFlowThreshold (currently 30sec)
        if ((pulseCount < PULSES_PER_LITER) && ((now - startTime) > noFlowThreshold)) {
            publishIrrigationStatus("no_flow");
            //Serial.println("no_flow");
            stopIrrigation();
            stopFertilizer();

            currentJobIndex++;
            startNextJob(); // ⬅️ continue despite no flow
            return;
        }
        // Completed irrigation water delivery
        if (pulseCount >= requiredPulses) {
            publishIrrigationProgress();                        //Final update prior to completion 
            publishIrrigationStatus("irrigation_complete");     //Complete
            Serial.println("irrigation_complete");
            stopIrrigation();
            stopFertilizer();

            currentJobIndex++;
            startNextJob(); // ⬅️ go to next job
            return;
        }
        // resets if irrigation never exceeds requiredPulses
        if (now - startTime > timeoutDuration) {
            publishIrrigationStatus("timeout");
            Serial.println("irrigation_timeout");
            stopIrrigation();
            stopFertilizer();

            currentJobIndex++;
            startNextJob(); // ⬅️ continue despite timeout
            return;
        }
    }
    }
    

       





 