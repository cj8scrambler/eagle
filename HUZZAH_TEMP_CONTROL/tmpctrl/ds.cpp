#include <OneWire.h>
#include <DallasTemperature.h>

#include "ds.h"
#include "tmpctrl.h"

OneWire  ds(digital_temp);
DallasTemperature sensors(&ds);

static bool detect_sensor(temp_sensor sensor) {
  
  /* If there is a sensor configured return true */
  if (g_settings.ds_addr[sensor][0] && g_settings.ds_addr[sensor][0] != 0xFF) {
    if (!sensors.isConnected(g_settings.ds_addr[sensor])) {
      Serial.print("WARNING: Pre-configured temp sensor-");
      Serial.print(sensor);
      Serial.print(" was not found on the bus: ");
      Serial.print(g_settings.ds_addr[sensor][0], HEX);
      Serial.print(g_settings.ds_addr[sensor][1], HEX);
      Serial.print(g_settings.ds_addr[sensor][2], HEX);
      Serial.print(g_settings.ds_addr[sensor][3], HEX);
      Serial.print(g_settings.ds_addr[sensor][4], HEX);
      Serial.print(g_settings.ds_addr[sensor][5], HEX);
      Serial.print(g_settings.ds_addr[sensor][6], HEX);
      Serial.println(g_settings.ds_addr[sensor][7], HEX);
    } else {
      Serial.print("Pre-configured temp sensor-");
      Serial.print(sensor);
      Serial.print(" found on the bus: ");
      Serial.print(g_settings.ds_addr[sensor][0], HEX);
      Serial.print(g_settings.ds_addr[sensor][1], HEX);
      Serial.print(g_settings.ds_addr[sensor][2], HEX);
      Serial.print(g_settings.ds_addr[sensor][3], HEX);
      Serial.print(g_settings.ds_addr[sensor][4], HEX);
      Serial.print(g_settings.ds_addr[sensor][5], HEX);
      Serial.print(g_settings.ds_addr[sensor][6], HEX);
      Serial.println(g_settings.ds_addr[sensor][7], HEX);
    }
    return true;
  }
  
  if (!ds.search(g_settings.ds_addr[sensor])) {
    Serial.println("No DS18 Temp Sensor found for sensor");
    goto no_device;
  }

  if (OneWire::crc8(g_settings.ds_addr[sensor], 7) != g_settings.ds_addr[sensor][7]) {
    Serial.println("Error: DS18 CRC is not valid for sensor");
    goto no_device;
  }

  Serial.print("Found Sensor-");
  Serial.print(sensor);
  Serial.print(": ");
  Serial.print(g_settings.ds_addr[sensor][0], HEX);
  Serial.print(g_settings.ds_addr[sensor][1], HEX);
  Serial.print(g_settings.ds_addr[sensor][2], HEX);
  Serial.print(g_settings.ds_addr[sensor][3], HEX);
  Serial.print(g_settings.ds_addr[sensor][4], HEX);
  Serial.print(g_settings.ds_addr[sensor][5], HEX);
  Serial.print(g_settings.ds_addr[sensor][6], HEX);
  Serial.println(g_settings.ds_addr[sensor][7], HEX);

  return true;
no_device:
  g_settings.ds_addr[sensor][0] = 0x00;
  return false;
}

