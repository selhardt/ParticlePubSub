// ArduinoJson - https://arduinojson.org
// Copyright © 2014-2025, Benoit BLANCHON
// MIT License
//
// This example shows how to implement an HTTP server that sends a JSON document
// in the response.
// It uses the Ethernet library but can be easily adapted for Wifi.
//
// The JSON document contains the values of the analog and digital pins.
// It looks like that:
// {
//   "analog": [0, 76, 123, 158, 192, 205],
//   "digital": [1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0]
// }
//
// https://arduinojson.org/v7/example/http-server/

#include <ArduinoJson.h>
#include <Ethernet.h>
#include <SPI.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
EthernetServer server(80);

void setup() {
  // Initialize serial port
  USBSerial.begin(9600);
  while (!USBSerial)
    continue;

  // Initialize Ethernet libary
  if (!Ethernet.begin(mac)) {
    USBSerial.println(F("Failed to initialize Ethernet library"));
    return;
  }

  // Start to listen
  server.begin();

  USBSerial.println(F("Server is ready."));
  USBSerial.print(F("Please connect to http://"));
  USBSerial.println(Ethernet.localIP());
}

void loop() {
  // Wait for an incomming connection
  EthernetClient client = server.available();

  // Do we have a client?
  if (!client)
    return;

  USBSerial.println(F("New client"));

  // Read the request (we ignore the content in this example)
  while (client.available())
    client.read();

  // Allocate a temporary JsonDocument
  JsonDocument doc;

  // Create the "analog" array
  JsonArray analogValues = doc["analog"].to<JsonArray>();
  for (int pin = 0; pin < 6; pin++) {
    // Read the analog input
    int value = analogRead(pin);

    // Add the value at the end of the array
    analogValues.add(value);
  }

  // Create the "digital" array
  JsonArray digitalValues = doc["digital"].to<JsonArray>();
  for (int pin = 0; pin < 14; pin++) {
    // Read the digital input
    int value = digitalRead(pin);

    // Add the value at the end of the array
    digitalValues.add(value);
  }

  USBSerial.print(F("Sending: "));
  serializeJson(doc, USBSerial);
  USBSerial.println();

  // Write response headers
  client.println(F("HTTP/1.0 200 OK"));
  client.println(F("Content-Type: application/json"));
  client.println(F("Connection: close"));
  client.print(F("Content-Length: "));
  client.println(measureJsonPretty(doc));
  client.println();

  // Write JSON document
  serializeJsonPretty(doc, client);

  // Disconnect
  client.stop();
}

// Performance issue?
// ------------------
//
// EthernetClient is an unbuffered stream, which is not optimal for ArduinoJson.
// See: https://arduinojson.org/v7/how-to/improve-speed/

// See also
// --------
//
// https://arduinojson.org/ contains the documentation for all the functions
// used above. It also includes an FAQ that will help you solve any
// serialization problem.
//
// The book "Mastering ArduinoJson" contains a tutorial on serialization.
// It begins with a simple example, then adds more features like serializing
// directly to a file or an HTTP client.
// Learn more at https://arduinojson.org/book/
// Use the coupon code TWENTY for a 20% discount ❤❤❤❤❤
