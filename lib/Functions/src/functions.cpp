#include <functions.h>
#include <globals.h>

extern int relayPins[];
extern volatile unsigned long pulseCount;

extern int currentRelay;
extern unsigned long requiredPulses;
extern unsigned long timeoutDuration;
extern unsigned long leakingPulseCount;

extern unsigned long startTime;

extern IrrigationState irrigationState;

extern int requiredSteps;
extern unsigned long intervalMicros;
extern unsigned long lastPulseTime;
extern int stepsSent;
extern bool dht20Error;
extern bool debugPrint;
extern bool particlePrint;

extern int jobRelay[MAX_RELAY];
extern int jobWaterQty[MAX_RELAY];

extern int totalJobs;
extern int currentJobIndex;
//extern bool jobsPending;
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


void handleAbortEvent(const char *event, const char *data) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data);

    if (err) {
        Particle.publish("resp/error", myZone + ": ❌ Invalid abort JSON in handleAbortEvent");
        return;
    }
    const char* targetZone = doc["zone"];

    if (String(targetZone) != myZone) {
        //printDebugMessage("⚠️ Abort ignored - device mismatch");
        return;
    }

    Particle.publish("resp/error", myZone + ": 🛑 Aborting Irrigation Event");
    stopIrrigation();


}

// void handleIrrigationEvent(const char *event, const char *data) {
//     if (!data || strcmp(data, "[object Object]") == 0 || strcmp(data, "next") == 0) return;

//     JsonDocument doc;
//     if (deserializeJson(doc, data)) {
//         //printDebugMessage("❌ JSON parse error in handleIrrigationEvent");
//         Particle.publish("resp/error", myZone + ": ❌ JSON parse error in handleIrrigationEvent");
//         return;
//     }

//     //Check if message is for this deviceID
//     const char* targetZone = doc["zone"];
//     if (String(targetZone) != myZone) {
//         //printDebugMessage("⚠️ Irrigation event not for this device");
//         return;
//     }

//     //If irrigation already in progress, ignore new irrigation jobs
//     //Happens if manually triggered on NR AND then the 30 minute check sees the calendar event
//     //Could also happen if the calendar event is longer than 30 minutes 
//     if(irrigationState == RUNNING){ //Handles re-activated job
//         //printDebugMessage("❌ Already irrigating - Ignoring Irrigation Event");
//         Particle.publish("resp/error", myZone + ": ❌ Already irrigating - Ignoring Irrigation Event");
//         return;
//     }

//     String relayStr = doc["relay"] | "";
//     String waterStr = doc["waterQty"] | "";

//     String relayParts[MAX_RELAY];
//     String waterParts[MAX_RELAY];

//     totalJobs = splitString(relayStr, relayParts, MAX_RELAY);
//     int waterCount = splitString(waterStr, waterParts, MAX_RELAY);

//     if (waterCount != totalJobs) {
//         //printDebugMessage("❌ Mismatched job field lengths");
//         Particle.publish("resp/error", myZone + ": ❌ Mismatched job field lengths in handleIrrigationEvent");
//         totalJobs = 0;
//         return;
//     }

//     for (int i = 0; i < totalJobs; i++) {
//         jobRelay[i] = relayParts[i].toInt();
//         jobWaterQty[i] = waterParts[i].toInt();
//     }

//     currentJobIndex = 0;
//     jobsPending = true;

//     //Trigger a temperature read once per Job set
//     double tempF;
//     double humidity;
//     const char *zone = myZone.c_str();

//     readTemp(zone, tempF, humidity);
//     char out[192];
//     snprintf(out, sizeof(out),
//       "{\"ok\":true,\"zone\":\"%s\",\"sensor\":\"temp\",\"temp\":%.2f,\"humidity\":%.2f}",
//       zone, tempF, humidity);
//     Particle.publish("resp/sensor", out, PRIVATE);
//     startNextJob();
// }

void publishIrrigationStatus(const char* status) {
    JsonDocument doc;
    doc["zone"] = myZone;
    doc["relay"] = currentRelay + 1;
    doc["status"] = status;
    doc["waterQty"] = (round)((double)pulseCount / (660.0 * 3.78541));

    char payload[256];
    serializeJson(doc, payload);
    Particle.publish("irrigation_status", payload, PRIVATE);

}

