#include <Arduino.h>
#include "MIDI.h"
#include "Adafruit_TinyUSB.h"
#include "spi.h"
#include "picomod_def.h"

// Expression output digipot
MCP41 digipot;

// USB MIDI object
Adafruit_USBD_MIDI usb_midi;

// Create new instances of the Arduino MIDI Library,
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, usbMidi);

void setup()
{
	// Serial config
	Serial.begin(9600);

	// USB device descriptors
	USBDevice.setManufacturerDescriptor("Pirate MIDI");
	USBDevice.setProductDescriptor("Pico Mod");

	// Setup Digipot
	digipot.spi = &SPI;
	digipot.chip = Mcp416;
	digipot.csPin = DIGIPOT_CS;
	mcp41_Init(&digipot);
}

void loop()
{
  
}