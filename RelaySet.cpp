#include "RelaySet.h"

RelaySet::RelaySet (uint8_t* pins, uint8_t* onState, uint8_t num, FS* fs) {
    filesystem = fs;
    if (num > 0 && num <= MAX_RELAYS) {
        relays = (relay_t*)malloc (sizeof (relay_t) * num);
        bootStatus = (relayContext_t*)malloc (sizeof (relayContext_t) * num);
        numRelays = num;
        Serial.printf ("%s:%d Relay Set instance with %u relays\n", __FUNCTION__, __LINE__, numRelays);
        for (uint8_t i = 0; i < num; i++) {
            relays[i].pin = pins[i];
            relays[i].onState = onState[i];
            bootStatus[i].bootStatus = BOOT_OFF;
            bootStatus[i].lastStatus = false;
            // pinMode (pins[i], OUTPUT);
            // bool pinState = digitalRead (relays[i].pin);
            // relays[i].status = relays[i].onState ? pinState : !pinState;
            //DEBUG_WARN ("Relay %u is %s", i, relays[i].status);
        }
    }
}

void RelaySet::begin (){
    for (uint8_t i = 0; i < numRelays; i++) {
        bool newStatus = false;
        
        pinMode (relays[i].pin, OUTPUT);
        bool pinState = get (i);/*digitalRead (relays[i].pin);*/
        relays[i].status = relays[i].onState ? pinState : !pinState;
        Serial.printf ("%s:%d configured relay %d Status: %d\n", __FUNCTION__, __LINE__, i, relays[i].status);
        Serial.printf ("%s:%d Boot status %d\n", __FUNCTION__, __LINE__, bootStatus[i].bootStatus);

        switch(bootStatus[i].bootStatus) {
            case BOOT_OFF:
                newStatus = false;
                break;
            case BOOT_ON:
                newStatus = true;
                break;
            case SAVE_RELAY_STATUS:
                newStatus = bootStatus[i].lastStatus;
                break;
        }
        //digitalWrite (relays[i].pin, relays[i].onState ? newStatus : !newStatus);
        set (i, newStatus);
        Serial.printf ("%s:%d configured relay %d Last: %d Status: %d\n", __FUNCTION__, __LINE__, i, bootStatus[i].lastStatus, relays[i].status);

    }
}

void RelaySet::set (uint8_t index, bool status) {
    Serial.printf ("%s:%d Set relay #%d to %d\n", __FUNCTION__, __LINE__, index, status);
    if (index < numRelays) {
        //bool newStatus = relays[index].onState ? status : !status;
        relays[index].status = status;
        digitalWrite (relays[index].pin, relays[index].onState ? status : !status);
        bootStatus[index].lastStatus = status;
        if (bootStatus[index].bootStatus == SAVE_RELAY_STATUS){
            Serial.printf ("%s:%d Pin %d saved: %d\n", __FUNCTION__, __LINE__, index, status);
            save ();
        }
        // Serial.println("OK");
    }
}

bool RelaySet::toggle (uint8_t index) {
    if (index < numRelays) {
        //relays[index].status = !relays[index].status;
        //relays[index].status = newStatus;
        Serial.printf ("%s:%d Toggle %d to %d\n", __FUNCTION__, __LINE__, index, relays[index].status);
        //digitalWrite (relays[index].pin, relays[index].onState ? relays[index].status : !relays[index].status);
        set (index, !relays[index].status);
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
        set (relays[i].pin, newStatus);
    }
}

bool RelaySet::load () {
    File relayFile;
    if (!filesystem) {
        return false;
    }
    if (!filesystem->exists (RELAY_FILE)) {
        return save ();
    }
    relayFile = filesystem->open (RELAY_FILE, "r");

    if (!relayFile) {
        return false;
    }
    if (relayFile.size () != numRelays * sizeof (relayContext_t)) {
        return false;
    }
    relayFile.read ((uint8_t*)bootStatus, numRelays * sizeof (relayContext_t));
    relayFile.close ();
    //Serial.printf ("Relay config loaded\n");
    return true;

}

bool RelaySet::save () {
    File relayFile;
    if (!filesystem) {
        return false;
    }
    relayFile = filesystem->open (RELAY_FILE, "w");

    if (!relayFile) {
        return false;
    }
    size_t size = relayFile.write ((uint8_t*)bootStatus, numRelays * sizeof (relayContext_t));
    relayFile.flush ();
    relayFile.close ();
    if (size == (numRelays * sizeof (relayContext_t))) {
        //Serial.printf ("Relay config saved\n");
        return true;
    } else {
        return false;
    }
}
