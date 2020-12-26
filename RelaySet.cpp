#include "RelaySet.h"

RelaySet::RelaySet (uint8_t* pins, uint8_t* onState, uint8_t num) {
    if (num > 0 && num <= MAX_RELAYS) {
        relays = (relay_t*)malloc (sizeof (relay_t) * num);
        numRelays = num;
        //DEBUG_WARN ("Relay Set instance with %u relays", numRelays);
        for (uint8_t i = 0; i < num; i++) {
            relays[i].pin = pins[i];
            relays[i].onState = onState[i];
            pinMode (pins[i], OUTPUT);
            bool pinState = digitalRead (relays[i].pin);
            relays[i].status = relays[i].onState ? pinState : !pinState;
            //DEBUG_WARN ("Relay %u is %s", i, relays[i].status);
        }
    }
}

void RelaySet::set (uint8_t index, bool status) {
    // Serial.printf ("Set relay #%d to %s\n", index, status ? "ON" : "OFF");
    if (index < numRelays) {
        //bool newStatus = relays[index].onState ? status : !status;
        relays[index].status = status;
        digitalWrite (relays[index].pin, relays[index].onState ? status : !status);
        // Serial.println("OK");
    }
}

bool RelaySet::toggle (uint8_t index) {
    if (index < numRelays) {
        relays[index].status = !relays[index].status;
        //relays[index].status = newStatus;
        //Serial.printf ("Toggle %d to %d\n", index, relays[index].status);
        digitalWrite (relays[index].pin, relays[index].onState ? relays[index].status : !relays[index].status);
        return relays[index].status;
    }
    return false;
}

bool RelaySet::get (uint8_t index) {
    bool status = false;
    if (index < numRelays) {
        status = digitalRead (relays[index].pin);
    }
    return relays[index].onState ? status : !status;
}

void RelaySet::setAll (bool status) {
    for (uint8_t i = 0; i < numRelays; i++){
        bool newStatus = relays[i].onState ? status : !status;
        digitalWrite (relays[i].pin, newStatus);
    }
}