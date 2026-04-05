#include "wifi_manager.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "menu.h"
#include "menu_audio.h"
#include "utils.h"

#define WIFI_IPC_DIR "/tmp/sai-wifi-ipc"
#define WIFI_MAX_SCAN_RESULTS 64

typedef struct {
    char ssid[128];
    char security[32];
    char signal[64];
} WifiScanResult;

typedef struct {
    int success;
    char message[256];
    char interface_name[64];
    char connected[32];
    char ssid[128];
    char ip_address[64];
    WifiScanResult scans[WIFI_MAX_SCAN_RESULTS];
    int scan_count;
} WifiResponse;

static int wifi_write_text_file(const char *path, const char *value) {
    FILE *file = fopen(path, "w");

    if (!file) {
        return 0;
    }

    if (value && value[0] != '\0') {
        fputs(value, file);
    }
    fclose(file);
    return 1;
}

static char *wifi_read_text_file(const char *path) {
    FILE *file;
    long size;
    char *data;

    file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    size = ftell(file);
    if (size < 0) {
        fclose(file);
        return NULL;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    data = (char *)malloc((size_t)size + 1);
    if (!data) {
        fclose(file);
        return NULL;
    }

    if (size > 0) {
        fread(data, 1, (size_t)size, file);
    }
    data[size] = '\0';
    fclose(file);

    while (size > 0 && (data[size - 1] == '\n' || data[size - 1] == '\r')) {
        data[size - 1] = '\0';
        size--;
    }

    return data;
}

static void wifi_cleanup_request_dir(const char *request_dir) {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(request_dir);
    if (!dir) {
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        char child_path[PATH_MAX];

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(child_path, sizeof(child_path), "%s/%s", request_dir, entry->d_name);
        unlink(child_path);
    }

    closedir(dir);
    rmdir(request_dir);
}

static void wifi_store_response_value(char *buffer, size_t buffer_size, char *value) {
    if (!buffer || buffer_size == 0) {
        free(value);
        return;
    }

    if (value) {
        snprintf(buffer, buffer_size, "%s", value);
        free(value);
    } else {
        buffer[0] = '\0';
    }
}

static int wifi_load_scan_results(const char *request_dir, WifiResponse *response) {
    char path[PATH_MAX];
    FILE *file;
    char line[512];

    if (!response) {
        return 0;
    }

    snprintf(path, sizeof(path), "%s/scan.tsv", request_dir);
    file = fopen(path, "r");
    if (!file) {
        return 0;
    }

    response->scan_count = 0;
    while (fgets(line, sizeof(line), file) && response->scan_count < WIFI_MAX_SCAN_RESULTS) {
        char *ssid;
        char *security;
        char *signal;
        WifiScanResult *item;

        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') {
            continue;
        }

        ssid = strtok(line, "\t");
        security = strtok(NULL, "\t");
        signal = strtok(NULL, "\t");
        if (!ssid || ssid[0] == '\0') {
            continue;
        }

        item = &response->scans[response->scan_count++];
        snprintf(item->ssid, sizeof(item->ssid), "%s", ssid);
        snprintf(item->security, sizeof(item->security), "%s", security ? security : "unknown");
        snprintf(item->signal, sizeof(item->signal), "%s", signal ? signal : "?");
    }

    fclose(file);
    return 1;
}

