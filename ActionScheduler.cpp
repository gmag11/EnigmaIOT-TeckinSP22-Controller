#include "ActionScheduler.h"

int8_t ActionScheduler::add (schedule_t* entry) {
    int8_t index = searchNextFree ();
    Serial.printf ("Index = %d\n", index);
    
    schedError_t result = checkEntry (entry);
    Serial.printf ("Entry is%s valid\n", result == correct ? "" : " not");
    
    if (result) {
        Serial.printf ("Error %d\n", result);
        return result;
    }
    
    Serial.printf ("Entry: -----\n %s\n", entry2str (entry));

    if (index >= 0) {
        //memcpy (&(entries[index]), entry, sizeof(schedule_t));
        entries[index] = *entry;
        entries[index].used = true;
        entries[index].executed = checkFutureEvent (&(entries[index])) == past;
        Serial.printf ("Added\n");
    } else {
        Serial.println ("List full");
    }
    
    return index;
}

char* ActionScheduler::entry2str (schedule_t* entry) {
    static char output[255];
    
    snprintf (output, 255, "Action: %d\nEnabled: %d Executed: %d\nHour: %u:%u Repeat: %d\nUsed: %d\nMask: " BYTE_TO_BINARY_PATTERN,
              entry->action, entry->enabled, entry->executed, entry->hour, entry->minute, entry->repeat, entry->used, BYTE_TO_BINARY(entry->weekMask));
    
    return output;
}

bool ActionScheduler::remove (uint8_t index) {
    if (index < SCHED_MAX_ENTRIES) {
        entries[index].used = false;
        return true;
    }
    return false;
}

int8_t ActionScheduler::replace (uint8_t index, schedule_t* entry) {
    schedError_t result = checkEntry (entry);

    if (!result) {
        return result;
    }

    if (index < SCHED_MAX_ENTRIES) {
        memccpy (&(entries[index]), entry, 1, sizeof (schedule_t));
        entries[index].used = true;
        return index;
    }
    return indexOutOfBounds;
}

schedule_t* ActionScheduler::get (uint8_t index){
    if (index < SCHED_MAX_ENTRIES) {
        return &(entries[index]);
    }
    return NULL;
}

int8_t ActionScheduler::searchNextFree () {
    Serial.printf ("SCHED_MAX_ENTRIES = %d\n", SCHED_MAX_ENTRIES);
    for (int8_t i = 0; i < SCHED_MAX_ENTRIES; i++){
        Serial.printf ("Entry %d is %s\n", i, entries[i].used? "used":"free");
        if (!entries[i].used){
            return i;
        }
    }
    
    return noFreeSlot;
}

void ActionScheduler::loop () {
    time_t currenttime = time (NULL);
    tm* splitTime = localtime (&currenttime);
    if (!splitTime) {
        return;
    }
    
    for (int8_t i = 0; i < SCHED_MAX_ENTRIES; i++) {
        if (entries[i].used) {
            bool today = checkWeekDay (entries[i].weekMask, splitTime->tm_wday);
            //Serial.printf ("loop: i %d enabled %d executed %d today %d repeat %d Alarm %02u:%02u Time %02u:%02u:%02u\n",
            //            i, entries[i].enabled, entries[i].executed, today, entries[i].repeat,
            //            entries[i].hour, entries[i].minute, splitTime->tm_hour, splitTime->tm_min, splitTime->tm_sec);
            if (entries[i].enabled &&
                !entries[i].executed &&
                today &&
                splitTime->tm_hour == entries[i].hour &&
                splitTime->tm_min == entries[i].minute) {
                    entries[i].executed = true; // Mark as executed
                    if (!entries[i].repeat) {
                        entries[i].enabled = false;
                        entries[i].used = false;
                    }
                    sched_event_t event;
                    event.action = entries[i].action;
                    event.index = entries[i].index;
                    event.repeat = entries[i].repeat;
                    if (callback){
                        callback (event);
                    }
                    //return &(entries[i]);
            }
            if (entries[i].enabled && entries[i].repeat &&
                (splitTime->tm_hour != entries[i].hour ||
                splitTime->tm_min != entries[i].minute) && !today) {
                    entries[i].executed = false;
            }
        }
    }
    //return NULL;
}

bool ActionScheduler::checkWeekDay (uint8_t weekMask, int weekDay) {
    if ((weekMask & 0b01111111) == 0 || weekDay < 0 || weekDay > 6) {
        return false;
    }
    int today = 1 << weekDay;
    
    bool result = (weekMask & today) != 0;
    
    //Serial.printf ("Today: %d => " BYTE_TO_BINARY_PATTERN ". weekMask " BYTE_TO_BINARY_PATTERN ". Match: %d\n",
    //                    weekDay,   BYTE_TO_BINARY (today),              BYTE_TO_BINARY(weekMask),     result);
    
    return result;
}

schedError_t ActionScheduler::checkEntry (schedule_t* entry) {
    if (entry->hour > 23 || entry->minute > 59) {
        return invalidTime;
    }
    if (entry->weekMask > 0b01111111U) { // 0 is valid for non repeat
        return invalidWeekMask;
    }
    return correct;
}


