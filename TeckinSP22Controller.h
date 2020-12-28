// BasicController.h

#ifndef _BASICCONTROLLER_h
#define _BASICCONTROLLER_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "Arduino.h"
#else
	#include "WProgram.h"
#endif

#ifndef ENABLE_HLW8012
#define ENABLE_HLW8012  1
#endif //ENABLE_HLW8012

#if ENABLE_HLW8012
#include <HLW8012.h>
#endif // ENABLE_HLW8012
#include <DebounceEvent.h>
#include "RelaySet.h"
#include "ActionScheduler.h"
#include <TZ.h>

//#define DEBUG_SERIAL

#include <EnigmaIOTjsonController.h>
//#define TeckinSP22Controller BasicController
#define CONTROLLER_CLASS_NAME SmartSwitchController
static const char* CONTROLLER_NAME = "Teckin SP22 v1.4";

// --------------------------------------------------
// You may define data structures and constants here
// --------------------------------------------------
//constexpr auto RED_LED = 0;
//constexpr auto BLUE_LED = 2;
#ifndef BUTTON
constexpr auto BUTTON = 1;
#endif
constexpr auto RED_LED_INV = 3;
#ifndef BLUE_LED_INV
constexpr auto BLUE_LED_INV = 13;
#endif
#ifndef RELAY
constexpr auto RELAY = 14;
#endif
#if ENABLE_HLW8012
constexpr auto HLW_CF = 4;
constexpr auto HLW_CF1 = 5;
constexpr auto HLW_SEL = 12;

#define UPDATE_TIME                     10000
#define CURRENT_MODE                    LOW
// These are the nominal values for the resistors in the circuit
#define CURRENT_RESISTOR                0.001
#define VOLTAGE_RESISTOR_UPSTREAM       ( 5 * 470000 ) // Real: 2280k
#define VOLTAGE_RESISTOR_DOWNSTREAM     ( 1000 ) // Real 1.009k
#define HLW8012_SEL_CURRENT         LOW
#define HLW8012_CURRENT_RATIO       25610 //20730
#define HLW8012_VOLTAGE_RATIO       281105 //264935
#define HLW8012_POWER_RATIO         3304057 //2533110
#define HLW8012_INTERRUPT_ON        FALLING
#endif // ENABLE_HLW8012

#define RELAY_ON_POLARITY HIGH
//#define RELAY_OFF !RELAY_ON_POLARITY
#define NUM_RELAYS 1

#define LED_ON LOW
#define RELED_OFF !LED_ON

#define RELAY_LED RED_LED_INV

// typedef enum {
// 	BOOT_OFF = 0,
//     BOOT_ON = 1,
//     BOOT_SAVE = 2
// } bootRelayStatus_t;

typedef enum {
    TURN_OFF = 0,
    TURN_ON = 1,
    TOGGLE = 2
} relayAction_t;

/*typedef struct {
	bootRelayStatus_t bootStatus;
	uint period;
	bool lastRelayStatus;
} relayConfig_t;*/

class CONTROLLER_CLASS_NAME : EnigmaIOTjsonController {
protected:
	// --------------------------------------------------
	// add all parameters that your project needs here
	// --------------------------------------------------


public:
	void setup (EnigmaIOTNodeClass* node, void* data = NULL);
	
	bool processRxCommand (const uint8_t* address, const uint8_t* buffer, uint8_t length, nodeMessageType_t command, nodePayloadEncoding_t payloadEncoding);
	
	void loop ();
	
	~CONTROLLER_CLASS_NAME ();
    
    CONTROLLER_CLASS_NAME () {
        uint8_t relayPins[] = { RELAY };
        uint8_t relayOnStates[] = { RELAY_ON_POLARITY };
        relays = new RelaySet (relayPins, relayOnStates, NUM_RELAYS, &FILESYSTEM);
        scheduler = new ActionScheduler (&FILESYSTEM);
    }

	/**
	 * @brief Called when wifi manager starts config portal
	 * @param node Pointer to EnigmaIOT gateway instance
	 */
	void configManagerStart ();

	/**
	 * @brief Called when wifi manager exits config portal
	 * @param status `true` if configuration was successful
	 */
	void configManagerExit (bool status);

	/**
	 * @brief Loads output module configuration
	 * @return Returns `true` if load was successful. `false` otherwise
	 */
	bool loadConfig ();

	void connectInform ();

protected:
	/**
	  * @brief Saves output module configuration
	  * @return Returns `true` if save was successful. `false` otherwise
	  */
	bool saveConfig ();
    
    void onSchedulerEvent (sched_event_t event);
	
	bool sendCommandResp (const char* command, bool result);
	
    bool sendStartAnouncement () {
        // You can send a 'hello' message when your node starts. Useful to detect unexpected reboot
        const size_t capacity = JSON_OBJECT_SIZE (10);
        DynamicJsonDocument json (capacity);
        json["status"] = "start";
        json["device"] = CONTROLLER_NAME;
        char version_buf[10];
        snprintf (version_buf, 10, "%d.%d.%d",
                  ENIGMAIOT_PROT_VERS[0], ENIGMAIOT_PROT_VERS[1], ENIGMAIOT_PROT_VERS[2]);
        json["version"] = String (version_buf);

        return sendJson (json);
    }

	// ------------------------------------------------------------
	// You may add additional method definitions that you need here
	// ------------------------------------------------------------
	//relayConfig_t relayConfig;
	DebounceEvent* button;
	bool restartTriggd = false;
	//bool relayStatus;
    RelaySet *relays;
    ActionScheduler* scheduler;
    AsyncWiFiManagerParameter* bootStatusParam;
    AsyncWiFiManagerParameter* bootStatusListParam;

#if ENABLE_HLW8012
    bool doCalibration = false;
    double lastEnergy;

	void setInterrupts ();

	void calibrate ();

    void sendHLWmeasurement ();
#endif // ENABLE_HLW8012
    
	void buttonCb (uint8_t pin, uint8_t event, uint8_t count, uint16_t length);

	//void setRelay (bool state);

	//void toggleRelay ();

	bool sendRelayStatus ();
    
    bool sendSchedulerList (char* list);

	void sendButtonEvent (uint8_t pin, uint8_t event, uint8_t count, uint16_t length);
    
    bool sendBootStatus ();
    
};

#endif

