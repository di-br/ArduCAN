#ifndef	DEFAULTS_H
#define	DEFAULTS_H

#define	P_MOSI          B,3          // digital pin 11   <- translates to
#define	P_MISO          B,4          // digital pin 12
#define	P_SCK           B,5          // digital pin 13

//#define MCP2515_CS      D,3 // Rev A
#define MCP2515_CS      B,2 // Rev B // digital pin 10
#define MCP2515_INT     D,2          // digital pin 2
#define LED2_HIGH       B,0          // digital pin 8
#define LED2_LOW        B,0          //

#define CANSPEED_125    7               // CAN speed at 125 kbps
#define CANSPEED_250    3               // CAN speed at 250 kbps
#define CANSPEED_500    1               // CAN speed at 500 kbps

#endif	// DEFAULTS_H
