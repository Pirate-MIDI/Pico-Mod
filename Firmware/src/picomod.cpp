#include "picomod.h"
#include "string.h"

// USB MIDI object
Adafruit_USBD_MIDI usb_midi;

// Create new instances of the Arduino MIDI Library,
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, usbMidi);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, trsMidi);

// Switch inputs
Button buttons[NUM_SWITCHES];

// Expression output digipot
MCP41 digipot;

// Settings and Presets
GlobalConfig globalConfig;
Preset preset;

// LEDs
Adafruit_NeoPixel leds(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Private Function Prototypes
void picoMod_Boot();
void picoMod_NewDevice();
void picoMod_PrintSystem();
void getFlashUid(char* str);
void softwareReset();

void switch1ISR();
void switch2ISR();
void switch1Handler(ButtonState state);
void switch2Handler(ButtonState state);
void genSwitchHandler(uint8_t index, ButtonState state);
void processAction(Action* action);
void processMidiActionEvent(ActionEvent* event);
void processExpActionEvent(ActionEvent* event);
void processOutputActionEvent(ActionEvent* event);
void processLedActionEvent(ActionEvent* event);


//-------------------- Global Functions --------------------//
void picoMod_Init()
{
	// GPIO config
	pinMode(SWITCH1_PIN, INPUT_PULLUP);
	pinMode(SWITCH2_PIN, INPUT_PULLUP);

	attachInterrupt(digitalPinToInterrupt(SWITCH1_PIN), switch1ISR, CHANGE);
	attachInterrupt(digitalPinToInterrupt(SWITCH2_PIN), switch2ISR, CHANGE);

	pinMode(BYPASS_RELAY_PIN, OUTPUT);
	pinMode(AUX_RELAY_PIN, OUTPUT);
	pinMode(SWITCH_OUT_PIN, OUTPUT);

	digitalWrite(BYPASS_RELAY_PIN, LOW);
	digitalWrite(AUX_RELAY_PIN, LOW);
	digitalWrite(SWITCH_OUT_PIN, LOW);

	// Switch inputs
	buttons[0].mode = Momentary;
	buttons[0].logicMode = ActiveLow;
	buttons[0].handler = switch1Handler;
	buttons[0].pin = SWITCH1_PIN;
	buttons[1].mode = Momentary;
	buttons[1].logicMode = ActiveLow;
	buttons[1].handler = switch2Handler;
	buttons[1].pin = SWITCH2_PIN;
	buttons_Init(&buttons[0]);
	buttons_Init(&buttons[1]);

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

	// EEPROM (flash emulation) init with 256 * 256byte sectors assigned
	EEPROM.begin(256*256);

	// Read the global config and check if new device
	EEPROM.get(0, globalConfig);
	
	if (globalConfig.bootState == DEVICE_CONFIGURED_VALUE)
	{
		// Device has aleady been configured
		picoMod_Boot();
	}
	else
	{
		// New device requiring setup and default config
		picoMod_NewDevice();
	}
	delay(5000);
	


	// Begin MIDI listening
	trsMidi.begin(globalConfig.midiChannel);
	usbMidi.begin(globalConfig.midiChannel); 
}

// These functions are wrappers for the onboard GPIO.
// A slight performance overhead is incurred due to the additional function call,
// however, for an inexperienced embedded programmer, they may be easier to use.
void relayBypassOn()
{
	digitalWrite(BYPASS_RELAY_PIN, HIGH);
}

void relayBypassOff()
{
	digitalWrite(BYPASS_RELAY_PIN, LOW);
}

void relayBypassToggle()
{

}

void relayAuxOn()
{
	digitalWrite(AUX_RELAY_PIN, HIGH);
}

void relayAuxOff()
{
	digitalWrite(AUX_RELAY_PIN, LOW);
}

void relayAuxToggle()
{

}

bool getSwitch1State()
{
	return gpio_get(SWITCH1_PIN);
}

bool getSwitch2State()
{
	return gpio_get(SWITCH2_PIN);
}


//-------------------- Local Functions --------------------//
//------------------ System ------------------//
// Standard boot procedure and EEPROM recall
void picoMod_Boot()
{
  // Read the preset data
  for (uint8_t i = 0; i < NUM_PRESETS; i++)
  {
    EEPROM.get(sizeof(GlobalConfig) + sizeof(Preset) * i, preset);
  }
}

// Configures the device to the default state
void picoMod_NewDevice()
{
	globalConfig.bootState = DEVICE_CONFIGURED_VALUE;
	globalConfig.currentPreset = 0;
	globalConfig.midiChannel = MIDI_CHANNEL_OMNI;
	strcpy(globalConfig.deviceName, DEFAULT_DEVICE_NAME);
	// Save the default config to eeprom
	EEPROM.put(0, globalConfig);

	// Initialise all presets to contain no triggered actions
	for (uint8_t i = 0; i < NUM_PRESETS; i++)
	{
		preset.numActions = 0;
		for(uint8_t j=0; j<NUM_SWITCH_ACTIONS; j++)
		{
			preset.actions[j].trigger.type = TriggerNone;
		}
		EEPROM.put(sizeof(GlobalConfig) + sizeof(Preset) * i, preset);
	}
	EEPROM.commit();

	softwareReset();
}

void picoMod_PrintSystem()
{
	// Print device structure information
	Serial.print("Pico Mod: ");
	Serial.print(globalConfig.deviceName);
	Serial.print("   FW: ");
	Serial.println(FW_VERSION);
	Serial.print("Global configuration size: ");
	Serial.println(sizeof(GlobalConfig));
	Serial.print("Preset size: ");
	Serial.print(sizeof(Preset));
	Serial.print(" * ");
	Serial.print(NUM_PRESETS);
	Serial.print(". Total preset size: ");
	Serial.println(sizeof(Preset)*NUM_PRESETS);
}

// Returns the UID of the NOR flash chip
// str must have at least 2*PICO_UNIQUE_BOARD_ID_SIZE_BYTES +1 allocated (17 chars)
void getFlashUid(char* str)
{
  pico_get_unique_board_id_string(str, 2*PICO_UNIQUE_BOARD_ID_SIZE_BYTES +1);
}

void softwareReset()
{
  watchdog_reboot(0, 0, 0);
}


//------------ Preset Management ------------//
void readCurrentPreset()
{
  EEPROM.get(sizeof(GlobalConfig) + sizeof(Preset) * globalConfig.currentPreset, preset);
}

void saveCurrentPreset()
{
  EEPROM.put(sizeof(GlobalConfig) + sizeof(Preset) * globalConfig.currentPreset,
                preset);
  EEPROM.commit();
  delay(1);
}

void readGlobalConfig()
{
  EEPROM.get(sizeof(GlobalConfig) + sizeof(Preset) * globalConfig.currentPreset,
                preset);
}

void saveGlobalConfig()
{
  EEPROM.put(0, globalConfig);
  EEPROM.commit();
}

void presetUp()
{
  // Increment presets
  if(globalConfig.currentPreset >= NUM_PRESETS)
  {
    globalConfig.currentPreset = 0;
  }
  else
  {
    globalConfig.currentPreset++;
  }
  readCurrentPreset();
  saveGlobalConfig();
}

void presetDown()
{
  // Increment presets
  if(globalConfig.currentPreset == 0)
  {
    globalConfig.currentPreset = NUM_PRESETS;
  }
  else
  {
    globalConfig.currentPreset--;
  }
  readCurrentPreset();
  saveGlobalConfig();
}

void goToPreset(uint8_t newPreset)
{
  if(newPreset > NUM_PRESETS)
  {
    return;
  }
  globalConfig.currentPreset = newPreset;
  readCurrentPreset();
  saveGlobalConfig();
}


//------------- Switch Inputs -------------//
void switch1ISR()
{
	buttons_ExtiGpioCallback(&buttons[0], ButtonEmulateNone);
}

void switch2ISR()
{
	buttons_ExtiGpioCallback(&buttons[1], ButtonEmulateNone);
}

void switch1Handler(ButtonState state)
{
	genSwitchHandler(0, state);
}

void switch2Handler(ButtonState state)
{
	genSwitchHandler(1, state);
}

void genSwitchHandler(uint8_t index, ButtonState state)
{
	// Check for any assigned actions
	{
		for(uint8_t action=0; action<preset.numActions; action++)
		{
			// Corresponding TriggerType enum matches the switch index
			if(preset.actions[action].trigger.type == index)
			{
				processAction(&preset.actions[action]);
			}
		}
	}
}

void processAction(Action* action)
{
	switch(action->type)
	{
		case ActionEventMidi:
		processMidiActionEvent(&action->event);
		break;

		case ActionEventExp:
		processExpActionEvent(&action->event);
		break;

		case ActionEventOutput:
		processOutputActionEvent(&action->event);
		break;

		case ActionEventLed:
		processLedActionEvent(&action->event);
		break;
	}
}

void processMidiActionEvent(ActionEvent* event)
{
	trsMidi.send(	event->midiMessage.type,
						event->midiMessage.data1,
						event->midiMessage.data2,
						event->midiMessage.channel);
}

void processExpActionEvent(ActionEvent* event)
{
	mcp41_Write(&digipot, event->expMessage.value);
}

void processOutputActionEvent(ActionEvent* event)
{

}

void processLedActionEvent(ActionEvent* event)
{

}