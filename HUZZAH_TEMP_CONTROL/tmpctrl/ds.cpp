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
      Serial.print("WARNING: Pre-configured ");
      Serial.print("temp sensor could not be found on the bus:");
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

  Serial.print("Resolution for sensor: ");
  Serial.print(sensors.getResolution(g_settings.ds_addr[sensor]), DEC); 
  Serial.println();

  return true;
no_device:
  g_settings.ds_addr[sensor][0] = 0x00;
  return false;
}

bool ds_setup(void) {
  ds.reset_search();
  if (detect_sensor(TEMP_MAIN)) {
    if (detect_sensor(TEMP_AUX)) {
      if(!memcmp(g_settings.ds_addr[TEMP_MAIN], g_settings.ds_addr[TEMP_MAIN], 8)) {
        Serial.println("Error: MAIN sensor detected twice; retry aux search");
        g_settings.ds_addr[TEMP_AUX][0] = 0x00;
        if (!detect_sensor(TEMP_AUX)) {
            Serial.println("Warning: No AUX temp sensor detected\n");
        }
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
  memcpy(tmp_addr, g_settings.ds_addr[0], sizeof(g_settings.ds_addr[0]));
  memcpy(g_settings.ds_addr[0], g_settings.ds_addr[1], sizeof(g_settings.ds_addr[0]));
  memcpy(g_settings.ds_addr[1], tmp_addr, sizeof(g_settings.ds_addr[0]));
}

bool ds_delete_sensors(void){
  memset(g_settings.ds_addr[0], 0xFF, sizeof(g_settings.ds_addr[0]));
  memset(g_settings.ds_addr[1], 0xFF, sizeof(g_settings.ds_addr[0]));
}

int16_t get_temp(temp_sensor sensor){
  float temp = sensors.getTempF(g_settings.ds_addr[sensor]);
  return (100 * (temp+0.005));
}

bool ds_is_present(temp_sensor sensor) {
  return (sensors.isConnected(g_settings.ds_addr[sensor]));
}

void start_temp_reading(void) {
  sensors.requestTemperatures();
}

