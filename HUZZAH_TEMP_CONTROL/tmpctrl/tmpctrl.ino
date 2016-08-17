/*
    Temp controller with data logger
*/

#include <os_type.h>

#include <Encoder.h>
#include <Wire.h>
#include <Time.h>

#include <Adafruit_NeoPixel.h>
#include <Adafruit_LEDBackpack.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266SSDP.h>

#include "credentials.h"

extern void ntp_setup();

#define BUTTON_RELEASED     1
#define BUTTON_PRESSED      2
#define BUTTON_TIMER_EXPIRE 3
#define UI_TIMER_2S_EXPIRE  4
#define UI_TIMER_10S_EXPIRE 4
#define SCROLL              5

#define NUM_LEDS            2
#define STATUS_LED          0
#define WIFI_LED            1
#define RED          0xff0000
#define ORANGE       0xff8000
#define YELLOW       0xffff00
#define GREEN        0x00ff00
#define BLUE         0x0000ff
#define VILOET       0xff00ff
#define WHITE        0xffffff

#define HYS_MAX           9.9
#define MIN_COMPRESSOR_SECONDS  180
#define NUM_DATA_SAMPLES  512

typedef enum {
  UI_IDLE,
  UI_IDLE_SHOW_SETPOINT,
  UI_SET_MODE,
  UI_SET_SETPOINT,
  UI_SET_HYST,
  UI_SET_COMP,
} ui_state;

typedef enum {
  MODE_HEAT,
  MODE_COOL,
} temp_mode;

typedef struct {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  bool blink;
} led;

typedef struct {
  uint32_t timestamp;
  uint8_t enabled;
  float temp;
  float setpoint;
  float hyst;
  uint8_t mode;
  uint8_t comp_mode;
} datapoint;

const int buttonPin     = 0;
const int neoPixelPin   = 2;
const int disp_sda      = 4;
const int disp_scl      = 5;
const int relayPin      = 12;
const int rotary1Pin    = 13;
const int rotary2pin    = 14;

float currentTemp = 25.0;
float setpoint;
temp_mode mode = MODE_HEAT;
float hysteresis;
bool comp_mode;

ui_state ui = UI_IDLE;


volatile int idleTime = 0;
volatile int buttonEvent = 0;
int scrollDelta = 0;

os_timer_t buttonTimer;
os_timer_t blinkTimer;
os_timer_t idleTimer;

led ledData[NUM_LEDS];
datapoint dataLog[NUM_DATA_SAMPLES];
int readIndex=0, writeIndex=0;

Encoder knob(rotary1Pin,rotary2pin);
Adafruit_NeoPixel statusLEDs = Adafruit_NeoPixel(NUM_LEDS, neoPixelPin, NEO_GRB + NEO_KHZ800);
Adafruit_AlphaNum4 alpha4 = Adafruit_AlphaNum4();
ESP8266WebServer HTTP(80);

char *uiToString(ui_state current)
{
  switch (current)
  {
  case UI_IDLE:
    return "Idle";
  case UI_IDLE_SHOW_SETPOINT:
    return "Idle: Showing setpoint";
  case UI_SET_MODE:
    return "Change mode";
  case UI_SET_SETPOINT:
    return "Change setpoint";
  case UI_SET_HYST:
    return "Change hysteresis";
  case UI_SET_COMP:
    return "Change compressor mode";
  default:
    return "Unkown";
  }
}

