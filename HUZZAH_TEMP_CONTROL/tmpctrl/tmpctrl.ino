/*
    Temp controller with data logger
*/
#include <os_type.h>

#include <Encoder.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266SSDP.h>

#include "credentials.h"

#define BUTTON_RELEASED     1
#define BUTTON_PRESSED      2
#define BUTTON_TIMER_EXPIRE 3
#define SCROLL              4

typedef enum {
  UI_IDLE,
  UI_IDLE_SHOW_SETPOINT,
  UI_SET_SETPOINT_HEAT,
  UI_SET_SETPOINT_COOL,
  UI_SET_SSID,
  UI_SET_PASSWD
} ui_state;

typedef enum {
  MODE_HEAT,
  MODE_COOL,
} temp_mode;

const int buttonPin     = 0;
const int neoPixelPin   = 2;
const int disp_sda      = 4;
const int disp_scl      = 5;
const int unused        = 12;
const int rotary1Pin    = 13;
const int rotary2Pin    = 14;

int currentTemp = 25;
int tempSetpoint, heatSetpoint, coolSetpoint;
ui_state ui = UI_IDLE;
temp_mode mode = MODE_HEAT;

volatile int buttonEvent = 0;
int scrollDelta = 0;
os_timer_t buttonTimer;

//Encoder knob(rotary1Pin, rotary2pin);
Encoder knob(13,14);

Adafruit_NeoPixel statusLEDs = Adafruit_NeoPixel(3, neoPixelPin, NEO_GRB + NEO_KHZ800);
ESP8266WebServer HTTP(80);

char *uiToString(ui_state current)
{
  switch (current)
  {
  case UI_IDLE:
    return "Idle";
  case UI_IDLE_SHOW_SETPOINT:
    return "Idle: Showing setpoint";
  case UI_SET_SETPOINT_HEAT:
    return "Change heat setpoint";
  case UI_SET_SETPOINT_COOL:
    return "Change cool setpoint";
  case UI_SET_SSID:
    return "Set Wifi SSID";
  case UI_SET_PASSWD:
    return "Set Wifi Password";
  default:
    return "Unkown";
  }
}

