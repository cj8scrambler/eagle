typedef enum {
  MODE_HEAT,
  MODE_COOL,
} temp_mode;

#define buttonPin     0  /* also red LED */
#define digital_temp  2  /* also blue led */
#define disp_sda      4
#define disp_scl      5
#define neoPixelPin   12
#define rotary2Pin    13
#define rotary1Pin    14
#define relayPin      15  /* bootmode pin has pulldown */

#define TEMP_SAMPLE_INTERVAL    2000  /* ms */
#define LOG_DATA_INTERVAL       5000  /* ms */
#define MIN_COMPRESSOR_TIME      180  /* seconds */
#define WIFI_CONNECT_INTERVAL 300000  /* ms */

#define SETPOINT_MIN              10
#define SETPOINT_MAX           10000
#define HYSTERESIS_MIN            10
#define HYSTERESIS_MAX           990

extern int16_t currentTemp;
extern int16_t setpoint;
extern bool full_status;

typedef struct {
  int16_t setpoint;
  temp_mode mode;
  int16_t hysteresis;
  bool comp_mode;
  byte ds_addr[2][8];
} settings;

extern settings g_settings;

bool getPower(void);
