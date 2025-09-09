//
//    FILE: DHT20_read_status.ino
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

uint8_t count = 0;


void setup()
{
  USBSerial.begin(115200);
  USBSerial.println(__FILE__);
  USBSerial.print("DHT20 LIBRARY VERSION: ");
  USBSerial.println(DHT20_LIB_VERSION);
  USBSerial.println();

  Wire.begin();
  Wire.setClock(400000);

  DHT.begin();    //  ESP32 default pins 21 22

  delay(2000);
}


void loop()
{
  int status = DHT.readStatus();
  USBSerial.println(status);
  delay(1000);
}


//  -- END OF FILE --
