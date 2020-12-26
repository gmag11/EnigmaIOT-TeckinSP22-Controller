// 
// 
// 

#include <Arduino.h>
#include <functional>
#include "TeckinSP22Controller.h"

using namespace std;
using namespace placeholders;

constexpr auto CONFIG_FILE = "/customconf.json"; ///< @brief Custom configuration file name

// -----------------------------------------
// You may add some global variables you need here,
// like serial port instances, I2C, etc
// -----------------------------------------
#if HLW8012
HLW8012 hlw8012;
#endif

const char* relayKey = "rly";
const char* commandKey = "cmd";
const char* restartValue = "restart";
const char* toggleKey = "toggle";
const char* buttonKey = "button";
const char* linkKey = "link";
const char* bootStateKey = "bstate";
const char* scheduleKey = "sched";

#if HLW8012
void ICACHE_RAM_ATTR hlw8012_cf1_interrupt () {
	hlw8012.cf1_interrupt ();
}
void ICACHE_RAM_ATTR hlw8012_cf_interrupt () {
	hlw8012.cf_interrupt ();
}

void CONTROLLER_CLASS_NAME::setInterrupts () {
	attachInterrupt (HLW_CF1, hlw8012_cf1_interrupt, HLW8012_INTERRUPT_ON);
	attachInterrupt (HLW_CF, hlw8012_cf_interrupt, HLW8012_INTERRUPT_ON);
}
void CONTROLLER_CLASS_NAME::calibrate () {
	// Let some time to register values
	unsigned long timeout = millis ();
	while ((millis () - timeout) < 10000) {
		delay (1);
	}

	// Calibrate using a 60W bulb (pure resistive) on a 230V line
	hlw8012.expectedActivePower (60);
	hlw8012.expectedVoltage (226.0);
	hlw8012.expectedCurrent (60.0 / 226.0);

	// TODO: Show corrected factors
	//debugW ("[HLW] New current multiplier : %f", hlw8012.getCurrentMultiplier ());
	//debugW ("[HLW] New voltage multiplier : %f", hlw8012.getVoltageMultiplier ());
	//debugW ("[HLW] New power multiplier   : %f", hlw8012.getPowerMultiplier ());
}
#endif // HLW8012