void handleRoot() {
 String message =  "<html><head><title>Wifi Temp Controller</title></head><body>Current Temp: ";
  message += currentTemp;
  message += "<p>\nCurrent State: ";
  message += uiToString(ui);
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

void buttonTimerCallback(void *pArg) {
  handleUIEvent(BUTTON_TIMER_EXPIRE);
}

void blinkTimerCallback(void *pArg) {
  int i;
  uint32_t current=0;

  for(i = 0; i < NUM_LEDS; i++) {
    if (ledData[i].blink)
      current = statusLEDs.getPixelColor(i);
    statusLEDs.setPixelColor(i,
                             current?0:ledData[i].red,
                             current?0:ledData[i].green,
                             current?0:ledData[i].blue);
  }
  statusLEDs.show();
}

void restartIdleTimer(void) {
  idleTime = 2;
  os_timer_disarm(&idleTimer);
  os_timer_arm(&idleTimer, 2000, 0);
}

void cancelIdleTimer(void) {
  idleTime = 0;
  os_timer_disarm(&idleTimer);
}

void idleTimerCallback(void *pArg) {
  if (idleTime == 2) {
    os_timer_arm(&idleTimer, 8000, 0);
    idleTime = 10;
    handleUIEvent(UI_TIMER_2S_EXPIRE);
  }
  idleTime = 0;
  handleUIEvent(UI_TIMER_10S_EXPIRE);
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

void updateStatusLED(uint8_t led, uint32_t rgb_color, bool blink){
  ledData[led].red = (rgb_color && 0xFF0000) >> 4;
  ledData[led].green = (rgb_color && 0x00FF00) >> 2;
  ledData[led].blue = (rgb_color && 0x0000FF);
  ledData[led].blink = blink;
}

void updateDisplay(char *data) {
  int led=3;
  int pos=strlen(data);
  bool dot=false;
  
  if (pos < 1 || pos > 8) {
    Serial.println("Error: invalid string length");
    return;
  }

  do {
    if (data[pos] == '.') {
      dot = true;
      pos--;
    }
    alpha4.writeDigitAscii(led, data[pos], dot);
    dot=false;
  } while ((pos--) && (led--));
  alpha4.writeDisplay();
}

void handleUIEvent(int event) {
  char ui_string[9];

  switch (ui)
  {
  case UI_IDLE:
    if (event == BUTTON_PRESSED)
      ui = UI_IDLE_SHOW_SETPOINT;
    else if (event == BUTTON_TIMER_EXPIRE)
      ui = UI_SET_MODE;
    break;
  case UI_IDLE_SHOW_SETPOINT:
    if (!digitalRead(buttonPin)) {
      if (event == SCROLL) {
        os_timer_disarm(&buttonTimer);
        cancelIdleTimer();
        setpoint += scrollDelta/10;
        scrollDelta = 0;
      }
    }
    else if (event == BUTTON_PRESSED)
      restartIdleTimer();
    else if (event == BUTTON_RELEASED)
      restartIdleTimer();
    else if (event == UI_TIMER_2S_EXPIRE)
      ui = UI_IDLE;
    else if (event == BUTTON_TIMER_EXPIRE)
      ui = UI_SET_MODE;
    break;
  case UI_SET_MODE:
    if (event == SCROLL) {
      restartIdleTimer();
      mode = (mode==MODE_HEAT)?MODE_COOL:MODE_HEAT;
      scrollDelta = 0;
    } else if (event == BUTTON_PRESSED) {
      restartIdleTimer();
      ui = UI_SET_SETPOINT;
    } else if (event == BUTTON_TIMER_EXPIRE) {
      cancelIdleTimer();
      ui = UI_IDLE;
    } else if (event == UI_TIMER_10S_EXPIRE)
      ui = UI_IDLE;
    break;
  case UI_SET_SETPOINT:
    if (event == SCROLL) {
      restartIdleTimer();
      setpoint += scrollDelta;
      scrollDelta = 0;
    } else if (event == BUTTON_PRESSED) {
      restartIdleTimer();
      ui = UI_SET_HYST;
    } else if (event == BUTTON_TIMER_EXPIRE) {
      cancelIdleTimer();
      ui = UI_IDLE;
    } else if (event == UI_TIMER_10S_EXPIRE)
      ui = UI_IDLE;
    break;
  case UI_SET_HYST:
    if (event == SCROLL) {
      restartIdleTimer();
      hysteresis += scrollDelta;
      scrollDelta = 0;
      if (hysteresis < 0)
        hysteresis = 0;
      else if (hysteresis > HYS_MAX)
        hysteresis = HYS_MAX;
    } else if (event == BUTTON_PRESSED) {
      restartIdleTimer();
      ui = UI_SET_COMP;
    } else if (event == BUTTON_TIMER_EXPIRE) {
      cancelIdleTimer();
      ui = UI_IDLE;
    } else if (event == UI_TIMER_10S_EXPIRE)
      ui = UI_IDLE;
    break;
  case UI_SET_COMP:
    if (event == SCROLL) {
      restartIdleTimer();
      comp_mode = !comp_mode;
      scrollDelta = 0;
    } else if (event == BUTTON_PRESSED) {
      restartIdleTimer();
      ui = UI_SET_MODE;
    } else if (event == BUTTON_TIMER_EXPIRE) {
      cancelIdleTimer();
      ui = UI_IDLE;
    } else if (event == UI_TIMER_10S_EXPIRE)
      ui = UI_IDLE;
    break;
  }

  /* now update the display accordingly */
  switch (ui)
  {
  case UI_IDLE:
    updateStatusLED(STATUS_LED, (mode==MODE_HEAT)?RED:BLUE, false);
    snprintf(ui_string, 9, "%3.1f", currentTemp);
    break;
  case UI_IDLE_SHOW_SETPOINT:
    snprintf(ui_string, 9, "%3.1f", setpoint);
    updateStatusLED(STATUS_LED, (mode==MODE_HEAT)?RED:BLUE, true);
    break;
  case UI_SET_MODE:
    updateStatusLED(STATUS_LED, (mode==MODE_HEAT)?RED:BLUE, true);
    snprintf(ui_string, 9, "%s", (mode==MODE_HEAT)?"HEAT":"COOL");
    break;
  case UI_SET_SETPOINT:
    updateStatusLED(STATUS_LED, (mode==MODE_HEAT)?RED:BLUE, true);
    snprintf(ui_string, 9, "%3.1f", setpoint);
    break;
  case UI_SET_HYST:
    updateStatusLED(STATUS_LED, (mode==MODE_HEAT)?RED:BLUE, true);
    snprintf(ui_string, 9, "H %1.1f", hysteresis);
    break;
  case UI_SET_COMP:
    updateStatusLED(STATUS_LED, (mode==MODE_HEAT)?RED:BLUE, true);
    snprintf(ui_string, 9, "%s", comp_mode?"COMP":"REG");
    break;
  default:
    snprintf(ui_string, 9, "???");
  }

  /* parse string and push it to the 4 digit display */
  updateDisplay(ui_string);
  Serial.println(ui_string);
}

int checkWifi(void)
{
  int timeout = 20;
  if (WiFi.status() == WL_CONNECTED) {
    return 0;
  }

  updateStatusLED(WIFI_LED, ORANGE, false);

  WiFi.mode(WIFI_STA); /* client mode */
  WiFi.begin(defaultSSID, defaultPasswd);

  while (WiFi.status() != WL_CONNECTED && timeout--) {
    delay(500);
    Serial.print(".");
  } 

  if (WiFi.status() != WL_CONNECTED) {
    updateStatusLED(WIFI_LED, RED, true);
    return 1;
  }
  
  ntp_setup();
  updateStatusLED(WIFI_LED, GREEN, true);

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

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, 0);

  os_timer_setfn(&buttonTimer, buttonTimerCallback, NULL);
  os_timer_setfn(&blinkTimer, blinkTimerCallback, NULL);
  os_timer_setfn(&idleTimer, idleTimerCallback, NULL);

  Wire.pins(disp_sda, disp_scl);

  statusLEDs.begin();
  statusLEDs.show();
  updateStatusLED(STATUS_LED, GREEN, false);
  updateStatusLED(WIFI_LED, RED, false);
  os_timer_arm(&blinkTimer, 500, 1);

  alpha4.begin(0x70);

  // We start by connecting to a WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(defaultSSID);

  pinMode(buttonPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(buttonPin), handleButtonPress, CHANGE);

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

void loop() {
  float trippoint;
  long newKnob, oldKnob = 0;
  int color=0;

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

  newKnob = knob.read();
  if (newKnob != oldKnob) {
    scrollDelta = newKnob - oldKnob;
    color = (color + (scrollDelta) ) % 255;
    oldKnob = newKnob;
  }

  if (buttonEvent)
    handleUIEvent(buttonEvent);
  
  Serial.print("going set led to:");
  Serial.println(Wheel(color));

  statusLEDs.setPixelColor(0, Wheel(color));
  statusLEDs.show();

  if (!checkWifi()) {
    uploadCloudData();
    HTTP.handleClient();
  }

  delay(20); /* hopefully get rid of this */
}
