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
//extern bool pulseStarted;

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
    //pulseStarted = false;
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
    if(irrigationState == RUNNING){
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
    Serial.print("totalJobs ");
    Serial.println(totalJobs);
    int waterCount = splitString(waterStr, waterParts, MAX_JOBS);
    int fertCount = splitString(fertStr, fertParts, MAX_JOBS);
    Serial.print("waterCount ");
    Serial.println(waterCount);

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

    startNextJob();
}

void publishIrrigationStatus(const char* status) {
    JsonDocument doc;
    doc["device"] = System.deviceID();
    doc["relay"] = currentRelay + 1;
    doc["status"] = status;
    doc["waterQty"] = jobWaterQty[currentJobIndex];
    doc["fertQty"] = jobFertQty[currentJobIndex];

    char payload[256];
    serializeJson(doc, payload);
    Particle.publish("irrigation_status", payload, PRIVATE);
    printDebugMessage(String::format("✅ Published status: %s", payload));
}


// void publishIrrigationStatus(const char* status) {
//     JsonDocument doc;
//     doc["device"] = System.deviceID();
//     doc["relay"] = currentRelay + 1;
//     doc["status"] = status;
//     doc["waterQty"] = pulseCount / PULSES_PER_GALLON;
//     doc["fertQty"] = stepsSent / (120 * 200);   //steps * 10z/120 rot * 1 rot/200 steps = oz

//     char payload[256];
//     serializeJson(doc, payload);
//     Particle.publish("irrigation_status", payload, PRIVATE);
//     printDebugMessage(String::format("✅ Published status: %s", payload));
//   }

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

    // // Only proceed if upstream is idle
    // if (devicePosition != FIRST && !upstreamIdle) {
    //     //printDebugMessage("⏳ Waiting for upstream device to become idle");
    //     return;  // Will retry on next loop cycle
    // }
    
    if (currentJobIndex >= totalJobs) {
        jobsPending = false;
        printDebugMessage("✅ All irrigation jobs complete");
        //publishDeviceCommStatus("idle");    //Device to device for sequencing
        return;
    }

    int relay = jobRelay[currentJobIndex];
    int waterQty = jobWaterQty[currentJobIndex];
    int fertQty = jobFertQty[currentJobIndex];

    printDebugMessage(String::format("🚿 Job #%d: Relay %d, %d gal, %d oz", currentJobIndex+1, relay, waterQty, fertQty));

    currentRelay = relay - 1;
    requiredPulses = (unsigned long)(waterQty * PULSES_PER_GALLON);
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
    //publishDeviceCommStatus("irrigating");  //device to device for sequencing
    publishIrrigationStatus("in_progress");
}

void publishIrrigationProgress() {
    JsonDocument doc;
    doc["device"] = System.deviceID();
    doc["relay"] = currentRelay + 1;
    doc["status"] = "in_progress";
    doc["waterQty"] = (round)((double)pulseCount / (660.0 * 3.78541));
    doc["fertQty"] = (int)(stepsSent / (120 * 200));  // integer oz estimate
    //printDebugMessage(String::format("📶 pulseCount: %lu", pulseCount));
    char payload[256];
    serializeJson(doc, payload);
    Particle.publish("irrigation_status", payload, PRIVATE);
    printDebugMessage(String::format("📶 Progress update: %s", payload));
}

// void handleDeviceComm(const char *event, const char *data) {
//     JsonDocument doc;
//     if (deserializeJson(doc, data)) return;

//     const char* dev = doc["device"];
//     const char* status = doc["status"];

//     if (!dev || !status) return;

//     // Update upstream status
//     if (String(dev) == upstreamDevice) {

//         if (upstreamDevice == "") {
//             upstreamIdle = true;  // No upstream device = first device
//         } else {
//             upstreamIdle = (String(status) == "idle");
//         }
//     }

//     // Also allow first device to proceed immediately (no upstream device)
//     if (upstreamDevice == "") {
//         upstreamIdle = true;
//     }
// }

// void publishDeviceCommStatus(const char* status) {
//     JsonDocument doc;
//     doc["device"] = System.deviceID();
//     doc["status"] = status;

//     char payload[128];
//     serializeJson(doc, payload);
//     Particle.publish("deviceComm", payload, PRIVATE);
// }
