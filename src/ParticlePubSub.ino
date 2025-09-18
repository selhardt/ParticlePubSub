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
//  8/6/25      27          Fix conversino to gallons in status update - seems to work
//  8/7/25      28          Irrigation event now triggers 1 temperature read - it is no longer sequenced by NodeRed
//                          Changed the status report to be actual dispensed for low_flow or no_flow
//  8/7/25                  Un commented sequencing code - both online devices are starting
//                          Revised device Comm Function
//                          Job >= changed to ==
//  8/7/25      29          Backed out all sequencing code as it does not work
//  8/9/25      31          8/9/25 - Successful 1 to 2 sequence, configuring 3rd device for full test
//  8/10/25                 Provisioning 3rd Device
//  8/10/25     32          Changes for IrrigationState on SECOND and THIRD to be WAITING for next calendar event
//  8/13/25     32          Change to DHT20 Temp/Humidity Sensor - see TODO's
//  8/24/25     33          Removing Stepper Driver COde
//                          Adding support for rev C Circuit board
//  8/25/25     34          Sequencing is not working - back to debugging that
//  8/25/25     35          Sequence Runs again - trying to make sure if irrigation aborted - left in condition to restart
//              36          More changes for second start
//              37          For 7 relays rev B
//  8/28/25     38          7 Relays rev B with Particle command for parser
//  9/5/25      39          7 Relays rev C (14 relays does not seem to start)
//  9/6/25      40          7 Relays rev C - fixed the MAX_RELAY I had hard coded as 7
//  9/9/25                  Pushing this revision to github - need to change over to new sequencing
//  9/9/25      41          Reverting back to Nodered Sequencing - this revision functions for all 3 zones and cloud works as well.
//  9/14/25     42          Adding the master water vavle support
//              43          Stuff trying to figure out why 5th Subscribe does not work
//              44          Single Subscribe handler
//                          
//    
//  *******************************************************************************************************************************************
//  Names for Devices at bottom of screen for direct flash
//  Name in VS Code             deviceID                        Relay Number            Relay Name
//  Pool_Fruit_Trees            e00fce68db61e483e6b7a085        1,2,3,4,5,6,7           Fig,Orange,LizLemon,MeyerLemon,3Bells,2Bells,Vitex
//  Pool_Palms_and_Flowers      e00fce687edca63b21266dfc        1,2,3,4,5,6,7           Palm1,Palm2,BirdParadise,3Lantana,SmallPalm,BigPalmSteps,RosataPalm
//  ZONE3                       e00fce68195036200d338fb6        1,2,3,4,5,6,7           Tangelo,4Lantana,GarageMesquite,Ironwood,WestMesquite,MiddleMesquite,EastMesquite
//                      
//  *******************************************************************************************************************************************
// Strikeout CmdK - release, then press delete
//  *******************************************************************************************************************************************
// BORON RESTORE:
// Use Particle CLI to flash recovery firmware manually:
// Open terminal - cd downloads
// 1. particle flash --usb tinker-serial1-debugging-0.8.0-rc.27-boron-mono.bin
// 2. particle flash --usb boron-system-part1@1.1.0.bin
// Then perform the factory reset (MODE + RESET until white blink). - didn't have to do this
// 3. reset button then has it blinking blue
// 4. Finally, re-add the device via the mobile app.
//
// BORON CLI flash
// /Users/scottelhardt/Documents/TreeWateringProject/Particle_Pub_Sub/ParticlePubSub/target/6.3.3/boron/ParticlePubSub37RevB.bin
// cd to the boron directory
// put device in dfu mode
// particle flash --usb ParticlePubSub37RevB.bin
//  *******************************************************************************************************************************************
//  WARNING!!!!
//  If Firmware rev is locked in Particle Dashboard - physical flash changes will revert to locked version creating havoc
//
//
//  *******************************************************************************************************************************************
//  WARNING!
//  Particle allows only 4 Particle.Subscribes ! - Took 2 days to find this out

