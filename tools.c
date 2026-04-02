#include "tools.h"
#include <ctype.h>
#include <stdarg.h>
#include <strings.h>

#include "download_ui.h"
#include "entertainment.h"
#include "menu_audio.h"
#include "menu.h"

static void append_accessible_line(char lines[][256], int *count, int max_lines, const char *format, ...) {
    va_list args;

    if (!count || *count >= max_lines) {
        return;
    }

    va_start(args, format);
    vsnprintf(lines[*count], 256, format, args);
    va_end(args);
    (*count)++;
}

static void speak_menu_option(const char *title, int selected, int last_spoken) {
    if (title && selected != last_spoken) {
        menu_audio_speak(title);
    }
}

static int calculator_is_input_char(int key) {
    return (key >= '0' && key <= '9') || key == '.' || key == '-';
}

static int calculator_apply(double *result, char op, double operand) {
    if (!result) {
        return 0;
    }

    switch (op) {
        case '+':
            *result += operand;
            return 1;
        case '-':
            *result -= operand;
            return 1;
        case '*':
            *result *= operand;
            return 1;
        case '/':
            if (operand == 0.0) {
                return 0;
            }
            *result /= operand;
            return 1;
        default:
            return 0;
    }
}

void system_ui_run_calculator(void) {
    double result = 0;
    char current_input[64] = {0};
    char pending_op = 0;
    char message[128] = {0};
    int input_len = 0;
    int has_result = 0;

    while (1) {
        printf("\033[H\033[J--- Calculator ---\n");
        if (has_result) {
            printf("Current Result: %g\n", result);
        } else {
            printf("%s\n", menu_translate("ui_ready_for_new_calculation", "Ready for new calculation."));
        }
        if (pending_op) {
            printf("%s: %c\n", menu_translate("ui_pending_operator", "Pending Operator"), pending_op);
        }
        printf("%s: %s\n",
               menu_translate("ui_current_input", "Current Input"),
               input_len > 0 ? current_input : menu_translate("ui_empty_value", "(empty)"));
        printf("---------------------------\n");
        printf("%s\n", menu_translate("ui_footer_calculator", "[Esc: Exit | 'c': Clear | '=': Final Result]"));
        printf("%s\n", menu_translate("ui_calculator_help", "Type numbers and operators directly."));
        if (message[0]) {
            printf("\n%s\n", message);
        }
        fflush(stdout);

        {
            int key = read_key();

            if (key == KEY_ESC) {
                return;
            }

            message[0] = '\0';

            if (key == KEY_BACKSPACE) {
                if (input_len > 0) {
                    input_len--;
                    current_input[input_len] = '\0';
                }
                continue;
            }

            if (tolower(key) == 'c') {
                result = 0;
                pending_op = 0;
                input_len = 0;
                current_input[0] = '\0';
                has_result = 0;
                continue;
            }

            if (calculator_is_input_char(key)) {
                if (input_len < (int)sizeof(current_input) - 1) {
                    current_input[input_len++] = (char)key;
                    current_input[input_len] = '\0';
                }
                continue;
            }

            if (key == '+' || key == '-' || key == '*' || key == '/' || key == '=') {
                double value;

                if (input_len == 0 && !has_result) {
                    snprintf(message, sizeof(message), "%s", menu_translate("ui_enter_first_number", "Enter first number"));
                    continue;
                }

                if (input_len > 0) {
                    value = atof(current_input);
                    if (!has_result) {
                        result = value;
                        has_result = 1;
                    } else if (pending_op) {
                        if (!calculator_apply(&result, pending_op, value)) {
                            snprintf(message, sizeof(message), "%s", menu_translate("ui_division_by_zero", "Error: Division by zero!"));
                            pending_op = 0;
                            input_len = 0;
                            current_input[0] = '\0';
                            continue;
                        }
                    }
                    input_len = 0;
                    current_input[0] = '\0';
                }

                if (key == '=') {
                    pending_op = 0;
                    snprintf(message, sizeof(message), "%s: %g",
                             menu_translate("ui_final_result", "Final Result"),
                             result);
                } else {
                    pending_op = (char)key;
                }
                continue;
            }
        }
    }
}

