#ifndef PICOMOD_H_
#define PICOMOD_H_

#include "spi.h"
#include "picomod.h"
#include "mcp41xx.h"
#include "MIDI.h"
#include "Adafruit_TinyUSB.h"
#include "EEPROM.h"
#include "buttons.h"

//------------- Pin Definitions -------------//
// Switch Inputs
#define SWITCH1_PIN     	7
#define SWITCH2_PIN     	6

// Switched Outputs
#define BYPASS_RELAY_PIN	3
#define AUX_RELAY_PIN      8
#define SWITCH_OUT_PIN  	9

// Digipot
#define SPI_SCK         	10
#define SPI_MOSI        	11
#define DIGIPOT_CS      	12

// General Purpose Pins
#define GP1_PIN         	18
#define GP2_PIN         	19
#define GP3_PIN         	23 
#define GP4_PIN         	26
#define GP5_PIN         	27
#define GP6_PIN         	28
#define GP7_PIN         	29


//----------- Config Stack Sizes -----------//
#define NUM_PRESETS			127
#define NUM_SWITCH_ACTIONS	16
#define DEVICE_NAME_LEN		16
#define NUM_SWITCHES			2


//-------------- Config Flags --------------//
#define DEVICE_CONFIGURED_VALUE 114
#define DEFAULT_DEVICE_NAME		"New Pico Mod"


//------------------ Types -----------------//
typedef struct
{
	uint8_t bootState;
	uint8_t midiChannel;
	uint8_t currentPreset;
	char deviceName[DEVICE_NAME_LEN];
} GlobalConfig;

typedef struct
{
	uint8_t channel;
	uint8_t type;
	uint8_t data1;
	uint8_t data2;
} MidiMessage;

typedef struct
{
	uint16_t value;
} ExpMessage;

typedef enum
{
	OutputBypassRelay,
	OutputAuxRelay,
	OutputAnalog,
	OutputGpio
} OutputTarget;

typedef struct
{
	OutputTarget target;
	uint8_t value;
} OutputMessage;

typedef struct
{
	uint16_t index;
	uint32_t colour;
} LedMessage;

typedef enum
{
	ActionEventMidi,
	ActionEventExp,
	ActionEventOutput,
	ActionEventLed
} ActionEventType;

typedef union
{
	MidiMessage midiMessage;
	ExpMessage expMessage;
	OutputMessage outputMessage;
	LedMessage ledMessage;
} ActionEvent;

typedef enum
{
	TriggerSwitch1 = 0,
	TriggerSwitch2 = 1,
	TriggerGpio1,
	TriggerGpio2,
	TriggerGpio3,
	TriggerGpio4,
	TriggerGpio5,
	TriggerGpio6,
	TriggerGpio7,
	MidiCC,
	MidiNoteOn,
	MidiNoteOff,
	TriggerNone
} TriggerType;


typedef struct
{
	uint16_t midiNum;
	uint8_t midiValue;
} MidiTriggerValue;

typedef union
{
	ButtonState buttonTrigger;
	MidiTriggerValue midiTrigger;
} TriggerValue;

typedef struct
{
	TriggerType type;
	TriggerValue value;
} ActionTrigger;

typedef struct
{
	ActionTrigger trigger;
	ActionEventType type;
	ActionEvent event;
} Action;

typedef struct
{
	uint8_t numActions;
	Action actions[NUM_SWITCH_ACTIONS];
} Preset;

//------------- Global Variables -------------/
extern MIDI_NAMESPACE::MidiInterface<MIDI_NAMESPACE::SerialMIDI<HardwareSerial>> trsMidi;
extern MIDI_NAMESPACE::MidiInterface<MIDI_NAMESPACE::SerialMIDI<Adafruit_USBD_MIDI>> usbMidi;

extern Button switches[NUM_SWITCHES];
extern MCP41 digipot;
extern GlobalConfig globalConfig;
extern Preset preset;


//-------------------- Global Functions --------------------//
void picoMod_Init();
void relayBypassOn();
void relayBypassOff();
void relayBypassToggle();
void relayAuxOn();
void relayAuxOff();
void relayAuxToggle();
bool getSwitch1State();
bool getSwitch2State();

#endif /* PICOMOD_H_ */
