#include "tools.h"
#include <ctype.h>
#include <strings.h>

#include "download_ui.h"

void system_ui_run_calculator(void) {
    double result = 0;
    char input[64];
    int first_op = 1;

    while (1) {
        printf("\033[H\033[J--- Calculator ---\n");
        if (!first_op) {
            printf("Current Result: %g\n", result);
        } else {
            printf("Ready for new calculation.\n");
        }
        printf("---------------------------\n");
        printf("[Esc: Exit | 'c': Clear | '=': Final Result]\n");
        fflush(stdout);

        if (first_op) {
            get_user_input(input, sizeof(input), "Enter first number");
            if (strlen(input) == 0) return;
            result = atof(input);
            first_op = 0;
            continue;
        }

        get_user_input(input, sizeof(input), "Enter operator (+, -, *, /) or '=' or 'c'");
        if (strlen(input) == 0) continue;
        char op = input[0];

        if (op == '=') {
            printf("\nFinal Result: %g\nPress any key to start over...", result);
            fflush(stdout);
            read_key();
            first_op = 1;
            continue;
        } else if (tolower(op) == 'c') {
            first_op = 1;
            continue;
        }

        get_user_input(input, sizeof(input), "Enter next number");
        if (strlen(input) == 0) continue;
        double next_num = atof(input);

        switch (op) {
            case '+': result += next_num; break;
            case '-': result -= next_num; break;
            case '*': result *= next_num; break;
            case '/': 
                if (next_num != 0) result /= next_num; 
                else {
                    printf("\nError: Division by zero!\nPress any key...");
                    fflush(stdout); read_key();
                }
                break;
            default:
                printf("\nInvalid operator! Use +, -, *, or /.\nPress any key...");
                fflush(stdout); read_key();
                break;
        }
    }
}

void system_ui_show_weather(void) {
    char *city = get_setting("city");
    char error[256] = {0};
    char *response;
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

            printf("\033[H\033[J--- Weather: %s ---\n\n", city);
            if (temp && cJSON_IsString(temp)) printf("Temperature: %s\n", temp->valuestring);
            if (wind && cJSON_IsString(wind)) printf("Wind: %s\n", wind->valuestring);
            if (desc && cJSON_IsString(desc)) printf("Description: %s\n", desc->valuestring);

            cJSON *forecast = cJSON_GetObjectItemCaseSensitive(json, "forecast");
            if (cJSON_IsArray(forecast)) {
                printf("\nForecast:\n");
                int count = cJSON_GetArraySize(forecast);
                for (int i = 0; i < count; i++) {
                    cJSON *item = cJSON_GetArrayItem(forecast, i);
                    cJSON *f_day = cJSON_GetObjectItemCaseSensitive(item, "day");
                    cJSON *f_temp = cJSON_GetObjectItemCaseSensitive(item, "temperature");
                    cJSON *f_wind = cJSON_GetObjectItemCaseSensitive(item, "wind");
                    printf("Day %s: %s, Wind %s\n", 
                        f_day ? f_day->valuestring : "?", 
                        f_temp ? f_temp->valuestring : "?", 
                        f_wind ? f_wind->valuestring : "?");
                }
            }
            cJSON_Delete(json);
        } else {
            printf("\nError parsing weather data.");
        }
    } else {
        printf("\n%s", error[0] ? error : "Failed to connect to Weather API.");
    }

    printf("\nPress any key to go back...");
    fflush(stdout);
    read_key();
    free(city);
}

void system_ui_show_current_time_date(void) {
    char *city = get_setting("city");
    char error[256] = {0};
    char *response;
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
                    printf("\nDate: %s\n", date_str);
                    printf("Time: %s\n", time_str);
                    printf("Timezone: %s\n", timezone);
                } else {
                    printf("\nRaw DateTime: %s\n", datetime->valuestring);
                }
            } else {
                printf("\nError: Could not retrieve time for %s.", city);
            }
            cJSON_Delete(json);
        } else {
            printf("\nError parsing time data.");
        }
    } else {
        printf("\n%s", error[0] ? error : "Failed to connect to Time API.");
    }

    printf("\nPress any key to go back...");
    fflush(stdout);
    read_key();
    free(city);
}

void system_ui_show_news(void) {
    printf("\033[H\033[J--- News ---\n");
    printf("Fetching latest news...\n");
    printf("\n(Placeholder: News feature coming soon!)\n");
    printf("\nPress any key to go back...");
    fflush(stdout);
    read_key();
}

void system_ui_set_city(void) {
    char *current = get_setting("city");
    printf("\033[H\033[J--- Set City ---\n");
    printf("Current City: %s\n", current ? current : "Pune (Default)");
    if (current) free(current);

    char new_city[256];
    get_user_input(new_city, sizeof(new_city), "Enter new city name");
    if (strlen(new_city) > 0) {
        save_setting("city", new_city);
        printf("\nCity updated to '%s'! Press any key...", new_city);
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

    while (1) {
        printf("\033[H\033[J--- Select Time Zone ---\n");
        printf("Search: %s_\n", search_term);
        printf("---------------------------\n");

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
            printf("  (No matching time zones)\n");
        } else {
            int end = scroll + PAGE_SIZE;
            if (end > match_count) end = match_count;

            for (int i = scroll; i < end; i++) {
                const char *tz = cJSON_GetArrayItem(tz_array, matches[i])->valuestring;
                if (i == sel) printf("> %s\n", tz);
                else printf("  %s\n", tz);
            }
        }

        printf("\n[Arrows: Navigate | Enter: Select | Esc: Cancel | Type to Search]\n");
        fflush(stdout);

        int key = read_key();
        if (key == KEY_UP && sel > 0) {
            sel--;
        } else if (key == KEY_DOWN && sel < match_count - 1) {
            sel++;
        } else if (key == KEY_ENTER && match_count > 0) {
            const char *selected_tz = cJSON_GetArrayItem(tz_array, matches[sel])->valuestring;
            save_setting("timezone", selected_tz);
            printf("\nTime Zone updated to '%s'! Press any key...", selected_tz);
            fflush(stdout); read_key();
            break;
        } else if (key == KEY_ESC) {
            break;
        } else if (key == KEY_BACKSPACE) {
            int slen = (int)strlen(search_term);
            if (slen > 0) {
                search_term[slen - 1] = '\0';
                sel = 0;
                scroll = 0;
            }
        } else if (key > 0 && key < 1000 && isprint(key)) {
            int slen = (int)strlen(search_term);
            if (slen < 254) {
                search_term[slen] = (char)key;
                search_term[slen + 1] = '\0';
                sel = 0;
                scroll = 0;
            }
        }
    }
    cJSON_Delete(json);
}

