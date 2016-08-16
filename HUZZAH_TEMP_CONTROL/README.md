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

###UI States
 
``` 
  ###.# - Idle mode
    Status LED - red/blue for heat/cool mode
    shows current temp
    button press shows setpoint for 2 seconds after release
    while holding button, rotary changes setpoint
    hold button for 5 seconds to go to settings mode

  Settings Mode
    button press advances to the next state
    hold button for 5 seconds to go to idle mode
    15 second idle timeout returns to idle mode

    HEAT - Heat mode
      Status LED flashes red
      rotary changes setpoint temp.
    COOL - Cool mode
      Status LED flashes blue
      rotary changes setpoint temp.
    WIFI - select SSID
      Wifi LED flashes orange
      rotary scrolls through SSID list which is scrolling across dislay
    ___A - set passwd
      Wifi LED solid orange
      rotary selects char A-Za-z0-9[puncts]
      button chooses char and shifts display left
      hold button for 5 seconds to set password
      15 second idle timeout cancels password change and returns to idle mode
``` 

###Wifi LED
  * solid green  - Connected and communicating with server
  * flash green  - Connected and attemting to communicate with server
  * solid blue   - Failed to authenticate
  * flash red    - Attempting to connect to network
  * solid red    - NO SSID found
  * flash orange - currently selecting SSID
  * solid orange - currently setting password (see above)

###Status LED
  * solid red    - Running in heat mode
  * flash red    - Setting heat setpoing
  * solid blue   - Running in cool mode
  * flash blue   - Setting cool setpoing
