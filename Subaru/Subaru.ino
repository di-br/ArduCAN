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

// Have 'I/O' to serial console (comment) or uSD (include _USD_IO)?
#define _USD_IO

// log CAN frames to uSD card?
//#define _LOG

// Flash LEDs for break/clutch pedal
//   LED1 -> break pedal
//   LED2 -> clutch pedal
//#define _LBC

// Flash LEDs when DPF regen is active
//     not implemented (yet?)
//   LED1 will blink upon sending a frame
//   LED2 will blink when receiving a frame
//   LED1+LED2 will blink on an error
#define _DPF

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

typedef union {
  uint16_t id;
  struct {
    uint8_t id_lo;
    uint8_t id_hi;
  };
} IDUnion;

//********************************setup loop*********************************//
// Fhe setup loop will use serial output for now, to be disabled later

void setup() {
  // for debug use
  Serial.begin(115200);
  delay(100);

  // initialise MCP2515 CAN controller at the specified speed
  if (mcp2515_init(CANSPEED_500))
    Serial.println(F("CAN Init: ok"));
  else
    Serial.println(F("CAN Init: failed"));
  delay(500);

#ifdef _USD_IO
  // check if uSD card initialised
  if (!SD.begin(chipSelect))
  {
    Serial.println(F("uSD card: failed to initialise or not present"));
    return;
  }
  else {
    Serial.println(F("uSD card: initialised"));
    delay(50);
  }
  delay(500);

  // open uSD file to start logging data, this will append to the file
  File dataFile = SD.open("can.log", FILE_WRITE);
  if (dataFile)
  {
    // log that we did initialise to separate different 'sessions'
    dataFile.print(F("Init OK"));
    dataFile.println();
    // Flush and close file
    dataFile.flush();
    dataFile.close();
  }
#endif // _USD_IO

  // finish by setting up LEDs
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);

  // accept all frames
  mcp2515_init_filters(true);

#if defined(_DPF) && ! (defined(_LBC) || defined(_LOG))
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
#ifdef _LOG
  unsigned long timeStamp;
#endif

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
#ifdef _LOG // log any CAN frame encountered
        timeStamp = millis();

        // Output received data to either Serial or uSD.
        //   Mimic the candump format here for easier replay.
        //   Start with timestamp and 'interface'
        IO_U.print(F("("));
        IO_U.print(timeStamp);
        IO_U.print(F(") arcan0 "));
        // CAN id
        if (message.id < 0x100) IO_U.print(F("0"));
        if (message.id < 0x10)  IO_U.print(F("0"));
        IO_U.print(message.id, HEX);
        IO_U.print(F("#"));
        // CAN payload
        for (int i = 0; i < message.header.length; i++)
        {
          if (message.data[i] < 0x10) IO_U.print(F("0"));
          IO_U.print(message.data[i], HEX);
          IO_U.print(F(" "));
        }
        // CRLF and flush
        IO_U.println(F(""));
        IO_U.flush();
#endif // _LOG

#ifdef _LBC // Flash LED for break/clutch pedal
        // inspect CAN frame 0x411
        if (message.id == 0x411)
        {
          // check for break switch
          if (message.data[6] & 0x1 << 4)
          {
            SET(LED1S);
            //IO_U.println(F("BREAK"));
          }
          else RESET(LED1S);
        }
        // inspect CAN frame 0x600
        if (message.id == 0x600)
        {
          // Check for clutch switch
          if (message.data[6] & 0x1 << 2)
          {
            SET(LED2S);
            //IO_U.println(F("CLUTCH"));
          }
          else RESET(LED2S);
        }
#endif // _LBC
      }
    }