static int wifi_send_request(const char *command,
                             const char *ssid,
                             const char *password,
                             WifiResponse *response) {
    char request_template[] = WIFI_IPC_DIR "/req-XXXXXX";
    char done_path[PATH_MAX];
    char path[PATH_MAX];
    int waited_ms = 0;

    if (!command || !response) {
        return 0;
    }

    memset(response, 0, sizeof(*response));
    snprintf(response->message, sizeof(response->message), "%s",
             menu_translate("wifi_error_service_unavailable",
                            "Wi-Fi service is not available."));

    mkdir(WIFI_IPC_DIR, 0777);
    if (!mkdtemp(request_template)) {
        snprintf(response->message, sizeof(response->message),
                 "%s: %s",
                 menu_translate("wifi_error_prepare_request", "Unable to prepare Wi-Fi request"),
                 strerror(errno));
        return 0;
    }

    snprintf(path, sizeof(path), "%s/command", request_template);
    wifi_write_text_file(path, command);

    if (ssid) {
        snprintf(path, sizeof(path), "%s/ssid", request_template);
        wifi_write_text_file(path, ssid);
    }
    if (password) {
        snprintf(path, sizeof(path), "%s/password", request_template);
        wifi_write_text_file(path, password);
    }

    snprintf(done_path, sizeof(done_path), "%s/done", request_template);
    while (access(done_path, F_OK) != 0) {
        if (waited_ms >= 30000) {
            snprintf(response->message, sizeof(response->message),
                     "%s",
                     menu_translate("wifi_error_timeout", "Wi-Fi operation timed out."));
            wifi_cleanup_request_dir(request_template);
            return 0;
        }
        usleep(100000);
        waited_ms += 100;
    }

    snprintf(path, sizeof(path), "%s/status", request_template);
    {
        char *status = wifi_read_text_file(path);
        response->success = (status && strcmp(status, "ok") == 0);
        free(status);
    }

    snprintf(path, sizeof(path), "%s/message", request_template);
    wifi_store_response_value(response->message, sizeof(response->message), wifi_read_text_file(path));

    snprintf(path, sizeof(path), "%s/interface", request_template);
    wifi_store_response_value(response->interface_name, sizeof(response->interface_name), wifi_read_text_file(path));

    snprintf(path, sizeof(path), "%s/connected", request_template);
    wifi_store_response_value(response->connected, sizeof(response->connected), wifi_read_text_file(path));

    snprintf(path, sizeof(path), "%s/current_ssid", request_template);
    wifi_store_response_value(response->ssid, sizeof(response->ssid), wifi_read_text_file(path));

    snprintf(path, sizeof(path), "%s/ip_address", request_template);
    wifi_store_response_value(response->ip_address, sizeof(response->ip_address), wifi_read_text_file(path));

    wifi_load_scan_results(request_template, response);
    wifi_cleanup_request_dir(request_template);

    return response->success;
}

static void wifi_wait_for_exit(void) {
    printf("\n%s", menu_translate("ui_press_any_key_to_continue", "Press any key to continue..."));
    fflush(stdout);
    read_key();
}

static void wifi_show_status_screen(void) {
    WifiResponse response;
    char spoken[512];

    wifi_send_request("status", NULL, NULL, &response);

    printf("\033[H\033[J--- %s ---\n\n",
           menu_translate("wifi_status_title", "Wi-Fi Status"));
    printf("%s: %s\n",
           menu_translate("wifi_interface_label", "Interface"),
           response.interface_name[0] ? response.interface_name :
           menu_translate("wifi_not_available", "Not available"));
    printf("%s: %s\n",
           menu_translate("wifi_connection_label", "Connection"),
           strcmp(response.connected, "yes") == 0
               ? menu_translate("wifi_connected", "Connected")
               : menu_translate("wifi_disconnected", "Disconnected"));
    printf("%s: %s\n",
           menu_translate("wifi_network_label", "Network"),
           response.ssid[0] ? response.ssid :
           menu_translate("wifi_not_connected", "Not connected"));
    printf("%s: %s\n",
           menu_translate("wifi_ip_label", "IP Address"),
           response.ip_address[0] ? response.ip_address :
           menu_translate("wifi_not_assigned", "Not assigned"));
    if (response.message[0]) {
        printf("\n%s\n", response.message);
    }

    snprintf(spoken, sizeof(spoken), "%s. %s. %s.",
             strcmp(response.connected, "yes") == 0
                 ? menu_translate("wifi_connected", "Connected")
                 : menu_translate("wifi_disconnected", "Disconnected"),
             response.ssid[0] ? response.ssid : menu_translate("wifi_not_connected", "Not connected"),
             response.ip_address[0] ? response.ip_address : menu_translate("wifi_not_assigned", "Not assigned"));
    menu_audio_speak(spoken);

    wifi_wait_for_exit();
}