int ActionScheduler::checkFutureEvent (schedule_t* entry) {
    time_t currenttime = time (NULL);
    tm* splitTime = gmtime (&currenttime);
    if (!splitTime) {
        return invalidTime;
    }

    time_t currentSecSinceMidnight = secSinceMidnight (splitTime);
    Serial.printf ("Current time today: %ld\n", currentSecSinceMidnight);
    
    if (!setFromHourMinute(entry->hour,entry->minute,splitTime)){
        return invalidTime;
    }
    
        // TODO: Check week mask. If other day return false
    if (entry->weekMask == 0){
        entry->weekMask = 0b01111111;
    }
    
    Serial.printf ("Weekday %d WeekMask " BYTE_TO_BINARY_PATTERN " Today: " BYTE_TO_BINARY_PATTERN "\n", splitTime->tm_wday, BYTE_TO_BINARY (entry->weekMask), BYTE_TO_BINARY (1 << splitTime->tm_wday));
    if (!(entry->weekMask & (1 << splitTime->tm_wday))) {
        Serial.print ("Not today\nFuture\n");
        return future;
    } else {
        Serial.print ("Today\n");
    }
    
    time_t entrySecSinceMidnight = secSinceMidnight (splitTime);
    Serial.printf ("Alarm time today: %ld\n", entrySecSinceMidnight);

    if (entrySecSinceMidnight >= currentSecSinceMidnight) {
        Serial.println ("Future");
        return future;
    } else {
        Serial.println ("Past");
        return past;
    }
}

tm* ActionScheduler::setFromHourMinute (uint8_t hour, uint8_t minute, tm* splitTime) {
    if (hour > 23 || minute > 59) {
        return NULL;
    }
    splitTime->tm_hour = hour;
    splitTime->tm_isdst = 0;
    splitTime->tm_mday = 1;
    splitTime->tm_min = minute;
    splitTime->tm_mon = 0;
    splitTime->tm_sec = 0;
    splitTime->tm_year = 70;
    
    return splitTime;
}

time_t ActionScheduler::secSinceMidnight (tm* date) {
    tm tempDate = *date;
    
    tempDate.tm_mday = 1;
    tempDate.tm_mon = 0;
    tempDate.tm_sec = 0;
    //date->tm_wday = 0;
    tempDate.tm_year = 70;
        
    time_t unixtime = mktime (&tempDate);
    return unixtime;
}

char* ActionScheduler::getJsonChr (uint8_t index){
    static char output[256];
    int remaining = sizeof (output);
    int strIndex = 0;
    int count;
    
    if (index > SCHED_MAX_ENTRIES) {
        return NULL;
    }

    count = snprintf (output, remaining, "{");
    strIndex += count;
    remaining -= count;

    if (remaining > 0) {
        if (entries[index].used){
            count = snprintf (output + strIndex, remaining, "'index':%d,'action':%d,'hour':%u,'min':%u",
                              entries[index].index,
                              entries[index].action,
                              entries[index].hour,
                              entries[index].minute);
            strIndex += count;
            remaining -= count;
            if (!entries[index].enabled) {
                count = snprintf (output + strIndex, remaining, "',enabled':%d", entries[index].enabled);
                strIndex += count;
                remaining -= count;
            }

            if (!entries[index].repeat) {
                count = snprintf (output + strIndex, remaining, ",'repeat':%d", entries[index].repeat);
                strIndex += count;
                remaining -= count;
            }

            if (entries[index].weekMask != 0 && entries[index].weekMask != 127) {
                count = snprintf (output + strIndex, remaining, ",'weekmask':%u", entries[index].weekMask);
                strIndex += count;
                remaining -= count;
            }
        }
    }
    if (remaining > 0) {
        count = snprintf (output + strIndex, remaining, "}");
    }
    return output;
}

char* ActionScheduler::getJsonChr () {    
    static char output[1024];
    
    int remaining = sizeof (output);
    int strIndex = 0;
    int count;
    
    count = snprintf (output, remaining, "[");
    strIndex += count;
    remaining -= count;
        
    for (uint8_t i = 0; i < SCHED_MAX_ENTRIES; i++){
        //Serial.printf ("Index: %u, used: %d, remaining: %d\n", i, entries[i].used, remaining);
        if (remaining > 0) {
            char* element = getJsonChr (i);
            //strncpy (output + strIndex, element, remaining);
            count = snprintf (output + strIndex, remaining, "%s", element);
            strIndex += count;
            remaining -= count;
            // count = snprintf (output + strIndex, remaining, "{");
            // strIndex += count;
            // remaining -= count;

            // if (entries[i].used) {
            //     count = snprintf (output + strIndex, remaining, "'index':%d,'action':%d,'hour':%u,'min':%u",
            //                       entries[i].index,
            //                       entries[i].action,
            //                       entries[i].hour,
            //                       entries[i].minute);
            //     strIndex += count;
            //     remaining -= count;

            //     if (!entries[i].enabled) {
            //         count = snprintf (output + strIndex, remaining, "',enabled':%d", entries[i].enabled);
            //         strIndex += count;
            //         remaining -= count;
            //     }

            //     if (!entries[i].repeat) {
            //         count = snprintf (output + strIndex, remaining, ",'repeat':%d", entries[i].repeat);
            //         strIndex += count;
            //         remaining -= count;
            //     }

            //     if (entries[i].weekMask != 0 && entries[i].weekMask != 127) {
            //         count = snprintf (output + strIndex, remaining, ",'weekmask':%u", entries[i].weekMask);
            //         strIndex += count;
            //         remaining -= count;
            //     }            
            // }
            
            // count = snprintf (output + strIndex, remaining, "}");
            // strIndex += count;
            // remaining -= count;

        }
        if (i < SCHED_MAX_ENTRIES - 1 /*&& entries[i + 1].used*/ && remaining > 0) {
            count = snprintf (output + strIndex, remaining, ",");
            strIndex += count;
            remaining -= count;
        }
    }
    if (remaining > 0) {
        count = snprintf (output + strIndex, remaining, "]");
    }
    
    return output;
}

bool ActionScheduler::enable (uint8_t index, bool enableFlag) {
    if (index < SCHED_MAX_ENTRIES) {
        entries[index].enabled = enableFlag;
        return true;
    }
    return false;
}