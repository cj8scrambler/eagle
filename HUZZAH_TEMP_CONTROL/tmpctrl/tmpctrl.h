typedef enum {
  MODE_HEAT,
  MODE_COOL,
} temp_mode;

#define buttonPin     0
#define neoPixelPin   2
#define disp_sda      4
#define disp_scl      5
#define relayPin      12
#define rotary1Pin    13
#define rotary2pin    14

extern int16_t currentTemp;
extern int16_t setpoint;
extern temp_mode mode;
extern int16_t hysteresis;
extern bool comp_mode;