bool ds_setup(void) {
  ds.reset_search();
  Serial.println("Looking for MAIN temp sensor:");
  if (detect_sensor(TEMP_MAIN)) {
    Serial.println("Found MAIN temp sensor");
    Serial.println("Looking for AUX temp sensor:");
    if (detect_sensor(TEMP_AUX)) {
      Serial.println("Found AUX temp sensor");
      if(!memcmp(g_settings.ds_addr[TEMP_MAIN], g_settings.ds_addr[TEMP_AUX], 8)) {
        Serial.println("Error: MAIN sensor detected twice; retry aux search");
        g_settings.ds_addr[TEMP_AUX][0] = 0x00;
        Serial.println("Looking for AUX temp sensor again:");
        if (!detect_sensor(TEMP_AUX)) {
            Serial.println("Warning: No AUX temp sensor detected\n");
        } else if(!memcmp(g_settings.ds_addr[TEMP_MAIN], g_settings.ds_addr[TEMP_AUX], 8)) {
          Serial.println("aux retry found same one, check again");
           g_settings.ds_addr[TEMP_AUX][0] = 0x00;
          if (!detect_sensor(TEMP_AUX)) {
            Serial.println("Warning: No AUX temp sensor detected\n");
          } else {
            Serial.println("Found AUX temp sensor on 3rd try:");
            Serial.print(": ");
            Serial.print(g_settings.ds_addr[TEMP_AUX][0], HEX);
            Serial.print(g_settings.ds_addr[TEMP_AUX][1], HEX);
            Serial.print(g_settings.ds_addr[TEMP_AUX][2], HEX);
            Serial.print(g_settings.ds_addr[TEMP_AUX][3], HEX);
            Serial.print(g_settings.ds_addr[TEMP_AUX][4], HEX);
            Serial.print(g_settings.ds_addr[TEMP_AUX][5], HEX);
            Serial.print(g_settings.ds_addr[TEMP_AUX][6], HEX);
            Serial.println(g_settings.ds_addr[TEMP_AUX][7], HEX);
          }
        } else
        Serial.println("Found AUX temp sensor on 2nd try:");
        Serial.print(g_settings.ds_addr[TEMP_AUX][0], HEX);
        Serial.print(g_settings.ds_addr[TEMP_AUX][1], HEX);
        Serial.print(g_settings.ds_addr[TEMP_AUX][2], HEX);
        Serial.print(g_settings.ds_addr[TEMP_AUX][3], HEX);
        Serial.print(g_settings.ds_addr[TEMP_AUX][4], HEX);
        Serial.print(g_settings.ds_addr[TEMP_AUX][5], HEX);
        Serial.print(g_settings.ds_addr[TEMP_AUX][6], HEX);
        Serial.println(g_settings.ds_addr[TEMP_AUX][7], HEX);
      }
    } else {
      Serial.println("Warning: No AUX temp sensor detected\n");
    }
  } else {
    Serial.println("Error: No MAIN temp sensor detected");
    return false;
  }

  sensors.requestTemperatures();

  return true;
}

bool ds_swap_sensors(void){
  byte tmp_addr[8];
  Serial.println("ds_swap_sensors()");
  memcpy(tmp_addr, g_settings.ds_addr[0], sizeof(g_settings.ds_addr[0]));
  memcpy(g_settings.ds_addr[0], g_settings.ds_addr[1], sizeof(g_settings.ds_addr[0]));
  memcpy(g_settings.ds_addr[1], tmp_addr, sizeof(g_settings.ds_addr[0]));
}

bool ds_delete_sensors(void){
  memset(g_settings.ds_addr[0], 0xFF, sizeof(g_settings.ds_addr[0]));
  memset(g_settings.ds_addr[1], 0xFF, sizeof(g_settings.ds_addr[0]));
}

int16_t get_temp(temp_sensor sensor){

#if DEBUG
  Serial.print("reading temp-");
  Serial.print(sensor);
  Serial.print(": ");
  Serial.print(g_settings.ds_addr[sensor][0], HEX);
  Serial.print(g_settings.ds_addr[sensor][1], HEX);
  Serial.print(g_settings.ds_addr[sensor][2], HEX);
  Serial.print(g_settings.ds_addr[sensor][3], HEX);
  Serial.print(g_settings.ds_addr[sensor][4], HEX);
  Serial.print(g_settings.ds_addr[sensor][5], HEX);
  Serial.print(g_settings.ds_addr[sensor][6], HEX);
  Serial.print(g_settings.ds_addr[sensor][7], HEX);
#endif
  
  float temp = sensors.getTempF(g_settings.ds_addr[sensor]);
#if DEBUG
  Serial.print(": ");
  Serial.println(temp);
#endif
  return (100 * (temp+0.005));
}

bool ds_is_present(temp_sensor sensor) {
  return (sensors.isConnected(g_settings.ds_addr[sensor]));
}

void start_temp_reading(void) {
  sensors.requestTemperatures();
}

