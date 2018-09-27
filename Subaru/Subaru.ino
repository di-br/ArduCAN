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


   SSM protocol and CAN ID's taken from https://subdiesel.wordpress.com/

   All of the CAN communication via MCP2515 is done w/o using interrupts. This
   means timing may be a crucial factor for the code to work.

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

#if (defined(_DPF) && ! (defined(_LBC) || defined(_LOG)))
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

#if (defined(_LOG) || defined (_LBC))
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
        for (uint8_t i = 0; i < message.header.length; i++)
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
#endif

#ifdef _DPF // check DPF related stats
    // Hmm... initially I thought DPF regen would be announced in a CAN frame somewhere.
    // This is not the case. So we have to query the ECU if we want to indicate this.
    // To add some fun to this: we need ISO 15765-4 for this...

    // A multi-address request would need a multi-frame message:
    //   DPF regen count: addresses 0x00029E + 0x00029D
    //   send 0x7e0 : 10 08 A8 00 00 02 9E 00             This would be an initial frame (10)
    //                                                    of a multi-frame message w/ 8 bytes payload (2nd byte)
    //   receive 0x7e8 : 30 00 00 00 00 00 00 00          We then wait for a signal to go on.
    //                                                    If instead we see something like 31 ...
    //                                                    or 32 ... we are asked to wait or abort.
    //   send 0x7e0 : 21 02 9D 00 00 00 00 00             This is a follow up frame (21) with only payload
    //   receive 0x7e8 ...
    //
    // A single address request is easier since it's shorter, so...
    //
    IO_U.println(F("Trying to receive DPF regen count"));
    // The single-frame way (msg.length < 8), rather clumsy

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

    // test ISO-TP send/recv w/ data for ECU
    uint8_t longq[16];
    longq[0] = 0x05; // 5 bytes will follow
    longq[1] = 0xA8; // SSM read mem address
    longq[2] = 0x00; // required
    longq[3] = 0x00;
    longq[4] = 0x02;
    longq[5] = 0x75; // address 0x000275

    if (iso_tp_send(longq, 6))
    {
      uint8_t bts;
      bts = 16;
      if (iso_tp_recv(longq, &bts))
      {
        IO_U.println(F("successfully read ISO-TP frame"));
        IO_U.print(F("received "));
        IO_U.print(bts);
        IO_U.print(F("bytes"));
        IO_U.println(longq[0], HEX);
        IO_U.println(longq[1], HEX);
        IO_U.println(longq[2], HEX);
      }
    }
    longq[0] = 0x08; // 8 bytes will follow
    longq[1] = 0xA8; // SSM read mem address
    longq[2] = 0x00; // required
    longq[3] = 0x00;
    longq[4] = 0x02;
    longq[5] = 0x75; // address 0x000275 -> ash
    longq[6] = 0x00;
    longq[7] = 0x02;
    longq[8] = 0x7B; // address 0x00027B -> soot

    if (iso_tp_send(longq, 9))
    {
      uint8_t bts;
      bts = 16;
      if (iso_tp_recv(longq, &bts))
      {
        IO_U.println(F("successfully read ISO-TP frame"));
        IO_U.print(F("received "));
        IO_U.print(bts);
        IO_U.print(F("bytes"));
        IO_U.println(longq[0], HEX);
        IO_U.println(longq[1], HEX);
        IO_U.println(longq[2], HEX);
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
// This will do single-frame queries and try to gracefully timeout.
bool read_address(uint16_t id, uint8_t *answer) {

  IDUnion tmpid;
  tCAN rtx_message;

  tmpid.id = id;
  rtx_message.id = 0x7e0;
  rtx_message.header.rtr = 0;
  rtx_message.header.length = 8;     // CAN message always needs to be 8 bytes (SSM), so pad with 0's till the end
  rtx_message.data[0] = 0x05;        // 5 bytes actual payload (the first 4 bits make up 0 for a single frame)
  rtx_message.data[1] = 0xA8;        // A8 = request mem addr
  rtx_message.data[2] = 0x00;        // required to be 0x00
  rtx_message.data[3] = 0x00;        // 1st byte of addr
  rtx_message.data[4] = tmpid.id_hi; // 2nd byte of addr
  rtx_message.data[5] = tmpid.id_lo; // 3rd byte of addr
  rtx_message.data[6] = 0x00;        // 0x00 to pad to 8 bytes
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


// send an ISO-TP formatted frame to the ECU
// we do not consider any timeouts or wait periods here
bool iso_tp_send(uint8_t *message, uint8_t len)
{
  tCAN rtx_message; // CAN frame buffer
  uint8_t count;    // count the bytes we've sent already

  // single- or multi-frame?
  if (len < 8)
  {
    // single-frame

    // indicate transmit
    SET(LED1S);
    delay(200);

    rtx_message.id = 0x7e0;                             // send to ECU
    rtx_message.header.rtr = 0;
    rtx_message.header.length = 8;                      // CAN message always needs to be 8 bytes (SSM), so pad with 0's till the end
    for (count = 0; count < len; count++)
    {
      rtx_message.data[count] = message[count];         // fill in message bytes
                                                        // number of bytes to follow (the first 4 bits make up 0 for a single frame)
                                                        // needs to be byte 0 from input message
    }
    memset(&rtx_message.data[count], 0, 8 - count); // pad rest of the frame w/ 0's

    mcp2515_bit_modify(CANCTRL, (1 << REQOP2) | (1 << REQOP1) | (1 << REQOP0), 0);
    if (!mcp2515_send_message(&rtx_message))
    {
      // We could not find a free buffer (we have 3), so wait a half a second then return
      delay(500);
      return false;
    }
    // switch LED off and return w/ success
    RESET(LED1S);
    return true;
  }
  else
  {
    // multi-frame

    // indicate transmit
    SET(LED1S);
    delay(200);

    rtx_message.id = 0x7e0;                         // send to ECU
    rtx_message.header.rtr = 0;
    rtx_message.header.length = 8;                  // CAN message always needs to be 8 bytes (SSM), so pad with 0's till the end
    rtx_message.data[0] = 0x10;                     // first frame of multi-frame message
    rtx_message.data[1] = len;                      // number of bytes to follow for all frames combined
    for (count = 0; count < 6; count++)
    {
      rtx_message.data[count + 2] = message[count]; // fill in message bytes
    } // no padding necessary...

    mcp2515_bit_modify(CANCTRL, (1 << REQOP2) | (1 << REQOP1) | (1 << REQOP0), 0);
    if (!mcp2515_send_message(&rtx_message))
    {
      // We could not find a free buffer (we have 3), so wait a half a second then return
      delay(500);
      return false;
    }

    // now wait for reply from ECU to know we can continue
    bool MSG_RCV = false;
    while (!MSG_RCV)
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
            if ( rtx_message.data[0] == 0x30 ) // we expect a 'continue transmission frame', bail otherwise
            {
              if ( rtx_message.data[1] == 0x00 )
              {
                if ( rtx_message.data[2] == 0x00 )
                {
                  MSG_RCV = true;
                }
                else return false;
              }
              else return false;
            }
            else return false; // some other reply, so we're screwed
          }
        }
      }
    }

    // got it, send remaining frames
    for (uint8_t fcount = 0; fcount < ((len - 6) / 7 + ((len - 6) % 7 != 0)); fcount++)
    {
      rtx_message.id = 0x7e0;                  // send to ECU
      rtx_message.header.rtr = 0;
      rtx_message.header.length = 8;           // CAN message always needs to be 8 bytes (SSM), so pad with 0's till the end
      rtx_message.data[0] = 0x20 + fcount + 1; // consecutive frames
      rtx_message.data[1] = len;               // number of bytes to follow for all frames combined
      uint8_t i = 1;
      for (count; count < (len < 7 + fcount * 7 + 6 ? len : 7 + fcount * 7 + 6); count++)
      {
        rtx_message.data[i] = message[count];  // fill in message bytes
        i++;
      }
      memset(&rtx_message.data[i], 0, 8 - i);  // pad rest of the frame w/ 0's

      mcp2515_bit_modify(CANCTRL, (1 << REQOP2) | (1 << REQOP1) | (1 << REQOP0), 0);
      if (!mcp2515_send_message(&rtx_message))
      {
        // We could not find a free buffer (we have 3), so wait a half a second then return
        delay(500);
        return false;
      }
    }

    // switch LED off and return w/ success
    RESET(LED1S);
    return true;
  }
}

