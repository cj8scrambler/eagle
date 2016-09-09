#define STATUS_LED          0
#define RELAY_LED           1
#define WIFI_LED            2

#define OFF          0x000000
#define BLACK        0x000000
#define RED          0xff0000
#define ORANGE       0xff8000
#define YELLOW       0xffff00
#define GREEN        0x00ff00
#define BLUE         0x0000ff
#define VILOET       0xff00ff
#define WHITE        0xffffff

char *divide_100(int16_t value);
char *uiToString(void);
void updateStatusLED(uint8_t led, uint32_t rgb_color, bool blink);
void uiSetup(void);
void uiLoop(void);
