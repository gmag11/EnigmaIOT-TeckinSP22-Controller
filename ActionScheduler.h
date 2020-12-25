#ifndef ACTION_SCHED_H
#define ACTION_SCHED_H

#include <Arduino.h>
#include <sys/time.h>

#ifndef SCHED_MAX_ENTRIES
#define SCHED_MAX_ENTRIES 10
#endif

typedef enum {
    future = 2,
    past = 1,
    correct = 0,
    noFreeSlot = -1,
    invalidTime = -2,
    invalidWeekMask = -3,
    indexOutOfBounds = -4
} schedError_t;

typedef struct {
    int action;
    int index;
    uint8_t hour;
    uint8_t minute;
    uint8_t weekMask = 0;
    bool repeat = true;
    bool executed;
    bool enabled = true;
    bool used = false;
} schedule_t;

typedef struct {
    int action;
    int index;
    bool repeat = true;
} sched_event_t;

typedef std::function<void (sched_event_t)> onSchedEventCb_t; ///< @brief Event notifier callback

class ActionScheduler {
    schedule_t entries[SCHED_MAX_ENTRIES];
    onSchedEventCb_t callback;
    
public:
    int8_t add (schedule_t* entry);
    
    bool remove (uint8_t index);
    
    int8_t replace (uint8_t index, schedule_t* entry);
    
    schedule_t* get (uint8_t index);
    
    void loop ();
    
    char* getJsonChr ();
    
    void onEvent (onSchedEventCb_t handler){
        callback = handler;
    }
    
protected:
    
    int8_t searchNextFree ();
    
    int checkFutureEvent (schedule_t* entry);
    
    static time_t secSinceMidnight (tm* date);
    
    static tm* setFromHourMinute (uint8_t hour, uint8_t minute, tm* splitTime);
    
    static schedError_t checkEntry (schedule_t* entry);
    
    static bool checkWeekDay (uint8_t weekMask, int weekDay);
    
    static char* entry2str (schedule_t* entry);
};

#endif // ACTION_SCHED_H