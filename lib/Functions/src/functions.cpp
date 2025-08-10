#include <functions.h>
#include <globals.h>

extern int relayPins[];
extern unsigned long lastHeartbeat;
extern const unsigned long heartbeatInterval;
extern volatile unsigned long pulseCount;

extern int currentRelay;
extern unsigned long requiredPulses;
extern unsigned long timeoutDuration;

extern unsigned long startTime;

extern IrrigationState irrigationState;
extern IrrigationState fertilizerState;

extern bool fertEnabled;

extern int requiredSteps;
extern unsigned long intervalMicros;
extern unsigned long lastPulseTime;
extern int stepsSent;
extern bool bme280Error;
extern unsigned long lastPulseTimeStepper;
extern bool debugPrint;
extern bool particlePrint;

extern float tempF;
extern float humidity;

extern int stepperSleep;

const int stepperPin = D4;

extern int jobRelay[MAX_JOBS];
extern int jobWaterQty[MAX_JOBS];
extern int jobFertQty[MAX_JOBS];
extern int totalJobs;
extern int currentJobIndex;
extern bool jobsPending;
extern String jobDevice;
extern unsigned long lastStatusUpdate;
extern bool jobJustStarted;

// extern bool upstreamIdle;
// extern String upstreamDevice;
// extern DevicePosition devicePosition;

Adafruit_BME280 bme; // Define the object declared in the header

//Particle Function - no need to check deviceID as sent from particle dashboard
int stepperRotations(String rotations){

    int irotations = (int)rotations.toInt();

    // Format message using snprintf
    char msg[64];
    snprintf(msg, sizeof(msg), "✅ Running %d stepper rotations", irotations);
    printDebugMessage(msg);

    digitalWrite(stepperSleep, HIGH);  
    unsigned long now = millis();

    for (int i = 0; i < irotations * 200; i++) { 
        digitalWrite(stepperPin, HIGH);
        delayMicroseconds(STEPPER_DELAY_US);
        digitalWrite(stepperPin, LOW);
        delayMicroseconds(STEPPER_DELAY_US);
    }

    digitalWrite(stepperSleep, LOW); 

    // Optional performance message
    unsigned long elapsed = millis() - now;
    snprintf(msg, sizeof(msg), "✅ Stepper done in %lu ms", elapsed);
    printDebugMessage(msg);

    return 0;
}
//Particle Function - no need to check deviceID as sent from particle dashboard
int resetBoron(String cmd){
    printDebugMessage("⚠️ Reboot Initiated");
    delay(30000);   //wait for system to send debug message
    System.reset();
    return 0;
}

void stopFertilizer() {
    if (fertilizerState == RUNNING){
        printDebugMessage("✅ Stopping MoonJuice Dispense");
        stepsSent = 0;
        digitalWrite(stepperSleep, LOW); 
        fertilizerState = IDLE;
    }
}

void stopIrrigation(){
    digitalWrite(relayPins[currentRelay], LOW);     //relay drive off
    pulseCount = 0;
    irrigationState = IDLE;
    printDebugMessage("✅ Stopping Water Dispense");
}

//Read Temp Sensor
void handleTempSensorRead(const char *event, const char *data) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data);

    if (err) {
        printDebugMessage("❌ Temp Sensor Read Error");
        return;
    }

    const char* targetDevice = doc["device"];

    if (!targetDevice || String(targetDevice) != System.deviceID()) {
        printDebugMessage("⚠️ Temp read ignored - device mismatchxxx");
        return;
    }

    char payload[256];

    if (!bme280Error) {
        tempF = bme.readTemperature() * 9.0 / 5.0 + 32.0;
        humidity = bme.readHumidity();
    } else {
        tempF = 999.0;
        humidity = 999.0;
    }

    snprintf(payload, sizeof(payload),
        "{\"device\":\"%s\",\"tempF\":\"%.2f\",\"hum\":\"%.2f\"}",
        System.deviceID().c_str(), tempF, humidity);

    Serial.print("TemF = ");
    Serial.println(tempF);

    Serial.print("Hum = ");
    Serial.println(humidity);

    Particle.publish("sensorData", payload, PRIVATE);
    printDebugMessagef("🌡 Temp Read: %.2f °F, Humidity: %.2f %%", tempF, humidity);

}


void handleAbortEvent(const char *event, const char *data) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data);

    if (err) {
        printDebugMessage("❌ Invalid abort JSON");
        return;
    }

    const char* targetDevice = doc["device"];
    if (!targetDevice || String(targetDevice) != System.deviceID()) {
        printDebugMessage("⚠️ Abort ignored - device mismatch");
        return;
    }

    printDebugMessage("🛑 Aborting Irrigation Event");
    stopFertilizer();
    stopIrrigation();
}