bool CONTROLLER_CLASS_NAME::processRxCommand (const uint8_t* address, const uint8_t* buffer, uint8_t length, nodeMessageType_t command, nodePayloadEncoding_t payloadEncoding) {
	// Process incoming messages here
	// They are normally encoded as MsgPack so you can confert them to JSON very easily
	if (command != nodeMessageType_t::DOWNSTREAM_DATA_GET && command != nodeMessageType_t::DOWNSTREAM_DATA_SET) {
		DEBUG_WARN ("Wrong message type");
		return false;
	}
	// Check payload encoding
	if (payloadEncoding != MSG_PACK) {
		DEBUG_WARN ("Wrong payload encoding");
		return false;
	}

	// Decode payload
	DynamicJsonDocument doc (256);
	uint8_t tempBuffer[MAX_MESSAGE_LENGTH];

	memcpy (tempBuffer, buffer, length);
	DeserializationError error = deserializeMsgPack (doc, tempBuffer, length);
	// Check decoding
	if (error != DeserializationError::Ok) {
		DEBUG_WARN ("Error decoding command: %s", error.c_str ());
		return false;
	}

	DEBUG_WARN ("Command: %d = %s", command, command == nodeMessageType_t::DOWNSTREAM_DATA_GET ? "GET" : "SET");

	// Dump debug data
	size_t strLen = measureJson (doc) + 1;
	char* strBuffer = (char*)malloc (strLen);
	serializeJson (doc, strBuffer, strLen);
	DEBUG_WARN ("Data: %s", strBuffer);
	free (strBuffer);

	// Check cmd field on JSON data
	if (!doc.containsKey (commandKey)) {
		DEBUG_WARN ("Wrong format");
		return false;
	}

	if (command == nodeMessageType_t::DOWNSTREAM_DATA_GET) {
		if (!strcmp (doc[commandKey], relayKey)) {
			DEBUG_WARN ("Request relay status. Relay = %s", relays->get(0)? "ON" : "OFF");
			if (!sendRelayStatus ()) {
				DEBUG_WARN ("Error sending relay status");
				return false;
			}
		} /*else if (!strcmp (doc[commandKey], linkKey)) {
			DEBUG_WARN ("Request link status. Link = %s", config.linked ? "enabled" : "disabled");
			if (!sendLinkStatus ()) {
				DEBUG_WARN ("Error sending link status");
				return false;
			}

		}*/ /*else if (!strcmp (doc[commandKey], bootStateKey)) {
			DEBUG_WARN ("Request boot status configuration. Boot = %d",
						config.bootStatus);
			if (!sendBootStatus ()) {
				DEBUG_WARN ("Error sending boot status configuration");
				return false;
			}

		}*/
	}

	if (command == nodeMessageType_t::DOWNSTREAM_DATA_SET) {
        int index = 0;
		if (!strcmp (doc[commandKey], relayKey)) {
			if (!doc.containsKey (relayKey)) {
				DEBUG_WARN ("Wrong format");
				return false;
			}
            if (doc.containsKey ("index")){
                index = doc["index"].as<int> ();
            }
			char temp[200];
			serializeJson (doc, temp, 200);
			DEBUG_DBG ("%s", temp);
            DEBUG_INFO ("Set relay status. Relay %d = %d", index, doc[relayKey].as<int> ());
			if (doc[relayKey].is<int> ()) {
				int value = doc[relayKey].as<int> ();
				DEBUG_DBG ("%s is %d", relayKey, value);
				switch (value) {
				case 0:
					DEBUG_INFO ("setRelay (false)");
                    relays->set (index, false);
					break;
				case 1:
					DEBUG_INFO ("setRelay (true)");
                    relays->set (index, true);
					break;
				case 2:
                    relays->toggle (index);
					break;
				default:
					return false;
				}
			} else {
				DEBUG_WARN ("Wrong value");
				return false;
			}

			if (!sendRelayStatus ()) {
				DEBUG_WARN ("Error sending relay status");
				return false;
			}


        } else if (!strcmp (doc[commandKey], scheduleKey)){
            /****
            {
                "cmd":"sched",
                "hour":12,
                "min":0,
                "action":1,
                "idx":0,
                ---- optional
                "mask":127,
                "rep":true,
                "enable":true
            }
            *****/
            schedule_t sched_entry;
            
            if (!doc.containsKey ("hour") || !doc.containsKey ("min") || !doc.containsKey ("action") || !doc.containsKey ("idx")) {
                DEBUG_WARN ("Wrong format");
                return false;
            }
            
            sched_entry.action = doc["action"].as<int> ();
            if (sched_entry.action < TURN_OFF || sched_entry.action > TOGGLE) {
                DEBUG_WARN ("Wrong format");
                return false;
            }
            
            sched_entry.index = doc["idx"].as<int> ();
            if (sched_entry.index < 0 || sched_entry.index >= NUM_RELAYS) {
                DEBUG_WARN ("Wrong format");
                return false;
            }
            
            sched_entry.hour = doc["hour"].as<int> ();
            sched_entry.minute = doc["min"].as<int> ();
            if (sched_entry.hour >= 23 || sched_entry.hour >= 59) {
                DEBUG_WARN ("Wrong format");
                return false;
            }
            
            if (doc.containsKey ("mask")) {
                sched_entry.weekMask = doc["mask"].as<int>();
            }
            
            if (doc.containsKey ("rep")) {
                sched_entry.repeat = doc["rep"].as<int> ();
            }
            
            if (doc.containsKey ("enable")) {
                sched_entry.enabled = doc["enable"].as<int> ();
            }

            DEBUG_WARN ("Set schedule Time = %u:%u (%s). Mask " BYTE_TO_BINARY_PATTERN ". Action %d in relay %d. %s",
                        sched_entry.hour, sched_entry.minute, sched_entry.repeat ? "repeat" : "single", 
                        BYTE_TO_BINARY (sched_entry.weekMask), sched_entry.action, sched_entry.index, sched_entry.enabled?"Enabled":"Disabled");
            
            int result = scheduler.add (&sched_entry);
            
            DEBUG_WARN ("Result = %d", result);
        }
        
        /*else if (!strcmp (doc[commandKey], linkKey)) {
			if (!doc.containsKey (linkKey)) {
				DEBUG_WARN ("Wrong format");
				return false;
			}
			DEBUG_WARN ("Set link status. Link = %s", doc[linkKey].as<bool> () ? "enabled" : "disabled");

			setLinked (doc[linkKey].as<bool> ());

			if (!sendLinkStatus ()) {
				DEBUG_WARN ("Error sending link status");
				return false;
			}

		}*//* else if (!strcmp (doc[commandKey], bootStateKey)) {
			if (!doc.containsKey (bootStateKey)) {
				DEBUG_WARN ("Wrong format");
				return false;
			}
			DEBUG_WARN ("Set boot status. Link = %d", doc[bootStateKey].as<int> ());

			setBoot (doc[bootStateKey].as<int> ());

			if (!sendBootStatus ()) {
				DEBUG_WARN ("Error sending boot status configuration");
				return false;
			}

		}*/
	}

	return true;
}