void handleIrrigationEvent(const char *event, const char *data) {
    if (!data || strcmp(data, "[object Object]") == 0 || strcmp(data, "next") == 0) {
        Particle.publish("resp/debug", String::format("%s: 🚫 handleIrrigationEvent ignored payload", myZone.c_str()), PRIVATE);
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, data)) {
        Particle.publish("resp/error", myZone + ": ❌ JSON parse error in handleIrrigationEvent", PRIVATE);
        return;
    }

    const char* targetZone = doc["zone"];
    if (!targetZone || !targetZone[0]) {
        Particle.publish("resp/error", myZone + ": ❌ Missing zone in irrigate", PRIVATE);
        return;
    }

    // 1) Zone gate
    if (String(targetZone) != myZone) {
        Particle.publish("resp/debug", String::format("%s: ↩️ Not my zone (got %s)", myZone.c_str(), targetZone), PRIVATE);
        return;
    }

    // 2) State gate
    if (irrigationState == RUNNING) {
        Particle.publish("resp/error", myZone + ": ❌ Already irrigating - Ignoring Irrigation Event", PRIVATE);
        return;
    }

    String relayStr = doc["relay"]     | "";
    String waterStr = doc["waterQty"]  | "";

    // Trim whitespace just in case
    relayStr.trim();
    waterStr.trim();

    String relayParts[MAX_RELAY];
    String waterParts[MAX_RELAY];

    totalJobs = splitString(relayStr, relayParts, MAX_RELAY);
    int waterCount = splitString(waterStr, waterParts, MAX_RELAY);

    if (totalJobs <= 0 || waterCount <= 0) {
        Particle.publish("resp/error", myZone + ": ❌ No jobs parsed (relay/waterQty empty)", PRIVATE);
        totalJobs = 0;
        return;
    }

    if (waterCount != totalJobs) {
        Particle.publish("resp/error", myZone + ": ❌ Mismatched job field lengths in handleIrrigationEvent", PRIVATE);
        totalJobs = 0;
        return;
    }

    for (int i = 0; i < totalJobs; i++) {
        jobRelay[i]    = relayParts[i].toInt();
        jobWaterQty[i] = waterParts[i].toInt();
    }

    currentJobIndex = 0;
    //jobsPending     = true;

    Particle.publish("resp/debug", String::format("%s: ✅ Accepted irrigate jobs=%d (relays=%s | qtys=%s)",
                        myZone.c_str(), totalJobs, relayStr.c_str(), waterStr.c_str()), PRIVATE);

    // Optional: read & publish temp once per set
    double tempF = NAN, humidity = NAN;
    readTemp(myZone.c_str(), tempF, humidity);
    char out[192];
    snprintf(out, sizeof(out),
        "{\"ok\":true,\"zone\":\"%s\",\"sensor\":\"temp\",\"temp\":%.2f,\"humidity\":%.2f}",
        myZone.c_str(), tempF, humidity);
    Particle.publish("resp/sensor", out, PRIVATE);

    // 3) Actually kick the first job
    startNextJob();
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
        //jobsPending = false;
        char payload[256];
        snprintf(payload, sizeof(payload),
            "{\"zone\":\"%s\",\"status\":\"%s\"}",
            myZone.c_str(), "jobs_complete");

        Particle.publish("jobStatus", payload, PRIVATE); // signals subsequent devices to progress
        return;
    }

    currentRelay = relay - 1;
    requiredPulses = (unsigned long)((double)waterQty * 660.0 * 3.78541);
    timeoutDuration = waterQty * 90000UL;
    pulseCount = 0;

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

// Relay14 --- MASTER WATER VALVE
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
        double tempF;
        double humidity;
        const char *zone = myZone.c_str();

        readTemp(zone, tempF, humidity);

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

void enableMasterValve() {
    if (myZone == "ZONE1") {
        digitalWrite(relayPins[13], HIGH);      //Turn on Master Valve
    }
    return;
}

void disableMasterValve(){
    if (myZone == "ZONE1") {
        digitalWrite(relayPins[13], LOW);      //Turn off Master Valve
    }
    return;
}

void handleMasterValveEvent(const char *event, const char *data) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data);

    if (err) {
        //printDebugMessage("❌ Invalid masterValve JSON");
        Particle.publish("resp/error", myZone + ": ❌ Invalid abort JSON in handleMasterValveEvent");
        return;
    }
    const char* state = doc["state"];
  
    if (myZone == "ZONE1") {
    //if (myZone == "ZONE4") {    //TODO PUT BACK TO ZONE 1
        if(String(state)=="enable"){
            digitalWrite(relayPins[13], HIGH);      //Turn on Master Valve
            USBSerial.println("Enabling Master Valve");
        }
        else if (String(state)=="disable"){
            digitalWrite(relayPins[13], LOW);      //Turn off Master Valve
            USBSerial.println("Disabling Master Valve");
        }
    }

}