void handleIrrigationEvent(const char *event, const char *data) {
    if (!data || strcmp(data, "[object Object]") == 0 || strcmp(data, "next") == 0) return;

    JsonDocument doc;
    if (deserializeJson(doc, data)) {
        printDebugMessage("❌ JSON parse error in irrigation event");
        return;
    }

    //Check if message is for this deviceID
    const char* target = doc["device"];
    if (!target || strcmp(target, System.deviceID().c_str()) != 0) {
        //printDebugMessage("⚠️ Irrigation event not for this device");
        return;
    }

    //If irrigation already in progress, ignore new irrigation jobs
    //Happens if manually triggered on NR AND then the 30 minute check sees the calendar event
    //Could also happen if the calendar event is longer than 30 minutes 
    if(irrigationState == RUNNING){ //Handles re-activated job
        printDebugMessage("❌ Already irrigating - Ignoring Irrigation Event");
        return;
    }

    String relayStr = doc["relay"] | "";
    String waterStr = doc["waterQty"] | "";
    String fertStr = doc["fertQty"] | "";

    String relayParts[MAX_JOBS];
    String waterParts[MAX_JOBS];
    String fertParts[MAX_JOBS];

    totalJobs = splitString(relayStr, relayParts, MAX_JOBS);
    //Serial.print("totalJobs ");
    //Serial.println(totalJobs);
    int waterCount = splitString(waterStr, waterParts, MAX_JOBS);
    int fertCount = splitString(fertStr, fertParts, MAX_JOBS);
    //Serial.print("waterCount ");
    //Serial.println(waterCount);

    if (waterCount != totalJobs || fertCount != totalJobs) {
        printDebugMessage("❌ Mismatched job field lengths");
        totalJobs = 0;
        return;
    }

    for (int i = 0; i < totalJobs; i++) {
        jobRelay[i] = relayParts[i].toInt();
        jobWaterQty[i] = waterParts[i].toInt();
        jobFertQty[i] = fertParts[i].toInt();
    }

    currentJobIndex = 0;
    jobsPending = true;
    jobDevice = target;

    //Trigger a temperature read once per Job set
    if (!bme280Error) {
        tempF = bme.readTemperature() * 9.0 / 5.0 + 32.0;
        humidity = bme.readHumidity();
    } else {
        tempF = 999.0;
        humidity = 999.0;
    }

    char payload[256];
    snprintf(payload, sizeof(payload),
    "{\"device\":\"%s\",\"tempF\":\"%.2f\",\"hum\":\"%.2f\"}",
    System.deviceID().c_str(), tempF, humidity);

    Particle.publish("sensorData", payload, PRIVATE);
    //printDebugMessagef("🌡 Temp Read: %.2f °F, Humidity: %.2f %%", tempF, humidity);

    startNextJob();
}

void publishIrrigationStatus(const char* status) {
    JsonDocument doc;
    doc["device"] = System.deviceID();
    doc["relay"] = currentRelay + 1;
    doc["status"] = status;
    //if in progress, use the qty to dispense, if low_flow, no_flow etc use quantity actually dispensed
    // if(String(status) == "in_progress"){
    //     doc["waterQty"] = jobWaterQty[currentJobIndex];
    //     doc["fertQty"] = jobFertQty[currentJobIndex];
    // }
    // else{
    //Always show as dispensed
    doc["waterQty"] = (round)((double)pulseCount / (660.0 * 3.78541));
    doc["fertQty"] = (int)(stepsSent / (120 * 200));  // integer oz estimate
    // }

    char payload[256];
    serializeJson(doc, payload);
    Particle.publish("irrigation_status", payload, PRIVATE);
    //printDebugMessage(String::format("✅ Irrigation status: %s", payload));
}

// void publishJobStatus(const char* status) {
//     JsonDocument doc;
//     doc["device"] = System.deviceID();
//     doc["status"] = status;
//     char payload[256];
//     serializeJson(doc, payload);
//     printDebugMessage(String::format("✅✅✅✅ Job status: %s", payload));
//     Particle.publish("jobStatus", payload, PRIVATE);
// }

void printDebugMessage(String msg) {
    const size_t maxLen = 256;
    char debugText[maxLen];

    // Sanitize input to prevent malformed JSON
    msg.replace("\"", "'");  // Replace quotes in message to avoid breaking JSON

    // Build properly quoted JSON
    snprintf(debugText, maxLen,
        "{\"device\":\"%s\",\"msg\":\"%s\"}",
        System.deviceID().c_str(),
        msg.c_str()
    );

    if (debugPrint) Serial.println(debugText);
    if (particlePrint) Particle.publish("TreeWaterDebug", debugText, PRIVATE);
}

void printDebugMessagef(const char* format, ...) {
    const size_t maxLen = 128;
    char formatted[maxLen];

    va_list args;
    va_start(args, format);
    vsnprintf(formatted, maxLen, format, args);
    va_end(args);

    printDebugMessage(String(formatted));
}