bool CONTROLLER_CLASS_NAME::sendCommandResp (const char* command, bool result) {
	// Respond to command with a result: true if successful, false if failed 
	return true;
}

// bool CONTROLLER_CLASS_NAME::sendStartAnouncement () {
// 	// You can send a 'hello' message when your node starts. Useful to detect unexpected reboot
// 	const size_t capacity = JSON_OBJECT_SIZE (2);
// 	DynamicJsonDocument json (capacity);
// 	json["status"] = "start";
// 	json["device"] = "Teckin SP22 v1.4";

// 	return sendJson (json);
// }

void CONTROLLER_CLASS_NAME::sendButtonEvent (uint8_t pin, uint8_t event, uint8_t count, uint16_t length) {
	const size_t capacity = JSON_OBJECT_SIZE (5);
	DynamicJsonDocument json (capacity);
	json[commandKey] = buttonKey;
	json[buttonKey] = pin;
	json["event"] = event;
	json["count"] = count;
	json["len"] = length;
	sendJson (json);
}

void CONTROLLER_CLASS_NAME::buttonCb (uint8_t pin, uint8_t event, uint8_t count, uint16_t length) {
	//bool stateRed, stateBlue;

	if (event == EVENT_RELEASED || event == EVENT_CHANGED) {
		sendButtonEvent (pin, event, count, length);
		switch (count) {
		case 2:
			if (length < 1000) {
                relays->toggle (0);
				sendRelayStatus ();
			}
			break;
		default:
			if (count > 5 && length > 2000) {
				if (enigmaIotNode) {
					enigmaIotNode->restart (USER_RESET);
				}
			}
		}
	}
}

void CONTROLLER_CLASS_NAME::setup (EnigmaIOTNodeClass *node, void* data) {
	enigmaIotNode = node;

	// You do node setup here. Use it as it was the normal setup() Arduino function
	button = new DebounceEvent (BUTTON, std::bind(&CONTROLLER_CLASS_NAME::buttonCb, this, _1, _2, _3, _4), BUTTON_PUSHBUTTON | BUTTON_DEFAULT_HIGH | BUTTON_SET_PULLUP);
	pinMode (RED_LED_INV, OUTPUT);
	digitalWrite (RED_LED_INV, HIGH);
	pinMode (BLUE_LED_INV, OUTPUT);
	digitalWrite (BLUE_LED_INV, HIGH);
	//pinMode (RELAY, OUTPUT);
    uint8_t relayPins[] = { RELAY };
    uint8_t relayOnStates[] = { RELAY_ON };
    relays = new RelaySet (relayPins, relayOnStates, NUM_RELAYS);

    scheduler.onEvent (std::bind (&CONTROLLER_CLASS_NAME::onSchedulerEvent, this, _1));

	// Send a 'hello' message when initalizing is finished
	sendStartAnouncement ();
#if HLW8012

	hlw8012.begin (HLW_CF, HLW_CF1, HLW_SEL, CURRENT_MODE, true);
	hlw8012.setResistors (CURRENT_RESISTOR, VOLTAGE_RESISTOR_UPSTREAM, VOLTAGE_RESISTOR_DOWNSTREAM);
	hlw8012.setCurrentMultiplier (HLW8012_CURRENT_RATIO);
	hlw8012.setPowerMultiplier (HLW8012_POWER_RATIO);
	hlw8012.setVoltageMultiplier (HLW8012_VOLTAGE_RATIO);
	setInterrupts ();
#endif // HLW8012

    //relayStatus = relays->get (0);
	//digitalWrite (RELAY_LED, relayStatus);

	if (!sendRelayStatus ()) {
		DEBUG_WARN ("Error sending relay status");
	}

	DEBUG_DBG ("Finish begin");

// If your node should sleep after sending data do all remaining tasks here

}

