/*
    Temp controller with data logger
*/

#include <Time.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266SSDP.h>

#include "tmpctrl.h"
#include "credentials.h"
#include "ui.h"
#include "ntp.h"

#define MIN_COMPRESSOR_SECONDS  180
#define NUM_DATA_SAMPLES  512

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

datapoint dataLog[NUM_DATA_SAMPLES];
int readIndex=0, writeIndex=0;

ESP8266WebServer HTTP(80);

void handleRoot() {
 String message =  "<html><head><title>Wifi Temp Controller</title></head><body>Current Temp: ";
  message += currentTemp;
  message += "<p>\nCurrent State: ";
  message += uiToString();
  if (mode == MODE_HEAT) {
    message += "<p>\nCurrent Mode: Heat<p>\n";
  } else {
    message += "<p>\nCurrent Mode: Cool<p>\n";
  }
  message += "Current Setpoint: ";
  message += setpoint;
  message += "<p>\n";
  HTTP.send(200, "text/html", message);
}

void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += HTTP.uri();
  message += "\nMethod: ";
  message += (HTTP.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += HTTP.args();
  message += "\n";
  for (uint8_t i=0; i<HTTP.args(); i++){
    message += " " + HTTP.argName(i) + ": " + HTTP.arg(i) + "\n";
  }
  HTTP.send(404, "text/plain", message);
}

int checkWifi(void)
{
  int timeout = 20;
  if (WiFi.status() == WL_CONNECTED) {
    return 0;
  }

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

  byte macInt[6];
  String macStr;
  WiFi.macAddress(macInt);
  macStr.concat (String(macInt[5], HEX));
  macStr.concat (String(macInt[4], HEX));
  macStr.concat (String(macInt[3], HEX));
  macStr.concat (String(macInt[2], HEX));
  macStr.concat (String(macInt[1], HEX));
  macStr.concat (String(macInt[0], HEX));

  SSDP.setSchemaURL("description.xml");
  SSDP.setHTTPPort(80);
  SSDP.setName("WiFi IR Remote");
  SSDP.setSerialNumber(macStr);
  SSDP.setURL("index.html");
  SSDP.setModelName("Custom WiFi based IR transmitter");
  SSDP.setModelNumber("v1,0");
  SSDP.setModelURL("https://github.com/cj8scrambler/eagle/tree/master/HUZZAH_IR_SHIELD/hw");
  SSDP.setManufacturer("cj8scrambler");
  SSDP.setManufacturerURL("https://github.com/cj8scrambler/eagle/tree/master/HUZZAH_IR_SHIELD");
  SSDP.setDeviceType("urn:schemas-upnp-org:device:WifiRemote:1");

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("Signal Strength (RSSI): ");
  Serial.print(WiFi.RSSI());
  Serial.println("dBm");
  Serial.print("MAC address: ");
  Serial.println(macStr);

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
  Serial.begin(115200);
  delay(100);

  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, 0);

  uiSetup();

  // We start by connecting to a WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(defaultSSID);

  pinMode(A0, INPUT); /* ADC on LM50 temp sensor */
  
  checkWifi();

  Serial.printf("Starting HTTP...\n");
  HTTP.on("/", handleRoot);
  HTTP.on("/index.html", HTTP_GET, [](){
    HTTP.send(200, "text/plain", "Hello World!");
  });
  HTTP.on("/description.xml", HTTP_GET, [](){
    SSDP.schema(HTTP.client());
  });

  Serial.printf("Starting SSDP...\n");

  SSDP.begin();

  HTTP.onNotFound(handleNotFound);
  HTTP.begin();
}

void loop() {
  float trippoint;

  currentTemp = analogRead(A0);
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
    HTTP.handleClient();
  }

  delay(20); /* hopefully get rid of this */
}
