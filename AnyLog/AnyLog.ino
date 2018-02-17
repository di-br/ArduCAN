/*
  MCP2515 CAN Interface Using SPI

  Author: David Harding

  Created: 11/08/2010
  Modified: 6/26/12 by RechargeCar Inc.
  Modified: 2018 di-br

  For further information see:

  http://ww1.microchip.com/downloads/en/DeviceDoc/21801e.pdf
  http://en.wikipedia.org/wiki/CAN_bus

  The MCP2515 Library files also contain important information.

  This sketch is configured to work with some specific interface board,
  CS_PIN, INT_PIN, and interup channel are specific to this board.

  This example code is in the public domain.

*/

//********************************switch functionality***********************//

// have 'I/O' to serial console (comment) or uSD (include _USD_IO)?
//#define _USD_IO <-- this won't work

//********************************headers************************************//

#include "defaults.h"
#include <SPI.h>
#include "MCP2515.h"

#ifdef _USD_IO
#include <SD.h>
#endif

//********************************declarations*******************************//

// switch between serial and uSD I/O for the main loop
#ifdef _USD_IO
// declare SD File
File dataFile;

// initialize uSD pins
const int chipSelect = 9;

#define IO_U dataFile
#else
#define IO_U Serial
#endif

// Create CAN object with pins as defined
MCP2515 CAN(CS_PIN, INT_PIN);

void CANHandler() {
  CAN.intHandler();
}

//********************************setup loop*********************************//
// the setup loop will use serial output for now, to be disabled later

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("Initializing ...");

  // Set up SPI Communication
  // dataMode can be SPI_MODE0 or SPI_MODE3 only for MCP2515
  SPI.setClockDivider(SPI_CLOCK_DIV2);
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);
  SPI.begin();
  delay(250);

  // continue by setting up LEDs
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);

  // Initialize MCP2515 CAN controller at the specified clock frequency
  // (Note:  This is the oscillator attached to the MCP2515, not the Arduino oscillator)
  // while having it scan for a guesses CAN speed
  if (CAN.Init(0, 16))
  {
    Serial.println("MCP2515: Init OK ...");
    SET(LED1S);
  } else {
    Serial.println("MCP2515: Init failed ...");
    RESET(LED1S);
  }
  delay(250);

  // Now go to listen only mode to be more safe
  if (CAN.Mode(MODE_LISTEN))
  {
    Serial.println("MCP2515: change to listen-only mode ...");
    SET(LED1S);
  } else {
    Serial.println("MCP2515: mode change failed ...");
    RESET(LED1S);
  }
  delay(250);
  
#ifdef _USD_IO
  // check if uSD card initialised
  if (!SD.begin(chipSelect))
  {
    Serial.println("uSD card: failed to initialise or not present");
    RESET(LED1S);
    return;
  }
  else {
    Serial.println("uSD card: initialised");
    SET(LED1S);
  }
  delay(250);

  // open uSD file to start logging data, this will append to the file
  File dataFile = SD.open("anycan.log", FILE_WRITE);
  if (dataFile)
  {
    // log that we did initialise to separate different 'sessions'
    dataFile.print("Init OK");
    dataFile.println();
    dataFile.print("AutoBaud: ");
    dataFile.print(CAN.whatSpeed());
    dataFile.println(" kbps");
    // flush and close file
    dataFile.flush();
    dataFile.close();
  }
#endif // _USD_IO
  Serial.print("AutoBaud: ");
  Serial.print(CAN.whatSpeed());
  Serial.println(" kbps ...");


  attachInterrupt(0, CANHandler, FALLING);
  // This code would only accept frames with ID of 0x510 - 0x51F. All other frames
  // will be ignored.
  //CAN.InitFilters(false);
  //CAN.SetRXMask(MASK0, 0x7F0, 0); // match all but bottom four bits
  //CAN.SetRXFilter(FILTER0, 0x511, 0); // allows 0x510 - 0x51F

  Serial.println("Ready ...");
}

byte i = 0;

// CAN message frame (actually just the parts that are exposed by the MCP2515 RX/TX buffers)
Frame message;

//********************************main loop*********************************//
// the main loop will either use serial ouput or log to uSD

void loop() {

#ifdef _USD_IO
  // open uSD file to log data
  File dataFile = SD.open("anycan.log", FILE_WRITE);
  if (dataFile)
  {
    SET(LED1S);
#endif // _USD_IO

    if (CAN.GetRXFrame(message)) {
      SET(LED2S);
      // Print message
#ifdef _USD_IO
      Serial.print("ID: ");
      if (message.id < 0x100) Serial.print("0");
      if (message.id < 0x10) Serial.print("0");
      Serial.print(message.id, HEX);
#endif // _USD_IO
      IO_U.print("ID: ");
      if (message.id < 0x100) IO_U.print("0");
      if (message.id < 0x10) IO_U.print("0");
      IO_U.print(message.id, HEX);
      IO_U.print(" | Extended: ");
      if (message.extended) {
        IO_U.print("Yes");
      } else {
        IO_U.print("No");
      }
      IO_U.print(" | Length: ");
      IO_U.print(message.length, DEC);
      IO_U.print(" || ");
      for (i = 0; i < message.length; i++) {
        if (message.data.byte[i] < 0x10) IO_U.print("0");
        IO_U.print(message.data.byte[i], HEX);
        IO_U.print(" ");
      }
      IO_U.println();
      RESET(LED2S);
    }

#ifdef _USD_IO
    // flush and close file
    dataFile.flush();
    dataFile.close();
  } // if(dataFile)
  else {
    RESET(LED1S);
  }
#endif // _USD_IO  
}

