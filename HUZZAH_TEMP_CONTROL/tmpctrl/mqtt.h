void mqtt_addValue(String key, String value);
void mqtt_addValue(String key, String value, time_t time);
void mqtt_addValue(String key, int value);
void mqtt_addValue(String key, int value, time_t time);
void mqtt_addValue_rawStr(String key, String value);
void mqtt_addValue_rawStr(String key, String value, time_t time);
void mqtt_send();
void mqtt_setup(uint32_t uniqueId);
void mqtt_loop();
