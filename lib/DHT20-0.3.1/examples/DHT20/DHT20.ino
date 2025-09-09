//
//    FILE: DHT20.ino
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
  DHT.begin();    //  ESP32 default pins 21 22


  delay(1000);
}


void loop()
{
  if (millis() - DHT.lastRead() >= 1000)
  {
    //  READ DATA
    uint32_t start = micros();
    int status = DHT.read();
    uint32_t stop = micros();

    if ((count % 10) == 0)
    {
      count = 0;
      USBSerial.println();
      USBSerial.println("Type\tHumidity (%)\tTemp (°C)\tTime (µs)\tStatus");
    }
    count++;

    USBSerial.print("DHT20 \t");
    //  DISPLAY DATA, sensor has only one decimal.
    USBSerial.print(DHT.getHumidity(), 1);
    USBSerial.print("\t\t");
    USBSerial.print(DHT.getTemperature(), 1);
    USBSerial.print("\t\t");
    USBSerial.print(stop - start);
    USBSerial.print("\t\t");
    switch (status)
    {
      case DHT20_OK:
        USBSerial.print("OK");
        break;
      case DHT20_ERROR_CHECKSUM:
        USBSerial.print("Checksum error");
        break;
      case DHT20_ERROR_CONNECT:
        USBSerial.print("Connect error");
        break;
      case DHT20_MISSING_BYTES:
        USBSerial.print("Missing bytes");
        break;
      case DHT20_ERROR_BYTES_ALL_ZERO:
        USBSerial.print("All bytes read zero");
        break;
      case DHT20_ERROR_READ_TIMEOUT:
        USBSerial.print("Read time out");
        break;
      case DHT20_ERROR_LASTREAD:
        USBSerial.print("Error read too fast");
        break;
      default:
        USBSerial.print("Unknown error");
        break;
    }
    USBSerial.print("\n");
  }
}


//  -- END OF FILE --
