#include <Arduino.h>
#include "picomod.h"

void setup()
{
	picoMod_Init();
}

void loop()
{
	// If new serial data is available
	if(Serial.available())
	{
		// Read the new data
		// Caution! This will overwirte any existing data in the buffer
		uint16_t counter = 0;
		while(Serial.available())
		{
			serialRxBuffer[counter] = Serial.read();
			counter++;
		}
		// Once all data has been read, call the picomod handler
		picoMod_SerialRx(counter);
	}
}