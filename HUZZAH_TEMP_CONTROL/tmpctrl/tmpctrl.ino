/*
    Temp controller with data logger
*/

#include <Time.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <limits.h>

#include "RunningAverage.h"
#include "tmpctrl.h"
#include "credentials.h"
#include "ui.h"
#include "ntp.h"
#include "http.h"
#include "mqtt.h"
#include "ds.h"

#define NUM_DATA_SAMPLES         1024

/* Temp in 1/100 degrees */
#define LM50_TO_C(a)  (((int32_t)976*(a)-500000)/100)
#define LM50_TO_F(a)  (((int32_t)8789*(a)-4500000)/500+3200)
#define DS18_TO_C(a)  (a)
#define DS18_TO_F(a)  (((9*(a))/5) + 3200)

typedef enum {
  LOG_TEMPERATURE,
  LOG_AUXTEMP,
  LOG_SETPOINT,
  LOG_ENABLED,
  LOG_HYSTERESIS,
  LOG_MODE,
  LOG_COMP_MODE,
  LOG_IP
} logtype;

typedef struct {
  time_t timestamp;
  logtype type;
  int16_t value;
} datapoint;

settings g_settings;
static settings old_settings;

bool full_status = 0;
int16_t currentTemp = 7200;
int16_t auxTemp = 7200;

static datapoint dataLog[NUM_DATA_SAMPLES];
static int readIndex = 0, writeIndex = 0;

RunningAverage curTempRA(10);
RunningAverage auxTempRA(10);

