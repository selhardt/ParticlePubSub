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

//TODO
//***************************************REV 41 Items******************************************************************************************
//  remove all FIRST/SECON/THIRD references and rever to NR handled sequencing
//  r̶e̶m̶o̶v̶e̶ W̶A̶I̶T̶I̶N̶G̶ a̶s̶ a̶ s̶t̶a̶t̶e̶
//  r̶e̶m̶o̶v̶e̶ j̶o̶b̶s̶t̶a̶t̶u̶s̶ s̶u̶b̶s̶c̶r̶i̶b̶e̶
//  c̶h̶a̶n̶g̶e̶ t̶o̶ a̶ t̶o̶t̶a̶l̶ o̶f̶ 1̶4̶ r̶e̶l̶a̶y̶s̶ a̶n̶d̶ r̶e̶m̶o̶v̶e̶ 7̶/̶1̶4̶ o̶p̶t̶i̶o̶n̶
//  r̶e̶p̶l̶a̶c̶e̶ M̶A̶X̶_̶J̶O̶B̶S̶ (̶w̶a̶s̶ 8̶)̶ w̶i̶t̶h̶ M̶A̶X̶_̶R̶E̶L̶A̶Y̶ (̶m̶a̶y̶b̶e̶ s̶h̶o̶u̶l̶d̶ b̶e̶ +̶1̶)̶
//  c̶o̶n̶v̶e̶r̶t̶ a̶l̶l̶ d̶e̶v̶i̶c̶e̶I̶D̶'̶s̶ t̶o̶ m̶y̶Z̶o̶n̶e̶ n̶a̶m̶e̶
//  Reduce debug prints and fix any old device ID mapping in NR
//****************************************BUGS*************************************************************************************************


#include "Particle.h"
#include "ArduinoJson.h"
#include "globals.h"
#include "functions.h"
#include <Wire.h>

//This must be directly programmed once prior to OTA updates
PRODUCT_VERSION(41); // Increment this with each new upload

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
int relayPins[MAX_RELAY] = {A0, A1, A2, A3, D11, D4, D3, D8, A4, A5, D13, D12, D10, D9}; //Rev C

// Job Iteration
int jobRelay[MAX_RELAY];
int jobWaterQty[MAX_RELAY];
int totalJobs = 0;
int currentJobIndex = 0;
bool jobsPending = false;
bool jobJustStarted = false;

// Flow Variables
const unsigned long noFlowThreshold = 30000; // 30 seconds without pulses to allow for valve to open fully

// Temperature Sensor
bool dht20Error = false;
float tempF = 0.0;
float humidity = 0.0;

// Debug Print Enable or Disable
bool debugPrint = true;
bool particlePrint = true;

//Device Zone Mapping
String myZone;

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

    //Use to replace device ID with Zone Name
    if(System.deviceID() == "e00fce68db61e483e6b7a085"){
        myZone = "ZONE1";
    }else if (System.deviceID() == "e00fce687edca63b21266dfc"){
        myZone = "ZONE2";
    }else if(System.deviceID() == "e00fce68195036200d338fb6"){
        myZone = "ZONE3";
    }

    // Set relay pins as outputs and turn off initially
    for (int i = 0; i < MAX_RELAY; i++) {
        pinMode(relayPins[i], OUTPUT);
        digitalWrite(relayPins[i], LOW);
    }
    // Set up pulse input
    pinMode(flowPin, INPUT_PULLUP);
    attachInterrupt(flowPin, pulseISR, FALLING);

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
    // Main Irrigation Event
    Particle.subscribe("irrigate", handleIrrigationEvent);
    // Activates Temperature read from cloud
    Particle.subscribe("read_sensors", handleTempSensorRead);
    // Abort Irrigation
    Particle.subscribe("abortIrrigate", handleAbortEvent);
    // Announce being up
    //printDebugMessage("✅ Ready for irrigation events :-)");
    //Function on Particle dashboard to handle general commands
	Particle.function("CMD Write", consoleCmd);

    //Print firmware rev at start
    USBSerial.print("firmware_version"); USBSerial.println(String::format(": %d", __system_product_version));


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
    

       





 