void system_ui_show_weather(void) {
    char *city = get_setting("city");
    char error[256] = {0};
    char *response;
    char lines[16][256];
    int line_count = 0;
    char text[4096];
    if (!city) city = strdup("Pune");

    char *encoded_city = url_encode(city);
    char url[512];
    snprintf(url, sizeof(url), "https://goweather.xyz/weather/%s", encoded_city);
    free(encoded_city);

    response = fetch_text_with_progress_ui("Weather", url, "weather data", error, sizeof(error));
    if (response) {
        cJSON *json = cJSON_Parse(response);
        free(response);
        if (json) {
            cJSON *temp = cJSON_GetObjectItemCaseSensitive(json, "temperature");
            cJSON *wind = cJSON_GetObjectItemCaseSensitive(json, "wind");
            cJSON *desc = cJSON_GetObjectItemCaseSensitive(json, "description");
            char title[256];

            append_accessible_line(lines, &line_count, 16, "City: %s", city);
            if (temp && cJSON_IsString(temp)) append_accessible_line(lines, &line_count, 16, "Temperature: %s", temp->valuestring);
            if (wind && cJSON_IsString(wind)) append_accessible_line(lines, &line_count, 16, "Wind: %s", wind->valuestring);
            if (desc && cJSON_IsString(desc)) append_accessible_line(lines, &line_count, 16, "Description: %s", desc->valuestring);

            cJSON *forecast = cJSON_GetObjectItemCaseSensitive(json, "forecast");
            if (cJSON_IsArray(forecast)) {
                int count = cJSON_GetArraySize(forecast);
                for (int i = 0; i < count; i++) {
                    cJSON *item = cJSON_GetArrayItem(forecast, i);
                    cJSON *f_day = cJSON_GetObjectItemCaseSensitive(item, "day");
                    cJSON *f_temp = cJSON_GetObjectItemCaseSensitive(item, "temperature");
                    cJSON *f_wind = cJSON_GetObjectItemCaseSensitive(item, "wind");
                    append_accessible_line(lines, &line_count, 16,
                                           "Forecast day %s: %s, wind %s",
                                           f_day ? f_day->valuestring : "?",
                                           f_temp ? f_temp->valuestring : "?",
                                           f_wind ? f_wind->valuestring : "?");
                }
            }
            cJSON_Delete(json);
            snprintf(title, sizeof(title), "Weather: %s", city);
            text[0] = '\0';
            for (int i = 0; i < line_count; i++) {
                strncat(text, lines[i], sizeof(text) - strlen(text) - 1);
                strncat(text, "\n", sizeof(text) - strlen(text) - 1);
            }
            content_ui_show_spoken_text(title, title, text);
        } else {
            append_accessible_line(lines, &line_count, 16, "%s", "Error parsing weather data.");
            snprintf(text, sizeof(text), "%s", lines[0]);
            content_ui_show_spoken_text("Weather", "Weather", text);
        }
    } else {
        append_accessible_line(lines, &line_count, 16, "%s",
                               error[0] ? error : "Failed to connect to Weather API.");
        snprintf(text, sizeof(text), "%s", lines[0]);
        content_ui_show_spoken_text("Weather", "Weather", text);
    }
    free(city);
}

void system_ui_show_current_time_date(void) {
    char *city = get_setting("city");
    char error[256] = {0};
    char *response;
    char lines[8][256];
    int line_count = 0;
    char text[2048];
    if (!city) city = strdup("Pune");

    const char *timezone = "Asia/Kolkata";
    if (city) {
        if (strcasecmp(city, "London") == 0) timezone = "Europe/London";
        else if (strcasecmp(city, "New York") == 0) timezone = "America/New_York";
        else if (strcasecmp(city, "Tokyo") == 0) timezone = "Asia/Tokyo";
        else if (strcasecmp(city, "Sydney") == 0) timezone = "Australia/Sydney";
        else if (strcasecmp(city, "Paris") == 0) timezone = "Europe/Paris";
        else if (strcasecmp(city, "Berlin") == 0) timezone = "Europe/Berlin";
    }

    char url[512];
    snprintf(url, sizeof(url), "http://worldtimeapi.org/api/timezone/%s", timezone);

    response = fetch_text_with_progress_ui("Current Time and Date", url, "time data", error, sizeof(error));
    if (response) {
        cJSON *json = cJSON_Parse(response);
        free(response);
        if (json) {
            cJSON *datetime = cJSON_GetObjectItemCaseSensitive(json, "datetime");
            if (datetime && cJSON_IsString(datetime)) {
                char date_str[11] = {0};
                char time_str[9] = {0};
                if (strlen(datetime->valuestring) >= 19) {
                    strncpy(date_str, datetime->valuestring, 10);
                    strncpy(time_str, datetime->valuestring + 11, 8);
                    append_accessible_line(lines, &line_count, 8, "Date: %s", date_str);
                    append_accessible_line(lines, &line_count, 8, "Time: %s", time_str);
                    append_accessible_line(lines, &line_count, 8, "Timezone: %s", timezone);
                } else {
                    append_accessible_line(lines, &line_count, 8, "Date and time: %s", datetime->valuestring);
                }
            } else {
                append_accessible_line(lines, &line_count, 8, "Error: Could not retrieve time for %s.", city);
            }
            cJSON_Delete(json);
        } else {
            append_accessible_line(lines, &line_count, 8, "%s", "Error parsing time data.");
        }
    } else {
        append_accessible_line(lines, &line_count, 8, "%s",
                               error[0] ? error : "Failed to connect to Time API.");
    }
    text[0] = '\0';
    for (int i = 0; i < line_count; i++) {
        strncat(text, lines[i], sizeof(text) - strlen(text) - 1);
        strncat(text, "\n", sizeof(text) - strlen(text) - 1);
    }
    content_ui_show_spoken_text("Current Time and Date", "Current Time and Date", text);
    free(city);
}