// receive an ISO-TP formatted frame from the ECU
// data needs to be large enough to hold all received data...
bool iso_tp_recv(uint8_t *data, uint8_t *len)
{
  tCAN rtx_message; // CAN frame buffer
  uint8_t count;    // count the bytes we've received already

  // we expect a message from the ECU, so wait/check for it
  bool MSG_RCV = false;
  while (!MSG_RCV)
  {
    if (mcp2515_check_message())
    {
      if (mcp2515_get_message(&rtx_message))
      {
        // DBG: display message
        // CAN id
        if (rtx_message.id < 0x100) IO_U.print(F("0"));
        if (rtx_message.id < 0x10)  IO_U.print(F("0"));
        IO_U.print(rtx_message.id, HEX);
        IO_U.print(F("#"));
        // CAN payload
        for (uint8_t i = 0; i < rtx_message.header.length; i++)
        {
          if (rtx_message.data[i] < 0x10) IO_U.print(F("0"));
          IO_U.print(rtx_message.data[i], HEX);
          IO_U.print(F(" "));
        }
        // CRLF and flush
        IO_U.println(F(""));
        IO_U.flush();

        if (rtx_message.id == 0x7e8) // anything else would be a surprise anyway, see filters in setup()
        {
          // indicate receive
          SET(LED2S);

          // check CAN payload to tell single- and multi-frame messages apart

          // Up to 7 bytes returned from the ECU will be within a single-frame message.
          // (We need 1 byte to indicate message type). The 7 bytes will have to include
          // E8 to signal the answer to A8, so an actual payload of 6 bytes should be
          // possible before we need multi-frame receives?
          if ( rtx_message.data[0] == 0x10 ) // a multi-frame message
          {
            // send abort and implement later
            rtx_message.id = 0x7e0;
            rtx_message.header.rtr = 0;
            rtx_message.header.length = 8;     // ISO-TP message w/ 3 bytes, SSM wants 8?
            rtx_message.data[0] = 0x32;        // this should signal an abort?
            rtx_message.data[1] = 0x00;        // 0x00 to pad to 8 bytes
            rtx_message.data[2] = 0x00;
            rtx_message.data[3] = 0x00;
            rtx_message.data[4] = 0x00;
            rtx_message.data[5] = 0x00;
            rtx_message.data[6] = 0x00;
            rtx_message.data[7] = 0x00;

            // Indicate transmit
            SET(LED1S);

            mcp2515_bit_modify(CANCTRL, (1 << REQOP2) | (1 << REQOP1) | (1 << REQOP0), 0);
            mcp2515_send_message(&rtx_message);

            delay(200);
            RESET(LED1S);
            RESET(LED2S);
            return false;

          }
          else // we have a single frame message (or something different alltogether)
          {
            // get number of bytes to expect
            count = rtx_message.data[0] - 1; // -1 since we expect the E8
            if ( count > *len) return false; // make sure we have enough space
            *len = count;
            IO_U.println(count);
            if (rtx_message.data[1] == 0xE8) // answer should start with E8 (SSM)
            {
              uint8_t i = 0;
              for ( count; count > 0; count--)
              {
                IO_U.println(i);
                IO_U.println(rtx_message.data[i + 2], HEX);
                data[i] = rtx_message.data[i + 2];
                i++;
              }
              delay(200);
              RESET(LED2S);
              return true;
            }
          } // frame type
          return false;
        } // is correct message
        return false;
      } // get message
      return false;
      MSG_RCV = true;
    } // check msg
  }
  return false;
}

// Flash LED1 and LED2 in turn to indicate something
void indicate(uint8_t num)
{
  for (uint8_t c = 0; c < num; c++)
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