int splitString(const String& input, String output[], int maxParts) {
    int count = 0;
    int start = 0;
    int idx = input.indexOf(',', start);
    while (idx != -1 && count < maxParts) {
        output[count++] = input.substring(start, idx);
        start = idx + 1;
        idx = input.indexOf(',', start);
    }
    if (start < (int)input.length() && count < maxParts) {
        output[count++] = input.substring(start);
    }
    return count;
}

void startNextJob() {
    lastStatusUpdate = millis();  // reset when new job starts
    int relay = jobRelay[currentJobIndex];
    int waterQty = jobWaterQty[currentJobIndex];
    int fertQty = jobFertQty[currentJobIndex];

    //System.deviceID()
    //If mydeviceID is e00fce68db61e483e6b7a085, press on and start
    //If mydeviceID is e00fce687edca63b21266dfc wait for e00fce68db61e483e6b7a085 to publish irrigation complete
    //if mydeviceID is e00fce68195036200d338fb6 wait for e00fce687edca63b21266dfc to publish irrigatino complete

    if(System.deviceID() == FIRST){             //First Job in queue - of to start
        //nothing required
    }
    else if(System.deviceID() == SECOND){       //2nd Job in queue - wait for first to finish
        if(irrigationState == WAITING){
            printDebugMessage("❌ SECOND is WAITING");
            return;
        }
    }
    else if(System.deviceID() == THIRD){        //3rd Job in queue - wait for second to finish
        if(irrigationState == WAITING){
            printDebugMessage("❌ THIRD is WAITING");
            return;
        }
    }

    if (currentJobIndex >= totalJobs)
    {
        //printDebugMessage(String::format("🚿 Job #%d: Relay %d, %d gal, %d oz", currentJobIndex+1, relay, waterQty, fertQty));
        //printDebugMessage(String::format("🚿 Job #%d: Total Jobs %d", currentJobIndex, totalJobs));
        jobsPending = false;
        printDebugMessage("✅ All irrigation jobs complete");

        char payload[256];
        snprintf(payload, sizeof(payload),
                 "{\"device\":\"%s\",\"status\":\"%s\"}",
                 System.deviceID().c_str(), "jobs_complete");
        //TODO something better to make sure that the publish happens
        Particle.publish("jobStatus", payload, PRIVATE); // signals SECOND and THIRD devices to progress
        return;
    }

    currentRelay = relay - 1;
    //requiredPulses = (unsigned long)(waterQty * PULSES_PER_GALLON);
    requiredPulses = (unsigned long)((double)waterQty * 660.0 * 3.78541);
    timeoutDuration = waterQty * 90000UL;
    pulseCount = 0;

    fertEnabled = (fertQty > 0);
    if (fertEnabled) {
        digitalWrite(stepperSleep, HIGH);
        requiredSteps = fertQty * 120 * 200; // oz * rot/oz * steps/rot
        intervalMicros = (5UL * 60 * 1000000UL) / requiredSteps; // 5 min deploy
        stepsSent = 0;
        lastPulseTimeStepper = micros();
        fertilizerState = RUNNING;
    }

    digitalWrite(relayPins[currentRelay], HIGH);
    jobJustStarted = true;
    startTime = millis();
    irrigationState = RUNNING;
    ////publishDeviceCommStatus("irrigating");  //device to device for sequencing
    publishIrrigationStatus("in_progress");
}

void handleIrrigationJobs(const char *event, const char *data){
    //Each Device only publishes one "jobs_complete"
    JsonDocument doc;
    if (deserializeJson(doc, data)) {
        printDebugMessage("❌ JSON parse error in irrigation jobs");
        return;
    }

    const char* dev = doc["device"];
    const char* status = doc["status"];

    //This happens
    // Serial.println(SECOND);
    if(System.deviceID() == SECOND){
        Serial.println(dev);
        Serial.println(status);
    }

    char payload[256];
    serializeJson(doc, payload);
    printDebugMessage(String::format("✅ handleIrrigationJobs Job status: %s", payload));

    if(String(dev) == FIRST && String(status) == "jobs_complete"){
        if(System.deviceID() == SECOND) Serial.println("FIRST is jobs_complete");
        if (System.deviceID() == SECOND)
        {
            Serial.println("✅✅✅ Set 2nd Device to IDLE - should now start");
            printDebugMessage("✅✅✅ Set 2nd Device to IDLE - should now start");
            irrigationState = IDLE;             //Move SECOND to IDLE so that start next job runs and sets RUNNING
            startNextJob();
            return;
        }
    }
    if(String(dev) == SECOND && String(status) == "jobs_complete"){
        if(System.deviceID() == THIRD){
            printDebugMessage("✅✅✅ Set 3rd Device to IDLE - should now start");
            irrigationState = IDLE;             //Move SECOND to IDLE so that start next job runs and sets RUNNING
            startNextJob();
            return;
        }
    }


}


