//
//    FILE: DHT20_async.ino
//  AUTHOR: Rob Tillaart
// PURPOSE: Demo for DHT20 I2C humidity & temperature sensor
//     URL: https://github.com/RobTillaart/DHT20
//
//  Always check datasheet - front view
//
//          +--------------+
//  VDD ----| 1            |
//  SDA ----| 2    DHT20   |
//  GND ----| 3            |
//  SCL ----| 4            |
//          +--------------+


#include "DHT20.h"

DHT20 DHT;

uint32_t counter = 0;


void setup()
{
  USBSerial.begin(115200);
  USBSerial.println(__FILE__);
  USBSerial.print("DHT20 LIBRARY VERSION: ");
  USBSerial.println(DHT20_LIB_VERSION);
  USBSerial.println();

  Wire.begin();
  Wire.setClock(400000);

  DHT.begin();  //  ESP32 default 21, 22

  delay(2000);

  //  start with initial request
  USBSerial.println(DHT.requestData());
}


void loop()
{
  uint32_t now = millis();

  if (now - DHT.lastRead() > 1000)
  {
    DHT.readData();
    DHT.convert();

    USBSerial.print(DHT.getHumidity(), 1);
    USBSerial.print(" %RH \t");
    USBSerial.print(DHT.getTemperature(), 1);
    USBSerial.print(" °C\t");
    USBSerial.print(counter);
    USBSerial.print("\n");
    //  new request
    counter = 0;
    DHT.requestData();
  }

  //  other code here
  counter++;  //  dummy counter to show async behaviour
}


//  -- END OF FILE --
