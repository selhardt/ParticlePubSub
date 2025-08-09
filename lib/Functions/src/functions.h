#pragma once
#include "Particle.h"
#include "ArduinoJson.h"    
// Add this library via Particle Workbench

#include <Adafruit_Sensor.h>
#define ARDUINO 100         // Prevent warning from Adafruit_BME280.h
#include <Adafruit_BME280.h>
// Expose the BME280 object and function
extern Adafruit_BME280 bme;

void handleIrrigationEvent(const char *event, const char *data);
void stopFertilizer();
void stopIrrigation();
void publishIrrigationStatus(const char *status);
void handleTempSensorRead(const char *event, const char *data);
void handleAbortEvent(const char *event, const char *data);
void printDebugMessage(String msg);
void printDebugMessagef(const char *format, ...);
int splitString(const String &input, String output[], int maxParts);
void startNextJob();
// void handleDeviceComm(const char *event, const char *data);
// void publishDeviceCommStatus(const char *status);

// Particle Functions
int resetBoron(String cmd);
int stepperRotations(String cmd);