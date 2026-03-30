#ifndef TOOLS_H
#define TOOLS_H

#include "utils.h"
#include "config.h"
#include "cJSON.h"

/**
 * UI handler for the calculator.
 */
void handle_calculator();

/**
 * UI handler for weather information.
 */
void handle_weather();

/**
 * UI handler for current time and date.
 */
void handle_current_time_date();

/**
 * UI handler for news (placeholder).
 */
void handle_news();

/**
 * UI handler to set the city for weather and time.
 */
void handle_set_city();

/**
 * UI handler to change the timezone.
 */
void handle_timezone();

/**
 * UI handler for time format (12h/24h).
 */
void handle_time_format();

/**
 * UI handler for manual time setting.
 */
void handle_set_time_manual();

/**
 * UI handler for manual date setting.
 */
void handle_set_date_manual();

#endif /* TOOLS_H */
