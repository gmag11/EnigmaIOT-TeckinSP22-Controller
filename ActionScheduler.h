#ifndef ACTION_SCHED_H
#define ACTION_SCHED_H

#include <Arduino.h>
#include <sys/time.h>

#ifndef SCHED_MAX_ENTRIES
#define SCHED_MAX_ENTRIES 20
#endif

typedef struct {
    int action;
    int index;
    tm time;
    bool repeat;
    bool executed;
} schedule_t;

class ActionScheduler {
    schedule_t entries[SCHED_MAX_ENTRIES];
    
public:
    int8_t add (schedule_t* entry);
    
    void remove (uint8_t index);
    
    bool replace (uint8_t index, schedule_t* entry);
    
    schedule_t* get (uint8_t index);
    
    schedule_t* loop ();
};

#endif // ACTION_SCHED_H