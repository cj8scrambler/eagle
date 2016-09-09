#include <OneWire.h>

#include "tmpctrl.h"

OneWire  ds(digital_temp);
byte ds_addr[8];
bool type_s;

void ds_setup(void) {
  if ( !ds.search(ds_addr)) {
    Serial.println("No DS18 Temp Sensor found");
    goto no_device;
  }
  
  if (OneWire::crc8(ds_addr, 7) != ds_addr[7]) {
    Serial.println("Error: DS18 CRC is not valid!");
    goto no_device;
  }
 
  // the first ROM byte indicates which chip
  switch (ds_addr[0]) {
    case 0x10:
      Serial.println("Found DS18S20");  // or old DS1820
      type_s = 1;
      break;
    case 0x28:
      Serial.println("Found DS18B20");
      type_s = 0;
      break;
    case 0x22:
      Serial.println("Found DS1822");
      type_s = 0;
      break;
    default:
      Serial.print("Device found, but it is not a DS18x20 family device: 0x");
      Serial.println(ds_addr[0], HEX);
      return;
  } 

  ds.reset();
  ds.select(ds_addr);
  ds.write(0x44);        // start the next conversion
  
  return;
no_device:
  ds_addr[0] = 0x00;
  return;
}

int16_t read_ds_temp(void) {
  byte data[12];
  int i;
  byte present;
  
  present = ds.reset();
  ds.select(ds_addr);    
  ds.write(0xBE);
  
  for ( i = 0; i < 9; i++) {
    data[i] = ds.read();
  }

  /* start the next conversion*/
  ds.reset();
  ds.select(ds_addr);
  ds.write(0x44);  
  
  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }

  return raw;
}
