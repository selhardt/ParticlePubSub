#include <functions.h>
#include <globals.h>

extern int relayPins[];
extern volatile unsigned long pulseCount;

extern int currentRelay;
extern unsigned long requiredPulses;
extern unsigned long timeoutDuration;

extern unsigned long startTime;

extern IrrigationState irrigationState;

extern int requiredSteps;
extern unsigned long intervalMicros;
extern unsigned long lastPulseTime;
extern int stepsSent;
extern bool dht20Error;
extern bool debugPrint;
extern bool particlePrint;

extern float tempF;
extern float humidity;

extern int jobRelay[MAX_RELAY];
extern int jobWaterQty[MAX_RELAY];

extern int totalJobs;
extern int currentJobIndex;
extern bool jobsPending;
extern unsigned long lastStatusUpdate;
extern bool jobJustStarted;
extern String myZone;

//Command Parser
size_t readBufOffset = 0;
extern char readBuf[];

DHT20 dht;

//Particle Function - no need to check deviceID as sent from particle dashboard
int resetBoron(String cmd){
    printDebugMessage("⚠️ Reboot Initiated");
    delay(30000);   //wait for system to send debug message
    System.reset();
    return 0;
}

void stopIrrigation(){
    digitalWrite(relayPins[currentRelay], LOW);     //relay drive off
    pulseCount = 0;
    irrigationState = IDLE;
    String myMsg = "✅ Stopping Water Dispense on relay: ";
    myMsg += String(currentRelay + 1);
    printDebugMessage(myMsg);
}

//Check for Command Line Commands
void checkUSBSerial()
{
    while (USBSerial.available()) // USB Serial Port
    {
        Particle.process();
        if (readBufOffset < 100)
        {
            char c = USBSerial.read();
            if (c != '\n')
            {
                readBuf[readBufOffset++] = c;
                Serial.print(String(c));
            }
            else
            {
                // End of line character found, process line
                Serial.println('\r');
                readBuf[readBufOffset] = 0;
                processCommand(readBuf);
                memset(readBuf, 0, 100);
                readBufOffset = 0;
            }
        }
        else
        {
            USBSerial.println("readBuf overflow, emptying buffer");
            readBufOffset = 0;
        }
    }
}

//Read Temp Sensor
void handleTempSensorRead(const char *event, const char *data) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data);

    if (err) {
        printDebugMessage("❌ Temp Sensor JSON Error");
        return;
    }
    const char* targetZone = doc["zone"];

    // USBSerial.print("target zone is ");
    // USBSerial.println(String(targetZone));
    // USBSerial.print("myZone is ");
    // USBSerial.println(myZone);

    if (String(targetZone) != myZone) {
        printDebugMessage("⚠️ Temp read ignored - device mismatchxxx");
        return;
    }

    char payload[256];

    //todo put the read here too
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

    snprintf(payload, sizeof(payload),
        "{\"zone\":\"%s\",\"tempF\":\"%.2f\",\"hum\":\"%.2f\"}",
        myZone.c_str(), tempF, humidity);
    Particle.publish("sensorData", payload, PRIVATE);

    USBSerial.print("dht error"); USBSerial.println(dht20Error);
    snprintf(payload, sizeof(payload),
             "tempF: %.1f, hum: %.1f%%",
             tempF, humidity);
    USBSerial.println(payload);

    printDebugMessagef("🌡 Temp Read: %.2f °F, Humidity: %.2f %%", tempF, humidity);

}


