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
//  *******************************************************************************************************************************************
//  Names for Devices at bottom of screen for direct flash
//  Name in VS Code             deviceID                        Relay Number            Relay Name
//  Pool_Fruit_Trees            e00fce68db61e483e6b7a085        1,2,3,4,5,6,7           Fig,Orange,LizLemon,MeyerLemon,3Bells,2Bells,Vitex
//  Pool_Palms_and_Flowers      e00fce687edca63b21266dfc        1,2,3,4,5,6,7           Palm1,Palm2,BirdParadise,3Lantana,SmallPalm,BigPalmSteps,RosataPalm
//  Garage_Flowers_and_Trees    e00fce68195036200d338fb6        1,2,3,4,5,6,7           Tangelo,4Lantana,GarageMesquite,Ironwood,WestMesquite,MiddleMesquite,EastMesquite
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

//TODO
//D̶e̶v̶i̶c̶e̶ S̶e̶q̶u̶e̶n̶c̶i̶n̶g̶ -̶ I̶m̶p̶l̶e̶m̶e̶n̶t̶e̶d̶ r̶e̶v̶ 3̶2̶
//***************************************REV 33 Items******************************************************************************************
//S̶w̶i̶t̶c̶h̶ t̶o̶ D̶H̶t̶2̶0̶ r̶u̶n̶n̶i̶n̶g̶ o̶f̶f̶ o̶f̶ 3̶.̶3̶V̶ f̶r̶o̶m̶ B̶o̶r̶o̶n̶
//R̶e̶m̶o̶v̶e̶ 3̶.̶3̶V̶ P̶S̶
//J̶u̶m̶p̶e̶r̶ +̶3̶.̶3̶V̶ t̶o̶ P̶a̶r̶t̶i̶c̶l̶e̶ 3̶.̶3̶V̶
//R̶e̶m̶o̶v̶e̶ B̶M̶E̶ 2̶8̶0̶
//W̶i̶r̶e̶ D̶H̶T̶2̶0̶ a̶s̶ n̶e̶e̶d̶e̶d̶ a̶t̶ B̶M̶E̶ l̶o̶c̶a̶t̶i̶o̶n̶
//P̶u̶l̶l̶ S̶t̶e̶p̶p̶e̶r̶ D̶r̶i̶v̶e̶r̶
//R̶e̶m̶o̶v̶e̶ A̶d̶a̶f̶r̶u̶i̶t̶_̶B̶M̶E̶2̶8̶0̶.̶h̶
//R̶e̶m̶o̶v̶e̶ A̶d̶a̶f̶r̶u̶i̶t̶_̶S̶e̶n̶s̶o̶r̶.̶h̶
//A̶d̶d̶ L̶i̶b̶r̶a̶r̶y̶ D̶H̶T̶2̶0̶-̶0̶.̶3̶.̶1̶
//U̶p̶d̶a̶t̶e̶ C̶o̶d̶e̶ f̶o̶r̶ D̶H̶T̶2̶0̶
//R̶e̶m̶o̶v̶e̶ S̶t̶e̶p̶p̶e̶r̶ C̶o̶d̶e̶
//Conditional compile for REV C PCB - not able to get this working sucessfully
//Remove the JSON references for fertilizer in Cloud messages
//  *******************************************************************************************************************************************
//
//B̶U̶G̶S̶ -̶ A̶L̶L̶ w̶a̶t̶e̶r̶Q̶t̶y̶ i̶n̶ s̶t̶a̶t̶u̶s̶ p̶u̶b̶l̶i̶s̶h̶e̶s̶ s̶h̶o̶w̶ 3̶0̶ g̶a̶l̶ - FIXED rev 30
//F̶a̶i̶l̶e̶d̶ R̶u̶n̶ h̶a̶d̶ t̶o̶ r̶e̶b̶o̶o̶t̶ a̶l̶l̶ d̶e̶v̶i̶c̶e̶s̶ a̶n̶d̶ t̶h̶e̶n̶ i̶t̶ s̶e̶q̶u̶e̶n̶c̶e̶d̶ p̶r̶o̶p̶e̶r̶l̶y̶ -̶ F̶I̶X̶E̶D̶ r̶e̶v̶ 3̶1̶
//Force Trigger device #2 results in SECOND is WAITING, - should override and let it run as long as irrigating is not true for upstream devices
//C̶a̶n̶'̶t̶ s̶e̶e̶m̶ t̶o̶ i̶n̶t̶e̶r̶r̶u̶p̶t̶ w̶i̶t̶h̶ t̶e̶r̶m̶i̶n̶a̶t̶e̶ e̶i̶t̶h̶e̶r̶
//Sometimes the 2nd is waiting does not happen - ending jobs on device 1 let's device 2 start, but it starts with relay 1 instead of the 7 it should have
//Time to move onto a new version that takes all devices and sequences on Nodered.



#include "Particle.h"
#include "ArduinoJson.h"
#include "globals.h"
#include "functions.h"
#include <Wire.h>



//This must be directly programmed once prior to OTA updates
PRODUCT_VERSION(40); // Increment this with each new upload

