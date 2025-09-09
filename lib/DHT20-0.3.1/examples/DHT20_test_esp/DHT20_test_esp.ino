//
//    FILE: DHT20_test_esp.ino
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

DHT20 DHT(&Wire1);  //  2nd I2C interface


void setup()
{
  USBSerial.begin(115200);
  USBSerial.println(__FILE__);
  USBSerial.print("DHT20 LIBRARY VERSION: ");
  USBSerial.println(DHT20_LIB_VERSION);
  USBSerial.println();

  Wire1.begin(12, 13);  //  select your pin numbers here

  delay(2000);

  USBSerial.println("Type,\tStatus,\tHumidity (%),\tTemperature (C)");
}


void loop()
{
  //  READ DATA
  USBSerial.print("DHT20, \t");
  int status = DHT.read();
  switch (status)
  {
  case DHT20_OK:
    USBSerial.print("OK,\t");
    break;
  case DHT20_ERROR_CHECKSUM:
    USBSerial.print("Checksum error,\t");
    break;
  case DHT20_ERROR_CONNECT:
    USBSerial.print("Connect error,\t");
    break;
  case DHT20_MISSING_BYTES:
    USBSerial.print("Missing bytes,\t");
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
    USBSerial.print("Unknown error,\t");
    break;
  }

  //  DISPLAY DATA, sensor has only one decimal.
  USBSerial.print(DHT.getHumidity(), 1);
  USBSerial.print(",\t");
  USBSerial.println(DHT.getTemperature(), 1);

  delay(2000);
}


// -- END OF FILE --
