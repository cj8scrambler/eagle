##Huzzah based temp controller with wifi
=======

This project is a Huzzah based temp controller with wifi
network access for remote data logging.  It's built using
existing feather shields from Adafruit.

Components
  * [Huzzah feather board](https://www.adafruit.com/products/2821) from Adafruit 
  * [Non-latching relay feather wing](https://www.adafruit.com/products/2895) from Adafruit.  Too under powered.
  * [Quad Alphanumeric display](https://www.adafruit.com/products/3128) from Adafruit.  Used for UI.  Uses 3V (about 80mA).  Communicates on I2C (GPIO 4/5) at address 0x70.
  * Rotary encoder
  * [Diffused 5mm NeoPixel] (https://www.adafruit.com/products/1938) from Adrafruit for status LEDs.
  * LM50 temp sensor (compatible with BeerBug)

=======
###Arduino Libraries
   * ESP8266 Board package: http://arduino.esp8266.com/stable/package_esp8266com_index.json
   * [TimeLib](http://www.pjrc.com/teensy/td_libs_Time.html)
   * [Encoder](http://www.pjrc.com/teensy/td_libs_Encoder.html)
   * [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel)
   * Adafruit GFX
   * Adafruit LEDBackpack
=======

###UI States
 
  * ###.# - Idle mode
    * Status LED - red/blue for heat/cool mode; flashing is off; solid is on
    * shows current temp
    * button press shows setpoint for 2 seconds after release
    * while holding button, rotary changes setpoint
    * hold button for 5 seconds to go to settings mode

  * Settings Mode
    * button press advances to the next state
    * hold button for 5 seconds to go to idle mode
    * 15 second idle timeout returns to idle mode

    * Set Mode: HEAT or COOL
      * rotary changes mode
    * Set Setpoint:
      * rotary changes setpoint temp.
    * Set Hysterisis:
      * rotary changes hysteresis temp (0 - 9.9)
        * In cool mode: device turns on above setpoint and turns off at (setpoint - hysteresis)
        * In heat mode: device turns on below setpoint and turns off at (setpoint + hysteresis)
    * Set Compressor Mode:
      * rotary changes mode (compressor mode or regular mode)
        * In compressor mode, a 3 minute minimum on or off time is enforced

###Status LED
  * off          - Starting up
  * solid red    - Running in heat mode
  * solid blue   - Running in cool mode

###Wifi LED
  * solid red    - Wifi disabled
  * flash orange - Attempting to connect to network
  * solid orange - Connected to wifi and no time sync
  * solid green  - Connected and time sync'ed

###Relay LED
  * off          - Relay off
  * solid red    - Relay on in heat mode
  * solid blue   - Relay on in cool mode


=======
##Cloud Backend

Investigating possible backends to hold data
   * [SensorCloud](https://github.com/LORD-MicroStrain/SensorCloud/blob/master/API/README.md) Free access, but requires HTTPS
   * [thethingsio](https://panel.thethings.io)  First device is free.  Very straightfoward; even includes client code.
   * [freeboard](https://freeboard.io)  Instant data is free; historical data seems to cost $1/month.
   * [AWS](https://learn.adafruit.com/cloud-thermometer/software-setup) complicated setup.
