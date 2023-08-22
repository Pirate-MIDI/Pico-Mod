#ifndef MCP41XX_H_
#define MCP41XX_H_

#include "Arduino.h"
#include "SPI.h"

typedef enum
{
   Mcp413,
   Mcp414,
   Mcp415,
   Mcp416
} Mcp41_Chip;

typedef enum
{
   Mcp41Ok,
   Mcp41Error
} Mcp41_Status;

typedef struct
{
   HardwareSPI* spi;       // Arduino hardware SPI peripheral instance
   int csPin;              // Chip select pin
   Mcp41_Chip chip;        // MCP41xx chipset. x1 (potentiometer) and x2 variants (rheostat) are handled identically
   Mcp41_Status status;    // Device status
} MCP41;

void mcp41_Init(MCP41* mcp41);
void mcp41_Write(MCP41* mcp41, uint16_t data);
void mcp41_Increment(MCP41* mcp41);
void mcp41_Decrement(MCP41* mcp41);

#endif /* MCP41XX_H_ */