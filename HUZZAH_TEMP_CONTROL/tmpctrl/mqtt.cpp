#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Time.h>
#include <ArduinoJson.h>

#include "tmpctrl.h"
#include "credentials.h"

String topic;

WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;
int cont = 0;

String msg ="";

void mqtt_addValue(String key, String value) {
    if (cont == 0) msg += "{\"keyname\":\""+key+"\",\"value\":\""+value+"\"}";
    else msg +=",{\"keyname\":\""+key+"\",\"value\":\""+value+"\"}";
    cont++;
}

void mqtt_addValue(String key, String value, time_t time) {
    char timestamp[24];
    snprintf(timestamp, 24, "%04d-%02d-%02d %02d:%02d:%02d", year(time), month(time), day(time), hour(time), minute(time), second(time));
    if (cont == 0) msg += "{\"keyname\":\""+key+"\",\"value\":\""+value+"\",\"datetime\":\""+String(timestamp)+"\"}";
    else msg +=",{\"keyname\":\""+key+"\",\"value\":\""+value+"\"}";
    cont++;
}

void mqtt_addValue(String key, int value) {
    if (cont == 0) msg += "{\"keyname\":\""+key+"\",\"value\":"+value+"}";
    else msg += ",{\"keyname\":\""+key+"\",\"value\":\""+value+"\"}";
    cont++;
}

void mqtt_addValue(String key, int value, time_t time) {
    char timestamp[24];
    snprintf(timestamp, 24, "%04d-%02d-%02d %02d:%02d:%02d", year(time), month(time), day(time), hour(time), minute(time), second(time));
    if (cont == 0) msg += "{\"keyname\":\""+key+"\",\"value\":"+value+",\"datetime\":\""+String(timestamp)+"\"}";
    else msg += ",{\"keyname\":\""+key+"\",\"value\":\""+value+"\"}";
    cont++;
}

/*
 * The _rawStr version takes a string argument for value, but passes it as
 * a native JSON type (without quotes).  This allows for strings like
 * "98.4" to be passed up as a native data type without float support.
 */
void mqtt_addValue_rawStr(String key, String value) {
    if (cont == 0) msg += "{\"keyname\":\""+key+"\",\"value\":"+value+"}";
    else msg += ",{\"keyname\":\""+key+"\",\"value\":\""+value+"\"}";
    cont++;
}

void mqtt_addValue_rawStr(String key, String value, time_t time) {
    char timestamp[24];
    snprintf(timestamp, 24, "%04d-%02d-%02d %02d:%02d:%02d", year(time), month(time), day(time), hour(time), minute(time), second(time));
    if (cont == 0) msg += "{\"keyname\":\""+key+"\",\"value\":"+value+",\"datetime\":\""+String(timestamp)+"\"}";
    else msg += ",{\"keyname\":\""+key+"\",\"value\":\""+value+"\"}";
    cont++;
}

void mqtt_send() {
    int ret=0;
    if (client.connected()) {
        ret = client.publish((char*)topic.c_str(),(char*)msg.c_str());
//Serial.println((char*)topic.c_str());
//Serial.println((char*)msg.c_str());
//Serial.print("Publish returned: ");
//Serial.println(ret);
        msg = "";
        cont = 0;
    }
}

static void callback(char* topic, byte* payload, unsigned int length) {
  StaticJsonBuffer<200> jsonBuffer;

//  Serial.print("Message arrived [");
//  Serial.print(topic);
//  Serial.print("] ");
//  for (int i = 0; i < length; i++) {
//    Serial.print((char)payload[i]);
//  }
//  Serial.println();

  JsonObject& root = jsonBuffer.parseObject((char *)payload);
  if (!root.success()) {
    Serial.println("Failed to parse incoming JSON message.");
    return;
  }

  String keyname = root["keyname"];
  String value = root["value"];
//  Serial.print("Got key: ");
//  Serial.println(keyname);

//  Serial.print("Value: ");
//  Serial.println(value);

  if (keyname == "new_mode") {
    g_settings.mode = value.toInt() ? MODE_COOL:MODE_HEAT;
    Serial.print("Updated MODE to: ");
    Serial.println(g_settings.mode);
  }  else if (keyname == "new_compmode") {
    g_settings.comp_mode = value.toInt();
    Serial.print("Updated COMPRESSOR MODE to: ");
    Serial.println(g_settings.comp_mode);
  }  else if (keyname == "full_status") {
    full_status = value.toInt();
    Serial.print("Force full status update");
    Serial.println(full_status);
  }  else if (keyname == "new_setpoint") {
    g_settings.setpoint = value.toInt();
    Serial.print("Updated setpoint to: ");
    Serial.println(g_settings.setpoint);
  }  else if (keyname == "new_hyst") {
    g_settings.hysteresis = value.toInt();
    Serial.print("Updated hysteresis to: ");
    Serial.println(g_settings.hysteresis);
  }

}

void mqtt_setup(uint32_t uniqueId) {
  topic += "/thermswitch/" + String(uniqueId, HEX);
  client.setServer(mqttServer, 1883);
  client.setCallback(callback);
}

static int reconnect() {
  /* Works out to about 1 minute */
  #define MAX_BACKOFF (1<<16)

  static int backoff=1, tries=0;

  if (!client.connected()) {
    if (++tries >= backoff) {
      Serial.print("Attempting MQTT connection...");
      // Attempt to connect
      if (client.connect("ThermSwitch", mqttUser, mqttPasswd)) {
        Serial.println("connected");
        client.subscribe((char*)topic.c_str());
        backoff=1;
        tries=0;
      } else {
        Serial.print("failed, rc=");
        Serial.println(client.state());
        tries=0;
        if (backoff <= (MAX_BACKOFF/2)) {
          backoff *= 2;
          Serial.print("Go to backoff=");
          Serial.println(backoff);
        }
        return -1;
      }
    } else {
      return -1;
    }
  }
  return 0;
}

void mqtt_loop() {
  if (!client.connected()) {
    if (reconnect()) {
      return;
    }
  }
  client.loop();
}
