/*
    Temp controller with data logger
*/

#include <Time.h>
#include <ESP8266WiFi.h>

#include "tmpctrl.h"
#include "credentials.h"
#include "ui.h"
#include "ntp.h"
#include "http.h"

#define MIN_COMPRESSOR_SECONDS  180
#define NUM_DATA_SAMPLES  512

#define C_TO_F(c)  (((c) * 1.8) + 32.0)

typedef struct {
  uint32_t timestamp;
  uint8_t enabled;
  float temp;
  float setpoint;
  float hyst;
  uint8_t mode;
  uint8_t comp_mode;
} datapoint;

float currentTemp = 25.0;
float setpoint;
temp_mode mode = MODE_HEAT;
float hysteresis;
bool comp_mode;

static datapoint dataLog[NUM_DATA_SAMPLES];
static int readIndex=0, writeIndex=0;

int checkWifi(void)
{
  int timeout = 20;
  if (WiFi.status() == WL_CONNECTED) {
    return 0;
  }

  Serial.print("Connecting to ");
  Serial.println(defaultSSID);

  updateStatusLED(WIFI_LED, ORANGE, true);

  WiFi.mode(WIFI_STA); /* client mode */
  WiFi.begin(defaultSSID, defaultPasswd);

  while (WiFi.status() != WL_CONNECTED && timeout--) {
    delay(500);
    Serial.print(".");
  } 

  if (WiFi.status() != WL_CONNECTED) {
    updateStatusLED(WIFI_LED, RED, false);
    ntpDisable();
    return 1;
  }

  updateStatusLED(WIFI_LED, ORANGE, false);
  ntpSetup();

  return 0;
}


void uploadCloudData() {
  /* TBD */
  while(readIndex != writeIndex) {
    /* send dataLog[readIndex] */
    readIndex = (readIndex + 1) % NUM_DATA_SAMPLES;
  }
}

void logDatapoint() {
  if (timeStatus() == timeSet) {
    dataLog[writeIndex].timestamp = now();
    dataLog[writeIndex].enabled = digitalRead(relayPin);
    dataLog[writeIndex].temp = currentTemp;
    dataLog[writeIndex].mode = mode;
    dataLog[writeIndex].setpoint = setpoint;
    dataLog[writeIndex].hyst = hysteresis;
    dataLog[writeIndex].comp_mode = comp_mode;

    writeIndex = (writeIndex + 1) % NUM_DATA_SAMPLES;
    if (writeIndex == readIndex)
      readIndex = (readIndex + 1) % NUM_DATA_SAMPLES; 
    
    Serial.print("Log data at ");
    Serial.println(now());
  } else {
    Serial.println("Could not log data: no time set");
  }
}

void setPower(bool enable) {
  static time_t lasttime;
  time_t deltatime;

  if (comp_mode) {
    deltatime = now() - lasttime;
    if (deltatime < MIN_COMPRESSOR_SECONDS)
      return;
  }

  if (digitalRead(relayPin) != enable) {
    digitalWrite(relayPin, enable);
    lasttime = now();
    updateStatusLED(RELAY_LED, enable?((mode==MODE_HEAT)?RED:BLUE):OFF, false);
  }
}

float getTrippoint(void) {

  if (digitalRead(relayPin)) {
    if (mode == MODE_HEAT)
      return (setpoint + hysteresis);
    else
      return (setpoint - hysteresis);
  } else
      return (setpoint);

}

void setup() {
  byte macInt[6];
  String macStr;

  Serial.begin(115200);
  delay(100);

  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, 0);

  uiSetup();

  Serial.println();
  Serial.println();

  pinMode(A0, INPUT); /* ADC on LM50 temp sensor */
  
  checkWifi();

  WiFi.macAddress(macInt);
  macStr.concat (String(macInt[5], HEX));
  macStr.concat (String(macInt[4], HEX));
  macStr.concat (String(macInt[3], HEX));
  macStr.concat (String(macInt[2], HEX));
  macStr.concat (String(macInt[1], HEX));
  macStr.concat (String(macInt[0], HEX));
  setupHTTP(macStr);

  Serial.printf("Starting SSDP...\n");
}

void loop() {
  float trippoint;

  currentTemp = C_TO_F(100.0 * (analogRead(A0) - 0.5));
  trippoint = getTrippoint();
  if (mode == MODE_HEAT)
    if (currentTemp < trippoint)
      setPower(1);
    else
      setPower(0);
  else
    if (currentTemp > trippoint)
      setPower(1);
    else
      setPower(0);
  logDatapoint();

  uiLoop();

  if (!checkWifi()) {
    uploadCloudData();
    loopHTTP();
  }

  //delay(20); /* hopefully get rid of this */
}