//TODO
//***************************************REV 44 Items******************************************************************************************
//  Reduce debug prints and fix any old device ID mapping in NR
//  Implement an Error or Notify Node on NR that the bebug prints that are bad report on
//      -Leaking Flow
//      -Se̶n̶s̶o̶r̶ n̶o̶ c̶o̶n̶n̶e̶c̶t̶
//      -̶B̶a̶d̶ J̶S̶O̶N̶s̶
//      -Others?
//      -add a notify publish if the gallon value is above a certain amount in readFlow
//      -decide how to clear the leakflow variable? Separate call?
//TODO so this is going to increment on ZONE2 and ZONE3 jobs - hmm needs more thought
//leakingPulseCount++;
//****************************************BUGS*************************************************************************************************

#include "Particle.h"
#include "ArduinoJson.h"
#include "globals.h"
#include "functions.h"
#include <Wire.h>

//This must be directly programmed once prior to OTA updates
PRODUCT_VERSION(44); // Increment this with each new upload

// Irrigation state and Tracking Variables
int currentRelay = -1;
unsigned long requiredPulses = 0;           //number of required pulses for dispense job
unsigned long pulseCount = 0;               //flow pulse count used for dispense job
unsigned long leakingPulseCount = 0;        //count flow pulses while no irrigation job is running
unsigned long startTime = 0;
unsigned long timeoutDuration = 0;
unsigned long lastPulseTime = 0;

IrrigationState irrigationState = IDLE;


// Designate GPIO pins
// GPIO pin for flow sensor
const int flowPin = D2;

// Relays - SEE GLOBALS FOR MAX RELAY
//int relayPins[MAX_RELAY] = {D8, D7, D6, D5, A0, A1, A2}; // Rev B
int relayPins[MAX_RELAY] = {A0, A1, A2, A3, D11, D4, D3, D8, A4, A5, D13, D12, D10, D9}; //Rev C

// Job Iteration
int jobRelay[MAX_RELAY];
int jobWaterQty[MAX_RELAY];
int totalJobs = 0;
int currentJobIndex = 0;
//bool jobsPending = false;   //TODO nothing checks this variable
bool jobJustStarted = false;

// Flow Variables
const unsigned long noFlowThreshold = 30000; // 30 seconds without pulses to allow for valve to open fully

// Temperature Sensor
bool dht20Error = false;

// Debug Print Enable or Disable
bool debugPrint = true;
bool particlePrint = true;

//Device Zone Mapping
String myZone;

//Status update loop timing
unsigned long lastStatusUpdate = 0;
const unsigned long statusInterval = 300000; // every 5 minutes

// Debug print array
char debugText[256];

// Command Buffer
char readBuf[100];

void pulseISR()
{
    // Only count pulses if the state is RUNNING
    // This flow sensor will run on 2nd and 3rd system in series so don't count those
    if (irrigationState == RUNNING)
    { // Count Flow only if RUNNING
        pulseCount++;
        lastPulseTime = millis();
    } else {
        //Zeroed on Read - probably should have a separate clear
        //TODO so this is going to increment on ZONE2 and ZONE3 jobs - hmm needs more thought
        leakingPulseCount++;
    }
}

