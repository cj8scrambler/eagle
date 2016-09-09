#include <ESP8266WebServer.h>
#include <ESP8266SSDP.h>

#include "tmpctrl.h"
#include "ui.h"

#define BUFFER_SIZE 32

ESP8266WebServer HTTP(80);

void handleRoot() {
  int16_t value;

  String text, color;
  String new_setpoint = HTTP.arg("setpoint");
  String new_hysteresis = HTTP.arg("hysteresis");
  String new_mode = HTTP.arg("mode");
  String new_comp_mode = HTTP.arg("comp_mode");

  if (new_setpoint.length()) {
    value = new_setpoint.toFloat()*100;
    if ((value >= SETPOINT_MIN) && (value <= SETPOINT_MAX)) {
      if (value != setpoint) {
        setpoint = value;
        Serial.print("Setpoint updated from HTTP to: ");
        Serial.println(divide_100(setpoint));
      }
    } else {
      Serial.print("Error: invalid setpoint configured from HTTP: ");
      Serial.println(new_setpoint);
    }
  }

  if (new_hysteresis.length()) {
    value = new_hysteresis.toFloat()*100;
    if ((value >= HYSTERESIS_MIN) && (value <= HYSTERESIS_MAX)) {
      if (value != hysteresis) {
        hysteresis = value;
        Serial.print("Hysteresis updated from HTTP to: ");
        Serial.println(divide_100(hysteresis));
      }
    } else {
      Serial.print("Error: invalid hysteresis configured from HTTP: ");
      Serial.println(new_setpoint);
    }
  }

  if (new_mode.length()){
    if (new_mode == "cool") {
      if (mode != MODE_COOL) {
        mode = MODE_COOL;
        Serial.println("Mode updated from HTTP to: cool");
      }
    } else if (new_mode == "heat") {
      if (mode != MODE_HEAT) {
        mode = MODE_HEAT;
        Serial.println("Mode updated from HTTP to: heat");
      }
    } else {
      Serial.print("Error: invalid mode configured from HTTP: ");
      Serial.println(new_mode);
    }
  }

  if (new_comp_mode.length()){
    if (new_comp_mode == "on") {
      if (!comp_mode) {
        comp_mode = 1;
        Serial.println("Compressor mode updated from HTTP to: on");
      }
    } else if (new_comp_mode == "off") {
      if (comp_mode) {
        comp_mode = 0;
        Serial.println("Compressor mode updated from HTTP to: off");
      }
    } else {
      Serial.print("Error: invalid compressor mode configured from HTTP: ");
      Serial.println(new_mode);
    }
  }

  String message =  "<html><head><title>Wifi Temp Controller</title></head>\n";
  message += "<style>\n";
  message += "  body{background-color: #0F5489;color: #eeeeee;font-family: verdana;font-size: 20px;}\n";
  message += "  input {width: 200px;padding: 12px 20px;margin: 8px 0;";
  message +=          "border-radius: 4px;background-color: #07426F;color: #eeeeee;}\n";
  message += "  input[type=submit] {background-color: #457DA8;color: #eeeeee}\n";
  message += "  table, th, td {border: 1px solid black;border-collapse: collapse;}\n";
  message += "</style>\n";

  message += "<body>\n";

  message += "<table cellborder=2>\n";
  message += "  <tr><th colspan=2>Settings</th><th width=100></th><th colspan=2>Status</th></tr>\n";
  message += "<form method='get' action=''>\n";

  message += "  <tr><td><label>Setpoint: </label></td><td><input type=\"number\" step=\"0.1\" min=\"0\" max=\"100\" name='setpoint' value=\"";
  message += divide_100(setpoint);
  message += "\"></td><td></td>\n";
  message += "<td><label>Current temperature: </label></td><td><input type=\"number\" readonly value=\"";
  message += divide_100(currentTemp);
  message += "\"></td></tr>\n";

  message += "  <tr><td><label>Hysteresis: </label></td><td><input type=\"number\" name='hysteresis' step=\"0.1\" min=\"0\" max=\"9.9\" value=\"";
  message += divide_100(hysteresis);
  message += "\"></td><td></td>\n";
  if (getPower()) {
    if (mode == MODE_COOL) {
      text += "COOL";
      color += "blue";
    } else if (mode == MODE_HEAT) {
      text += "HEAT";
      color += "red";
    }
  } else {
    text += "OFF";
    color += "grey";
  }
  message += "<td><label>Output: </label></td><td bgcolor=";
  message += color;
  message += "><input type=\"text\" readonly value=\"";
  message += text;
  message += "\"></td></tr>\n";

  message += "  <tr><td><label>Mode: </label></td><td><select name='mode'>\n";
  message += "    <option ";
  if (mode == MODE_HEAT)
    message += "selected ";
  message += "value=\"heat\">Heat</option>\n";
  message += "    <option ";
  if (mode == MODE_COOL)
    message += "selected ";
  message += "value=\"cool\">Cool</option>\n";
  message += "  </select></td></tr>\n";

  message += "  <tr><td><label>Compressor Mode: </label></td><td><select name='comp_mode'>\n";
  message += "    <option ";
  if (comp_mode)
    message += "selected ";
  message += "value=\"on\">On</option>\n";
  message += "    <option ";
  if (!comp_mode)
    message += "selected ";
  message += "value=\"off\">Off</option>\n";
  message += "  </select></td></tr>\n";

  message += "  <tr><td></td><td><input type=\"submit\" value=\"Update\"></td></tr>\n";
  message += "</form\n></table>";

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
}

void loopHTTP() {
  HTTP.handleClient();
}