void system_ui_change_timezone(void) {
    while (1) {
        char *current = get_setting("timezone");
        printf("\033[H\033[J--- Time Zone ---\n");
        printf("Current Time Zone: %s\n", current ? current : "Asia/Kolkata (Default)");
        if (current) free(current);

        printf("\n1. Change Time Zone\n");
        printf("Esc. Go Back\n");
        printf("\nSelect option: ");
        fflush(stdout);

        int key = read_key();
        if (key == '1') {
            handle_change_timezone();
        } else if (key == KEY_ESC) {
            break;
        }
    }
}

void system_ui_change_time_format(void) {
    int sel = 0;
    char *current = get_setting("time_format");
    if (current) {
        if (strcmp(current, "24h") == 0) sel = 1;
        free(current);
    }

    const char *options[] = {"12 Hours", "24 Hours"};
    const char *keys[] = {"12h", "24h"};

    while (1) {
        printf("\033[H\033[J--- Time Format ---\n");
        for (int i = 0; i < 2; i++) {
            if (i == sel) printf("> %s\n", options[i]);
            else printf("  %s\n", options[i]);
        }
        printf("\n[Arrows: Navigate | Enter: Select | Esc: Back]\n");
        fflush(stdout);

        int key = read_key();
        if (key == KEY_UP && sel > 0) sel--;
        else if (key == KEY_DOWN && sel < 1) sel++;
        else if (key == KEY_ENTER) {
            save_setting("time_format", keys[sel]);
            printf("\nTime Format updated to '%s'! Press any key...", options[sel]);
            fflush(stdout); read_key();
            break;
        } else if (key == KEY_ESC) {
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
    const char *options[] = {"Hour", "Minute", "Second", "Save and Back"};
    int num_options = 4;

    while (1) {
        printf("\033[H\033[J--- Set Time ---\n");
        printf("Current Selection: %02d:%02d:%02d\n", h, m, s);
        printf("---------------------------\n");
        
        for (int i = 0; i < num_options; i++) {
            if (i == sel) printf("> %s\n", options[i]);
            else printf("  %s\n", options[i]);
        }
        
        printf("\n[Arrows: Navigate | Enter: Select | Esc: Cancel]\n");
        fflush(stdout);

        int key = read_key();
        if (key == KEY_UP && sel > 0) {
            sel--;
        } else if (key == KEY_DOWN && sel < num_options - 1) {
            sel++;
        } else if (key == KEY_ENTER) {
            if (sel == 0) {
                int val = handle_value_picker("Select Hour", 0, 23, h);
                if (val != -1) h = val;
            } else if (sel == 1) {
                int val = handle_value_picker("Select Minute", 0, 59, m);
                if (val != -1) m = val;
            } else if (sel == 2) {
                int val = handle_value_picker("Select Second", 0, 59, s);
                if (val != -1) s = val;
            } else if (sel == 3) {
                char buffer[16];
                snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", h, m, s);
                save_setting("manual_time", buffer);
                printf("\nTime saved! Press any key...");
                fflush(stdout); read_key();
                break;
            }
        } else if (key == KEY_ESC) {
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
    const char *options[] = {"Month", "Year", "Date", "Save and Back"};
    int num_options = 4;

    while (1) {
        printf("\033[H\033[J--- Set Date ---\n");
        printf("Current Selection: %04d-%02d-%02d\n", year, month, day);
        printf("---------------------------\n");
        
        for (int i = 0; i < num_options; i++) {
            if (i == sel) printf("> %s\n", options[i]);
            else printf("  %s\n", options[i]);
        }
        
        printf("\n[Arrows: Navigate | Enter: Select | Esc: Cancel]\n");
        fflush(stdout);

        int key = read_key();
        if (key == KEY_UP && sel > 0) {
            sel--;
        } else if (key == KEY_DOWN && sel < num_options - 1) {
            sel++;
        } else if (key == KEY_ENTER) {
            if (sel == 0) {
                int val = handle_value_picker("Select Month", 1, 12, month);
                if (val != -1) {
                    month = val;
                    int max_days = get_days_in_month(month, year);
                    if (day > max_days) day = max_days;
                }
            } else if (sel == 1) {
                int val = handle_value_picker("Select Year", 2000, 2100, year);
                if (val != -1) {
                    year = val;
                    int max_days = get_days_in_month(month, year);
                    if (day > max_days) day = max_days;
                }
            } else if (sel == 2) {
                int max_days = get_days_in_month(month, year);
                int val = handle_value_picker("Select Date", 1, max_days, day);
                if (val != -1) day = val;
            } else if (sel == 3) {
                char buffer[16];
                snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", year, month, day);
                save_setting("manual_date", buffer);
                printf("\nDate saved! Press any key...");
                fflush(stdout); read_key();
                break;
            }
        } else if (key == KEY_ESC) {
            break;
        }
    }
}
