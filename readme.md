# OpenBot

For more information: 
https://wiki.leedshackspace.org.uk/wiki/Projects/OpenBot

Runs Arduino code on a NodeMCU ESP-12 dev board for future proofing - 
hackspace members will be able to modify the device without too much 
trouble as it is standard hardware. 

This is also the reason we are using ThingSpeak instead of a custom 
server install in the space - sometimes things can get a bit 
complicated for people not in the know.

## Software

Rename "keys_example.h" to "keys.h" and fill in the appropriate fields 
before compiling.

### Additional Libraries

* [ESP8266](https://github.com/esp8266/Arduino) - "ESP8266" in Library 
Manager
* [TimeZone](https://github.com/JChristensen/Timezone)
* [Time](https://github.com/PaulStoffregen/Time)

## Hardware

Schematic and PCB created in KiCad.

Gerbers as submitted to OSHPark are available in the gerbers folder 
(just upload the zip.) 
If you don't want to make any changes you can order 
[directly from them](https://oshpark.com/shared_projects/XAhVpFpk).

## Housing

The front panel was machined by 
[Schaeffer AG](http://www.schaeffer-ag.de/) and designed using Front 
Panel Designer (FPD). The design is also available in SVG and DXF 
formats (exported from FPD) both with and without reference points in 
the hope that they might be useful. They are handy in that the text 
output is in the form of a single, clean path.

## What does it look like?

![The Current OpenBot](https://adnbr.co.uk/images/openbot/openbot-teenager.jpg)