int checkWifi(void)
{
  int timeout = 20;
  int status = WiFi.status();
  unsigned long currentMillis = now();
  static unsigned long lastWifiCheck = 0;
  
  if (status == WL_CONNECTED) {
    return 0;
  }

  updateStatusLED(WIFI_LED, RED, false);

  if (!lastWifiCheck || 
      currentMillis - lastWifiCheck >= WIFI_CONNECT_INTERVAL) {
    lastWifiCheck = currentMillis;
    Serial.print("Connecting to ");
    Serial.print(wifiSSID);

    updateStatusLED(WIFI_LED, ORANGE, true);

    WiFi.mode(WIFI_STA); /* client mode */
    WiFi.begin(wifiSSID, wifiPasswd);

    /* TODO: break this into a timer based loop so
     *       that UI/control logic doesn't hang
     *       durring re-connect
     */
    while (WiFi.status() != WL_CONNECTED && timeout--) {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("");
      Serial.println("Failed to connect");
      updateStatusLED(WIFI_LED, RED, false);
      ntpDisable();
      return -1;
    }

    updateStatusLED(WIFI_LED, ORANGE, false);

    Serial.println("");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("Signal Strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println("dBm");

    ntpSetup();

    return 0;
  }
  return -2;
}

char * logtype_to_str(logtype type) {

  static char buffer[10];
  
  switch (type) {
  case LOG_TEMPERATURE:
    strncpy(buffer, "temp", 10);
    break;
  case LOG_AUXTEMP:
    strncpy(buffer, "auxtemp", 10);
    break;
  case LOG_SETPOINT:
    strncpy(buffer, "setpoint", 10);
    break;
  case LOG_ENABLED:
    strncpy(buffer, "on", 10);
    break;
  case LOG_HYSTERESIS:
    strncpy(buffer, "hyst", 10);
    break;
  case LOG_MODE:
    strncpy(buffer, "mode", 10);
    break;
  case LOG_COMP_MODE:
    strncpy(buffer, "comp", 10);
    break;
  case LOG_IP:
    strncpy(buffer, "ip", 10);
    break;
  default:
    snprintf(buffer, 10, "type-%d", type);
    break;
  }
}

void uploadCloudData() {
  /*
   * While the PubSub mqtt API allows concatenating multiple messages
   * together into a single transaction, the string length limit means
   * we can realisticly only send 1 message at a time
   */
  if (readIndex != writeIndex) {
      switch (dataLog[readIndex].type) {
      case LOG_TEMPERATURE:
        mqtt_addValue_rawStr(logtype_to_str(dataLog[readIndex].type),
                             divide_100(dataLog[readIndex].value),
                             dataLog[readIndex].timestamp);
        break;
      case LOG_AUXTEMP:
        mqtt_addValue_rawStr(logtype_to_str(dataLog[readIndex].type),
                             divide_100(dataLog[readIndex].value),
                             dataLog[readIndex].timestamp);
        break;
      case LOG_SETPOINT:
        mqtt_addValue_rawStr(logtype_to_str(dataLog[readIndex].type),
                             divide_100(dataLog[readIndex].value),
                             dataLog[readIndex].timestamp);
        break;
      case LOG_ENABLED:
        mqtt_addValue_rawStr(logtype_to_str(dataLog[readIndex].type),
                             dataLog[readIndex].value?"true":"false",
                             dataLog[readIndex].timestamp);
        break;
      case LOG_HYSTERESIS:
        mqtt_addValue_rawStr(logtype_to_str(dataLog[readIndex].type),
                             divide_100(dataLog[readIndex].value),
                             dataLog[readIndex].timestamp);
        break;
      case LOG_MODE:
        mqtt_addValue(logtype_to_str(dataLog[readIndex].type),
                      dataLog[readIndex].value==MODE_HEAT?"heat":"cool",
                      dataLog[readIndex].timestamp);
        break;
      case LOG_COMP_MODE:
        mqtt_addValue_rawStr(logtype_to_str(dataLog[readIndex].type),
                             dataLog[readIndex].value?"true":"false",
                             dataLog[readIndex].timestamp);
        break;
      case LOG_IP:
        mqtt_addValue(logtype_to_str(dataLog[readIndex].type),
                             WiFi.localIP().toString(),
                             dataLog[readIndex].timestamp);
        break;
      default:
        Serial.print("Unhanled log type: ");
        Serial.print(dataLog[readIndex].type);
        Serial.print("   value: ");
        Serial.print(dataLog[readIndex].value);
        Serial.print("   at: ");
        Serial.println(dataLog[readIndex].timestamp);
        break;

    }
    readIndex = (readIndex + 1) % NUM_DATA_SAMPLES;
    mqtt_send();
  }
}

void logDatapoint() {
  static int8_t old_enabled = -1;
  static int16_t old_temp = -50000;
  static int16_t old_auxtemp = -50000;
  static int8_t old_mode = -1;
  static int16_t old_setpoint = -50000;
  static int16_t old_hyst = -1;
  static int8_t old_comp_mode = -1;

  if (timeStatus() == timeSet) {
    time_t new_time = now();
    int8_t new_enabled = digitalRead(relayPin);
    int16_t new_temp = currentTemp;
    int16_t new_auxtemp = auxTemp;
    int8_t new_mode = g_settings.mode;
    int16_t new_setpoint = g_settings.setpoint;
    int16_t new_hyst = g_settings.hysteresis;
    int8_t new_comp_mode = g_settings.comp_mode;
    
    if (writeIndex == (readIndex - 1))
      Serial.println("Buffer overflow");
  
    /* Mode needs to go out before 'on' state' */
    if (new_mode != old_mode || full_status) {        
      old_mode = new_mode;
      dataLog[writeIndex].timestamp = new_time;
      dataLog[writeIndex].type = LOG_MODE;
      dataLog[writeIndex].value = new_mode;
      writeIndex = (writeIndex + 1) % NUM_DATA_SAMPLES;
      if (writeIndex == readIndex)
        readIndex = (readIndex + 1) % NUM_DATA_SAMPLES;
      Serial.print("New mode data (");
      Serial.print(new_mode);
      Serial.print(") logged at ");
      Serial.println(now());
    }
    
    if (new_enabled != old_enabled || full_status) {
      old_enabled = new_enabled;
      dataLog[writeIndex].timestamp = new_time;
      dataLog[writeIndex].type = LOG_ENABLED;
      dataLog[writeIndex].value = new_enabled;
      writeIndex = (writeIndex + 1) % NUM_DATA_SAMPLES;
      if (writeIndex == readIndex)
        readIndex = (readIndex + 1) % NUM_DATA_SAMPLES;
      Serial.print("New enabled data (");
      Serial.print(new_enabled);
      Serial.print(") logged at ");
      Serial.println(now());
    }
    if (new_temp != old_temp || full_status) {
      old_temp = new_temp;
      dataLog[writeIndex].timestamp = new_time;
      dataLog[writeIndex].type = LOG_TEMPERATURE;
      dataLog[writeIndex].value = new_temp;
      writeIndex = (writeIndex + 1) % NUM_DATA_SAMPLES;
      if (writeIndex == readIndex)
        readIndex = (readIndex + 1) % NUM_DATA_SAMPLES;
//      Serial.print("New temperature data (");
//      Serial.print(divide_100(new_temp));
//      Serial.print(") logged at ");
//      Serial.println(now());
    }
    if (new_auxtemp != old_auxtemp || full_status) {
      old_auxtemp = new_auxtemp;
      dataLog[writeIndex].timestamp = new_time;
      dataLog[writeIndex].type = LOG_AUXTEMP;
      dataLog[writeIndex].value = new_auxtemp;
      writeIndex = (writeIndex + 1) % NUM_DATA_SAMPLES;
      if (writeIndex == readIndex)
        readIndex = (readIndex + 1) % NUM_DATA_SAMPLES;
      Serial.print("New aux temperature data (");
      Serial.print(divide_100(new_auxtemp));
      Serial.print(") logged at ");
      Serial.println(now());
    }

    if (new_setpoint != old_setpoint || full_status) {
      old_setpoint = new_setpoint;
      dataLog[writeIndex].timestamp = new_time;
      dataLog[writeIndex].type = LOG_SETPOINT;
      dataLog[writeIndex].value = new_setpoint;
      writeIndex = (writeIndex + 1) % NUM_DATA_SAMPLES;
      if (writeIndex == readIndex)
        readIndex = (readIndex + 1) % NUM_DATA_SAMPLES;
      Serial.print("New setpoint data (");
      Serial.print(divide_100(new_setpoint));
      Serial.print(") logged at ");
      Serial.println(now());
    }
    if (new_hyst != old_hyst || full_status) {
      old_hyst = new_hyst;
      dataLog[writeIndex].timestamp = new_time;
      dataLog[writeIndex].type = LOG_HYSTERESIS;
      dataLog[writeIndex].value = new_hyst;
      writeIndex = (writeIndex + 1) % NUM_DATA_SAMPLES;
      if (writeIndex == readIndex)
        readIndex = (readIndex + 1) % NUM_DATA_SAMPLES;
      Serial.print("New hysteresis data (");
      Serial.print(divide_100(new_hyst));
      Serial.print(") logged at ");
      Serial.println(now());
    }
    if (new_comp_mode != old_comp_mode || full_status) {
      old_comp_mode = new_comp_mode;      
      dataLog[writeIndex].timestamp = new_time;
      dataLog[writeIndex].type = LOG_COMP_MODE;
      dataLog[writeIndex].value = new_comp_mode;
      writeIndex = (writeIndex + 1) % NUM_DATA_SAMPLES;
      if (writeIndex == readIndex)
        readIndex = (readIndex + 1) % NUM_DATA_SAMPLES;
      Serial.print("New compressor mode data (");
      Serial.print(new_comp_mode);
      Serial.print(") logged at ");
      Serial.println(now());
    }
    if (full_status) {
      dataLog[writeIndex].timestamp = new_time;
      dataLog[writeIndex].type = LOG_IP;
      dataLog[writeIndex].value = 0; /* value is ignored for LOG_IP */
      writeIndex = (writeIndex + 1) % NUM_DATA_SAMPLES;
      if (writeIndex == readIndex)
        readIndex = (readIndex + 1) % NUM_DATA_SAMPLES;

      full_status = 0;
    }
  } else {
    Serial.print("Skipped data log timeStatus() = ");
    Serial.println(timeStatus());
  }
}

bool getPower(void){
  return (digitalRead(relayPin));
}
void setPower(bool enable) {
  static time_t lasttime;
  time_t deltatime;

  if (g_settings.comp_mode) {
    deltatime = now() - lasttime;
    if (deltatime < MIN_COMPRESSOR_TIME){
      Serial.println("In compressor mode timeout");
      return;
    }
  }

  if (digitalRead(relayPin) != enable) {
    digitalWrite(relayPin, enable);
    lasttime = now();
    updateStatusLED(RELAY_LED, enable ? ((g_settings.mode == MODE_HEAT) ? RED : BLUE) : OFF, false);
  }
}

uint16_t getTrippoint(void) {

  if (digitalRead(relayPin)) {
    if (g_settings.mode == MODE_HEAT)
      return (g_settings.setpoint + g_settings.hysteresis);
    else
      return (g_settings.setpoint - g_settings.hysteresis);
  } else
    return (g_settings.setpoint);

}

template <class T> int EEPROM_write(int ee, const T& value)
{
    const byte* p = (const byte*)(const void*)&value;
    unsigned int i;
    for (i = 0; i < sizeof(value); i++)
          EEPROM.write(ee++, *p++);
    EEPROM.commit();
    return i;
}

template <class T> int EEPROM_read(int ee, T& value)
{
    byte* p = (byte*)(void*)&value;
    unsigned int i;
    for (i = 0; i < sizeof(value); i++)
          *p++ = EEPROM.read(ee++);
    return i;
}

void setup() {
  byte macInt[6];
  String macStr;
  uint32_t uniqueId;
  int i=0;

  Serial.begin(115200);
  delay(100);
  Serial.println("");

  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, 0);

  EEPROM.begin(512);
  EEPROM_read(0, g_settings);
  memcpy(&old_settings, &g_settings, sizeof(g_settings));

  uiSetup();

  while (!digitalRead(buttonPin))
    if (i++ == 50) {
      uiShowReset();
      memset(&g_settings, 0, sizeof(g_settings));
      EEPROM_write(0, g_settings);
      delay(1000);
      ESP.restart();
    }
    delay(100);
  uiWaitForTempSensor();

  Serial.println();
  Serial.println();

  checkWifi();

  WiFi.macAddress(macInt);
  macStr.concat (String(macInt[5], HEX));
  macStr.concat (String(macInt[4], HEX));
  macStr.concat (String(macInt[3], HEX));
  macStr.concat (String(macInt[2], HEX));
  macStr.concat (String(macInt[1], HEX));
  macStr.concat (String(macInt[0], HEX));
  setupHTTP(macStr);

  uniqueId = macInt[3] << 24 | macInt[2] << 16 | macInt[1] << 8 | macInt[0];
  mqtt_setup(uniqueId);
}