void handleRoot() {
 String message =  "<html><head><title>Wifi Temp Controller</title></head><body>Current Temp: ";
  message += currentTemp;
  message += "<p>Current Mode: ";
  message += uiToString(ui);
  message += "<p>Current Setpoint";
  if (mode == MODE_HEAT)
    message += heatSetpoint;
  else
    message += coolSetpoint;
  message += "<p>";
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

void buttonTimerCallback(void *pArg) {
  handleUIEvent(BUTTON_TIMER_EXPIRE);
}

void handleButtonPress(void) {
  int pressed = !digitalRead(buttonPin);

  if (pressed) {
    os_timer_arm(&buttonTimer, 5000, 0);
  } else { 
    os_timer_disarm(&buttonTimer);
  }
  /* BUTTON_RELEASED=1 BUTTON_PRESSED=2 */
  buttonEvent = pressed+1;
}

void handleUIEvent(int event) {
  char ui_string[9];

  switch (ui)
  {
  case UI_IDLE:
    if (event == BUTTON_PRESSED)
      ui = UI_IDLE_SHOW_SETPOINT;
    else if (event == BUTTON_TIMER_EXPIRE)
      ui = UI_SET_SETPOINT_HEAT;
    break;
  case UI_IDLE_SHOW_SETPOINT:
    if (event == BUTTON_RELEASED)
      ui = UI_IDLE;
    else if (event == BUTTON_TIMER_EXPIRE)
      ui = UI_SET_SETPOINT_HEAT;
    break;
  case UI_SET_SETPOINT_HEAT:
    if (event == SCROLL) {
      tempSetpoint += scrollDelta;
      scrollDelta = 0;
    } else if (event == BUTTON_PRESSED) {
      heatSetpoint = tempSetpoint;
      ui = UI_SET_SETPOINT_COOL;
    } else if (event == BUTTON_TIMER_EXPIRE) {
      heatSetpoint = tempSetpoint;
      ui = UI_IDLE;
    }
  case UI_SET_SETPOINT_COOL:
    if (event == SCROLL) {
      tempSetpoint += scrollDelta;
      scrollDelta = 0;
    } else if (event == BUTTON_PRESSED) {
      coolSetpoint = tempSetpoint;
      ui = UI_SET_SSID;
    } else if (event == BUTTON_TIMER_EXPIRE) {
      coolSetpoint = tempSetpoint;
      ui = UI_IDLE;
    }
  case UI_SET_SSID:
    if (event == SCROLL) {
      /* choose next SSID */
      scrollDelta = 0;
    } else if (event == BUTTON_PRESSED) {
      /* set SSID to selected one */
      ui = UI_SET_PASSWD;
    } else if (event == BUTTON_TIMER_EXPIRE) {
      /* set SSID to selected one */
      ui = UI_IDLE;
    }
  case UI_SET_PASSWD:
    if (event == SCROLL) {
      /* scroll selected ASCII char */
      scrollDelta = 0;
    } else if (event == BUTTON_PRESSED) {
      /* got to next char of passwd */
      ui = UI_SET_PASSWD;
    } else if (event == BUTTON_TIMER_EXPIRE) {
      /* set SSID to selected one */
      ui = UI_IDLE;
    }
  }

  /* now update the display accordingly */
  switch (ui)
  {
  case UI_IDLE:
    snprintf(ui_string, 9, "%3.1f", currentTemp);
    break;
  case UI_IDLE_SHOW_SETPOINT:
    if (mode == MODE_HEAT)
      snprintf(ui_string, 9, "%3.1f", heatSetpoint);
    else
      snprintf(ui_string, 9, "%3.1f", coolSetpoint);
    break;
  case UI_SET_SETPOINT_HEAT:
  case UI_SET_SETPOINT_COOL:
    snprintf(ui_string, 9, "%3.1f", tempSetpoint);
    break;
  case UI_SET_SSID:
    snprintf(ui_string, 9, "SSID");
    break;
  case UI_SET_PASSWD:
    snprintf(ui_string, 9, "PSWD");
    break;
  default:
    snprintf(ui_string, 9, "???");
  }

  /* parse string and push it to the 4 digit display */
  Serial.println(ui_string);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  os_timer_setfn(&buttonTimer, buttonTimerCallback, NULL);

  Wire.pins(disp_sda, disp_scl);
  statusLEDs.begin();
  statusLEDs.show();

  // We start by connecting to a WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(defaultSSID);

  pinMode(buttonPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(buttonPin), handleButtonPress, CHANGE);

  pinMode(A0, INPUT); /* ADC on LM50 temp sensor */
  
  WiFi.mode(WIFI_STA); /* client mode */
  WiFi.begin(defaultSSID, defaultPasswd);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  } 

  byte macInt[6];
  String macStr;
  WiFi.macAddress(macInt);
  macStr.concat (String(macInt[5], HEX));
  macStr.concat (String(macInt[4], HEX));
  macStr.concat (String(macInt[3], HEX));
  macStr.concat (String(macInt[2], HEX));
  macStr.concat (String(macInt[1], HEX));
  macStr.concat (String(macInt[0], HEX));

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

  Serial.printf("Starting HTTP...\n");
  HTTP.on("/", handleRoot);
  HTTP.on("/index.html", HTTP_GET, [](){
    HTTP.send(200, "text/plain", "Hello World!");
  });
  HTTP.on("/description.xml", HTTP_GET, [](){
    SSDP.schema(HTTP.client());
  });

  Serial.printf("Starting SSDP...\n");
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
  SSDP.begin();

  HTTP.onNotFound(handleNotFound);
  HTTP.begin();
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return statusLEDs.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return statusLEDs.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return statusLEDs.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void loop() {
  long newKnob, oldKnob = 0;
  int color=0;

  currentTemp = analogRead(A0);

  newKnob = knob.read();
  if (newKnob != oldKnob) {
    scrollDelta = newKnob - oldKnob;
    color = (color + (scrollDelta) ) % 255;
    oldKnob = newKnob;
  };

  if (buttonEvent)
    handleUIEvent(buttonEvent);
  
  Serial.print("going set led to:");
  Serial.println(Wheel(color));

  statusLEDs.setPixelColor(0, Wheel(color));
  statusLEDs.show();
  delay(20);


  HTTP.handleClient();
}