void handleAbortEvent(const char *event, const char *data) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data);

    if (err) {
        printDebugMessage("❌ Invalid abort JSON");
        return;
    }
    const char* targetZone = doc["zone"];

    if (String(targetZone) != myZone) {
        printDebugMessage("⚠️ Abort ignored - device mismatch");
        return;
    }

    printDebugMessage("🛑 Aborting Irrigation Event");
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
    const char* targetZone = doc["zone"];
    if (String(targetZone) != myZone) {
        printDebugMessage("⚠️ Irrigation event not for this device");
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

    String relayParts[MAX_RELAY];
    String waterParts[MAX_RELAY];

    totalJobs = splitString(relayStr, relayParts, MAX_RELAY);
    int waterCount = splitString(waterStr, waterParts, MAX_RELAY);

    if (waterCount != totalJobs) {
        printDebugMessage("❌ Mismatched job field lengths");
        totalJobs = 0;
        return;
    }

    for (int i = 0; i < totalJobs; i++) {
        jobRelay[i] = relayParts[i].toInt();
        jobWaterQty[i] = waterParts[i].toInt();
    }

    currentJobIndex = 0;
    jobsPending = true;

    //Trigger a temperature read once per Job set
    if (!dht20Error)
    {
        int status = dht.read();
        if(status == DHT20_OK){
            tempF = (dht.getTemperature() * 9.0 / 5.0) + 32.0;
            humidity = dht.getHumidity();
        }
        else{
            tempF = 999.0;
            humidity = 999.0;
        }
    }
    else
    {
        tempF = 999.0;
        humidity = 999.0;
    }

    char payload[256];

    snprintf(payload, sizeof(payload),
    "{\"zone\":\"%s\",\"tempF\":\"%.1f\",\"hum\":\"%.1f\"}",
    myZone.c_str(), tempF, humidity);
    Particle.publish("sensorData", payload, PRIVATE);

    startNextJob();
}

void publishIrrigationStatus(const char* status) {
    JsonDocument doc;
    doc["zone"] = myZone;
    doc["relay"] = currentRelay + 1;
    doc["status"] = status;
    doc["waterQty"] = (round)((double)pulseCount / (660.0 * 3.78541));

    char payload[256];
    serializeJson(doc, payload);
    Particle.publish("irrigation_status", payload, PRIVATE);
    //printDebugMessage(String::format("✅ Irrigation status: %s", payload));
}

void printDebugMessage(String msg) {
    const size_t maxLen = 256;
    char debugText[maxLen];

    // Sanitize input to prevent malformed JSON
    msg.replace("\"", "'");  // Replace quotes in message to avoid breaking JSON

    // Build properly quoted JSON
    snprintf(debugText, maxLen,
        "{\"zone\":\"%s\",\"msg\":\"%s\"}",
        myZone.c_str(),
        msg.c_str()
    );

    if (debugPrint) USBSerial.println(debugText);
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

    Particle.process();

    if (currentJobIndex >= totalJobs)
    {
        //printDebugMessage(String::format("🚿 Job #%d: Relay %d, %d gal", currentJobIndex+1, relay, waterQty));
        // printDebugMessage(String::format("🚿 Job #%d: Total Jobs %d", currentJobIndex, totalJobs));
        jobsPending = false;
        printDebugMessage("✅ All irrigation jobs complete");

        char payload[256];
        snprintf(payload, sizeof(payload),
            "{\"zone\":\"%s\",\"status\":\"%s\"}",
            myZone.c_str(), "jobs_complete");

        Particle.publish("jobStatus", payload, PRIVATE); // signals SECOND and THIRD devices to progress
        return;
    }

    currentRelay = relay - 1;
    requiredPulses = (unsigned long)((double)waterQty * 660.0 * 3.78541);
    timeoutDuration = waterQty * 90000UL;
    pulseCount = 0;

    // USBSerial.print("currentRelay ");
    // USBSerial.println(currentRelay);
    // USBSerial.print("relayPins[currentRelay] ");
    // USBSerial.println(relayPins[currentRelay]);

    digitalWrite(relayPins[currentRelay], HIGH);
    jobJustStarted = true;
    startTime = millis();
    irrigationState = RUNNING;
    publishIrrigationStatus("in_progress");
}