void system_ui_show_news(void) {
    char lines[4][256];
    int line_count = 0;
    char text[512];

    append_accessible_line(lines, &line_count, 4, "%s",
                           menu_translate("ui_fetching_latest_news", "Fetching latest news"));
    append_accessible_line(lines, &line_count, 4, "%s", "News feature coming soon.");
    snprintf(text, sizeof(text), "%s\n%s", lines[0], lines[1]);
    content_ui_show_spoken_text("News", "News", text);
}

void system_ui_set_city(void) {
    char *current = get_setting("city");
    printf("\033[H\033[J--- Set City ---\n");
    printf("%s: %s\n",
           menu_translate("ui_current_city", "Current City"),
           current ? current : "Pune (Default)");
    if (current) free(current);

    char new_city[256];
    get_user_input(new_city, sizeof(new_city), menu_translate("ui_enter_new_city_name", "Enter new city name"));
    if (strlen(new_city) > 0) {
        save_setting("city", new_city);
        printf("\n%s '%s'! %s",
               menu_translate("ui_city_updated_to", "City updated to"),
               new_city,
               menu_translate("ui_press_any_key", "Press any key..."));
    } else {
        printf("\nNo changes made. Press any key...");
    }
    fflush(stdout);
    read_key();
}

static void handle_change_timezone() {
    FILE *f = fopen("timezones.json", "rb");
    if (!f) {
        printf("\nError: timezones.json not found. Press any key...");
        fflush(stdout); read_key();
        return;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *data = (char*)malloc(len + 1);
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);

    cJSON *json = cJSON_Parse(data);
    free(data);
    if (!json) {
        printf("\nError parsing timezones.json. Press any key...");
        fflush(stdout); read_key();
        return;
    }

    cJSON *tz_array = cJSON_GetObjectItemCaseSensitive(json, "timezones");
    if (!cJSON_IsArray(tz_array)) {
        printf("\nError: 'timezones' array not found. Press any key...");
        fflush(stdout); read_key();
        cJSON_Delete(json);
        return;
    }

    int total_count = cJSON_GetArraySize(tz_array);
    char search_term[256] = {0};
    int sel = 0;
    int scroll = 0;
    int PAGE_SIZE = 15;
    int last_spoken = -1;

    while (1) {
        char *current_timezone = get_setting("timezone");
        printf("\033[H\033[J--- Select Time Zone ---\n");
        printf("%s: %s\n",
               menu_translate("ui_selected_value", "Selected Value"),
               current_timezone ? current_timezone : "Asia/Kolkata");
        printf("%s: %s_\n", menu_translate("ui_search", "Search"), search_term);
        printf("---------------------------\n");
        free(current_timezone);

        int matches[total_count];
        int match_count = 0;
        char lower_search[256] = {0};
        for(int i=0; search_term[i]; i++) lower_search[i] = (char)tolower(search_term[i]);

        for (int i = 0; i < total_count; i++) {
            const char *tz = cJSON_GetArrayItem(tz_array, i)->valuestring;
            char lower_tz[256] = {0};
            for(int j=0; tz[j] && j < 255; j++) lower_tz[j] = (char)tolower(tz[j]);

            if (strlen(lower_search) == 0 || strstr(lower_tz, lower_search) != NULL) {
                matches[match_count++] = i;
            }
        }

        if (sel >= match_count) sel = (match_count > 0) ? match_count - 1 : 0;
        if (sel < scroll) scroll = sel;
        if (sel >= scroll + PAGE_SIZE) scroll = sel - PAGE_SIZE + 1;

        if (match_count == 0) {
            printf("  (%s)\n", menu_translate("ui_no_matching_timezones", "No matching time zones"));
        } else {
            int end = scroll + PAGE_SIZE;
            if (end > match_count) end = match_count;

            for (int i = scroll; i < end; i++) {
                const char *tz = cJSON_GetArrayItem(tz_array, matches[i])->valuestring;
                if (i == sel) printf("> %s\n", tz);
                else printf("  %s\n", tz);
            }

            if (sel != last_spoken) {
                menu_audio_speak(cJSON_GetArrayItem(tz_array, matches[sel])->valuestring);
                last_spoken = sel;
            }
        }

        printf("\n%s\n", menu_translate("ui_footer_search", "[Arrows: Navigate | Enter: Select | Esc: Cancel | Type to Search]"));
        fflush(stdout);

        int key = read_key();
        if (key == KEY_UP) {
            menu_audio_stop();
            sel = menu_next_index(sel, -1, match_count);
        } else if (key == KEY_DOWN) {
            menu_audio_stop();
            sel = menu_next_index(sel, 1, match_count);
        } else if (key == KEY_ENTER && match_count > 0) {
            menu_audio_stop();
            const char *selected_tz = cJSON_GetArrayItem(tz_array, matches[sel])->valuestring;
            save_setting("timezone", selected_tz);
            break;
        } else if (key == KEY_ESC) {
            menu_audio_stop();
            break;
        } else if (key == KEY_BACKSPACE) {
            menu_audio_stop();
            int slen = (int)strlen(search_term);
            if (slen > 0) {
                search_term[slen - 1] = '\0';
                sel = 0;
                scroll = 0;
                last_spoken = -1;
            }
        } else if (key > 0 && key < 1000 && isprint(key)) {
            menu_audio_stop();
            int slen = (int)strlen(search_term);
            if (slen < 254) {
                search_term[slen] = (char)key;
                search_term[slen + 1] = '\0';
                sel = 0;
                scroll = 0;
                last_spoken = -1;
            }
        }
    }
    menu_audio_stop();
    cJSON_Delete(json);
}

