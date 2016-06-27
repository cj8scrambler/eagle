
/*
    Simple HTTP get webclient test
*/

#include <DHT.h>
#include <Wire.h>
#include <IRremoteESP8266.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266SSDP.h>
#include <Adafruit_TCS34725.h>

const char* ssid     = "MM-Guest";
const char* password = "GuestAccess2016";

const int irLedPin      = 2;
const int irSensorPin   = 13;
const int motionPin     = 5;
const int tempSensorPin = 12;
const int color_sda     = 4;
const int color_scl     = 5;

int temp_enabled = 0;
int color_enabled = 0;

ESP8266WebServer HTTP(80);
IRsend irsend(irLedPin);
IRrecv irrecv(irSensorPin);
DHT dht(tempSensorPin, DHT11);
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_700MS, TCS34725_GAIN_1X);

void  ircode (decode_results *results)
{
  // Panasonic has an Address
  if (results->decode_type == PANASONIC) {
    Serial.print(results->panasonicAddress, HEX);
    Serial.print(":");
  }

  // Print Code
  Serial.print(results->value, HEX);
}
void  encoding (decode_results *results)
{
  switch (results->decode_type) {
    default:
    case UNKNOWN:      Serial.print("UNKNOWN");       break ;
    case NEC:          Serial.print("NEC");           break ;
    case SONY:         Serial.print("SONY");          break ;
    case RC5:          Serial.print("RC5");           break ;
    case RC6:          Serial.print("RC6");           break ;
    case DISH:         Serial.print("DISH");          break ;
    case SHARP:        Serial.print("SHARP");         break ;
    case JVC:          Serial.print("JVC");           break ;
    case SANYO:        Serial.print("SANYO");         break ;
    case MITSUBISHI:   Serial.print("MITSUBISHI");    break ;
    case SAMSUNG:      Serial.print("SAMSUNG");       break ;
    case LG:           Serial.print("LG");            break ;
    case WHYNTER:      Serial.print("WHYNTER");       break ;
    case AIWA_RC_T501: Serial.print("AIWA_RC_T501");  break ;
    case PANASONIC:    Serial.print("PANASONIC");     break ;
  }
}
void  dumpInfo (decode_results *results)
{
  // Show Encoding standard
  Serial.print("Encoding  : ");
  encoding(results);
  Serial.println("");

  // Show Code & length
  Serial.print("Code      : ");
  ircode(results);
  Serial.print(" (");
  Serial.print(results->bits, DEC);
  Serial.println(" bits)");
}
void  dumpRaw (decode_results *results)
{
  // Print Raw data
  Serial.print("Timing[");
  Serial.print(results->rawlen-1, DEC);
  Serial.println("]: ");

  for (int i = 1;  i < results->rawlen;  i++) {
    unsigned long  x = results->rawbuf[i] * USECPERTICK;
    if (!(i & 1)) {  // even
      Serial.print("-");
      if (x < 1000)  Serial.print(" ") ;
      if (x < 100)   Serial.print(" ") ;
      Serial.print(x, DEC);
    } else {  // odd
      Serial.print("     ");
      Serial.print("+");
      if (x < 1000)  Serial.print(" ") ;
      if (x < 100)   Serial.print(" ") ;
      Serial.print(x, DEC);
      if (i < results->rawlen-1) Serial.print(", "); //',' not needed for last one
    }
    if (!(i % 8))  Serial.println("");
  }
  Serial.println("");                    // Newline
}

void  dumpCode (decode_results *results)
{
  // Start declaration
  Serial.print("unsigned int  ");          // variable type
  Serial.print("rawData[");                // array name
  Serial.print(results->rawlen - 1, DEC);  // array size
  Serial.print("] = {");                   // Start declaration

  // Dump data
  for (int i = 1;  i < results->rawlen;  i++) {
    Serial.print(results->rawbuf[i] * USECPERTICK, DEC);
    if ( i < results->rawlen-1 ) Serial.print(","); // ',' not needed on last one
    if (!(i & 1))  Serial.print(" ");
  }

  // End declaration
  Serial.print("};");  //

  // Comment
  Serial.print("  // ");
  encoding(results);
  Serial.print(" ");
  ircode(results);

  // Newline
  Serial.println("");

  // Now dump "known" codes
  if (results->decode_type != UNKNOWN) {

    // Some protocols have an address
    if (results->decode_type == PANASONIC) {
      Serial.print("unsigned int  addr = 0x");
      Serial.print(results->panasonicAddress, HEX);
      Serial.println(";");
    }

    // All protocols have data
    Serial.print("unsigned int  data = 0x");
    Serial.print(results->value, HEX);
    Serial.println(";");
  }
}

void handleRoot() {
 HTTP.send(200, "text/html", "<html><head> <title>ESP8266 Demo</title></head><body><h1>Hello from ESP8266, you can send NEC encoded IR signals from here!</h1><p><a href=\"ir?code=16769055\">Send 0xFFE01F</a></p><p><a href=\"ir?code=16429347\">Send 0xFAB123</a></p><p><a href=\"ir?code=16771222\">Send 0xFFE896</a></p></body></html>");
}

void handleIr(){
  for (uint8_t i=0; i<HTTP.args(); i++){
    if(HTTP.argName(i) == "code")
    {
      unsigned long code = HTTP.arg(i).toInt();
      Serial.print("Sending 32Khz NEC code: ");
      Serial.println(code);
      irsend.sendNEC(code, 32);
    }
  }
  handleRoot();
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

void setup() {
  Serial.begin(115200);
  delay(100);

  Wire.pins(color_sda, color_scl); /* 4 = SDA / 14 = SCL */

  // We start by connecting to a WiFi network

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  irsend.begin();
  irrecv.enableIRIn();
  dht.begin();

  if (tcs.begin()) {
    Serial.println("Found color sensor");
    color_enabled=1;
  } else {
    Serial.println("No TCS34725 color sensor found");
  }
  WiFi.mode(WIFI_STA); /* client mode */
  WiFi.begin(ssid, password);

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
  HTTP.on("/ir", handleIr);
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

uint16_t counter=0;

void loop() {
  decode_results  results;        // Somewhere to store the results
  uint16_t r, g, b, c, colorTemp, lux;
  
  if (irrecv.decode(&results)) {  // Grab an IR code
    dumpInfo(&results);           // Output the results
    dumpRaw(&results);            // Output the results in RAW format
    dumpCode(&results);           // Output the results as source code
    Serial.println("");           // Blank line between entries
    irrecv.resume();              // Prepare for the next value
  }

  if (counter++ == 0) {
    if (temp_enabled) {
      Serial.print("Temp: ");
      Serial.println(dht.readTemperature());
      Serial.print("Humidity: ");
      Serial.println(dht.readHumidity());
    }
    
    if (color_enabled) {
      tcs.getRawData(&r, &g, &b, &c);
      colorTemp = tcs.calculateColorTemperature(r, g, b);
      lux = tcs.calculateLux(r, g, b);
  
      Serial.print("Color Temp: "); Serial.print(colorTemp, DEC); Serial.print(" K - ");
      Serial.print("Lux: "); Serial.print(lux, DEC); Serial.print(" - ");
      Serial.print("R: "); Serial.print(r, DEC); Serial.print(" ");
      Serial.print("G: "); Serial.print(g, DEC); Serial.print(" ");
      Serial.print("B: "); Serial.print(b, DEC); Serial.print(" ");
      Serial.print("C: "); Serial.print(c, DEC); Serial.print(" ");
      Serial.println(" ");
    }
  }
  
  HTTP.handleClient();
}
