/****************************************************************************
 * Based on the CAN Read Demo for the SparkFun CAN Bus Shield,
 * written by Stephen McCoy. 
 * His original tutorial is available here:
 * http://www.instructables.com/id/CAN-Bus-Sniffing-and-Broadcasting-with-Arduino
 * Used with permission 2016. License CC BY SA.
 *
 * Some initial extensions inspired by SparkFun's examples made available
 * for the CAN bus shield:
 * https://github.com/sparkfun/SparkFun_CAN-Bus_Arduino_Library
 * 
 * Distributed as-is; no warranty is given.
 ****************************************************************************/

//********************************headers************************************//
#include "defaults.h"
#include "MCP2515.h"
#include "MCP2515_defs.h"
#include "global.h"
#include <SD.h>

//********************************switch functionality***********************//

// have 'I/O' to serial console (comment) or uSD (include _USD_IO)?
#define _USD_IO

// log CAN frames to uSD card?
#define _LOG

// flash LEDs for break/clutch pedal
//#define _LBC

// flash LEDs when DPF regen is active
//     not implemented (yet?)
//#define _DPF

//********************************declarations*******************************//

// declare SD File
File dataFile;

// initialize uSD pins
const int chipSelect = 9;

// LEDs on the CAN shield
const int LED1 = 7; // green  PORTD 7
const int LED2 = 8; // green  PORTB 0
#define LED1S D,7   // and a short-cut for speedier changes
#define LED2S B,0

/* this does not blink when uSD in use?
 // LED on the RedBoard
 const int LED3 = 13; // blue */

// switch between serial and uSD I/O for the main loop
#ifdef _USD_IO
#define IO_U dataFile
#else
#define IO_U Serial
#endif

//********************************setup loop*********************************//
// the setup loop will use serial output for now, to be disabled later

void setup() {
  // for debug use
  Serial.begin(115200);
  delay(100);

  // initialise MCP2515 CAN controller at the specified speed
  if(mcp2515_init(CANSPEED_500))
    Serial.println("CAN Init: ok");
  else
    Serial.println("CAN Init: failed");
  delay(500);

#ifdef _USD_IO
  // check if uSD card initialised
  if (!SD.begin(chipSelect)) {
    Serial.println("uSD card: failed to initialise or not present");
    return;
  }
  else{
    Serial.println("uSD card: initialised");
    delay(50);
  }   
  delay(500);

  // open uSD file to start logging data, this will append to the file
  File dataFile = SD.open("can.log", FILE_WRITE);
  if (dataFile)
  {
    // log that we did initialise to separate different 'sessions'
    dataFile.print("Init OK");
    dataFile.println();
    // flush and close file
    dataFile.flush();
    dataFile.close();
  }
#endif

  // finish by setting up LEDs
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
}

//********************************main loop*********************************//
// the main loop will either use serial ouput or log to uSD

void loop(){

  tCAN message;
  unsigned long timeStamp;

#ifdef _USD_IO
  // open uSD file to log data
  File dataFile = SD.open("can.log", FILE_WRITE);
  if(dataFile)
  {
#endif

    if (mcp2515_check_message())
    {
      if (mcp2515_get_message(&message))
      {
#ifdef _LOG
        timeStamp = millis();

        // output received data to either Serial or uSD
        // mimic the candump format here for easier replay
        // timestamp and 'interface'
        IO_U.print("(");
        IO_U.print(timeStamp);
        IO_U.print(") arcan0 ");
        // CAN id
        if (message.id < 0x100) IO_U.print("0");
        if (message.id < 0x10)  IO_U.print("0");
        IO_U.print(message.id,HEX);
        IO_U.print("#");
        // CAN payload
        IO_U.print(message.header.length,DEC);
        for(int i=0;i<message.header.length;i++)
        {
          if (message.data[i] < 0x10) IO_U.print("0");
          IO_U.print(message.data[i],HEX);
          IO_U.print(" ");
        }
        // CRLF and flush
        IO_U.println("");
        IO_U.flush();
#endif

#ifdef _LBC
        // inspect CAN frame 0x411
        if (message.id == 0x411)
        {
          // check for break switch
          if (message.data[6] & 0x1<<4) {
            SET(LED1S);
            //IO_U.println("BREAK");
          }
          else RESET(LED1S);
        }
        // inspect CAN frame 0x600
        if (message.id == 0x600)
        {
          // check for clutch switch
          if (message.data[6] & 0x1<<2) {
            SET(LED2S);
            //IO_U.println("CLUTCH");
          }
          else RESET(LED2S);
        }
#endif

#ifdef _DPF
        // Hmm... initially I thought DPF regen would be announced in a CAN frame somewhere.
        // This is not the case. So we have to query the ECU if we want to indicate this.
        // To add some fun to this: we need ISO 15765-4 for this...

        // DPF regen count: addresses 0x00029E + 0x00029D
        // send 0x7e0 : 10 08 A8 00 00 02 9E 00
        // receive 0x7e8 : 03 00 00 00 00 00 00 00
        // send 0x7e0 : 21 02 9D 00 00 00 00 00
        // receive 0x7e8 ...

        // DPF regen active: address 0x0001CE
        // send 0x7e0 : 05 A8 00 00 01 CE 00 00
        // reveive 0x7e8 ...
#endif
      }

    }

#ifdef _USD_IO
  }
  // flush and close file
  dataFile.flush();
  dataFile.close();
#endif
}
