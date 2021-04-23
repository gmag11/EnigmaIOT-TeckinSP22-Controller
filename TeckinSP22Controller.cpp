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
#if ENABLE_HLW8012
HLW8012 hlw8012;
#endif // ENABLE_HLW8012

const char* relayKey = "swi";
const char* commandKey = "cmd";
const char* restartValue = "restart";
const char* toggleKey = "toggle";
const char* buttonKey = "button";
const char* linkKey = "link";
const char* bootStateKey = "bstate";
const char* scheduleKey = "sched";
const char* scheduleAddKey = "schedadd";
const char* scheduleDelKey = "scheddel";
const char* elemKey = "entry";
const char* calibrateKey = "cal";
const char* resetEnergyKey = "resetEnergy";

#if ENABLE_HLW8012
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
void CONTROLLER_CLASS_NAME::calibrate (unsigned int power, unsigned int voltage) {
	// Let some time to register values
    unsigned long timeout = millis ();
    // while ((millis () - timeout) < 10000) {
    //     //delay (1);
    //     yield ();
    //     ESP.wdtFeed ();
    // }

	// Calibrate using a 60W bulb (pure resistive) on a 230V line
#if !TEST_MODE
    hlw8012.expectedActivePower (power);
    hlw8012.expectedVoltage (voltage);
	hlw8012.expectedCurrent ((double)power / (double)voltage);
#endif // TEST_MODE
	// TODO: Show corrected factors
	//debugW ("[HLW] New current multiplier : %f", hlw8012.getCurrentMultiplier ());
	//debugW ("[HLW] New voltage multiplier : %f", hlw8012.getVoltageMultiplier ());
	//debugW ("[HLW] New power multiplier   : %f", hlw8012.getPowerMultiplier ());
}
#endif // ENABLE_HLW8012

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

	DEBUG_INFO ("Command: %d = %s", command, command == nodeMessageType_t::DOWNSTREAM_DATA_GET ? "GET" : "SET");

	// Dump debug data
	size_t strLen = measureJson (doc) + 1;
	char* strBuffer = (char*)malloc (strLen);
	serializeJson (doc, strBuffer, strLen);
	DEBUG_DBG ("Data: %s", strBuffer);
	free (strBuffer);

	// Check cmd field on JSON data
	if (!doc.containsKey (commandKey)) {
		DEBUG_WARN ("Wrong format");
		return false;
	}

	if (command == nodeMessageType_t::DOWNSTREAM_DATA_GET) {
		if (!strcmp (doc[commandKey], relayKey)) {
			DEBUG_INFO ("Request relay status. Relay = %s", relays->get(0)? "ON" : "OFF");
			if (!sendRelayStatus ()) {
				DEBUG_WARN ("Error sending relay status");
				return false;
			}
        } else if (!strcmp (doc[commandKey], scheduleKey)) {
            DEBUG_INFO ("Request schedule list");
            int index = -1;
            if (doc.containsKey (elemKey)) {
                index = doc[elemKey].as<int> ();
            }
            char* schedStr = NULL;;
            if (index < 0) {
                schedStr = scheduler->getJsonChr ();
            } else if (index < SCHED_MAX_ENTRIES) {
                schedStr = scheduler->getJsonChr (index);
            }
            if (!schedStr){
                DEBUG_WARN ("Error in schedule list");
                return false;
            }
            if (!sendSchedulerList (schedStr)) {
                DEBUG_WARN ("Error sending schedule list");
                return false;
            }

        } else if (!strcmp (doc[commandKey], bootStateKey)) {
			DEBUG_INFO ("Request boot status configuration. Boot = %d",
                        relays->getBootStatus());
			if (!sendBootStatus ()) {
				DEBUG_WARN ("Error sending boot status configuration");
				return false;
			}

		}
	}

	if (command == nodeMessageType_t::DOWNSTREAM_DATA_SET) {
        int index = 0;
		if (!strcmp (doc[commandKey], relayKey)) {
			if (!doc.containsKey (relayKey)) {
				DEBUG_WARN ("Wrong format");
				return false;
			}
            if (doc.containsKey ("idx")){
                index = doc["idx"].as<int> ();
            }
			char temp[200];
			serializeJson (doc, temp, 200);
			DEBUG_DBG ("%s", temp);
            DEBUG_INFO ("Set relay status. Relay %d = %d", index, doc[relayKey].as<int> ());
			if (doc[relayKey].is<int> ()) {
				int value = doc[relayKey].as<int> ();
				DEBUG_DBG ("%s is %s", relayKey, value.c_str());
                if (value == 0) {
                    DEBUG_INFO ("setRelay (false)");
                    relays->set (index, false);
                } else if (value == 1) {
                    DEBUG_INFO ("setRelay (true)");
                    relays->set (index, true);
                } else if (value == 2) {
                    relays->toggle (index);
                } else {
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


        } else if (!strcmp (doc[commandKey], scheduleAddKey)){
            /****
            {
                "cmd":"schedadd",
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
            if (sched_entry.hour > 23 || sched_entry.hour > 59) {
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

            DEBUG_INFO ("Set schedule Time = %u:%u (%s). Mask " BYTE_TO_BINARY_PATTERN ". Action %d in relay %d. %s",
                        sched_entry.hour, sched_entry.minute, sched_entry.repeat ? "repeat" : "single", 
                        BYTE_TO_BINARY (sched_entry.weekMask), sched_entry.action, sched_entry.index, sched_entry.enabled?"Enabled":"Disabled");
            
            int result = unidentifiedError;
            
            if (doc.containsKey (elemKey)) {
                int entry = -1;
                entry = doc[elemKey].as<int> ();
                if (entry >= 0 || entry < SCHED_MAX_ENTRIES) {
                    DEBUG_DBG ("Entry replaced");
                    result = scheduler->replace (entry, &sched_entry);
                } else { 
                    DEBUG_WARN ("Wrong entry value");
                    return false;
                }
            } else {
                result = scheduler->add (&sched_entry);
            }
            DEBUG_DBG ("Result = %d", result);
            if (result >= 0){
                return scheduler->save ();
            }
        } else if (!strcmp (doc[commandKey], scheduleDelKey)) {
            DEBUG_DBG ("Remove Schedule request");
            if (!doc.containsKey (elemKey)) {
                DEBUG_WARN ("Wrong format");
                return false;
            }
            int entry = -1;
            int result = unidentifiedError;
            entry = doc[elemKey].as<int> ();
            if (entry >= 0 || entry < SCHED_MAX_ENTRIES) {
                DEBUG_DBG ("Entry replaced");
                result = scheduler->remove (entry);
            } else {
                DEBUG_WARN ("Wrong entry value");
                return false;
            }
            DEBUG_DBG ("Result = %d", result);
            if (result >= 0) {
                return scheduler->save ();
            }
        } else if (!strcmp (doc[commandKey], bootStateKey)) {
			if (!doc.containsKey (bootStateKey)) {
				DEBUG_WARN ("Wrong format");
				return false;
			}
			DEBUG_INFO ("Set boot status = %d", doc[bootStateKey].as<int> ());
            bootRelayStatus_t newBootStatus;
            switch (doc[bootStateKey].as<int> ()){
                case 1:
                    newBootStatus = BOOT_ON;
                    break;
                case 2:
                    newBootStatus = SAVE_RELAY_STATUS;
                    break;
                default:
                    newBootStatus = BOOT_OFF;
                    break;
            }
            relays->setBootStatus (newBootStatus);

			if (!sendBootStatus ()) {
				DEBUG_WARN ("Error sending boot status configuration");
				return false;
			}

        }
#if ENABLE_HLW8012
        else if (!strcmp (doc[commandKey], calibrateKey)) {
            unsigned int power;
            unsigned int voltage;
            if (doc.containsKey ("pow") && doc["pow"].is<int> ()) {
                power = doc["pow"].as<int> ();
                if (doc.containsKey ("volt") && doc["volt"].is<int> ()) {
                    voltage = doc["volt"];
                    calibrate (power, voltage);
                } else {
                    calibrate (power);
                }
            } else {
                calibrate ();
            }
        } else if (!strcmp (doc[commandKey], resetEnergyKey)) {
            hlw8012.resetEnergy ();
        }
#endif // ENABLE_HLW8012
    }

	return true;
}


bool CONTROLLER_CLASS_NAME::sendCommandResp (const char* command, bool result) {
	// Respond to command with a result: true if successful, false if failed 
	return true;
}

void CONTROLLER_CLASS_NAME::sendButtonEvent (uint8_t pin, uint8_t event, uint8_t count, uint16_t length) {
	const size_t capacity = JSON_OBJECT_SIZE (5);
	DynamicJsonDocument json (capacity);
	json[commandKey] = buttonKey;
	json[buttonKey] = pin;
	json["event"] = event;
	json["count"] = count;
    json["len"] = length < LONG_PUSH_THRESHOLD ? "short" : "long";

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
    // uint8_t relayPins[] = { RELAY };
    // uint8_t relayOnStates[] = { RELAY_ON_POLARITY };
    // relays = new RelaySet (relayPins, relayOnStates, NUM_RELAYS);

    scheduler->onEvent (std::bind (&CONTROLLER_CLASS_NAME::onSchedulerEvent, this, _1));

	// Send a 'hello' message when initalizing is finished
	sendStartAnouncement ();
#if ENABLE_HLW8012 && !TEST_MODE
	hlw8012.begin (HLW_CF, HLW_CF1, HLW_SEL, CURRENT_MODE, true);
	hlw8012.setResistors (CURRENT_RESISTOR, VOLTAGE_RESISTOR_UPSTREAM, VOLTAGE_RESISTOR_DOWNSTREAM);
	hlw8012.setCurrentMultiplier (HLW8012_CURRENT_RATIO);
	hlw8012.setPowerMultiplier (HLW8012_POWER_RATIO);
	hlw8012.setVoltageMultiplier (HLW8012_VOLTAGE_RATIO);
	setInterrupts ();
#endif // ENABLE_HLW8012

    haCallQueue.push (std::bind (&CONTROLLER_CLASS_NAME::buildHACurrent, this));
    haCallQueue.push (std::bind (&CONTROLLER_CLASS_NAME::buildHAdCurrent, this));
    haCallQueue.push (std::bind (&CONTROLLER_CLASS_NAME::buildHAPFactor, this));
    haCallQueue.push (std::bind (&CONTROLLER_CLASS_NAME::buildHAPowerVA, this));
    haCallQueue.push (std::bind (&CONTROLLER_CLASS_NAME::buildHAPowerW, this));
    haCallQueue.push (std::bind (&CONTROLLER_CLASS_NAME::buildHAdPower, this));
    haCallQueue.push (std::bind (&CONTROLLER_CLASS_NAME::buildHAEnergyWh, this));
    haCallQueue.push (std::bind (&CONTROLLER_CLASS_NAME::buildHAVoltage, this));
    haCallQueue.push (std::bind (&CONTROLLER_CLASS_NAME::buildHASwitch, this));
    haCallQueue.push (std::bind (&CONTROLLER_CLASS_NAME::buildHAShortButton, this));
    haCallQueue.push (std::bind (&CONTROLLER_CLASS_NAME::buildHALongButton, this));

    //relayStatus = relays->get (0);
	//digitalWrite (RELAY_LED, relayStatus);
    relays->begin ();

	if (!sendRelayStatus ()) {
		DEBUG_WARN ("Error sending relay status");
	}

	DEBUG_DBG ("Finish begin");

// If your node should sleep after sending data do all remaining tasks here

}

#if ENABLE_HLW8012
void CONTROLLER_CLASS_NAME::sendHLWmeasurement () {
	const size_t capacity = JSON_OBJECT_SIZE (10);
	DynamicJsonDocument json (capacity);
#if !TEST_MODE
	json["Power_W"] = hlw8012.getActivePower ();
	json["Power_VA"] = hlw8012.getApparentPower ();
	json["P_Factor"] = (int)(100 * hlw8012.getPowerFactor ());
	unsigned int voltage = hlw8012.getVoltage ();
	json["Voltage"] = voltage;
    json["Current"] = roundf (hlw8012.getCurrent () * 1000) / 1000;
	double energy = (double)hlw8012.getEnergy () / 3600.0;
#else
    json["Power_W"] = 888;
    json["Power_VA"] = 77;
    json["P_Factor"] = 99;
    unsigned int voltage = hlw8012.getVoltage ();
    json["Voltage"] = 222;
    json["Current"] = 66;
    double energy = 33;
#endif // TEST_MODE
    json["Wh"] = roundf (energy * 100) / 100;
	if (voltage != 0) {
        const int period = 3600 * 1000 / (millis () - lastGotEnergy);
        double dcurrent = (energy - lastEnergy) * period / voltage;
        if (dcurrent > 0) {
            json["dcurrent"] = roundf (dcurrent * 1000) / 1000;
            double dW = dcurrent * (double)voltage;
            json["dW"] = roundf (dW * 1000) / 1000;
        }
    }
    String key = relayKey;
    for (uint i = 0; i < NUM_RELAYS; i++) {
        key += i;
        json[key.c_str ()] = relays->get (i) ? 1 : 0;
    }
    //json["rly"] = relays->get(0);
    lastEnergy = energy;
    lastGotEnergy = millis ();
	sendJson (json);
}
#endif // ENABLE_HLW8012

void CONTROLLER_CLASS_NAME::onSchedulerEvent (sched_event_t event) {
    DEBUG_INFO ("Scheduler event %d, index %d, repeat %d", event.action, event.index, event.repeat);
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
    
    scheduler->loop ();

#if ENABLE_HLW8012
    static unsigned long last = millis ();
	if ((millis () - last) > UPDATE_TIME && !enigmaIotNode->getOTArunning()) {
		last = millis ();
		sendHLWmeasurement ();
	}
#endif // ENABLE_HLW8012

	// TODO: Sync LED with relay
	digitalWrite (RELAY_LED, relays->get(0) ? LED_ON : LED_OFF);
    
    // static time_t last = millis ();
    // if (millis () - last > 1000) {
    //     last = millis ();

    //     time_t now = time (NULL);
    //     Serial.printf ("Current time %s", ctime (&now));
    // }

}

CONTROLLER_CLASS_NAME::~CONTROLLER_CLASS_NAME () {
	// It your class uses dynamic data free it up here
	// This is normally not needed but it is a good practice
}

void CONTROLLER_CLASS_NAME::configManagerStart () {
    DEBUG_INFO ("==== CCost Controller Configuration start ====");
    // If you need to add custom configuration parameters do it here

    bootStatusParam = new AsyncWiFiManagerParameter ("bootStatus", "Boot Relay Status", "", 6, "required type=\"text\" list=\"bootStatusList\" pattern=\"^ON$|^OFF$|^SAVE$\"");
    bootStatusListParam = new AsyncWiFiManagerParameter ("<datalist id=\"bootStatusList\">" \
                                                         "<option value = \"OFF\" >" \
                                                         "<option value = \"ON\">" \
                                                         "<option value = \"SAVE\">" \
                                                         "</datalist>");

    EnigmaIOTNode.addWiFiManagerParameter (bootStatusListParam);
    EnigmaIOTNode.addWiFiManagerParameter (bootStatusParam);
}

void CONTROLLER_CLASS_NAME::configManagerExit (bool status) {
    DEBUG_INFO ("==== CCost Controller Configuration result ====");
    // You can read configuration paramenter values here
    DEBUG_INFO ("Boot Relay Status: %s", bootStatusParam->getValue ());

    // TODO: Finish bootStatusParam analysis

    if (status) {
        if (!strncmp (bootStatusParam->getValue (), "ON", 6)) {
            relays->setBootStatus (bootRelayStatus_t::BOOT_ON);
        } else if (!strncmp (bootStatusParam->getValue (), "SAVE", 6)) {
            relays->setBootStatus (bootRelayStatus_t::SAVE_RELAY_STATUS);
        } else {
            relays->setBootStatus (bootRelayStatus_t::BOOT_OFF);
        }

        if (!saveConfig ()) {
            DEBUG_ERROR ("Error writting blind controller config to filesystem.");
        } else {
            DEBUG_INFO ("Configuration stored");
        }
    } else {
        DEBUG_INFO ("Configuration does not need to be saved");
    }
}

bool CONTROLLER_CLASS_NAME::loadConfig () {
	// If you need to read custom configuration data do it here
    bool result = true;
    if (!relays->load ()) {
        DEBUG_WARN ("Error loading relays info");
        result = false;
    }
    if (!scheduler->load ()) {
        DEBUG_WARN ("Error loading schedule info");
        result = false;
    }
    DEBUG_INFO ("Configuration loaded. Result: %d", result);
    return result;

}

bool CONTROLLER_CLASS_NAME::saveConfig () {
	// If you need to save custom configuration data do it here
	// Save measure period 3-X, initial_status, last_relay_status
    bool result = true;
    if (!relays->save ()) {
        result = false;
    }
    if (!scheduler->save ()) {
        result = false;
    }
    return result;
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

bool CONTROLLER_CLASS_NAME::sendSchedulerList (char* list) {
    DEBUG_DBG ("Schedule list\n%s", list);
    
    DynamicJsonDocument json (1024);
    
    json["cmd"] = scheduleKey;
    
    DynamicJsonDocument schedListArray (1024);
    
    DeserializationError result = deserializeJson (schedListArray, list);
    
    json[scheduleKey] = schedListArray;
    
    if (result != DeserializationError::Ok) {
        DEBUG_WARN ("JSON string deserialization error %s", result.c_str ());
        return false;
    }
    
    return sendJson (json);
    
}

bool CONTROLLER_CLASS_NAME::sendBootStatus () {
    const size_t capacity = JSON_OBJECT_SIZE (2);
    DynamicJsonDocument json (capacity);

    json[commandKey] = bootStateKey;
    int bootStatus = relays->getBootStatus ();
    json[bootStateKey] = bootStatus;

    return sendJson (json);
}

void CONTROLLER_CLASS_NAME::buildHASwitch () {

    HASwitch* haEntity = new HASwitch ();

    uint8_t* msgPackBuffer;

    if (!haEntity) {
        DEBUG_WARN ("JSON object instance does not exist");
        return; // If json object was not created
    }

    haEntity->setNameSufix ("switch");
    haEntity->setPayloadOff ("{\"cmd\":\"swi\",\"swi\":0,\"idx\":0}");
    haEntity->setPayloadOn ("{\"cmd\":\"swi\",\"swi\":1,\"idx\":0}");
    haEntity->setValueField ("swi0");
    haEntity->setStateOff (0);
    haEntity->setStateOn (1);
    
    size_t bufferLen = haEntity->measureMessage ();

    msgPackBuffer = (uint8_t*)malloc (bufferLen);

    size_t len = haEntity->getAnounceMessage (bufferLen, msgPackBuffer);

    DEBUG_WARN ("Resulting MSG pack length: %d", len);

    if (!sendHADiscovery (msgPackBuffer, len)) {
        DEBUG_WARN ("Error sending HA discovery message");
    }

    if (haEntity) {
        delete (haEntity);
    }
    if (msgPackBuffer) {
        free (msgPackBuffer);
    }
}

void CONTROLLER_CLASS_NAME::buildHACurrent () {

    HASensor* haEntity = new HASensor ();

    uint8_t* msgPackBuffer;

    if (!haEntity) {
        DEBUG_WARN ("JSON object instance does not exist");
        return; // If json object was not created
    }

    haEntity->setNameSufix ("current");
    haEntity->setDeviceClass (sensor_current);
    haEntity->setExpireTime (600);
    haEntity->setValueField ("Current");
    haEntity->setUnitOfMeasurement ("A");
    
    size_t bufferLen = haEntity->measureMessage ();

    msgPackBuffer = (uint8_t*)malloc (bufferLen);

    size_t len = haEntity->getAnounceMessage (bufferLen, msgPackBuffer);

    DEBUG_WARN ("Resulting MSG pack length: %d", len);

    if (!sendHADiscovery (msgPackBuffer, len)) {
        DEBUG_WARN ("Error sending HA discovery message");
    }

    if (haEntity) {
        delete (haEntity);
    }
    if (msgPackBuffer) {
        free (msgPackBuffer);
    }

}

void CONTROLLER_CLASS_NAME::buildHAdCurrent () {

    HASensor* haEntity = new HASensor ();

    uint8_t* msgPackBuffer;

    if (!haEntity) {
        DEBUG_WARN ("JSON object instance does not exist");
        return; // If json object was not created
    }

    haEntity->setNameSufix ("dcurrent");
    haEntity->setDeviceClass (sensor_current);
    haEntity->setExpireTime (600);
    haEntity->setValueField ("dcurrent");
    haEntity->setUnitOfMeasurement ("A");

    size_t bufferLen = haEntity->measureMessage ();

    msgPackBuffer = (uint8_t*)malloc (bufferLen);

    size_t len = haEntity->getAnounceMessage (bufferLen, msgPackBuffer);

    DEBUG_WARN ("Resulting MSG pack length: %d", len);

    if (!sendHADiscovery (msgPackBuffer, len)) {
        DEBUG_WARN ("Error sending HA discovery message");
    }

    if (haEntity) {
        delete (haEntity);
    }
    if (msgPackBuffer) {
        free (msgPackBuffer);
    }

}

void CONTROLLER_CLASS_NAME::buildHAPowerW () {

    HASensor* haEntity = new HASensor ();

    uint8_t* msgPackBuffer;

    if (!haEntity) {
        DEBUG_WARN ("JSON object instance does not exist");
        return; // If json object was not created
    }

    haEntity->setNameSufix ("powerw");
    haEntity->setDeviceClass (sensor_power);
    haEntity->setExpireTime (600);
    haEntity->setValueField ("Power_W");
    haEntity->setUnitOfMeasurement ("W");
    haEntity->allowSendAttributes ();

    size_t bufferLen = haEntity->measureMessage ();

    msgPackBuffer = (uint8_t*)malloc (bufferLen);

    size_t len = haEntity->getAnounceMessage (bufferLen, msgPackBuffer);

    DEBUG_WARN ("Resulting MSG pack length: %d", len);

    if (!sendHADiscovery (msgPackBuffer, len)) {
        DEBUG_WARN ("Error sending HA discovery message");
    }

    if (haEntity) {
        delete (haEntity);
    }
    if (msgPackBuffer) {
        free (msgPackBuffer);
    }

}

void CONTROLLER_CLASS_NAME::buildHAdPower () {

    HASensor* haEntity = new HASensor ();

    uint8_t* msgPackBuffer;

    if (!haEntity) {
        DEBUG_WARN ("JSON object instance does not exist");
        return; // If json object was not created
    }

    haEntity->setNameSufix ("dpowerw");
    haEntity->setDeviceClass (sensor_power);
    haEntity->setExpireTime (600);
    haEntity->setValueField ("dW");
    haEntity->setUnitOfMeasurement ("W");

    size_t bufferLen = haEntity->measureMessage ();

    msgPackBuffer = (uint8_t*)malloc (bufferLen);

    size_t len = haEntity->getAnounceMessage (bufferLen, msgPackBuffer);

    DEBUG_WARN ("Resulting MSG pack length: %d", len);

    if (!sendHADiscovery (msgPackBuffer, len)) {
        DEBUG_WARN ("Error sending HA discovery message");
    }

    if (haEntity) {
        delete (haEntity);
    }
    if (msgPackBuffer) {
        free (msgPackBuffer);
    }

}

void CONTROLLER_CLASS_NAME::buildHAEnergyWh () {

    HASensor* haEntity = new HASensor ();

    uint8_t* msgPackBuffer;

    if (!haEntity) {
        DEBUG_WARN ("JSON object instance does not exist");
        return; // If json object was not created
    }

    haEntity->setNameSufix ("energy");
    haEntity->setDeviceClass (sensor_energy);
    haEntity->setExpireTime (600);
    haEntity->setValueField ("Wh");
    haEntity->setUnitOfMeasurement ("Wh");

    size_t bufferLen = haEntity->measureMessage ();

    msgPackBuffer = (uint8_t*)malloc (bufferLen);

    size_t len = haEntity->getAnounceMessage (bufferLen, msgPackBuffer);

    DEBUG_WARN ("Resulting MSG pack length: %d", len);

    if (!sendHADiscovery (msgPackBuffer, len)) {
        DEBUG_WARN ("Error sending HA discovery message");
    }

    if (haEntity) {
        delete (haEntity);
    }
    if (msgPackBuffer) {
        free (msgPackBuffer);
    }

}

void CONTROLLER_CLASS_NAME::buildHAPowerVA () {

    HASensor* haEntity = new HASensor ();

    uint8_t* msgPackBuffer;

    if (!haEntity) {
        DEBUG_WARN ("JSON object instance does not exist");
        return; // If json object was not created
    }

    haEntity->setNameSufix ("powerva");
    haEntity->setDeviceClass (sensor_power);
    haEntity->setExpireTime (600);
    haEntity->setValueField ("Power_VA");
    haEntity->setUnitOfMeasurement ("VA");

    size_t bufferLen = haEntity->measureMessage ();

    msgPackBuffer = (uint8_t*)malloc (bufferLen);

    size_t len = haEntity->getAnounceMessage (bufferLen, msgPackBuffer);

    DEBUG_WARN ("Resulting MSG pack length: %d", len);

    if (!sendHADiscovery (msgPackBuffer, len)) {
        DEBUG_WARN ("Error sending HA discovery message");
    }

    if (haEntity) {
        delete (haEntity);
    }
    if (msgPackBuffer) {
        free (msgPackBuffer);
    }

}

void CONTROLLER_CLASS_NAME::buildHAVoltage () {

    HASensor* haEntity = new HASensor ();

    uint8_t* msgPackBuffer;

    if (!haEntity) {
        DEBUG_WARN ("JSON object instance does not exist");
        return; // If json object was not created
    }

    haEntity->setNameSufix ("volt");
    haEntity->setDeviceClass (sensor_voltage);
    haEntity->setExpireTime (600);
    haEntity->setValueField ("Voltage");
    haEntity->setUnitOfMeasurement ("V");

    size_t bufferLen = haEntity->measureMessage ();

    msgPackBuffer = (uint8_t*)malloc (bufferLen);

    size_t len = haEntity->getAnounceMessage (bufferLen, msgPackBuffer);

    DEBUG_WARN ("Resulting MSG pack length: %d", len);

    if (!sendHADiscovery (msgPackBuffer, len)) {
        DEBUG_WARN ("Error sending HA discovery message");
    }

    if (haEntity) {
        delete (haEntity);
    }
    if (msgPackBuffer) {
        free (msgPackBuffer);
    }

}

void CONTROLLER_CLASS_NAME::buildHAPFactor () {

    HASensor* haEntity = new HASensor ();

    uint8_t* msgPackBuffer;

    if (!haEntity) {
        DEBUG_WARN ("JSON object instance does not exist");
        return; // If json object was not created
    }

    haEntity->setNameSufix ("pf");
    haEntity->setDeviceClass (sensor_power_factor);
    haEntity->setExpireTime (600);
    haEntity->setValueField ("P_Factor");
    haEntity->setUnitOfMeasurement ("%");

    size_t bufferLen = haEntity->measureMessage ();

    msgPackBuffer = (uint8_t*)malloc (bufferLen);

    size_t len = haEntity->getAnounceMessage (bufferLen, msgPackBuffer);

    DEBUG_WARN ("Resulting MSG pack length: %d", len);

    if (!sendHADiscovery (msgPackBuffer, len)) {
        DEBUG_WARN ("Error sending HA discovery message");
    }

    if (haEntity) {
        delete (haEntity);
    }
    if (msgPackBuffer) {
        free (msgPackBuffer);
    }

}

void CONTROLLER_CLASS_NAME::buildHAShortButton () {
    HATrigger* haEntity = new HATrigger ();

    uint8_t* msgPackBuffer;

    if (!haEntity) {
        DEBUG_WARN ("JSON object instance does not exist");
        return; // If json object was not created
    }

    haEntity->setNameSufix ("short");
    haEntity->setType (button_short_press);
    haEntity->setSubtype (button_1);
    char pld[100];
    snprintf (pld, 100, "{\"cmd\":\"button\",\"button\":%d,\"event\":3,\"count\":1,\"len\":\"short\"}", BUTTON);
    haEntity->setPayload (pld);

    size_t bufferLen = haEntity->measureMessage ();

    msgPackBuffer = (uint8_t*)malloc (bufferLen);

    size_t len = haEntity->getAnounceMessage (bufferLen, msgPackBuffer);

    DEBUG_WARN ("Resulting MSG pack length: %d", len);

    if (!sendHADiscovery (msgPackBuffer, len)) {
        DEBUG_WARN ("Error sending HA discovery message");
    }

    if (haEntity) {
        delete (haEntity);
    }
    if (msgPackBuffer) {
        free (msgPackBuffer);
    }
}

void CONTROLLER_CLASS_NAME::buildHALongButton () {
    HATrigger* haEntity = new HATrigger ();

    uint8_t* msgPackBuffer;

    if (!haEntity) {
        DEBUG_WARN ("JSON object instance does not exist");
        return; // If json object was not created
    }

    haEntity->setNameSufix ("long");
    haEntity->setType (button_long_press);
    haEntity->setSubtype (button_1);
    char pld[100];
    snprintf (pld, 100, "{\"cmd\":\"button\",\"button\":%d,\"event\":3,\"count\":1,\"len\":\"long\"}", BUTTON);
    haEntity->setPayload (pld);
    size_t bufferLen = haEntity->measureMessage ();

    msgPackBuffer = (uint8_t*)malloc (bufferLen);

    size_t len = haEntity->getAnounceMessage (bufferLen, msgPackBuffer);

    DEBUG_WARN ("Resulting MSG pack length: %d", len);

    if (!sendHADiscovery (msgPackBuffer, len)) {
        DEBUG_WARN ("Error sending HA discovery message");
    }

    if (haEntity) {
        delete (haEntity);
    }
    if (msgPackBuffer) {
        free (msgPackBuffer);
    }
}