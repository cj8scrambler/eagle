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


#define TEMP_SAMPLE_INTERVAL    2000  /*mS*/
#define MIN_COMPRESSOR_SECONDS   180

#define SETPOINT_MIN              10
#define SETPOINT_MAX           10000
#define HYSTERESIS_MIN            10
#define HYSTERESIS_MAX           990

extern int16_t currentTemp;
extern int16_t setpoint;
extern temp_mode mode;
extern int16_t hysteresis;
extern bool comp_mode;

bool getPower(void);