void loop() {
  uint16_t trippoint;
  static  unsigned long prevTempSample = 0,
                        prevLogData = 0;
  unsigned long currentMillis = millis();

  if (currentMillis - prevTempSample >= TEMP_SAMPLE_INTERVAL) {
    if(!ds_is_present(TEMP_MAIN)) {
      setPower(0);
      uiWaitForTempSensor();
    }

    prevTempSample = currentMillis;
    curTempRA.addValue(get_temp(TEMP_MAIN));
    currentTemp = curTempRA.getAverage();

    if(ds_is_present(TEMP_AUX)) {
      curTempRA.addValue(get_temp(TEMP_AUX));
      auxTemp = auxTempRA.getAverage();
    }
    start_temp_reading();

  }

  trippoint = getTrippoint();
  if (g_settings.mode == MODE_HEAT)
    if (currentTemp < trippoint)
      setPower(1);
    else
      setPower(0);
  else if (currentTemp > trippoint)
    setPower(1);
  else
    setPower(0);
    
  if (currentMillis - prevLogData >= LOG_DATA_INTERVAL) {
    prevLogData = currentMillis;
    logDatapoint();
  }

  uiLoop();

  if (!checkWifi()) {
    mqtt_loop();
    uploadCloudData();
    loopHTTP();
  }

  if (memcmp(&old_settings, &g_settings, sizeof(g_settings))) {
    Serial.println("Saving settings to EEPROM.");
    EEPROM_write(0, g_settings);
    memcpy(&old_settings, &g_settings, sizeof(g_settings));
  }
}
