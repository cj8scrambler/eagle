#include <ESP8266WebServer.h>
#include <ESP8266SSDP.h>

#include "tmpctrl.h"
#include "ui.h"

#define BUFFER_SIZE 32

ESP8266WebServer HTTP(80);

void handleRoot() {
  char buffer[BUFFER_SIZE];

  String message =  "<html><head><title>Wifi Temp Controller</title></head><body>";

  message += "<form method='get' action=''>\n";

  snprintf(buffer, BUFFER_SIZE, "%.1f", currentTemp);
  message += "<label>Current temperature: </label><input type=\"number\" readonly value=\"";
  message += buffer;
  message += "\">";

  snprintf(buffer, BUFFER_SIZE, "%.1f", setpoint);
  message += "<label>Setpoint: </label><input type=\"number\" step=\"0.1\" min=\"0\" max=\"100\" name='setpoint' value=\"";
  message += buffer;
  message += "\">";

  message += "<label>Mode: </label>\n<select name='mode'>\n";
  message += "  <option ";
  if (mode == MODE_HEAT)
    message += "selected ";
  message += "value=\"heat\">Heat</option>\n";
  message += "  <option ";
  if (mode == MODE_COOL)
    message += "selected ";
  message += "value=\"cool\">Cool</option>\n";
  message += "</select>";

  snprintf(buffer, BUFFER_SIZE, "%1.1f", hysteresis);
  message += "<label>Hysteresis: </label><input name='hysteresis' step=\"0.1\" min=\"0\" max=\"9.9\" value=\"";
  message += buffer;
  message += "\">";

  message += "<label>Compressor Mode: </label><input type=\"checkbox\" name=\"comp_mode\" value=\"1\"";

  message += "<input type=\"submit\" value=\"Update\">";
  message += "</form'>";

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

void setupHTTP(String macStr) {

  Serial.printf("Starting HTTP...\n");
  HTTP.on("/", handleRoot);
  HTTP.on("/index.html", HTTP_GET, [](){
    HTTP.send(200, "text/plain", "Hello World!");
  });
  HTTP.on("/description.xml", HTTP_GET, [](){
    SSDP.schema(HTTP.client());
  });

  HTTP.onNotFound(handleNotFound);
  HTTP.begin();

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

}

void loopHTTP() {
  HTTP.handleClient();
}
