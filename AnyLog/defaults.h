#ifndef	GLOBAL_H
#define	GLOBAL_H
// ----------------------------------------------------------------------------

// Pin definition for uSD card
#define SD_CS_PIN      9
#define SD_CS_FAST_PIN B,1

// Pin definitions specific to how the MCP2515 is wired up.
#define MCP_CS_PIN       10  // B,2
#define MCP_CS_FAST_PIN  B,2
#define MCP_INT_PIN      2   // D,2
#define MCP_INT_FAST_PIN D,2

// LEDs on the CAN shield
#define LED1             7   // green  PORTD 7
#define LED1S            D,7 // and a short-cut for speedier changes
#define LED2             8   // green  PORTB 0
#define LED2S            B,0

#define	RESET(x)        _XRS(x)
#define	SET(x)          _XS(x)
#define	TOGGLE(x)       _XT(x)
#define	SET_OUTPUT(x)   _XSO(x)
#define	SET_INPUT(x)    _XSI(x)
#define	IS_SET(x)       _XR(x)

#define	PORT(x)         _port2(x)
#define	DDR(x)          _ddr2(x)
#define	PIN(x)          _pin2(x)

#define	_XRS(x,y)       PORT(x) &= ~(1<<y)
#define	_XS(x,y)        PORT(x) |= (1<<y)
#define	_XT(x,y)        PORT(x) ^= (1<<y)

#define	_XSO(x,y)       DDR(x) |= (1<<y)
#define	_XSI(x,y)       DDR(x) &= ~(1<<y)

#define	_XR(x,y)        ((PIN(x) & (1<<y)) != 0)

#define	_port2(x)       PORT ## x
#define	_ddr2(x)        DDR ## x
#define	_pin2(x)        PIN ## x

#endif	// GLOBAL_H