#if HLW8012
void CONTROLLER_CLASS_NAME::sendHLWmeasurement () {
	const size_t capacity = JSON_OBJECT_SIZE (8);
	DynamicJsonDocument json (capacity);
	json["Power_W"] = hlw8012.getActivePower ();
	json["Power_VA"] = hlw8012.getApparentPower ();
	json["P_Factor"] = (int)(100 * hlw8012.getPowerFactor ());
	unsigned int voltage = hlw8012.getVoltage ();
	json["Voltage"] = voltage;
	json["Current"] = hlw8012.getCurrent ();
	double energy = (double)hlw8012.getEnergy () / 3600.0;
	json["Wh"] = energy;
	if (voltage != 0) {
		const int period = 3600 * 1000 / UPDATE_TIME;
		double dcurrent = (energy - lastEnergy) * period / voltage;
		json["dcurrent"] = dcurrent;
	}
    json["rly"] = relays->get(0);
	lastEnergy = energy;

	sendJson (json);
}
#endif // HLW8012

void CONTROLLER_CLASS_NAME::onSchedulerEvent (sched_event_t event) {
    DEBUG_WARN ("Scheduler event %d, index %d, repeat %d", event.action, event.index, event.repeat);
    if (event.action < TURN_OFF || event.action > TOGGLE) {
        DEBUG_WARN ("Schedule action error");
        return;
    }
    if (event.index < 0 || event.index >= NUM_RELAYS){
        DEBUG_WARN ("Schedule index error");
        return;
    }
    switch (event.action) {
        case TURN_OFF:
        case TURN_ON:
            relays->set (event.index, event.action);
            break;
        case TOGGLE:
            relays->toggle (event.index);
            break;
        default:
            break;
    }
    sendRelayStatus ();
}

void CONTROLLER_CLASS_NAME::loop () {

	// If your node stays allways awake do your periodic task here

	button->loop ();
    
    scheduler.loop ();

#if HLW8012
    static unsigned long last = millis ();
	if ((millis () - last) > UPDATE_TIME && !enigmaIotNode->getOTArunning()) {
		last = millis ();
		sendHLWmeasurement ();
	}
#endif // HLW8012

	// TODO: Sync LED with relay
	digitalWrite (RELAY_LED, relays->get(0) ? LED_ON : LED_OFF);
}

CONTROLLER_CLASS_NAME::~CONTROLLER_CLASS_NAME () {
	// It your class uses dynamic data free it up here
	// This is normally not needed but it is a good practice
}

void CONTROLLER_CLASS_NAME::configManagerStart () {
	DEBUG_INFO ("==== CCost Controller Configuration start ====");
	// If you need to add custom configuration parameters do it here
}

void CONTROLLER_CLASS_NAME::configManagerExit (bool status) {
	DEBUG_INFO ("==== CCost Controller Configuration result ====");
	// You can read configuration paramenter values here
}

bool CONTROLLER_CLASS_NAME::loadConfig () {
	// If you need to read custom configuration data do it here
	return true;
}

bool CONTROLLER_CLASS_NAME::saveConfig () {
	// If you need to save custom configuration data do it here
	// Save measure period 3-X, initial_status, last_relay_status
	return true;
}

bool CONTROLLER_CLASS_NAME::sendRelayStatus () {
	const size_t capacity = JSON_OBJECT_SIZE (2);
	DynamicJsonDocument json (capacity);

	json[commandKey] = relayKey;
    String key = relayKey;
    for (uint i = 0; i < NUM_RELAYS; i++) {
        key += i;
        json[key.c_str()] = relays->get (i) ? 1 : 0;    
    }

	return sendJson (json);
}

void CONTROLLER_CLASS_NAME::connectInform () {
	sendStartAnouncement ();
}