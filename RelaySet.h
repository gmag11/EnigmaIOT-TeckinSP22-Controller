#ifndef RELAY_SET_H
#define RELAY_SET_H

#include <Arduino.h>
// #include <EnigmaIoTconfig.h>
// #include <EnigmaIOTdebug.h>

#define MAX_RELAYS 10

struct relay_t {
    unsigned int pin;
    bool status;
    bool onState;
};

class RelaySet {
protected:
    relay_t* relays;
    int numRelays = 0;

public:
    RelaySet (uint8_t* pins, uint8_t* onState, uint8_t num);

    unsigned int getNumRelays () {
        return numRelays;
    }

    void set (uint8_t relay, bool status);
    
    void setAll (bool status);
    
    bool get (uint8_t index);
    
    bool toggle (uint8_t index);
};


#endif // RELAY_SET_H