void processCommand( char inBuf[])
{
char *output = NULL;

USBSerial.println("Executing Command: " + String(inBuf));
//Particle.publish("IOTDebug", "Executing Command: " + String(inBuf), 300, PRIVATE);

// Relay1
output = strstr(inBuf, "r1on");
if (output) digitalWrite(relayPins[0], HIGH);
output = strstr(inBuf, "r1off");
if (output) digitalWrite(relayPins[0], LOW);

// Relay2
output = strstr(inBuf, "r2on");
if (output) digitalWrite(relayPins[1], HIGH);
output = strstr(inBuf, "r2off");
if (output) digitalWrite(relayPins[1], LOW);

// Relay3
output = strstr(inBuf, "r3on");
if (output) digitalWrite(relayPins[2], HIGH);
output = strstr(inBuf, "r3off");
if (output) digitalWrite(relayPins[2], LOW);

// Relay4
output = strstr(inBuf, "r4on");
if (output) digitalWrite(relayPins[3], HIGH);
output = strstr(inBuf, "r4off");
if (output) digitalWrite(relayPins[3], LOW);

// Relay5
output = strstr(inBuf, "r5on");
if (output) digitalWrite(relayPins[4], HIGH);
output = strstr(inBuf, "r5off");
if (output) digitalWrite(relayPins[4], LOW);

// Relay6
output = strstr(inBuf, "r6on");
if (output) digitalWrite(relayPins[5], HIGH);
output = strstr(inBuf, "r6off");
if (output) digitalWrite(relayPins[5], LOW);

// Relay7
output = strstr(inBuf, "r7on");
if (output) digitalWrite(relayPins[6], HIGH);
output = strstr(inBuf, "r7off");
if (output) digitalWrite(relayPins[6], LOW);

// Relay8
output = strstr(inBuf, "r8on");
if (output) digitalWrite(relayPins[7], HIGH);
output = strstr(inBuf, "r8off");
if (output) digitalWrite(relayPins[7], LOW);

// Relay9
output = strstr(inBuf, "r9on");
if (output) digitalWrite(relayPins[8], HIGH);
output = strstr(inBuf, "r9off");
if (output) digitalWrite(relayPins[8], LOW);

// Relay10
output = strstr(inBuf, "r10on");
if (output) digitalWrite(relayPins[9], HIGH);
output = strstr(inBuf, "r10off");
if (output) digitalWrite(relayPins[9], LOW);

// Relay11
output = strstr(inBuf, "r11on");
if (output) digitalWrite(relayPins[10], HIGH);
output = strstr(inBuf, "r11off");
if (output) digitalWrite(relayPins[10], LOW);

// Relay12
output = strstr(inBuf, "r12on");
if (output) digitalWrite(relayPins[11], HIGH);
output = strstr(inBuf, "r12off");
if (output) digitalWrite(relayPins[11], LOW);

// Relay13
output = strstr(inBuf, "r13on");
if (output) digitalWrite(relayPins[12], HIGH);
output = strstr(inBuf, "r13off");
if (output) digitalWrite(relayPins[12], LOW);

// Relay14
output = strstr(inBuf, "r14on");
if (output) digitalWrite(relayPins[13], HIGH);
output = strstr(inBuf, "r14off");
if (output) digitalWrite(relayPins[13], LOW);

//All Relays
output = strstr(inBuf, "allron");
if (output){
    for (int i = 0; i < MAX_RELAY; i++) {
    digitalWrite(relayPins[i], HIGH);
    }
}
output = strstr(inBuf, "allroff");
if (output){
    for (int i = 0; i < MAX_RELAY; i++) {
    digitalWrite(relayPins[i], LOW);
    }
}


output = strstr(inBuf,"rts");	//Read Temp Sensor
    if(output)
    {
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
    }

}

int consoleCmd(String Command)
{
	Command.toCharArray(readBuf, 100);
	processCommand(readBuf);
	return 1;
}