bool readTemp(const char* zone, double& tempF, double& humidity) {
  tempF = 999.0; humidity = 999.0;

    if(String(zone) == myZone){
        if (!dht20Error)
        {
            int status = dht.read();
            if(status == DHT20_OK){
                tempF = (dht.getTemperature() * 9.0 / 5.0) + 32.0;
                humidity = dht.getHumidity();
            }
        }
        else Particle.publish("resp/error", myZone + ": ❌ DHT20 error in readTemp");
    }
return true;
}

bool readFlow(const char* zone, int& gallons) {
    //NOTE - use ZONE1 to read the leakingflow to all  zones - clear upon read
    if(String(zone) == myZone){
        //leakingPulseCount = 74950/2;  //TODO delete this line
        gallons = (round)((double)leakingPulseCount / (660.0 * 3.78541));
        leakingPulseCount = 0;
        //TODO add a notify publish if the gallon value is above a certain amount
    }
return true;
}

// ---- Single prefix subscribe + dispatcher ----
void onCmd(const char* event, const char* data) {
    USBSerial.print("event");
    USBSerial.println(event);
    USBSerial.print("data");
    USBSerial.println(data);
    if (!event)
        return;
    // event should be "cmd/<suffix>"
    const char *prefix = "cmd/";
    if (strncmp(event, prefix, strlen(prefix)) != 0)
    {
        Particle.publish("resp/error", "{\"ok\":false,\"where\":\"dispatch\",\"err\":\"bad prefix\"}", PRIVATE);
        return;
    }
  const char* cmd = event + strlen(prefix);

  if (strcmp(cmd, "irrigate") == 0) {
    handleIrrigationEvent(event, data);
    return;
  }
  if (strcmp(cmd, "abort") == 0) {
    handleAbortEvent(event, data);
    return;
  }
  if (strcmp(cmd, "master_valve") == 0) {
    handleMasterValveEvent(event, data); // your existing code (zone-gated, relayPins[13])
    return;
  }
  if (strcmp(cmd, "sensor") == 0) {
    handleSensorRequest(data);           // new JSON -> readTemp/readFlow -> publish resp
    return;
  }

  Particle.publish("resp/error", "{\"ok\":false,\"where\":\"dispatch\",\"err\":\"unknown cmd\"}", PRIVATE);
}

// ---- New: sensor request handler ----
void handleSensorRequest(const char* data) {
  if (!data) {
    Particle.publish("resp/error", "{\"ok\":false,\"where\":\"sensor\",\"err\":\"no data\"}", PRIVATE);
    return;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data);
  if (err) {
    Particle.publish("resp/error", "{\"ok\":false,\"where\":\"sensor\",\"err\":\"bad json\"}", PRIVATE);
    return;
  }

  const char* zone = doc["zone"] | "";
  const char* sensor = doc["sensor"] | "";

  if (!zone[0]) {
    Particle.publish("resp/error", "{\"ok\":false,\"where\":\"sensor\",\"err\":\"missing zone\"}", PRIVATE);
    return;
  }
  if (!sensor[0]) {
    Particle.publish("resp/error", "{\"ok\":false,\"where\":\"sensor\",\"err\":\"missing sensor\"}", PRIVATE);
    return;
  }

  if (strcmp(sensor, "temp") == 0) {
    double t = NAN, h = NAN;
    if (!readTemp(zone, t, h)) {
      Particle.publish("resp/error", "{\"ok\":false,\"where\":\"sensor/temp\",\"err\":\"read failed\"}", PRIVATE);
      return;
    }
    // publish a compact, predictable response
    char out[192];
    snprintf(out, sizeof(out),
      "{\"ok\":true,\"zone\":\"%s\",\"sensor\":\"temp\",\"temp\":%.2f,\"humidity\":%.2f}",
      zone, t, h);
    Particle.publish("resp/sensor", out, PRIVATE);
    return;
  }

  if (strcmp(sensor, "flow") == 0) {
    int gallons = 0;
    if (!readFlow(zone, gallons)) {
      Particle.publish("resp/error", "{\"ok\":false,\"where\":\"sensor/flow\",\"err\":\"read failed\"}", PRIVATE);
      return;
    }
    char out[160];
    snprintf(out, sizeof(out),
      "{\"ok\":true,\"zone\":\"%s\",\"sensor\":\"flow\",\"gallons\":%d}",
      zone, gallons);
    Particle.publish("resp/sensor", out, PRIVATE);
    return;
  }

  Particle.publish("resp/error", "{\"ok\":false,\"where\":\"sensor\",\"err\":\"unknown sensor\"}", PRIVATE);
}