static int wifi_show_scan_picker(WifiResponse *response) {
    int selected = 0;
    int last_spoken = -1;

    if (!response || response->scan_count <= 0) {
        return -1;
    }

    while (1) {
        int i;

        printf("\033[H\033[J--- %s ---\n\n",
               menu_translate("wifi_select_network_title", "Select Wi-Fi Network"));
        for (i = 0; i < response->scan_count; i++) {
            const WifiScanResult *item = &response->scans[i];
            const char *pointer = (i == selected) ? "> " : "  ";

            printf("%s%s [%s, %s]\n",
                   pointer,
                   item->ssid,
                   item->security[0] ? item->security : menu_translate("wifi_security_unknown", "unknown"),
                   item->signal[0] ? item->signal : "?");
        }
        printf("\n%s\n",
               menu_translate("ui_footer_back", "[Arrows: Navigate | Enter: Select | Esc: Back]"));
        fflush(stdout);

        if (selected != last_spoken) {
            char spoken[256];

            snprintf(spoken, sizeof(spoken), "%s. %s. %s.",
                     response->scans[selected].ssid,
                     response->scans[selected].security[0] ? response->scans[selected].security
                                                          : menu_translate("wifi_security_unknown", "unknown"),
                     response->scans[selected].signal[0] ? response->scans[selected].signal : "?");
            menu_audio_speak(spoken);
            last_spoken = selected;
        }

        {
            int key = read_key();

            if (key == KEY_UP) {
                selected = menu_next_index(selected, -1, response->scan_count);
            } else if (key == KEY_DOWN) {
                selected = menu_next_index(selected, 1, response->scan_count);
            } else if (key == KEY_ENTER) {
                return selected;
            } else if (key == KEY_ESC) {
                return -1;
            }
        }
    }
}

static void wifi_connect_flow(void) {
    WifiResponse scan_response;
    WifiResponse connect_response;
    int selection;
    char password[256] = {0};

    printf("\033[H\033[J--- %s ---\n\n%s\n",
           menu_translate("wifi_scan_title", "Scanning Wi-Fi"),
           menu_translate("wifi_scan_wait", "Scanning for available networks. Please wait."));
    fflush(stdout);

    wifi_send_request("scan", NULL, NULL, &scan_response);
    if (!scan_response.success || scan_response.scan_count <= 0) {
        printf("\033[H\033[J--- %s ---\n\n%s\n",
               menu_translate("wifi_scan_title", "Scanning Wi-Fi"),
               scan_response.message[0] ? scan_response.message
                                        : menu_translate("wifi_scan_none", "No Wi-Fi networks were found."));
        menu_audio_speak(scan_response.message[0] ? scan_response.message
                                                  : menu_translate("wifi_scan_none", "No Wi-Fi networks were found."));
        wifi_wait_for_exit();
        return;
    }

    selection = wifi_show_scan_picker(&scan_response);
    if (selection < 0 || selection >= scan_response.scan_count) {
        return;
    }

    if (strcmp(scan_response.scans[selection].security, "open") != 0) {
        get_user_input(password,
                       sizeof(password),
                       menu_translate("wifi_password_prompt", "Enter Wi-Fi password"));
    }

    printf("\033[H\033[J--- %s ---\n\n%s\n",
           menu_translate("wifi_connect_title", "Connecting to Wi-Fi"),
           menu_translate("wifi_connect_wait", "Connecting to the selected Wi-Fi network. Please wait."));
    fflush(stdout);

    wifi_send_request("connect",
                      scan_response.scans[selection].ssid,
                      password,
                      &connect_response);

    printf("\033[H\033[J--- %s ---\n\n%s\n",
           menu_translate("wifi_connect_title", "Connecting to Wi-Fi"),
           connect_response.message[0] ? connect_response.message
                                       : (connect_response.success
                                              ? menu_translate("wifi_connect_success", "Wi-Fi connected successfully.")
                                              : menu_translate("wifi_connect_failed", "Wi-Fi connection failed.")));

    if (connect_response.success) {
        if (scan_response.scans[selection].ssid[0]) {
            save_setting("wifi_last_ssid", scan_response.scans[selection].ssid);
        }
        if (connect_response.interface_name[0]) {
            save_setting("wifi_interface", connect_response.interface_name);
        }
    }

    menu_audio_speak(connect_response.message[0] ? connect_response.message
                                                 : (connect_response.success
                                                        ? menu_translate("wifi_connect_success", "Wi-Fi connected successfully.")
                                                        : menu_translate("wifi_connect_failed", "Wi-Fi connection failed.")));
    wifi_wait_for_exit();
}

