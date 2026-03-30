#ifndef CALENDAR_H
#define CALENDAR_H

#include "cJSON.h"
#include "menu.h"

typedef struct {
    char date[16];      // YYYY-MM-DD
    char time[16];      // HH:MM
    char title[128];
    char description[256];
    int reminder;       // 0 or 1
} CalendarEvent;

/**
 * Initializes the calendar from JSON.
 */
void init_calendar();

/**
 * Cleans up memory used by calendar.
 */
void cleanup_calendar();

/**
 * CRUD operations for calendar events.
 */
void add_event(CalendarEvent *e);
void edit_event(int index, CalendarEvent *e);
void delete_event(int index);
int get_event_count();
CalendarEvent* get_event(int index);
void save_calendar();

/**
 * UI handler for adding/editing an event.
 */
void event_form(CalendarEvent *e, int is_edit);

/**
 * Main dispatcher for calendar menu items.
 */
void handle_calendar(MenuNode *node);

#endif /* CALENDAR_H */
