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

int16_t currentTemp = 7200;
int16_t setpoint = 6800;
temp_mode mode = MODE_HEAT;
int16_t hysteresis = 200;
bool comp_mode = 1;

static datapoint dataLog[NUM_DATA_SAMPLES];
static int readIndex = 0, writeIndex = 0;


int checkWifi(void)
{
  int timeout = 20;
  int status = WiFi.status();

  if (status == WL_CONNECTED) {
    return 0;
  }

  Serial.print("Connecting to ");
  Serial.print(defaultSSID);

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


void uploadCloudData() {
  /* TBD */
  while (readIndex != writeIndex) {
    /* send dataLog[readIndex] */
    readIndex = (readIndex + 1) % NUM_DATA_SAMPLES;
  }
}

void logDatapoint() {
  int oldIndex;

  if (timeStatus() == timeSet) {
    datapoint newdata = {
      now(), digitalRead(relayPin), currentTemp,
      mode, setpoint, hysteresis, comp_mode,
    };

    oldIndex = writeIndex ? writeIndex - 1 : NUM_DATA_SAMPLES - 1;
    if (dataLog[oldIndex].enabled != newdata.enabled ||
 //       dataLog[oldIndex].temp != newdata.temp ||
        dataLog[oldIndex].mode != newdata.mode ||
        dataLog[oldIndex].setpoint != newdata.setpoint ||
        dataLog[oldIndex].hyst != newdata.hyst ||
        dataLog[oldIndex].comp_mode != newdata.comp_mode) {
      Serial.print("enabled - old: ");
      Serial.print(dataLog[oldIndex].enabled);
      Serial.print("enabled - new: ");
      Serial.println(newdata.enabled);

      Serial.print("temp - old: ");
      Serial.print(dataLog[oldIndex].temp);
      Serial.print("enabled - new: ");
      Serial.println(newdata.temp);

      Serial.print("mode - old: ");
      Serial.print(dataLog[oldIndex].mode);
      Serial.print("enabled - new: ");
      Serial.println(newdata.mode);

      Serial.print("setpoint - old: ");
      Serial.print(dataLog[oldIndex].setpoint);
      Serial.print("enabled - new: ");
      Serial.println(newdata.setpoint);

      Serial.print("hyst - old: ");
      Serial.print(dataLog[oldIndex].hyst);
      Serial.print("enabled - new: ");
      Serial.println(newdata.hyst);

      memcpy(&dataLog[writeIndex], &newdata, sizeof(datapoint));
      writeIndex = (writeIndex + 1) % NUM_DATA_SAMPLES;
      if (writeIndex == readIndex)
        readIndex = (readIndex + 1) % NUM_DATA_SAMPLES;

      Serial.print("New data logged at ");
      Serial.println(now());
    }
  }
}

bool getPower(void){
  return (digitalRead(relayPin));
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