// Designate GPIO pins
// GPIO pin for flow sensor
const int flowPin = D2;

// Irrigation state and Tracking Variables
int currentRelay = -1;
unsigned long requiredPulses = 0;
unsigned long pulseCount = 0;
unsigned long startTime = 0;
unsigned long timeoutDuration = 0;
unsigned long lastPulseTime = 0;

IrrigationState irrigationState = IDLE;

// Relays - SEE GLOBALS FOR MAX RELAY
//int relayPins[MAX_RELAY] = {D8, D7, D6, D5, A0, A1, A2}; // Rev B
//int relayPins[MAX_RELAY] = {A0, A1, A2, A3, D11, D4, D3, D8, A4, A5, D13, D12, D10, D9}; //Rev C
int relayPins[MAX_RELAY] = {A0, A1, A2, A3, D11, D4, D3}; //Rev C

// Job Iteration
String jobDevice;
int jobRelay[MAX_JOBS];
int jobWaterQty[MAX_JOBS];
int totalJobs = 0;
int currentJobIndex = 0;
bool jobsPending = false;
bool jobJustStarted = false;

// Moon Juice state and tracking
// int requiredSteps = 0;
// unsigned long intervalMicros = 0; // microseconds between pulses
// unsigned long lastPulseTimeStepper = 0;
// int stepsSent = 0;

// Heartbeat
//unsigned long lastHeartbeat = 0;
//const unsigned long heartbeatInterval = 60000 * 30; // 60 seconds * 30 minutes
// const unsigned long heartbeatInterval = 60000 * 5;    // 60 seconds * 5 minutes - for debugging

// Flow Variables
const unsigned long noFlowThreshold = 30000; // 30 seconds without pulses to allow for valve to open fully
//unsigned long lastPulseCount = 0;
//float flow = 0.0;

// Temperature Sensor
bool dht20Error = false;
float tempF = 0.0;
float humidity = 0.0;

// Debug Print Enable or Disable
bool debugPrint = true;
bool particlePrint = true;

//bool ForceIrrigate = false;

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
    // If it is counting and no other systems are irrigating - could be a leak?
    if (irrigationState == RUNNING)
    { // Count Flow only if RUNNING
        pulseCount++;
        lastPulseTime = millis();
    }
}

void setup() {
    Wire.begin();
    USBSerial.begin(9600);
    waitFor(USBSerial.isConnected, 10000);

    Particle.connect();
    waitUntil(Particle.connected);
    //printDebugMessage("✅ Particle cloud connected");

    // Set relay pins as outputs and turn off initially
    for (int i = 0; i < MAX_RELAY; i++) {
        pinMode(relayPins[i], OUTPUT);
        digitalWrite(relayPins[i], LOW);
    }
    // Set up pulse input
    pinMode(flowPin, INPUT_PULLUP);
    attachInterrupt(flowPin, pulseISR, FALLING);

    //Set states for sequencing
    if(System.deviceID() == FIRST){
        //irrigationState = IDLE;
    }
    else if(System.deviceID() == SECOND){
        irrigationState = WAITING;
    }
    else if(System.deviceID() == THIRD){
        irrigationState = WAITING;
    }

    memset(debugText, 0, sizeof(debugText));

    if(dht.begin()){
        dht20Error = false;
        USBSerial.println("✅ DHT20 connected - yay");
    }
    else{
        dht20Error = true;
        USBSerial.println("❌ DHT20 connect error - poop");
    }

    if (!dht20Error) {
        int status = dht.read();
        if(status == DHT20_OK){
            tempF = (dht.getTemperature() * 9.0 / 5.0) + 32.0;
            humidity = dht.getHumidity();
        }
        else{
            USBSerial.println(status);
            USBSerial.println("❌ DHT20 read error - poop");
            tempF = 999.0;
            humidity = 999.0;
        }
    } else {
        tempF = 999.0;
        humidity = 999.0;
    }

    char payload[256];
    snprintf(payload, sizeof(payload),
    "tempF: %.1f, hum: %.1f%%",
    tempF, humidity);
    USBSerial.println(payload);

    //Particle Functions
    Particle.function("reboot", resetBoron);

    //Particle.function("stepperRotations", stepperRotations);

    // Subscribe to cloud messages
    // Force Irrigation Event
    //Particle.subscribe("forceirrigate", handleForceIrrigationEvent);
    // Main Irrigation Event
    Particle.subscribe("irrigate", handleIrrigationEvent);
    // Activates Temperature read from cloud
    Particle.subscribe("read_sensors", handleTempSensorRead);
    // Abort Irrigation
    Particle.subscribe("abortIrrigate", handleAbortEvent);
    // Device status
    Particle.subscribe("jobStatus", handleIrrigationJobs);
    // Announce being up
    printDebugMessage("✅ Ready for irrigation events :-)");

    //Function on Particle dashboard to handle general commands
	Particle.function("CMD Write", consoleCmd);


    //Print firmware rev at start
    USBSerial.print("firmware_version"); USBSerial.println(String::format(": %d", __system_product_version));

    //This publish is successful
    //Particle.publish("jobStatus", "jobStatus Tester", PRIVATE);

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
    

       





 