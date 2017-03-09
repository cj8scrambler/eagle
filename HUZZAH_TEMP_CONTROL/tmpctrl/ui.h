#define STATUS_LED          0
#define RELAY_LED           1
#define WIFI_LED            2

#define OFF          0x000000
#define BLACK        0x000000
#define RED          0x1f0000
#define ORANGE       0x1f0900
#define YELLOW       0x1f1b00
#define GREEN        0x001f00
#define BLUE         0x00003f
#define VIOLET       0x1f001f
#define WHITE        0x1f1f3f

char *divide_100(int16_t value);
char *uiToString(void);
void updateStatusLED(uint8_t led, uint32_t rgb_color, bool blink);
void uiSetup(void);
void uiLoop(void);
void uiWaitForTempSensor(void);
void uiShowReset(void);
void uiLedTest(void);
bool uiIsIdleState(void);
