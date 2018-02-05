/****************************************************************************
 * CAN Read Demo for the SparkFun CAN Bus Shield. 
 * Written by Stephen McCoy. 
 * Original tutorial available here: http://www.instructables.com/id/CAN-Bus-Sniffing-and-Broadcasting-with-Arduino
 * Used with permission 2016. License CC By SA. 
 * Distributed as-is; no warranty is given.
 *************************************************************************/

#include <Canbus.h>
#include <defaults.h>
#include <global.h>
#include <mcp2515.h>
#include <mcp2515_defs.h>
#include <SD.h>


//********************************Declare stuff******************************//

//Declare SD File
File dataFile;

//Initialize uSD pins
const int chipSelect = 9;

//********************************Setup Loop*********************************//

void setup() {
  Serial.begin(115200); // For debug use
  Serial.println("CAN Read - Testing receival of CAN Bus message");  
  delay(250);

  if(Canbus.init(CANSPEED_500))  //Initialise MCP2515 CAN controller at the specified speed
    Serial.println("CAN Init ok");
  else
    Serial.println("Can't init CAN");
  delay(250);

  //Check if uSD card initialized
  if (!SD.begin(chipSelect)) {
    Serial.println("uSD card failed to initialize, or is not present");
    return;
  }
  else{
    Serial.println("uSD card initialized.");
    delay(50);
  }   
  delay(250);

  File dataFile = SD.open("data.txt", FILE_WRITE); //Open uSD file to log data
  if (dataFile){
    dataFile.print("Init OK");
    dataFile.println();
    dataFile.flush();
    dataFile.close(); //Close data logging file
  }
}

//********************************Main Loop*********************************//

void loop(){

  tCAN message;

  File dataFile = SD.open("data.txt", FILE_WRITE); //Open uSD file to log data
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

      // Output received data to uSD console
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

  dataFile.flush();
  dataFile.close(); //Close data logging file

}






