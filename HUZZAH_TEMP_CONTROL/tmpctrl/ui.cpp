/*
    Temp controller with data logger
*/

extern "C" {
#include <os_type.h>
}

#include <Encoder.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_LEDBackpack.h>

#include "tmpctrl.h"
#include "ui.h"

#define BUTTON_RELEASED     1
#define BUTTON_PRESSED      2
#define BUTTON_TIMER_EXPIRE 3
#define UI_TIMER_2S_EXPIRE  4
#define UI_TIMER_10S_EXPIRE 4
#define SCROLL              5

#define NUM_LEDS            3
#define HYS_MAX           9.9

typedef enum {
  UI_IDLE,
  UI_IDLE_SHOW_SETPOINT,
  UI_SET_MODE,
  UI_SET_SETPOINT,
  UI_SET_HYST,
  UI_SET_COMP,
} ui_state;

typedef struct {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  bool blink;
} led;

extern float currentTemp;
extern float setpoint;
extern temp_mode mode;
extern float hysteresis;
extern bool comp_mode;

static ui_state ui = UI_IDLE;

static volatile int idleTime = 0;
static volatile int buttonEvent = 0;
static int scrollDelta = 0;

static os_timer_t buttonTimer;
static os_timer_t blinkTimer;
static os_timer_t idleTimer;

static led ledData[NUM_LEDS];

static Encoder knob(rotary1Pin,rotary2pin);
static Adafruit_NeoPixel statusLEDs = Adafruit_NeoPixel(NUM_LEDS, neoPixelPin, NEO_GRB + NEO_KHZ800);
static Adafruit_AlphaNum4 alpha4 = Adafruit_AlphaNum4();

static void handleUIEvent(int event);

static void buttonTimerCallback(void *pArg) {
  handleUIEvent(BUTTON_TIMER_EXPIRE);
}

static void blinkTimerCallback(void *pArg) {
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

static void idleTimerCallback(void *pArg) {
  if (idleTime == 2) {
    os_timer_arm(&idleTimer, 8000, 0);
    idleTime = 10;
    handleUIEvent(UI_TIMER_2S_EXPIRE);
  }
  idleTime = 0;
  handleUIEvent(UI_TIMER_10S_EXPIRE);
}


static void restartIdleTimer(void) {
  idleTime = 2;
  os_timer_disarm(&idleTimer);
  os_timer_arm(&idleTimer, 2000, 0);
}

static void cancelIdleTimer(void) {
  idleTime = 0;
  os_timer_disarm(&idleTimer);
}

static void handleButtonPress(void) {
  int pressed = !digitalRead(buttonPin);

  if (pressed) {
    os_timer_arm(&buttonTimer, 5000, 0);
  } else { 
    os_timer_disarm(&buttonTimer);
  }
  /* BUTTON_RELEASED=1 BUTTON_PRESSED=2 */
  buttonEvent = pressed+1;
}


static void updateDisplay(char *data) {
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

static void handleUIEvent(int event) {
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
      updateStatusLED(STATUS_LED, (mode==MODE_HEAT)?RED:BLUE, false);
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
    snprintf(ui_string, 9, "%3.1f", currentTemp);
    break;
  case UI_IDLE_SHOW_SETPOINT:
    snprintf(ui_string, 9, "%3.1f", setpoint);
    break;
  case UI_SET_MODE:
    snprintf(ui_string, 9, "%s", (mode==MODE_HEAT)?"HEAT":"COOL");
    break;
  case UI_SET_SETPOINT:
    snprintf(ui_string, 9, "%3.1f", setpoint);
    break;
  case UI_SET_HYST:
    snprintf(ui_string, 9, "H %1.1f", hysteresis);
    break;
  case UI_SET_COMP:
    snprintf(ui_string, 9, "%s", comp_mode?"COMP":"REG");
    break;
  default:
    snprintf(ui_string, 9, "???");
  }

  /* parse string and push it to the 4 digit display */
  updateDisplay(ui_string);
  Serial.println(ui_string);
}

char *uiToString(void)
{
  switch (ui)
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

void updateStatusLED(uint8_t led, uint32_t rgb_color, bool blink){
  ledData[led].red = (rgb_color && 0xFF0000) >> 4;
  ledData[led].green = (rgb_color && 0x00FF00) >> 2;
  ledData[led].blue = (rgb_color && 0x0000FF);
  ledData[led].blink = blink;
}

void uiSetup() {
  os_timer_setfn(&buttonTimer, buttonTimerCallback, NULL);
  os_timer_setfn(&blinkTimer, blinkTimerCallback, NULL);
  os_timer_setfn(&idleTimer, idleTimerCallback, NULL);

  Wire.pins(disp_sda, disp_scl);

  statusLEDs.begin();
  statusLEDs.show();
  updateStatusLED(STATUS_LED, OFF, false);
  updateStatusLED(RELAY_LED, OFF, false);
  updateStatusLED(WIFI_LED, RED, false);
  os_timer_arm(&blinkTimer, 500, 1);

  alpha4.begin(0x70);

  pinMode(buttonPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(buttonPin), handleButtonPress, CHANGE);
}

void uiLoop() {

  long newKnob, oldKnob = 0;

  newKnob = knob.read();
  if (newKnob != oldKnob) {
    scrollDelta = newKnob - oldKnob;
    oldKnob = newKnob;
    handleUIEvent(SCROLL);
  }

  if (buttonEvent)
    handleUIEvent(buttonEvent);
  
}
