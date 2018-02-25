### ! This is another playground repository !
Absolutely everything is work-in-progress and will work for me and me alone.
You're on your own and have been warned.
In particular, it may do nasty things to your hardware.

# ArduCAN
**Ardu**ino **CAN**

This tries to sniff and log data on a CAN bus. Parts also attempt to query useful data.

### General idea
1. Get some cheap Arduino hardware
2. Add CAN bus shield w/ uSD capability
3. Come up with a sketch that continuously reads and dumps to the SD card
4. Have a format that can be used to replay with canplayer
5. *End up with a much more lightweight and smaller unit to log the CAN bus*

All been done before, but that's not the point...

6. Extend the thing to interact w/ the ECU and do other useful things (this will be car specific)

### Prerequisites
1. Some Arduino board. I use a SparkFun Redboard:
![SparkFun CAN-bus shield](https://cdn.sparkfun.com//assets/parts/1/1/7/2/2/13975-01.jpg)
2. A CAN bus shield. I use SparkFun's:
![SparkFun Redboard](https://cdn.sparkfun.com//assets/parts/1/0/4/6/6/13262-01.jpg)

## Current status
WiP...

**Subaru**

This sketch is built starting from SparkFun's examples on how to use their shield. So it uses the code found [there](https://github.com/sparkfun/SparkFun_CAN-Bus_Arduino_Library) to talk to the MCP2515. It currently has three 'options' (selectable at compile time):

1. Log data on high-speed bus (under steering column) to uSD. This *does work*, but seems to *loose* a lot of *frames*. I guess the sketch needs to speed up.
2. No Arduino example seems to be complete without a form of blinken lights. So here we go: flash the two LEDs when you break or disengage the clutch. This *works*, but is rather useless.
3. Query some simple stats for the DPF and either indicate via LED or log to uSD. This *works partially*. Indicating status via LED is still missing, querying the DPF stats is done simple minded and also seems to miss one of the answers. The general idea is there.

**AnyLog**

This sketch is built starting from macchina.cc's examples using their [code base](https://github.com/macchina/mcp2515). Their library for the MCP2515 seems more complete and looked after. The sketch attempts to auto-sense the baud rate and then simply log all frames in listen-only mode. Logging to uSD now seems to work, not tested how many frames are lost though.

It will indicate with LED1 if all initialisation steps are ok, disable LED1 if some failure occurred.
LED2 will be on while receiving/converting a CAN frame. 
