//
//    FILE: DHT20_I2C_speed.ino
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

//  NOTE datasheet states 400 KHz as maximum


#include "DHT20.h"

DHT20 DHT;

uint32_t start, stop;


void setup()
{
  USBSerial.begin(115200);
  USBSerial.println(__FILE__);
  USBSerial.print("DHT20 LIBRARY VERSION: ");
  USBSerial.println(DHT20_LIB_VERSION);
  USBSerial.println();

  USBSerial.println("\nNOTE: datasheet states 400 KHz as maximum.\n");

  Wire.begin();
  DHT.begin();  //  ESP32 default pins 21, 22
  delay(2000);

  for (uint32_t speed = 50000; speed < 850000; speed += 50000)
  {
    Wire.setClock(speed);
    USBSerial.print(speed);
    USBSerial.print("\t");
    USBSerial.print(DHT.read());  // status
    USBSerial.print("\t");
    USBSerial.print(DHT.getHumidity(), 1);
    USBSerial.print("\t");
    USBSerial.print(DHT.getTemperature(), 1);
    USBSerial.println();
    delay(1000);
  }

  USBSerial.println();
  for (uint32_t speed = 50000; speed < 850000; speed += 50000)
  {
    Wire.setClock(speed);
    start = micros();
    DHT.read();
    stop = micros();

    USBSerial.print(speed);
    USBSerial.print("\t");
    USBSerial.print(stop - start);  //  time
    USBSerial.print("\t");
    USBSerial.print(DHT.getHumidity(), 1);
    USBSerial.print("\t");
    USBSerial.print(DHT.getTemperature(), 1);
    USBSerial.println();
    delay(1000);
  }


  USBSerial.println();
  for (uint32_t speed = 50000; speed < 850000; speed += 50000)
  {
    DHT.requestData();
    while (DHT.isMeasuring());
    Wire.setClock(speed);
    start = micros();
    DHT.readData();
    stop = micros();
    DHT.convert();
    USBSerial.print(speed);
    USBSerial.print("\t");
    USBSerial.print(stop - start);  // time
    USBSerial.print("\t");
    USBSerial.print(DHT.getHumidity(), 1);
    USBSerial.print("\t");
    USBSerial.print(DHT.getTemperature(), 1);
    USBSerial.println();
    delay(1000);
  }

  USBSerial.println("\ndone...");
}


void loop()
{
}


//  -- END OF FILE --
