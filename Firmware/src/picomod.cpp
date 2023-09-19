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

// JSON Parsing
ParsingStatus parsingStatus;
char serialRxBuffer[JSON_RX_BUFFER_SIZE];


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

void noteOnHandler(byte channel, byte note, byte velocity);
void noteOffHandler(byte channel, byte note, byte velocity);
void controlChangeHandler(byte channel, byte number, byte value);
void programChangeHandler(byte channel, byte number);
void systemExclusiveHandler(byte* array, unsigned size);

void processGlobalConfigPacket(char* buffer);
void processPresetPacket(char* buffer);
void sendGlobalConfigPacket();
void sendPresetPacket(uint8_t presetIndex);


//--------------------  --------------------//
//------------------ System ------------------//
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

	// LEDs
	leds.begin();
	leds.clear();
	leds.setPixelColor(0, leds.Color(90, 0, 80));
	leds.show();

	// Serial config
	Serial.begin(9600);
	parsingStatus = ParsingReady;

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

	// Begin MIDI listening
	trsMidi.begin(globalConfig.midiChannel);
	usbMidi.begin(globalConfig.midiChannel); 

	// Active boot actions
	processTriggers(TriggerBoot);
	delay(3000);
	sendGlobalConfigPacket();
}

void picoMod_SerialRx(uint16_t len)
{
	// Compare the current parsing state to the received data packet
	// Prepare for a new packet
	if(parsingStatus == ParsingReady)
	{
		// Prepare to receive global settings
		if(strcmp(serialRxBuffer, "sendGlobal") == 0)
		{
			parsingStatus = ParsingGlobal;
			Serial.println("ok");
		}
		// Prepare to receive a preset
		else if(strcmp(serialRxBuffer, "sendPreset") == 0)
		{
			parsingStatus = ParsingPreset;
			Serial.println("ok");
		}
		// Request to send the global config settings
		else if(strcmp(serialRxBuffer, "receivePreset") == 0)
		{
			sendPresetPacket(globalConfig.currentPreset);
		}
		// Request to send the preset data
		else if(strcmp(serialRxBuffer, "receiveGlobal") == 0)
		{
			sendGlobalConfigPacket();
		}
		else
		{
			Serial.println("error");
		}
	}
	// Receive a global config packet
	else if(parsingStatus == ParsingGlobal)
	{
		processGlobalConfigPacket(serialRxBuffer);
		parsingStatus = ParsingReady;
		Serial.println("ok");
	}
	// Receive a preset packet
	else if(parsingStatus == ParsingPreset)
	{
		processPresetPacket(serialRxBuffer);
		parsingStatus = ParsingReady;
		Serial.println("ok");
	}
	// Flush the buffer
	for(uint16_t i=0; i<len; i++)
	{
		serialRxBuffer[i] = 0;
	}
}


//------------------ GPIO -------------------//
// These functions are wrappers for the onboard GPIO.
// A slight performance overhead is incurred due to the additional function call,
// however, for an inexperienced embedded programmer, they may be easier to use.
void relayBypassOn()
{
	preset.bypassRelayState = 1;
	gpio_put(BYPASS_RELAY_PIN, 1);
}

void relayBypassOff()
{
	preset.bypassRelayState = 0;
	gpio_put(BYPASS_RELAY_PIN, LOW);
}

void relayBypassToggle()
{
	preset.bypassRelayState =! preset.bypassRelayState;
	gpio_put(BYPASS_RELAY_PIN, preset.bypassRelayState);
}

void relayAuxOn()
{
	preset.auxRelayState = 1;
	gpio_put(AUX_RELAY_PIN, 1);
}

void relayAuxOff()
{
	preset.auxRelayState = 0;
	gpio_put(AUX_RELAY_PIN, 0);
}

void relayAuxToggle()
{
	preset.auxRelayState =! preset.auxRelayState;
	gpio_put(BYPASS_RELAY_PIN, preset.auxRelayState);
}

void analogSwitchOn()
{
	preset.analogSwitchState = 1;
	gpio_put(AUX_RELAY_PIN, 1);
}

void analogSwitchOff()
{
	preset.analogSwitchState = 0;
	gpio_put(AUX_RELAY_PIN, 0);
}

void analogSwitchToggle()
{
	preset.analogSwitchState =! preset.analogSwitchState;
	gpio_put(BYPASS_RELAY_PIN, preset.analogSwitchState);
}

bool getSwitch1State()
{
	return gpio_get(SWITCH1_PIN);
}

bool getSwitch2State()
{
	return gpio_get(SWITCH2_PIN);
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
	// Handle any actions triggered by the bank exit
  processTriggers(TriggerExitBank);
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
  // Handle any actions triggered by the bank entry
  processTriggers(TriggerEnterBank);
}

void presetDown()
{
	// Handle any actions triggered by the bank exit
  processTriggers(TriggerExitBank);
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
  // Handle any actions triggered by the bank entry
  processTriggers(TriggerEnterBank);
}