static void wifi_disconnect_flow(void) {
    WifiResponse response;

    printf("\033[H\033[J--- %s ---\n\n%s\n",
           menu_translate("wifi_disconnect_title", "Disconnect Wi-Fi"),
           menu_translate("wifi_disconnect_wait", "Disconnecting Wi-Fi. Please wait."));
    fflush(stdout);

    wifi_send_request("disconnect", NULL, NULL, &response);

    printf("\033[H\033[J--- %s ---\n\n%s\n",
           menu_translate("wifi_disconnect_title", "Disconnect Wi-Fi"),
           response.message[0] ? response.message
                               : (response.success
                                      ? menu_translate("wifi_disconnect_success", "Wi-Fi disconnected.")
                                      : menu_translate("wifi_disconnect_failed", "Unable to disconnect Wi-Fi.")));
    menu_audio_speak(response.message[0] ? response.message
                                         : (response.success
                                                ? menu_translate("wifi_disconnect_success", "Wi-Fi disconnected.")
                                                : menu_translate("wifi_disconnect_failed", "Unable to disconnect Wi-Fi.")));
    wifi_wait_for_exit();
}

void wifi_manager_show_menu(void) {
    const char *options[] = {
        "wifi_menu_status",
        "wifi_menu_scan_connect",
        "wifi_menu_disconnect",
        "wifi_menu_back"
    };
    const char *fallbacks[] = {
        "Current Wi-Fi Status",
        "Scan and Connect",
        "Disconnect Wi-Fi",
        "Back"
    };
    int selected = 0;
    int last_spoken = -1;
    int option_count = 4;

    while (1) {
        int i;

        printf("\033[H\033[J--- %s ---\n\n",
               menu_translate("setup_internet", "Setup Internet"));
        for (i = 0; i < option_count; i++) {
            printf("%s%s\n",
                   i == selected ? "> " : "  ",
                   menu_translate(options[i], fallbacks[i]));
        }
        printf("\n%s\n",
               menu_translate("ui_footer_back", "[Arrows: Navigate | Enter: Select | Esc: Back]"));
        fflush(stdout);

        if (selected != last_spoken) {
            menu_audio_speak(menu_translate(options[selected], fallbacks[selected]));
            last_spoken = selected;
        }

        {
            int key = read_key();

            if (key == KEY_UP) {
                selected = menu_next_index(selected, -1, option_count);
            } else if (key == KEY_DOWN) {
                selected = menu_next_index(selected, 1, option_count);
            } else if (key == KEY_ENTER) {
                if (selected == 0) {
                    wifi_show_status_screen();
                } else if (selected == 1) {
                    wifi_connect_flow();
                } else if (selected == 2) {
                    wifi_disconnect_flow();
                } else {
                    return;
                }
                last_spoken = -1;
            } else if (key == KEY_ESC) {
                return;
            }
        }
    }
}