void system_ui_change_timezone(void) {
    handle_change_timezone();
}

void system_ui_change_time_format(void) {
    int sel = 0;
    char *current = get_setting("time_format");
    if (current) {
        if (strcmp(current, "24h") == 0) sel = 1;
        free(current);
    }

    const char *options[] = {
        menu_translate("format_12h", "12 Hours"),
        menu_translate("format_24h", "24 Hours")
    };
    const char *keys[] = {"12h", "24h"};
    int last_spoken = -1;

    while (1) {
        printf("\033[H\033[J--- Time Format ---\n");
        printf("%s: %s\n\n", menu_translate("ui_selected_value", "Selected Value"), options[sel]);
        for (int i = 0; i < 2; i++) {
            if (i == sel) printf("> %s\n", options[i]);
            else printf("  %s\n", options[i]);
        }
        printf("\n%s\n", menu_translate("ui_footer_back", "[Arrows: Navigate | Enter: Select | Esc: Back]"));
        fflush(stdout);

        speak_menu_option(options[sel], sel, last_spoken);
        last_spoken = sel;

        int key = read_key();
        if (key == KEY_UP) {
            menu_audio_stop();
            sel = menu_next_index(sel, -1, 2);
        }
        else if (key == KEY_DOWN) {
            menu_audio_stop();
            sel = menu_next_index(sel, 1, 2);
        }
        else if (key == KEY_ENTER) {
            menu_audio_stop();
            save_setting("time_format", keys[sel]);
            break;
        } else if (key == KEY_ESC) {
            menu_audio_stop();
            break;
        }
    }
}