void setup() {
    Wire.begin();
    USBSerial.begin(9600);
    waitFor(USBSerial.isConnected, 10000);
    Particle.connect();
    waitUntil(Particle.connected);
    //printDebugMessage("✅ Particle cloud connected");

    //Use to replace device ID with Zone Name
    if(System.deviceID() == "e00fce68db61e483e6b7a085"){
        myZone = "ZONE1";
    }else if (System.deviceID() == "e00fce687edca63b21266dfc"){
        myZone = "ZONE2";
    }else if(System.deviceID() == "e00fce68195036200d338fb6"){
        myZone = "ZONE3";
    //}else if (System.deviceID() == "e00fce68f20c968df59370c6"){
    }else if (System.deviceID() == "e00fce6802f288257e9418c6"){
        myZone = "ZONE4";
    }

    // Set relay pins as outputs and turn off initially
    for (int i = 0; i < MAX_RELAY; i++) {
        pinMode(relayPins[i], OUTPUT);
        digitalWrite(relayPins[i], LOW);
    }
    enableMasterValve();  //Turn on Master Valve

    // Set up pulse input
    pinMode(flowPin, INPUT_PULLUP);
    attachInterrupt(flowPin, pulseISR, FALLING);

    //buffer used in printdebug message
    memset(debugText, 0, sizeof(debugText));


    //connect DHT20 temp/humidity sensor
    if(dht.begin()){
        dht20Error = false;
        USBSerial.println("✅ DHT20 connected - yay");
    }
    else{
        dht20Error = true;
        USBSerial.println("❌ DHT20 connect error - poop");
    }

    //Trigger a temperature read TODO - need this?
    double tempF;
    double humidity;
    const char *zone = myZone.c_str();

    readTemp(zone, tempF, humidity);
    char out[192];
    snprintf(out, sizeof(out),
      "{\"ok\":true,\"zone\":\"%s\",\"sensor\":\"temp\",\"temp\":%.2f,\"humidity\":%.2f}",
      zone, tempF, humidity);
    Particle.publish("resp/sensor", out, PRIVATE);
    USBSerial.println(out);


    //Particle Functions
    //Reboot the Particle
    Particle.function("reboot", resetBoron);
    //Parses CLI commands - the same as the serial port
	Particle.function("CMD Write", consoleCmd);

    // Particle Subscribes
    // Handle the Main irrigation watering events
    //Particle.subscribe("irrigate", handleIrrigationEvent);
    // Read Temperature and Jumidity
    //Particle.subscribe("read_sensors", handleTempSensorRead);
    // Abort Irrigation in Progress
    //Particle.subscribe("abortIrrigate", handleAbortEvent);
    // Enable/Disable Master Water Valve
    //Particle.subscribe("master_valve", handleMasterValveEvent);
    Particle.subscribe("cmd/", onCmd);

    // Announce being up
    // printDebugMessage("✅ Ready for irrigation events :-)");

    //Print firmware rev at start
    USBSerial.print("firmware_version"); USBSerial.println(String::format(": %d", __system_product_version));
    USBSerial.print("Device ID "); USBSerial.println(System.deviceID());
    USBSerial.print("ZONE "); USBSerial.println(myZone);
}

void loop() {

    unsigned long now = millis();
    Particle.process();
    checkUSBSerial();

    if (irrigationState == RUNNING) //main loop
    {
        // Publish updates every statusInterval
        if (millis() - lastStatusUpdate >= statusInterval)
        {
            lastStatusUpdate = millis();
            publishIrrigationStatus("in_progress");
        }
        // Handles no_flow on first relay - unshure why it trips as the math seems to be OK
        if (jobJustStarted)
        {
            jobJustStarted = false;
            return; // skip this loop to allow startTime to settle
        }

        // timeout if < 1 liter flows during noFlowThreshold (currently 30sec)
        if ((pulseCount < PULSES_PER_LITER) && ((now - startTime) > noFlowThreshold))
        {
            publishIrrigationStatus("no_flow");
            stopIrrigation();

            currentJobIndex++;
            startNextJob(); // ⬅️ continue despite no flow
            return;
        }
        // Completed irrigation water delivery for each job
        if (pulseCount >= requiredPulses)
        {
            publishIrrigationStatus("irrigation_complete"); // Complete

            stopIrrigation();

            currentJobIndex++;
            startNextJob(); // ⬅️ go to next job
            return;
        }
        // resets if irrigation never exceeds requiredPulses
        if (now - startTime > timeoutDuration)
        {
            publishIrrigationStatus("timeout");
            USBSerial.println("irrigation_timeout");
            stopIrrigation();

            currentJobIndex++;
            startNextJob(); // ⬅️ continue despite timeout
            return;
        }
    }
    }
    

       





 