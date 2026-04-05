#include "wifi_manager.h"

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "menu.h"
#include "menu_audio.h"
#include "platform_ops.h"
#include "utils.h"

static void wifi_wait_for_exit(void) {
    printf("\n%s", menu_translate("ui_press_any_key_to_continue", "Press any key to continue..."));
    fflush(stdout);
    read_key();
}

static void wifi_show_status_screen(void) {
    PlatformWifiResponse response;
    char spoken[512];

    platform_ops_wifi_status(&response);

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

static int wifi_show_scan_picker(PlatformWifiResponse *response) {
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
            const PlatformWifiScanResult *item = &response->scans[i];
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
    PlatformWifiResponse scan_response;
    PlatformWifiResponse connect_response;
    int selection;
    char password[256] = {0};

    printf("\033[H\033[J--- %s ---\n\n%s\n",
           menu_translate("wifi_scan_title", "Scanning Wi-Fi"),
           menu_translate("wifi_scan_wait", "Scanning for available networks. Please wait."));
    fflush(stdout);

    platform_ops_wifi_scan(&scan_response);
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

    platform_ops_wifi_connect(scan_response.scans[selection].ssid,
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
    PlatformWifiResponse response;

    printf("\033[H\033[J--- %s ---\n\n%s\n",
           menu_translate("wifi_disconnect_title", "Disconnect Wi-Fi"),
           menu_translate("wifi_disconnect_wait", "Disconnecting Wi-Fi. Please wait."));
    fflush(stdout);

    platform_ops_wifi_disconnect(&response);

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
        printf("%s: %s\n\n",
               menu_translate("ui_current_value", "Current Value"),
               platform_ops_get_mode_name());
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
