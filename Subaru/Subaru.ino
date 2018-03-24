/****************************************************************************
   Based on the CAN Read Demo for the SparkFun CAN Bus Shield,
   written by Stephen McCoy.
   His original tutorial is available here:
   http://www.instructables.com/id/CAN-Bus-Sniffing-and-Broadcasting-with-Arduino
   Used with permission 2016. License CC BY SA.

   Some initial extensions inspired by SparkFun's examples made available
   for the CAN bus shield:
   https://github.com/sparkfun/SparkFun_CAN-Bus_Arduino_Library

   Distributed as-is; no warranty is given.
 ****************************************************************************/

//********************************switch functionality***********************//

// have 'I/O' to serial console (comment) or uSD (include _USD_IO)?
//#define _USD_IO

// log CAN frames to uSD card?
#define _LOG

// flash LEDs for break/clutch pedal
//#define _LBC

// flash LEDs when DPF regen is active
//     not implemented (yet?)
//#define _DPF

//********************************headers************************************//
#include "defaults.h"
#include "MCP2515.h"
#include "MCP2515_defs.h"
#include "global.h"
#ifdef _USD_IO
#include <SD.h>
#endif

//********************************declarations*******************************//

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
// declare SD File
File dataFile;

// initialize uSD pins
const int chipSelect = 9;

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
  if (mcp2515_init(CANSPEED_500))
    Serial.println("CAN Init: ok");
  else
    Serial.println("CAN Init: failed");
  delay(500);

#ifdef _USD_IO
  // check if uSD card initialised
  if (!SD.begin(chipSelect))
  {
    Serial.println("uSD card: failed to initialise or not present");
    return;
  }
  else {
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
#endif // _USD_IO

  // finish by setting up LEDs
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);

  // accept all frames
  mcp2515_init_filters(true);

#ifdef _DPF
  // accept no frames unless filtered
  mcp2515_init_filters(false);
  mcp2515_set_mask_or_filter(MASK0, 0x7F0);
  mcp2515_set_mask_or_filter(FILTER0, 0x7E8);
  mcp2515_set_mask_or_filter(MASK1, 0x7F0);
  mcp2515_set_mask_or_filter(FILTER2, 0x7E8);
#endif

}

//********************************main loop*********************************//
// the main loop will either use serial ouput or log to uSD

void loop() {

  tCAN message;
  unsigned long timeStamp;

#ifdef _USD_IO
  // open uSD file to log data
  File dataFile = SD.open("can.log", FILE_WRITE);
  if (dataFile)
  {
#endif // _USD_IO

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
        IO_U.print(message.id, HEX);
        IO_U.print("#");
        // CAN payload
        for (int i = 0; i < message.header.length; i++)
        {
          if (message.data[i] < 0x10) IO_U.print("0");
          IO_U.print(message.data[i], HEX);
          IO_U.print(" ");
        }
        // CRLF and flush
        IO_U.println("");
        IO_U.flush();
#endif // _LOG

#ifdef _LBC
        // inspect CAN frame 0x411
        if (message.id == 0x411)
        {
          // check for break switch
          if (message.data[6] & 0x1 << 4)
          {
            SET(LED1S);
            //IO_U.println("BREAK");
          }
          else RESET(LED1S);
        }
        // inspect CAN frame 0x600
        if (message.id == 0x600)
        {
          // check for clutch switch
          if (message.data[6] & 0x1 << 2)
          {
            SET(LED2S);
            //IO_U.println("CLUTCH");
          }
          else RESET(LED2S);
        }
#endif // _LBC
      }
    }

