#ifndef TOOLS_H
#define TOOLS_H

#include "utils.h"
#include "config.h"
#include "cJSON.h"

/**
 * UI handler for the calculator.
 */
void system_ui_run_calculator(void);

/**
 * UI handler for weather information.
 */
void system_ui_show_weather(void);

void system_ui_show_world_clock(void);

/**
 * UI handler for news (placeholder).
 */
void system_ui_show_news(void);

/**
 * UI handler to set the city for weather and time.
 */
void system_ui_set_city(void);

/**
 * UI handler to change the timezone.
 */
void system_ui_change_timezone(void);

/**
 * UI handler for time format (12h/24h).
 */
void system_ui_change_time_format(void);
void system_ui_set_volume(void);

/**
 * UI handler for manual time setting.
 */
void system_ui_set_time_manual(void);

/**
 * UI handler for manual date setting.
 */
void system_ui_set_date_manual(void);

#endif /* TOOLS_H */
