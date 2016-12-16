=======
##Huzzah based temp controller with wifi

This project is a Huzzah Feather based temperature controller with wifi
network access for remote configuration and data logging.  It's built using
off the shelf components from Adafruit.

Components
  * [Huzzah feather board](https://www.adafruit.com/products/2821) Includes WiFi.
  * [FeatherWing Power Relay](https://www.adafruit.com/products/3191) Rated for 10A.  Will work for resistive heaters, but probably not enough for a compressor based cooling system.
  * [Quad Alphanumeric display](https://www.adafruit.com/products/3128) Used for UI.  Uses 3V (about 80mA).  Communicates on I2C (GPIO 4/5) at address 0x70.
  * [Rotary encoder w/push button](https://www.adafruit.com/products/377) Drives the UI.
  * [Diffused 5mm NeoPixel](https://www.adafruit.com/products/1938) 3 Status LEDs.
  * [DS18B20](https://www.adafruit.com/product/381) temp sensor support.  0.5C accuarcy
  * [FeatherWing Proto board](https://www.adafruit.com/products/2884) Gives some space to wire up all the external devices.
  * Misc: [stacking headers](https://www.adafruit.com/products/2830), [case](https://www.amazon.com/gp/product/B0002BSRIO/ref=oh_aui_detailpage_o02_s00?ie=UTF8&psc=1), outlet

=======
##Build and Flash

The Arduino IDE is used to build and flash the board.  In order to build the code, you'll
need to [install the following libraries](https://www.arduino.cc/en/Guide/Libraries):

####Standard Libraries
   * ESP8266 Board package from: http://arduino.esp8266.com/stable/package_esp8266com_index.json
   * ESP8266WiFi
   * ESP8266WebServer
   * ESP8266SSDP
   * [Encoder](http://www.pjrc.com/teensy/td_libs_Encoder.html)
   * [OneWire](http://www.pjrc.com/teensy/td_libs_OneWire.html)
   * [Dallas Temperature](https://github.com/milesburton/Arduino-Temperature-Control-Library)
   * [Time](http://playground.arduino.cc/code/time)
   * [ArduinoJson](https://github.com/bblanchon/ArduinoJson)
   * [PubSubClient](http://pubsubclient.knolleary.net/)

####[User installed libraries](https://learn.adafruit.com/adafruit-all-about-arduino-libraries-install-use/how-to-install-a-library)
   * [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel)
   * [Adafruit LEDBackpack](https://github.com/adafruit/Adafruit_LED_Backpack)
   * [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library)

Set the board type **Tools->Board:** to "Adafruit HUZZAH ESP8266".  If this choice doesn't exist, make sure you've installed the ESP8266 Board Package (above).

Connect a USB cable to the board.  Set the **Tools->Port** to the newly enumerated serial interface.

The Wifi and MQTT data is hardcoded in the build.  You must edit the [credentials.h] (https://github.com/cj8scrambler/eagle/blob/master/HUZZAH_TEMP_CONTROL/tmpctrl/credentials.h)
file to your approriate values.

Click the 'Verify' button to build the code.  Click the 'Upload' button to reflash the board.

=======
##Assemble Hardware

The [Huzzah](https://www.adafruit.com/products/2821), [Power Relay](https://www.adafruit.com/products/3191), and [Quad Alphanumeric display](https://www.adafruit.com/products/3128) simply stack on top of each other with the display on top.


The [Power Relay](https://www.adafruit.com/products/3191) needs a solder blob placed on pin 15.
![feather setup](https://raw.githubusercontent.com/cj8scrambler/eagle/master/HUZZAH_TEMP_CONTROL/hw/relay_feather.png)


The [Rotary encoder](https://www.adafruit.com/products/377), [NeoPixels](https://www.adafruit.com/products/1938), and [DS18B20 temp sensors](https://www.adafruit.com/product/381) need to be wired to the board.  I've made a custom feather board which provides a simple way to wire these up.  [![Order from OSH Park](https://oshpark.com/assets/badge-5b7ec47045b78aef6eb9d83b3bac6b1920de805e9a0c227658eac6e19a045b9c.png)](https://oshpark.com/shared_projects/WBo4cvpj)


If you want to wire it up manually instead, you could use a [FeatherWing Proto board](https://www.adafruit.com/products/2884).  Here is a rudimentary wiring diagram:
![huzzah wiriing](https://raw.githubusercontent.com/cj8scrambler/eagle/master/HUZZAH_TEMP_CONTROL/hw/wiring.png)

=======
##UI Definition
 
###States
  * Idle mode
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
  * flashing orange - Error state: No main temp sensor
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
##Backend data

Data is broadcast to an MQTT broker.  This allows it to be monitored in real time, but usually does
not provide any persistant storage.

I am using [a python script](https://gist.github.com/matbor/6532185) to subscribe the the data stream
and back it up in my own MySQL database.

[Hive](http://www.hivemq.com/try-out/) provides free MQTT broker services.  There are also a number
of free brokers you can [run on your own](http://blog.thingstud.io/getting-started/free-mqtt-brokers-for-thingstudio/).

=======
##Web interface
There is a simple native web interface running on the feather board.  This gives a way to check
the current status and change settings.  You will need to know the IP address of your board to access
this and you probably won't be able to access it remotely depending on your network configuration.

There is a more sophisitcated javascript interface that can be run from a remote server.  It uses
MQTT to get current data and send configuration commands to the device.  It can also use an SQL
database to display historical data as well.  This interface can be accessed remotely and doesn't
require you to know the IP of the device.
