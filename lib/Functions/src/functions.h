#pragma once
#include "Particle.h"
#include "ArduinoJson.h"    
#include "DHT20.h"

extern DHT20 dht;

void handleIrrigationEvent(const char *event, const char *data);
void stopIrrigation();
void publishIrrigationStatus(const char *status);
void handleAbortEvent(const char *event, const char *data);
void printDebugMessage(String msg);
void printDebugMessagef(const char *format, ...);
int splitString(const String &input, String output[], int maxParts);
void startNextJob();

void checkUSBSerial();
void processCommand(char inBuf[]);

void closeMasterValve();
void openMasterValve();
void handleMasterValveEvent(const char *event, const char *data);

bool readTemp(const char* zone, double& tempF, double& humidity);
void onCmd(const char *event, const char *data);
void handleSensorRequest(const char *data);


// Particle Functions
int resetBoron(String cmd);
int consoleCmd(String);