void system_ui_set_time_manual(void) {
    int h = 0, m = 0, s = 0;
    char *current = get_setting("manual_time");
    if (current) {
        sscanf(current, "%d:%d:%d", &h, &m, &s);
        free(current);
    }

    int sel = 0;
    const char *options[] = {
        menu_translate("hour", "Hour"),
        menu_translate("minute", "Minute"),
        menu_translate("second", "Second"),
        menu_translate("ui_save_and_back", "Save and Back")
    };
    int num_options = 4;
    int last_spoken = -1;

    while (1) {
        printf("\033[H\033[J--- Set Time ---\n");
        printf("%s: %02d:%02d:%02d\n\n", menu_translate("ui_selected_value", "Selected Value"), h, m, s);
        
        for (int i = 0; i < num_options; i++) {
            if (i == sel) printf("> %s\n", options[i]);
            else printf("  %s\n", options[i]);
        }
        
        printf("\n%s\n", menu_translate("ui_footer_cancel", "[Arrows: Navigate | Enter: Select | Esc: Cancel]"));
        fflush(stdout);

        speak_menu_option(options[sel], sel, last_spoken);
        last_spoken = sel;

        int key = read_key();
        if (key == KEY_UP) {
            menu_audio_stop();
            sel = menu_next_index(sel, -1, num_options);
        } else if (key == KEY_DOWN) {
            menu_audio_stop();
            sel = menu_next_index(sel, 1, num_options);
        } else if (key == KEY_ENTER) {
            menu_audio_stop();
            if (sel == 0) {
                int val = handle_value_picker(menu_translate("ui_select_hour", "Select Hour"), 0, 23, h);
                if (val != -1) h = val;
            } else if (sel == 1) {
                int val = handle_value_picker(menu_translate("ui_select_minute", "Select Minute"), 0, 59, m);
                if (val != -1) m = val;
            } else if (sel == 2) {
                int val = handle_value_picker(menu_translate("ui_select_second", "Select Second"), 0, 59, s);
                if (val != -1) s = val;
            } else if (sel == 3) {
                char buffer[16];
                snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", h, m, s);
                save_setting("manual_time", buffer);
                printf("\n%s %s",
                       menu_translate("ui_time_saved", "Time saved!"),
                       menu_translate("ui_press_any_key", "Press any key..."));
                fflush(stdout); read_key();
                break;
            }
        } else if (key == KEY_ESC) {
            menu_audio_stop();
            break;
        }
    }
}

static int is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int get_days_in_month(int month, int year) {
    int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap_year(year)) return 29;
    if (month >= 1 && month <= 12) return days[month - 1];
    return 31;
}

void system_ui_set_date_manual(void) {
    int day = 1, month = 1, year = 2026;
    char *current = get_setting("manual_date");
    if (current) {
        sscanf(current, "%d-%d-%d", &year, &month, &day);
        free(current);
    }

    int sel = 0;
    const char *options[] = {
        menu_translate("month", "Month"),
        menu_translate("year", "Year"),
        menu_translate("date", "Date"),
        menu_translate("ui_save_and_back", "Save and Back")
    };
    int num_options = 4;

    while (1) {
        printf("\033[H\033[J--- Set Date ---\n");
        printf("%s: %04d-%02d-%02d\n\n", menu_translate("ui_selected_value", "Selected Value"), year, month, day);
        
        for (int i = 0; i < num_options; i++) {
            if (i == sel) printf("> %s\n", options[i]);
            else printf("  %s\n", options[i]);
        }
        
        printf("\n%s\n", menu_translate("ui_footer_cancel", "[Arrows: Navigate | Enter: Select | Esc: Cancel]"));
        fflush(stdout);

        int key = read_key();
        if (key == KEY_UP) {
            sel = menu_next_index(sel, -1, num_options);
        } else if (key == KEY_DOWN) {
            sel = menu_next_index(sel, 1, num_options);
        } else if (key == KEY_ENTER) {
            if (sel == 0) {
                int val = handle_value_picker(menu_translate("ui_select_month", "Select Month"), 1, 12, month);
                if (val != -1) {
                    month = val;
                    int max_days = get_days_in_month(month, year);
                    if (day > max_days) day = max_days;
                }
            } else if (sel == 1) {
                int val = handle_value_picker(menu_translate("ui_select_year", "Select Year"), 2000, 2100, year);
                if (val != -1) {
                    year = val;
                    int max_days = get_days_in_month(month, year);
                    if (day > max_days) day = max_days;
                }
            } else if (sel == 2) {
                int max_days = get_days_in_month(month, year);
                int val = handle_value_picker(menu_translate("ui_select_date", "Select Date"), 1, max_days, day);
                if (val != -1) day = val;
            } else if (sel == 3) {
                char buffer[16];
                snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", year, month, day);
                save_setting("manual_date", buffer);
                printf("\n%s %s",
                       menu_translate("ui_date_saved", "Date saved!"),
                       menu_translate("ui_press_any_key", "Press any key..."));
                fflush(stdout); read_key();
                break;
            }
        } else if (key == KEY_ESC) {
            break;
        }
    }
}
