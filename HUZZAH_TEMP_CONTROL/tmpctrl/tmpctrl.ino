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

#define TEMP_SAMPLE_INTERVAL    2000 /*mS*/
#define MIN_COMPRESSOR_SECONDS   180
#define NUM_DATA_SAMPLES         512

/* Temp in 1/100 degrees */
#define LM50_TO_C(a)  (((int32_t)976*(a)-500000)/100)
#define LM50_TO_F(a)  (((int32_t)8789*(a)-4500000)/500+3200)

typedef struct {
  uint32_t timestamp;
  uint8_t enabled;
  int16_t temp;
  int16_t setpoint;
  int16_t hyst;
  uint8_t mode;
  uint8_t comp_mode;
} datapoint;

int16_t currentTemp = 2500;
int16_t setpoint;
temp_mode mode = MODE_HEAT;
int16_t hysteresis;
bool comp_mode;

static datapoint dataLog[NUM_DATA_SAMPLES];
static int readIndex = 0, writeIndex = 0;


int checkWifi(void)
{
  int timeout = 20;
  int status = WiFi.status();
  
  if (status == WL_CONNECTED) {
    return 0;
  }
  Serial.print("problem: status=");
  Serial.println(status);
  
  Serial.print("Connecting to ");
  Serial.print("Connecting to ");

  updateStatusLED(WIFI_LED, ORANGE, true);

  WiFi.mode(WIFI_STA); /* client mode */
  WiFi.begin(defaultSSID, defaultPasswd);

  while (WiFi.status() != WL_CONNECTED && timeout--) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("");
    Serial.println("Failed to connect");
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
  while (readIndex != writeIndex) {
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
    updateStatusLED(RELAY_LED, enable ? ((mode == MODE_HEAT) ? RED : BLUE) : OFF, false);
  }
}

uint16_t getTrippoint(void) {

  if (digitalRead(relayPin)) {
    if (mode == MODE_HEAT)
      return (setpoint + hysteresis);
    else
      return (setpoint - hysteresis);
  } else
    return (setpoint);

}

int16_t lowpass(int16_t temp) {
  #define alpha 5   /* % weight */
  return ((int16_t)((((int32_t)alpha*temp) + (int32_t)(100 - alpha) * currentTemp)/100));
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
  uint16_t trippoint;
  static  unsigned long previousMillis = 0;
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= TEMP_SAMPLE_INTERVAL) {
    previousMillis = currentMillis;
    currentTemp = lowpass(LM50_TO_F(analogRead(A0)));
Serial.print("Wifi status: ");
Serial.println(WiFi.status());
  }
  
  trippoint = getTrippoint();
  if (mode == MODE_HEAT)
    if (currentTemp < trippoint)
      setPower(1);
    else
      setPower(0);
  else if (currentTemp > trippoint)
    setPower(1);
  else
    setPower(0);
  //  logDatapoint();

  uiLoop();

    if (!checkWifi()) {
  //    uploadCloudData();
      loopHTTP();
    }

  //delay(20); /* hopefully get rid of this */
}