#ifdef _DPF
    // Hmm... initially I thought DPF regen would be announced in a CAN frame somewhere.
    // This is not the case. So we have to query the ECU if we want to indicate this.
    // To add some fun to this: we need ISO 15765-4 for this...

    // DPF regen count: addresses 0x00029E + 0x00029D
    // send 0x7e0 : 10 08 A8 00 00 02 9E 00             this would be an initial frame (10)
    //                                                  of a multiframe message
    // receive 0x7e8 : 30 00 00 00 00 00 00 00          where we wait for a signal to go on
    //                                                  if we see something like 31 ...
    //                                                  or 32 ... we are asked to wait or abort
    // send 0x7e0 : 21 02 9D 00 00 00 00 00             this is a follow up frame (21)
    // receive 0x7e8 ...
    IO_U.println("Trying to receive DPF regen count");
    // the single frame way (msg.length < 9), rather clumsy
    tCAN tx_message;

    tx_message.id = 0x7e0;
    tx_message.header.rtr = 0;
    tx_message.header.length = 8;
    tx_message.data[0] = 0x05;
    tx_message.data[1] = 0xA8;
    tx_message.data[2] = 0x00;
    tx_message.data[3] = 0x00;
    tx_message.data[4] = 0x02;
    tx_message.data[5] = 0x9E;
    tx_message.data[6] = 0x00;
    tx_message.data[7] = 0x00;

    SET(LED1S);
    delay(200);
    RESET(LED1S);
    // we will wait for the reply for 250ms
    unsigned long timeout;
    timeout = millis();

    mcp2515_bit_modify(CANCTRL, (1 << REQOP2) | (1 << REQOP1) | (1 << REQOP0), 0);
    if (mcp2515_send_message(&tx_message))
    {
      IO_U.println("1st request sent");
    }
    else IO_U.println("Some error...");

    // reveive 0x7e8 ...
    // loop until we have what we want or timeout is reached
    bool MSG_RCV = false;
    while (!MSG_RCV)
    {
      if (mcp2515_check_message())
      {
        if (mcp2515_get_message(&message))
        {
          if (message.id == 0x7e8)
          {
            IO_U.println("1st answer received:");
            SET(LED2S);
            delay(200);
            RESET(LED2S);
            // log message
            // CAN id
            IO_U.print(message.id, HEX);
            IO_U.print("#");
            // CAN payload
            for (int i = 0; i < message.header.length; i++)
            {
              if (message.data[i] < 0x10) IO_U.print("0");
              IO_U.print(message.data[i], HEX);
              IO_U.print(" ");
            }
            // CRLF and flush
            IO_U.println("");
            IO_U.flush();
            MSG_RCV = true;
          }
          else MSG_RCV = false;
        }
      }
      if (millis() > timeout + 250)
      {
        MSG_RCV = true;
        SET(LED1S);
        SET(LED2S);
        delay(200);
        RESET(LED1S);
        RESET(LED2S);
        IO_U.println("1st reply timed out");
      }
    }
    tx_message.id = 0x7e0;
    tx_message.header.rtr = 0;
    tx_message.header.length = 8;
    tx_message.data[0] = 0x05;
    tx_message.data[1] = 0xA8;
    tx_message.data[2] = 0x00;
    tx_message.data[3] = 0x00;
    tx_message.data[4] = 0x02;
    tx_message.data[5] = 0x9D;
    tx_message.data[6] = 0x00;
    tx_message.data[7] = 0x00;

    SET(LED1S);
    delay(200);
    RESET(LED1S);
    // we will wait for the reply for 250ms
    timeout = millis();

    mcp2515_bit_modify(CANCTRL, (1 << REQOP2) | (1 << REQOP1) | (1 << REQOP0), 0);
    if (mcp2515_send_message(&tx_message))
    {
      IO_U.println("2nd request sent");
    }
    else IO_U.println("Some error...");

    // reveive 0x7e8 ...
    // loop until we have what we want or timeout is reached
    MSG_RCV = false;
    while (!MSG_RCV)
    {
      if (mcp2515_check_message())
      {
        if (mcp2515_get_message(&message))
        {
          if (message.id == 0x7e8)
          {
            IO_U.println("2nd answer received:");
            SET(LED2S);
            delay(200);
            RESET(LED2S);
            // log message
            // CAN id
            IO_U.print(message.id, HEX);
            IO_U.print("#");
            // CAN payload
            for (int i = 0; i < message.header.length; i++)
            {
              if (message.data[i] < 0x10) IO_U.print("0");
              IO_U.print(message.data[i], HEX);
              IO_U.print(" ");
            }
            // CRLF and flush
            IO_U.println("");
            IO_U.flush();
            MSG_RCV = true;
          }
          else MSG_RCV = false;
        }
      }
      if (millis() > timeout + 250)
      {
        MSG_RCV = true;
        IO_U.println("2nd reply timed out");
        SET(LED1S);
        SET(LED2S);
        delay(200);
        RESET(LED1S);
        RESET(LED2S);
      }
    }

    IO_U.println("Trying to check active DPF regen");
    // DPF regen active: address 0x0001CE
    // send 0x7e0 : 05 A8 00 00 01 CE 00 00
    //    tCAN tx_message;

    tx_message.id = 0x7e0;
    tx_message.header.rtr = 0;
    tx_message.header.length = 8;
    tx_message.data[0] = 0x05;
    tx_message.data[1] = 0xA8;
    tx_message.data[2] = 0x00;
    tx_message.data[3] = 0x00;
    tx_message.data[4] = 0x00;//01;  00 64 is the light switch (4th bit?), so we can actually change it
    tx_message.data[5] = 0x64;//CE;
    tx_message.data[6] = 0x00;
    tx_message.data[7] = 0x00;

    SET(LED1S);
    delay(200);
    RESET(LED1S);
    // we will wait for the reply for 250ms
    //    unsigned long timeout;
    timeout = millis();

    mcp2515_bit_modify(CANCTRL, (1 << REQOP2) | (1 << REQOP1) | (1 << REQOP0), 0);
    if (mcp2515_send_message(&tx_message))
    {
      IO_U.println("Request sent");
    }
    else IO_U.println("Some error...");

    // reveive 0x7e8 ...
    // loop until we have what we want or timeout is reached
    //    bool MSG_RCV = false;
    MSG_RCV = false;
    while (!MSG_RCV)
    {
      if (mcp2515_check_message())
      {
        if (mcp2515_get_message(&message))
        {
          if (message.id == 0x7e8)
          {
            IO_U.println("Answer received:");
            SET(LED2S);
            delay(200);
            RESET(LED2S);
            // log message
            // CAN id
            IO_U.print(message.id, HEX);
            IO_U.print("#");
            // CAN payload
            for (int i = 0; i < message.header.length; i++)
            {
              if (message.data[i] < 0x10) IO_U.print("0");
              IO_U.print(message.data[i], HEX);
              IO_U.print(" ");
            }
            // CRLF and flush
            IO_U.println("");
            IO_U.flush();
            MSG_RCV = true;
          }
          else MSG_RCV = false;
        }
      }
      if (millis() > timeout + 250)
      {
        MSG_RCV = true;
        IO_U.println("Reply timed out");
        SET(LED1S);
        SET(LED2S);
        delay(200);
        RESET(LED1S);
        RESET(LED2S);
      }
    }
    delay(5000);
#endif // _DPF

#ifdef _USD_IO
    // flush and close file
    dataFile.flush();
    dataFile.close();
  } // if(dataFile)
#endif // _USD_IO
}
