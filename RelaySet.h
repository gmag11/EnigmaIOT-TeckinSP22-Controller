#ifndef RELAY_SET_H
#define RELAY_SET_H

#include <Arduino.h>
#include <FS.h>
// #include <EnigmaIoTconfig.h>
// #include <EnigmaIOTdebug.h>

#define MAX_RELAYS 10
constexpr auto RELAY_FILE = "/relays.json"; ///< @brief Custom relay status configuration file name

typedef enum {
    BOOT_OFF = 0,
    BOOT_ON = 1,
    SAVE_RELAY_STATUS = 2
} bootRelayStatus_t;

struct relay_t {
    unsigned int pin;
    bool status;
    bool onState;
};

struct relayContext_t {
    bootRelayStatus_t bootStatus = BOOT_OFF;
    bool lastStatus = false;
};

class RelaySet {
protected:
    relay_t* relays;
    relayContext_t* bootStatus;
    int numRelays = 0;
    FS* filesystem;

    bool setBootStatus (uint8_t index, bootRelayStatus_t status) {
        if (status < BOOT_OFF || status > SAVE_RELAY_STATUS || index > numRelays) {
            Serial.printf ("setBootStatus error\n");
            return false;
        }
        bootStatus[index].bootStatus = status;
        //save ();
        return true;
    }

    bootRelayStatus_t getBootStatus (uint8_t index) {
        if (index < numRelays) {
            return bootStatus[index].bootStatus;
        } else {
            return BOOT_OFF;
        }
    }

public:
    RelaySet (uint8_t* pins, uint8_t* onState, uint8_t num, FS* fs = NULL);

    unsigned int getNumRelays () {
        return numRelays;
    }

    void begin ();

    void set (uint8_t relay, bool status);
    
    void setAll (bool status);
    
    bool get (uint8_t index);
    
    bool toggle (uint8_t index);
    
    bool setBootStatus (bootRelayStatus_t status) {
        if (status < BOOT_OFF || status > SAVE_RELAY_STATUS) {
            Serial.printf ("setBootStatus error\n");
            return false;
        }

        for (uint8_t i = 0; i < numRelays; i++){
            setBootStatus (i, status);
        }
        return save ();
    }

    bootRelayStatus_t getBootStatus () {
        return getBootStatus (0);
    }
    
    bool save ();
    
    bool load ();
};


#endif // RELAY_SET_H