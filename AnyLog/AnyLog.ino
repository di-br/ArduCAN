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
//#define _USD_IO <-- well, this kind of works, but is _way_ too slow
//#define DBG

//********************************headers************************************//

#include "defaults.h"
#include <SPI.h>
#include "MCP2515.h"

#ifdef _USD_IO
#include <SD.h>
#endif

//********************************fiddle*************************************//
typedef struct
{
  uint8_t pad;        // pad to make it 16 bytes
  uint32_t timestamp; // timestamp of frame
  uint16_t id;        // SID, we ignore EID for now...
  uint8_t length;     // number of data bytes
  uint64_t data;      // 64 bits - lots of ways to access it.
} CompactFrame;

//********************************declarations / global variables ***********//

// switch between serial and uSD I/O for the main loop
#ifdef _USD_IO
// declare SD File
File dataFile;

// how many CAN frames do we attempt to buffer?
#define MAX_FRAMES 8
CompactFrame frame_buffer[MAX_FRAMES];
int frame_count = 0;
#endif

// CAN message frame (actually just the parts that are exposed by the MCP2515 RX/TX buffers)
Frame message;

// Create CAN object with pins as defined
MCP2515 CAN(MCP_CS_PIN, MCP_INT_PIN);

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
    return;
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
  if (!SD.begin(SPI_FULL_SPEED, SD_CS_PIN))
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


  attachInterrupt(digitalPinToInterrupt(MCP_INT_PIN), CANHandler, FALLING);
  // This code would only accept frames with ID of 0x510 - 0x51F. All other frames
  // will be ignored.
  //CAN.InitFilters(false);
  //CAN.SetRXMask(MASK0, 0x7F0, 0); // match all but bottom four bits
  //CAN.SetRXFilter(FILTER0, 0x511, 0); // allows 0x510 - 0x51F

  Serial.println("Ready ...");
}

//********************************main loop*********************************//
// the main loop will either use serial ouput or log to uSD

void loop() {

  if (CAN.GetRXFrame(message)) {
    SET(LED2S);

#ifdef _USD_IO
#ifdef DBG
    Serial.print("buffering...");
#endif
    // put message into our 'frame buffer'
    frame_buffer[frame_count].pad = 0x66; /* this is a dummy to better identify
                                             in the binary file (I have yet to see
                                             0x66 in a CAN stream)
*/
    frame_buffer[frame_count].timestamp = millis();
    frame_buffer[frame_count].id = message.id; // the latter is 4 bytes, but we
    // overwrite this w/ length and data?
    frame_buffer[frame_count].length = message.length;
    frame_buffer[frame_count].data = message.data.value;
    frame_count++;
#ifdef DBG
    Serial.println("done");
#endif
#else
    Serial.print("ID: ");
    if (message.id < 0x100) Serial.print("0");
    if (message.id < 0x10) Serial.print("0");
    Serial.print(message.id, HEX);
    Serial.print(" | Extended: ");
    if (message.extended) {
      Serial.print("Yes");
    } else {
      Serial.print("No");
    }
    Serial.print(" | Length: ");
    Serial.print(message.length, DEC);
    Serial.print(" || ");
    for (int i = 0; i < message.length; i++) {
      if (message.data.byte[i] < 0x10) Serial.print("0");
      Serial.print(message.data.byte[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
#endif
    RESET(LED2S);
  }

#ifdef _USD_IO
  // do we need to 'flush the frame_buffer'?
  if (frame_count == MAX_FRAMES) {

    cli(); // disable interrupts, this seems key to leave write operations to uSD doing their work

    // open uSD file to log data
    File dataFile = SD.open("anycan.raw", FILE_WRITE);
    if (dataFile)
    {
      dataFile.write((const uint8_t *) frame_buffer, sizeof(CompactFrame) * MAX_FRAMES);
#ifdef DBG
      Serial.println("buffer written");
#endif
      
      // flush and close file
      dataFile.flush();
      dataFile.close();

    } // if(dataFile)
    else {
      RESET(LED1S);
    }

    frame_count = 0; /* we re-use the buffer no matter what
                        so also in case we could not write to uSD
                        */

    sei(); // enable interrupts, listen for MCP again?

  }
#endif // _USD_IO
}