#ifdef _DPF // check DPF related stats
    // Hmm... initially I thought DPF regen would be announced in a CAN frame somewhere.
    // This is not the case. So we have to query the ECU if we want to indicate this.
    // To add some fun to this: we need ISO 15765-4 for this...

    // A multi-address request would need a multiframe message:
    //   DPF regen count: addresses 0x00029E + 0x00029D
    //   send 0x7e0 : 10 08 A8 00 00 02 9E 00             This would be an initial frame (10)
    //                                                    of a multiframe message/
    //   receive 0x7e8 : 30 00 00 00 00 00 00 00          We then wait for a signal to go on.
    //                                                    If instead we see something like 31 ...
    //                                                    or 32 ... we are asked to wait or abort.
    //   send 0x7e0 : 21 02 9D 00 00 00 00 00             This is a follow up frame (21)
    //   receive 0x7e8 ...
    //
    // A single address request is easier since it's shorter, so...
    //
    IO_U.println(F("Trying to receive DPF regen count"));
    // The single frame way (msg.length < 9), rather clumsy

    uint8_t byte1;
    uint8_t byte2;
    if ( read_address(0x029D, &byte1) )
    {
      IO_U.print(byte1, HEX);
      IO_U.println(F(" 1st answer received"));
      if ( read_address(0x029E, &byte2) )
      {
        IO_U.print(byte2, HEX);
        IO_U.println(F(" 2nd answer received"));
        IO_U.print(F("DPF regen count: "));
        IO_U.println(byte1 * 256 + byte2);
      }
      else IO_U.println(F("Some error with 2nd query..."));
    }
    else IO_U.println(F("Some error with 1st query..."));
    delay(250);

    // Get milage since injector replacement
    //    The idea here is that we can guess the milage from that if we know the
    //    replacement milage. So check odometer and add an offset to obtain current ODO reading
    IO_U.println(F("Guessing odometer reading"));
    if ( read_address(0x0205, &byte1) )
    {
      IO_U.print(byte1, HEX);
      IO_U.println(F(" 1st answer received"));
      if ( read_address(0x0204, &byte2) )
      {
        IO_U.print(byte2, HEX);
        IO_U.println(F(" 2nd answer received"));
        IO_U.print((byte2 * 256 + byte1) * 5 + 229060); // 229060 being the offset after injector replacement
        IO_U.println(F("km"));
      }
      else IO_U.println(F("Some error with 2nd query..."));
    }
    else IO_U.println(F("Some error with 1st query..."));
    delay(250);

    // now get ash and soot ratios
    IO_U.println(F("Reading ash and soot ratios"));
    if ( read_address(0x0275, &byte1) )
    {
      IO_U.print(F("Ash ratio "));
      IO_U.print(byte1);
      IO_U.println(F("%"));
    }
    else IO_U.println(F("Some error with query..."));
    if ( read_address(0x027B, &byte1) )
    {
      IO_U.print(F("Soot accumulation ratio "));
      IO_U.print(byte1);
      IO_U.println(F("%"));
    }
    else IO_U.println(F("Some error with query..."));
    delay(250);

    // get running distance since last regen
    IO_U.println(F("Getting running distance since last regen"));
    if ( read_address(0x029C, &byte1) )
    {
      IO_U.print(byte1, HEX);
      IO_U.println(F(" 1st answer received"));
      if ( read_address(0x029B, &byte2) )
      {
        IO_U.print(byte2, HEX);
        IO_U.println(F(" 2nd answer received"));
        IO_U.print(F("Running distance since last regen: "));
        IO_U.println(byte2 * 256 + byte1);
      }
      else IO_U.println(F("Some error with 2nd query..."));
    }
    else IO_U.println(F("Some error with 1st query..."));
    delay(250);

    IO_U.println(F("Trying to check active DPF regen"));
    // DPF regen active: address 0x0001CE
    //                           0x000064 is the light switch (4th bit?),
    //                                    so we can actually change it for testing
    // send 0x7e0 : 05 A8 00 00 01 CE 00 00
    if ( read_address(0x01CE, &byte1) )
    {
      IO_U.print(F("answer received: "));
      IO_U.println(byte1, HEX);
      if ( (byte1 & (1 << 3)) != 0 ) // We look for 0000 1000 or 0000 0000
      { //                                               \_ the 4th bit
        IO_U.println(F("DPF regen in progress"));
        // Flash LEDs for 5 secs
        indicate(5);
      }
      else
      {
        IO_U.println(F("DPF regen not in progress"));
        // Wait for 5 secs
        delay(5000);
      }
    }

