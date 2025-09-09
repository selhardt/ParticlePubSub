// ArduinoJson - https://arduinojson.org
// Copyright © 2014-2025, Benoit BLANCHON
// MIT License
//
// This example shows how to deserialize a MessagePack document with
// ArduinoJson.
//
// https://arduinojson.org/v7/example/msgpack-parser/

#include <ArduinoJson.h>

void setup() {
  // Initialize serial port
  USBSerial.begin(9600);
  while (!USBSerial)
    continue;

  // Allocate the JSON document
  JsonDocument doc;

  // The MessagePack input string
  uint8_t input[] = {131, 166, 115, 101, 110, 115, 111, 114, 163, 103, 112, 115,
                     164, 116, 105, 109, 101, 206, 80,  147, 50,  248, 164, 100,
                     97,  116, 97,  146, 203, 64,  72,  96,  199, 58,  188, 148,
                     112, 203, 64,  2,   106, 146, 230, 33,  49,  169};
  // This MessagePack document contains:
  // {
  //   "sensor": "gps",
  //   "time": 1351824120,
  //   "data": [48.75608, 2.302038]
  // }

  // Parse the input
  DeserializationError error = deserializeMsgPack(doc, input);

  // Test if parsing succeeded
  if (error) {
    USBSerial.print("deserializeMsgPack() failed: ");
    USBSerial.println(error.f_str());
    return;
  }

  // Fetch the values
  //
  // Most of the time, you can rely on the implicit casts.
  // In other case, you can do doc["time"].as<long>();
  const char* sensor = doc["sensor"];
  long time = doc["time"];
  double latitude = doc["data"][0];
  double longitude = doc["data"][1];

  // Print the values
  USBSerial.println(sensor);
  USBSerial.println(time);
  USBSerial.println(latitude, 6);
  USBSerial.println(longitude, 6);
}

void loop() {
  // not used in this example
}
