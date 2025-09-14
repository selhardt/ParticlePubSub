#pragma once
#include "Particle.h"
#include "ArduinoJson.h"    
#include "DHT20.h"

extern DHT20 dht;

void handleIrrigationEvent(const char *event, const char *data);
void stopIrrigation();
void publishIrrigationStatus(const char *status);
void handleTempSensorRead(const char *event, const char *data);
void handleAbortEvent(const char *event, const char *data);
void printDebugMessage(String msg);
void printDebugMessagef(const char *format, ...);
int splitString(const String &input, String output[], int maxParts);
void startNextJob();
//void handleForceIrrigationEvent(const char *event, const char *data);

void checkUSBSerial();
void processCommand(char inBuf[]);

// Particle Functions
int resetBoron(String cmd);
int consoleCmd(String);