#endif // _DPF

#ifdef _USD_IO
    // Flush and close file
    dataFile.flush();
    dataFile.close();
  } // if(dataFile)
#endif // _USD_IO
}

// Query a single address from the ECU and return answer.
// This will do single frame queries and try to gracefully timeout.
bool read_address(uint16_t id, uint8_t *answer) {

  IDUnion tmpid;
  tCAN rtx_message;

  tmpid.id = id;
  rtx_message.id = 0x7e0;
  rtx_message.header.rtr = 0;
  rtx_message.header.length = 8; // always needs to be 8 bytes (SSM)
  rtx_message.data[0] = 0x05;
  rtx_message.data[1] = 0xA8;
  rtx_message.data[2] = 0x00;
  rtx_message.data[3] = 0x00;
  rtx_message.data[4] = tmpid.id_hi;
  rtx_message.data[5] = tmpid.id_lo;
  rtx_message.data[6] = 0x00;
  rtx_message.data[7] = 0x00;

  // Indicate transmit
  SET(LED1S);
  delay(200);

  mcp2515_bit_modify(CANCTRL, (1 << REQOP2) | (1 << REQOP1) | (1 << REQOP0), 0);
  if (!mcp2515_send_message(&rtx_message))
  {
    // We could not find a free buffer (we have 3), so wait a second and retry
    delay(1000);
    if (!mcp2515_send_message(&rtx_message))
    {
      // We could again not find a free buffer, assume something went wrong
      // so do a soft reset and restart Arduino almost from scratch
      delay(1000);
      software_reset();
    }
  }
  RESET(LED1S);

  // we will wait for the reply for 250ms.
  unsigned long timeout;
  timeout = millis();

  // loop until we have an answer or timeout is reached.
  bool MSG_RCV = false;
  while (!MSG_RCV && (millis() < timeout + 250) )
  {
    if (mcp2515_check_message())
    {
      if (mcp2515_get_message(&rtx_message))
      {
        if (rtx_message.id == 0x7e8) // anything else would be a surprise anyway, see filters in setup()
        {
          // indicate receive
          SET(LED2S);
          delay(200);
          RESET(LED2S);
          // check CAN payload
          if ( rtx_message.data[0] == 0x02 ) // we expect two bytes as an answer (SSM)
          {
            if ( rtx_message.data[1] == 0xE8 ) // answer should start with E8 (SSM)
            {
              // we have what seems to be a valid answer, return in
              *answer = rtx_message.data[2];
              return true;
            }
            else
            {
              Serial.println(rtx_message.data[1], HEX);
            }
          }
          else
          {
            Serial.println(rtx_message.data[0], HEX);
          }
        }
        MSG_RCV = true; // we received our answer anyway, although might be erroneous
      }
      else MSG_RCV = false; // continue waiting for a message
    }
  }
  // Indicate error since we did not return a valid answer
  {
    SET(LED1S);
    SET(LED2S);
    delay(250);
    RESET(LED1S);
    RESET(LED2S);
  }

  // return 'null value'
  return false;
}

// Flash LED1 and LED2 in turn to indicate something
void indicate(int num)
{
  for (int c = 0; c < num; c++)
  {
    SET(LED1S);
    delay(500);
    RESET(LED1S);
    SET(LED2S);
    delay(500);
    RESET(LED2S);
  }
}

// Restarts program from beginning but does not reset the peripherals and registers
void software_reset()
{
  asm volatile ("  jmp 0");
}