void goToPreset(uint8_t newPreset)
{
  if(newPreset > NUM_PRESETS)
  {
    return;
  }
  // Handle any actions triggered by the bank exit
  processTriggers(TriggerExitBank);
  globalConfig.currentPreset = newPreset;
  readCurrentPreset();
  saveGlobalConfig();
  // Handle any actions triggered by the bank entry
  processTriggers(TriggerEnterBank);
}


//----------- Action Handling -----------//
void processTriggers(TriggerType triggerType)
{
	// Check for any assigned actions
	for(uint8_t action=0; action<preset.numActions; action++)
	{
		// Corresponding TriggerType enum matches the switch index
		if(preset.actions[action].trigger.type == triggerType)
		{
			processAction(&preset.actions[action]);
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
	OutputTarget target = event->outputMessage.target;
	OutputValue value = event->outputMessage.value;
	switch(target)
	{
		case OutputBypassRelay:
		if(value == OutputOn)
		{
			relayBypassOn();
		}
		else if(value == OutputOff)
		{
			relayBypassOff();
		}
		else if(value == OutputToggle)
		{
			relayBypassToggle();
		}
		break;

		case OutputAuxRelay:
		if(value == OutputOn)
		{
			relayAuxOn();
		}
		else if(value == OutputOff)
		{
			relayAuxOff();
		}
		else if(value == OutputToggle)
		{
			relayAuxToggle();
		}
		break;

		case OutputAnalogSwitch:
		if(value == OutputOn)
		{
			analogSwitchOn();
		}
		else if(value == OutputOff)
		{
			analogSwitchOff();
		}
		else if(value == OutputToggle)
		{
			analogSwitchToggle();
		}
		break;
	}
}

void processLedActionEvent(ActionEvent* event)
{
	leds.setPixelColor(event->ledMessage.index, event->ledMessage.colour);
	leds.show();
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

	// Initialise all presets to contain no actions and default states
	for (uint8_t i = 0; i < NUM_PRESETS; i++)
	{
		preset.expValue = 127;
		preset.switch1State = 0;
		preset.switch2State = 0;
		preset.analogSwitchState = 0;
		preset.bypassRelayState = 0;
		preset.auxRelayState = 0;
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
	processTriggers((TriggerType)index);
}


//------------ MIDI Callbacks ------------//
void controlChangeHandler(byte channel, byte number, byte value)
{
	processTriggers(TriggerCC);
}

void programChangeHandler(byte channel, byte number)
{
	if (number < NUM_PRESETS)
	{
		goToPreset(number);
	}
}

void systemExclusiveHandler(byte* array, unsigned size)
{

}


//------------ JSON Handling ------------//
void processGlobalConfigPacket(char* buffer)
{
	// Allocate the JSON document
	// If you add custom handling, ensure you allow enough memory
	StaticJsonDocument<100> json;

	// Deserialize the JSON document
	DeserializationError error = deserializeJson(json, buffer);

	// Test if parsing succeeds.
	if (error)
	{
		Serial.print(F("deserializeJson() failed: "));
		Serial.println(error.f_str());
		return;
	}
	// Device name
	const char* newDeviceName = json["deviceName"];
	strcpy(globalConfig.deviceName, newDeviceName);

	// MIDI channel
	globalConfig.midiChannel = json["midiChannel"];

	Serial.print("New device name: ");
	Serial.println(globalConfig.deviceName);
	Serial.print("MIDI Channel: ");
	Serial.println(globalConfig.midiChannel);
}	

void processPresetPacket(char* buffer)
{
	// Allocate the JSON document
	// If you add custom handling, ensure you allow enough memory
	StaticJsonDocument<4096> json;

	// Deserialize the JSON document
	DeserializationError error = deserializeJson(json, buffer);

	// Test if parsing succeeds
	if (error)
	{
		Serial.print(F("deserializeJson() failed: "));
		Serial.println(error.f_str());
		return;
	}

	// Record the current preset number and preserve the current preset
	uint8_t currentPreset = globalConfig.currentPreset;
	saveCurrentPreset();

	// Find the target preset for the incoming packet and load it
	uint8_t newPreset = json["index"];
	globalConfig.currentPreset = newPreset;
	readCurrentPreset();

	// Process the preset data
	preset.id = json["id"];
	preset.expValue = json["expValue"];
	preset.switch1State = json["switch1State"];
	preset.switch2State = json["switch2State"];
	preset.bypassRelayState = json["bypassRelayState"];
	preset.auxRelayState = json["auxRelayState"];
	preset.analogSwitchState = json["analogSwitchState"];
	preset.numActions = json["numActions"];

	// Process all actions
	for(uint16_t i=0; i<preset.numActions; i++)
	{
		// Action trigger
		preset.actions[i].trigger.type = json["actions"][i]["trigger"]["type"];
		// Button input triggers require the button state
		if(preset.actions[i].trigger.type <= TriggerGpio7)
		{
			preset.actions[i].trigger.value.buttonTrigger = json["actions"][i]["trigger"]["value"];
		}
		// MIDI CC triggers require the CC number and value
		else if(preset.actions[i].trigger.type == TriggerCC)
		{
			preset.actions[i].trigger.value.midiTrigger.midiNum = json["actions"][i]["trigger"]["number"];
			preset.actions[i].trigger.value.midiTrigger.midiValue = json["actions"][i]["trigger"]["value"];
		}
		// Action event type
		preset.actions[i].type = json["actions"][i]["type"];

		// Action event
		// MIDI event
		if(preset.actions[i].type == ActionEventMidi)
		{
			preset.actions[i].event.midiMessage.channel = json["actions"][i]["event"]["channel"];
			preset.actions[i].event.midiMessage.type = json["actions"][i]["event"]["type"];
			preset.actions[i].event.midiMessage.data1 = json["actions"][i]["event"]["data1"];
			preset.actions[i].event.midiMessage.data2 = json["actions"][i]["event"]["data2"];
		}
		// Expression event
		else if(preset.actions[i].type == ActionEventExp)
		{
			preset.actions[i].event.expMessage.value = json["actions"][i]["event"]["value"];
		}
		// Output event
		else if(preset.actions[i].type == ActionEventOutput)
		{
			preset.actions[i].event.outputMessage.value = json["actions"][i]["event"]["target"];
			preset.actions[i].event.outputMessage.value = json["actions"][i]["event"]["value"];
		}

		// LED event
		else if(preset.actions[i].type == ActionEventLed)
		{
			preset.actions[i].event.ledMessage.index = json["actions"][i]["event"]["value"];
			preset.actions[i].event.ledMessage.colour = json["actions"][i]["event"]["color"];
		}
	}

	// Save the preset data
	saveCurrentPreset();

	// Recall the previous preset
	globalConfig.currentPreset = currentPreset;
	readCurrentPreset();
}

void sendGlobalConfigPacket()
{
	// Allocate the JSON document
	// If you add custom handling, ensure you allow enough memory
	StaticJsonDocument<100> json;
	json["currentPreset"] = globalConfig.currentPreset;
	json["midiChannel"] = globalConfig.midiChannel;
	json["deviceName"] = globalConfig.deviceName;
	json["hwVersion"] = HW_VERSION;
	json["fwVersion"] = FW_VERSION;
	serializeJson(json, Serial);
}

void sendPresetPacket(uint8_t presetIndex)
{
	// Allocate the JSON document
	// If you add custom handling, ensure you allow enough memory
	StaticJsonDocument<4096> json;
	json["index"] = presetIndex;
	json["id"] = preset.id;
	json["switch1State"] = preset.switch1State;
	json["switch2State"] = preset.switch2State;
	json["bypassRelayState"] = preset.bypassRelayState;
	json["auxRelayState"] = preset.auxRelayState;
	json["analogSwitchState"] = preset.analogSwitchState;
	json["numActions"] = preset.numActions;

	// Process all actions
	for(uint16_t i=0; i<preset.numActions; i++)
	{
		// Action trigger
		json["actions"][i]["trigger"]["type"] = preset.actions[i].trigger.type;
		// Button input triggers require the button state
		if(preset.actions[i].trigger.type <= TriggerGpio7)
		{
			json["actions"][i]["trigger"]["value"] = preset.actions[i].trigger.value.buttonTrigger;
		}
		// MIDI CC triggers require the CC number and value
		else if(preset.actions[i].trigger.type == TriggerCC)
		{
			json["actions"][i]["trigger"]["number"] = preset.actions[i].trigger.value.midiTrigger.midiNum;
			json["actions"][i]["trigger"]["value"]= preset.actions[i].trigger.value.midiTrigger.midiValue;
		}
		// Action event type
		preset.actions[i].type = json["actions"][i]["type"];

		// Action event
		// MIDI event
		if(preset.actions[i].type == ActionEventMidi)
		{
			json["actions"][i]["event"]["channel"] = preset.actions[i].event.midiMessage.channel;
			json["actions"][i]["event"]["type"] = preset.actions[i].event.midiMessage.type;
			json["actions"][i]["event"]["data1"] = preset.actions[i].event.midiMessage.data1;
			json["actions"][i]["event"]["data2"] = preset.actions[i].event.midiMessage.data2;
		}
		// Expression event
		else if(preset.actions[i].type == ActionEventExp)
		{
			json["actions"][i]["event"]["value"] = preset.actions[i].event.expMessage.value;
		}
		// Output event
		else if(preset.actions[i].type == ActionEventOutput)
		{
			json["actions"][i]["event"]["target"] = preset.actions[i].event.outputMessage.value;
			json["actions"][i]["event"]["value"] = preset.actions[i].event.outputMessage.value;
		}

		// LED event
		else if(preset.actions[i].type == ActionEventLed)
		{
			json["actions"][i]["event"]["value"] = preset.actions[i].event.ledMessage.index;
			json["actions"][i]["event"]["color"] = preset.actions[i].event.ledMessage.colour;
		}
	}
	
	serializeJson(json, Serial);
}