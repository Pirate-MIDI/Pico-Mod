#include "mcp41xx.h"

// -------- Register Addresses -------- //
#define VOL_W0       0x00
#define VOL_W1       0x01
#define NON_VOL_W0   0x02
#define NON_VOL_W1   0x03
#define TCON         0x04
#define STAT         0x05


// ------------- Commands ------------- //
// Operates on volatile and non-volatile memory. 16-bit command packet
#define READ_DATA    0x11
#define WRITE_DATA   0x00
// Operates on volatile memory only. 8-bit command packet
#define INCREMENT    0x01
#define DECREMENT    0x10


// ----------- Register Bits ---------- //
// STAT (Status)
#define EEWA   4
#define WL1    3
#define WL0    2
#define SHDN   1
#define WP     0

// TCON (Terminal Control)
#define R1H1   7
#define R1A    6
#define R1W    5
#define R1B    4
#define R0HW   3
#define R0A    2
#define R0W    1
#define R0B    0

// By default, SPI bus speed is 1MHz
SPISettings spiSettings(1000000, MSBFIRST, SPI_MODE0);

void mcp41_Init(MCP41* mcp41)
{
	mcp41->spi->begin();
}



void mcp41_Write(MCP41* mcp41, uint16_t data)
{
	// valid data between 0-256 (extra bit for full scale with 257 steps
	if(data > 256)
	{
		return;
	}
	// Construct the data packet
	uint16_t tx16 = (VOL_W0 << 12) | (WRITE_DATA << 10) | (data & 0x1FF);
   mcp41->spi->beginTransaction(spiSettings);
	digitalWrite(mcp41->csPin, LOW);
	mcp41->spi->transfer16(tx16);
	digitalWrite(mcp41->csPin, HIGH);
	mcp41->spi->endTransaction();
}

void mcp41_Increment(MCP41* mcp41)
{
	// Construct the data packet
	uint8_t tx8 = (VOL_W0 << 4) | (INCREMENT << 2);
   mcp41->spi->beginTransaction(spiSettings);
	digitalWrite(mcp41->csPin, LOW);
	mcp41->spi->transfer(tx8);
	digitalWrite(mcp41->csPin, HIGH);
	mcp41->spi->endTransaction();
}

// 
void mcp41_Decrement(MCP41* mcp41)
{
	// Construct the data packet
	uint8_t tx8 = (VOL_W0 << 4) | (DECREMENT << 2);
   mcp41->spi->beginTransaction(spiSettings);
	digitalWrite(mcp41->csPin, LOW);
	mcp41->spi->transfer(tx8);
	digitalWrite(mcp41->csPin, HIGH);
	mcp41->spi->endTransaction();
}