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
#include <Canbus.h>
#include <defaults.h>
#include <global.h>
#include <mcp2515.h>
#include <mcp2515_defs.h>
#include <SD.h>

//********************************declarations*******************************//

// declare SD File
File dataFile;

// initialize uSD pins
const int chipSelect = 9;

//********************************setup loop*********************************//
// the setup loop will use serial output for now, to be disabled later

void setup() {
  // for debug use
  Serial.begin(115200);
  Serial.println("CAN Read - Testing receival of CAN Bus message");
  delay(250);

  // initialise MCP2515 CAN controller at the specified speed
  if(Canbus.init(CANSPEED_500))
    Serial.println("CAN Init ok");
  else
    Serial.println("Can't init CAN");
  delay(250);

  // check if uSD card initialised
  if (!SD.begin(chipSelect)) {
    Serial.println("uSD card failed to initialize, or is not present");
    return;
  }
  else{
    Serial.println("uSD card initialized.");
    delay(50);
  }   
  delay(250);

  // open uSD file to start logging data, this will append to the file
  File dataFile = SD.open("data.txt", FILE_WRITE);
  if (dataFile){
    // log that we did initialise to separate different 'sessions'
    dataFile.print("Init OK");
    dataFile.println();
    // flush and close file
    dataFile.flush();
    dataFile.close();
  }
}

//********************************main loop*********************************//
// the main loop will either use serial ouput or log to uSD

void loop(){

  tCAN message;

  // open uSD file to log data
  File dataFile = SD.open("data.txt", FILE_WRITE);
  unsigned long timeStamp;

  if (mcp2515_check_message())
  {
    if (mcp2515_get_message(&message))
    {
      timeStamp = millis();
      //if(message.id == 0x620 and message.data[2] == 0xFF)  //uncomment when you want to filter
      //{

      /* uncomment for serial output
       *
       * // Output received data to serial console
       * Serial.print("ID: ");
       * Serial.print(message.id,HEX);
       * Serial.print(", ");
       * Serial.print("Data: ");
       * Serial.print(message.header.length,DEC);
       * for(int i=0;i<message.header.length;i++) 
       * {	
       *   Serial.print(message.data[i],HEX);
       *   Serial.print(" ");
       * }
       * Serial.println("");
       *
       */

      // output received data to uSD
      // we mimik the candump format here for easier replay
      dataFile.print("(");
      dataFile.print(timeStamp);
      dataFile.print(") ");

      dataFile.print("arcan0 ");
      if (message.id<0x100) dataFile.print("0");
      if (message.id<0x10)  dataFile.print("0");
      dataFile.print(message.id,HEX);
      dataFile.print("#");
      dataFile.print(message.header.length,DEC);
      for(int i=0;i<message.header.length;i++) 
      {	
        if (message.data[i]<0x10) dataFile.print("0");
        dataFile.print(message.data[i],HEX);
      }
      dataFile.println("");
      dataFile.flush();

      //}

    }
  }

  // flush and close file
  dataFile.flush();
  dataFile.close();

}
