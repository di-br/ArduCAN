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

  // accept no frames unless filtered
  mcp2515_init_filters(false);
  mcp2515_set_mask_or_filter(MASK0, 0x7F0);
  mcp2515_set_mask_or_filter(FILTER0, 0x7E8);
  mcp2515_set_mask_or_filter(MASK1, 0x7F0);
  mcp2515_set_mask_or_filter(FILTER2, 0x7E8);
}

//********************************main loop*********************************//
// the main loop will either use serial ouput or log to uSD

void loop() {

  tCAN message;

#ifdef _USD_IO
  // open uSD file to log data
  File dataFile = SD.open("can.log", FILE_WRITE);
  if (dataFile)
  {
#endif // _USD_IO

    // check DPF related stats

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
    IO_U.println(F("Trying to query parameters from ECU"));

    // test ISO-TP send/recv w/ data for ECU
    uint8_t longq[16];
    longq[0] = 0xA8; // SSM read mem address
    longq[1] = 0x00; // required
    longq[2] = 0x00;
    longq[3] = 0x02;
    longq[4] = 0x75; // address 0x000275

    if (iso_tp_send(longq, 5))
    {
      uint8_t bts;
      bts = 16;
      if (iso_tp_recv(longq, &bts))
      {
        IO_U.println(F("successfully read ISO-TP frame"));
        IO_U.print(F("received "));
        IO_U.print(bts);
        IO_U.print(F(" bytes"));
        IO_U.println(longq[0], HEX);
        IO_U.println(longq[1], HEX);
        IO_U.println(longq[2], HEX);
      }
    }
    longq[0] = 0xA8; // SSM read mem address
    longq[1] = 0x00; // required
    longq[2] = 0x00;
    longq[3] = 0x02;
    longq[4] = 0x75; // address 0x000275 -> ash
    longq[5] = 0x00;
    longq[6] = 0x02;
    longq[7] = 0x7B; // address 0x00027B -> soot

    if (iso_tp_send(longq, 8))
    {
      uint8_t bts;
      bts = 16;
      if (iso_tp_recv(longq, &bts))
      {
        IO_U.println(F("successfully read ISO-TP frame"));
        IO_U.print(F("received "));
        IO_U.print(bts);
        IO_U.print(F(" bytes"));
        IO_U.println(longq[0], HEX);
        IO_U.println(longq[1], HEX);
        IO_U.println(longq[2], HEX);
      }
    }

#ifdef _USD_IO
    // Flush and close file
    dataFile.flush();
    dataFile.close();
  } // if(dataFile)
#endif // _USD_IO
}

// send an ISO-TP formatted frame to the ECU
// we do not consider any timeouts or wait periods here
bool iso_tp_send(uint8_t *message, uint8_t len)
{
  tCAN rtx_message; // CAN frame buffer
  uint8_t count;    // count the bytes we've sent already

  // single- or multi-frame?
  if (len < 8) // first byte will be length of message...
  {
    // single-frame

    // indicate transmit
    SET(LED1S);
    delay(200);

    rtx_message.id = 0x7e0;                             // send to ECU
    rtx_message.header.rtr = 0;
    rtx_message.header.length = 8;                      // CAN message always needs to be 8 bytes (SSM), so pad with 0's till the end
    memset(&rtx_message.data[0], 0, 8);                 // pad rest of the frame w/ 0's
    rtx_message.data[0] = len;                          //   number of bytes to follow (the first 4 bits make up 0 for a single frame)
    for (count = 1; count < len; count++)
    {
      rtx_message.data[count] = message[count - 1];     // fill in message bytes
    }

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
      memset(&rtx_message.data[0], 0, 8);      // pad rest of the frame w/ 0's
      rtx_message.data[0] = 0x20 + fcount + 1; // consecutive frames
      uint8_t i = 1;
      for (count; count < (len < 7 + fcount * 7 + 6 ? len : 7 + fcount * 7 + 6); count++)
      {
        rtx_message.data[i] = message[count];  // fill in message bytes
        i++;
      }

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
