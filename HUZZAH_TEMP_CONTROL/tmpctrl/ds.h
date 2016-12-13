typedef enum {
  TEMP_MAIN,
  TEMP_AUX,
} temp_sensor;

bool ds_setup(void);
int16_t read_ds_temp(temp_sensor sensor);
bool ds_is_present(temp_sensor sensor);
bool ds_swap_sensors(void);
bool ds_delete_sensors(void);
int16_t get_temp(temp_sensor sensor); /* return temp in 1/100th degrees */
void start_temp_reading(void);
