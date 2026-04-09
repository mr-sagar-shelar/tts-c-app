#include "voip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "config.h"
#include "contacts.h"
#include "entertainment.h"
#include "menu.h"
#include "menu_audio.h"
#include "ui_feedback.h"
#include "utils.h"

typedef struct {
    const char *menu_key;
    const char *config_key;
    const char *prompt_key;
    const char *prompt_fallback;
} VoipTextSetting;

typedef struct {
    const char *label_key;
    const char *label_fallback;
    const char *value;
} VoipChoice;

static const VoipTextSetting voip_text_settings[] = {
    {"voip_display_name", "voip_display_name", "voip_prompt_display_name", "Enter display name"},
    {"voip_sip_server", "voip_sip_server", "voip_prompt_sip_server", "Enter SIP server or proxy"},
    {"voip_sip_domain", "voip_sip_domain", "voip_prompt_sip_domain", "Enter SIP domain"},
    {"voip_username", "voip_username", "voip_prompt_username", "Enter SIP username"},
    {"voip_password", "voip_password", "voip_prompt_password", "Enter SIP password"}
};

static const VoipChoice voip_transport_choices[] = {
    {"voip_transport_udp", "UDP", "udp"},
    {"voip_transport_tcp", "TCP", "tcp"},
    {"voip_transport_tls", "TLS", "tls"}
};

static const VoipTextSetting *voip_find_text_setting(const char *menu_key) {
    size_t i;

    for (i = 0; i < sizeof(voip_text_settings) / sizeof(voip_text_settings[0]); i++) {
        if (strcmp(voip_text_settings[i].menu_key, menu_key) == 0) {
            return &voip_text_settings[i];
        }
    }

    return NULL;
}

static void voip_mask_password(const char *password, char *buffer, size_t buffer_size) {
    size_t i;
    size_t length;

    if (!buffer || buffer_size == 0) {
        return;
    }

    buffer[0] = '\0';
    if (!password || !password[0]) {
        return;
    }

    length = strlen(password);
    if (length >= buffer_size) {
        length = buffer_size - 1;
    }

    for (i = 0; i < length; i++) {
        buffer[i] = '*';
    }
    buffer[length] = '\0';
}

static char *voip_shell_quote(const char *text) {
    size_t length = 2;
    size_t i;
    char *quoted;
    char *cursor;

    if (!text) {
        return strdup("''");
    }

    for (i = 0; text[i]; i++) {
        if (text[i] == '\'') {
            length += 4;
        } else {
            length += 1;
        }
    }

    quoted = (char *)malloc(length + 1);
    if (!quoted) {
        return NULL;
    }

    cursor = quoted;
    *cursor++ = '\'';
    for (i = 0; text[i]; i++) {
        if (text[i] == '\'') {
            memcpy(cursor, "'\\''", 4);
            cursor += 4;
        } else {
            *cursor++ = text[i];
        }
    }
    *cursor++ = '\'';
    *cursor = '\0';

    return quoted;
}

static int voip_command_exists(const char *name) {
    const char *path = getenv("PATH");
    char *copy;
    char *token;

    if (!name || !name[0]) {
        return 0;
    }

    if (!path || !path[0]) {
        return access(name, X_OK) == 0;
    }

    copy = strdup(path);
    if (!copy) {
        return 0;
    }

    token = strtok(copy, ":");
    while (token) {
        char candidate[512];

        snprintf(candidate, sizeof(candidate), "%s/%s", token, name);
        if (access(candidate, X_OK) == 0) {
            free(copy);
            return 1;
        }
        token = strtok(NULL, ":");
    }

    free(copy);
    return 0;
}

static int voip_run_command(const char *command, char *output, size_t output_size) {
    FILE *pipe;
    int status;

    if (output && output_size > 0) {
        output[0] = '\0';
    }

    pipe = popen(command, "r");
    if (!pipe) {
        return 0;
    }

    if (output && output_size > 0) {
        size_t used = 0;

        while (fgets(output + used, (int)(output_size - used), pipe)) {
            used = strlen(output);
            if (used + 1 >= output_size) {
                break;
            }
        }
    } else {
        char discard[256];
        while (fgets(discard, sizeof(discard), pipe)) {
        }
    }

    status = pclose(pipe);
    return status == 0;
}

static char *voip_get_setting_value(const char *key) {
    char *value = get_setting(key);

    if (!value) {
        return strdup("");
    }

    return value;
}

static int voip_has_basic_config(void) {
    char *server = voip_get_setting_value("voip_sip_server");
    char *domain = voip_get_setting_value("voip_sip_domain");
    char *username = voip_get_setting_value("voip_username");
    char *password = voip_get_setting_value("voip_password");
    int ready = ((server[0] || domain[0]) && username[0] && password[0]);

    free(server);
    free(domain);
    free(username);
    free(password);
    return ready;
}

static void voip_show_message(const char *title, const char *message) {
    content_ui_show_spoken_text(title, title, message);
}

static void voip_append_line(char *buffer, size_t buffer_size, const char *label, const char *value) {
    char line[512];

    snprintf(line, sizeof(line), "%s: %s\n", label, value && value[0] ? value : "-");
    strncat(buffer, line, buffer_size - strlen(buffer) - 1);
}

static char *voip_build_target_uri(const char *target) {
    char *domain = voip_get_setting_value("voip_sip_domain");
    char *transport = voip_get_setting_value("voip_transport");
    char *uri;
    size_t needed;

    if (!target || !target[0]) {
        free(domain);
        free(transport);
        return NULL;
    }

    if (strstr(target, "sip:") == target || strchr(target, '@')) {
        needed = strlen(target) + strlen(transport) + 32;
        uri = (char *)malloc(needed);
        if (!uri) {
            free(domain);
            free(transport);
            return NULL;
        }
        snprintf(uri, needed, "%s%s%s",
                 target,
                 transport[0] ? ";transport=" : "",
                 transport[0] ? transport : "");
    } else if (domain[0]) {
        needed = strlen(target) + strlen(domain) + strlen(transport) + 32;
        uri = (char *)malloc(needed);
        if (!uri) {
            free(domain);
            free(transport);
            return NULL;
        }
        snprintf(uri, needed, "sip:%s@%s%s%s",
                 target,
                 domain,
                 transport[0] ? ";transport=" : "",
                 transport[0] ? transport : "");
    } else {
        uri = strdup(target);
    }

    free(domain);
    free(transport);
    return uri;
}

static int voip_linphone_prepare(char *message, size_t message_size) {
    char *server = voip_get_setting_value("voip_sip_server");
    char *domain = voip_get_setting_value("voip_sip_domain");
    char *username = voip_get_setting_value("voip_username");
    char *password = voip_get_setting_value("voip_password");
    char *host = NULL;
    char *server_q = NULL;
    char *username_q = NULL;
    char *password_q = NULL;
    char command[2048];
    int ok = 0;

    if (!voip_command_exists("linphonecsh")) {
        snprintf(message, message_size, "%s",
                 menu_translate("voip_error_client_missing", "linphonecsh is not installed on this system."));
        goto cleanup;
    }

    if (!((server[0] || domain[0]) && username[0] && password[0])) {
        snprintf(message, message_size, "%s",
                 menu_translate("voip_error_missing_config", "VoIP account settings are incomplete."));
        goto cleanup;
    }

    host = strdup(server[0] ? server : domain);
    server_q = voip_shell_quote(host);
    username_q = voip_shell_quote(username);
    password_q = voip_shell_quote(password);
    if (!host || !server_q || !username_q || !password_q) {
        snprintf(message, message_size, "%s",
                 menu_translate("voip_error_prepare", "Unable to prepare the VoIP command."));
        goto cleanup;
    }

    snprintf(command, sizeof(command),
             "linphonecsh init >/dev/null 2>&1 || true; "
             "linphonecsh register --host %s --username %s --password %s >/dev/null 2>&1",
             server_q,
             username_q,
             password_q);

    if (!voip_run_command(command, NULL, 0)) {
        snprintf(message, message_size, "%s",
                 menu_translate("voip_error_register", "Unable to register the VoIP account."));
        goto cleanup;
    }

    ok = 1;

cleanup:
    free(server);
    free(domain);
    free(username);
    free(password);
    free(host);
    free(server_q);
    free(username_q);
    free(password_q);
    return ok;
}

static void voip_show_registration_status(void) {
    char prepare_message[256] = {0};
    char output[2048] = {0};

    if (!voip_linphone_prepare(prepare_message, sizeof(prepare_message))) {
        voip_show_message(menu_translate("voip_status", "Registration Status"), prepare_message);
        return;
    }

    if (!voip_run_command("linphonecsh status register 2>&1", output, sizeof(output))) {
        voip_show_message(menu_translate("voip_status", "Registration Status"),
                          menu_translate("voip_error_status", "Unable to read the VoIP registration status."));
        return;
    }

    voip_show_message(menu_translate("voip_status_heading", "VoIP Registration Status"), output);
}

static void voip_hang_up(void) {
    if (voip_run_command("linphonecsh generic 'terminate' >/dev/null 2>&1", NULL, 0)) {
        voip_show_message(menu_translate("voip_hangup", "Hang Up Call"),
                          menu_translate("voip_hangup_success", "VoIP call ended."));
    } else {
        voip_show_message(menu_translate("voip_hangup", "Hang Up Call"),
                          menu_translate("voip_hangup_failed", "Unable to end the VoIP call."));
    }
}

static void voip_start_call_to_uri(const char *target, const char *display_name) {
    char prepare_message[256] = {0};
    char *uri = NULL;
    char *uri_q = NULL;
    char command[2048];
    char message[512];

    if (!target || !target[0]) {
        voip_show_message(menu_translate("voip_dial", "Dial SIP Address"),
                          menu_translate("voip_error_target_missing", "No SIP target was provided."));
        return;
    }

    if (!voip_linphone_prepare(prepare_message, sizeof(prepare_message))) {
        voip_show_message(menu_translate("voip_dial", "Dial SIP Address"), prepare_message);
        return;
    }

    uri = voip_build_target_uri(target);
    uri_q = voip_shell_quote(uri);
    if (!uri || !uri_q) {
        voip_show_message(menu_translate("voip_dial", "Dial SIP Address"),
                          menu_translate("voip_error_prepare", "Unable to prepare the VoIP command."));
        free(uri);
        free(uri_q);
        return;
    }

    snprintf(command, sizeof(command), "linphonecsh dial %s >/dev/null 2>&1", uri_q);
    if (voip_run_command(command, NULL, 0)) {
        snprintf(message, sizeof(message),
                 menu_translate("voip_call_started", "Calling %s"),
                 display_name && display_name[0] ? display_name : target);
        voip_show_message(menu_translate("voip_dial", "Dial SIP Address"), message);
    } else {
        snprintf(message, sizeof(message),
                 menu_translate("voip_call_failed", "Unable to start a call to %s."),
                 display_name && display_name[0] ? display_name : target);
        voip_show_message(menu_translate("voip_dial", "Dial SIP Address"), message);
    }

    free(uri);
    free(uri_q);
}

static void voip_dial_manual(void) {
    char target[256] = {0};

    get_user_input(target, sizeof(target),
                   menu_translate("voip_prompt_target", "Enter SIP address, extension, or number"));
    if (!target[0]) {
        return;
    }

    voip_start_call_to_uri(target, target);
}

static void voip_dial_contact(void) {
    int count = get_contact_count();
    int selected = 0;

    if (count <= 0) {
        voip_show_message(menu_translate("voip_call_contact", "Call Contact"),
                          menu_translate("voip_no_contacts", "No contacts are available for VoIP dialing."));
        return;
    }

    while (1) {
        int i;

        printf("\033[H\033[J--- %s ---\n",
               menu_translate("voip_call_contact", "Call Contact"));
        for (i = 0; i < count; i++) {
            Contact *contact = get_contact(i);
            char line[384];

            snprintf(line, sizeof(line), "%s %s (%s)",
                     contact->first_name,
                     contact->last_name,
                     contact->phone[0] ? contact->phone : menu_translate("ui_not_available", "Not available"));
            printf("%s%s\n", i == selected ? "> " : "  ", line);
            if (i == selected) {
                menu_audio_request(line);
            }
        }
        printf("\n%s\n", menu_translate("ui_footer_back", "[Arrows: Navigate | Enter: Select | Esc: Back]"));
        fflush(stdout);

        {
            int key = read_key();
            if (key == KEY_UP) {
                selected = menu_next_index(selected, -1, count);
            } else if (key == KEY_DOWN) {
                selected = menu_next_index(selected, 1, count);
            } else if (key == KEY_ENTER) {
                Contact *contact = get_contact(selected);
                char label[256];

                if (!contact->phone[0]) {
                    voip_show_message(menu_translate("voip_call_contact", "Call Contact"),
                                      menu_translate("voip_error_contact_target",
                                                     "The selected contact does not have a valid SIP address or number."));
                    return;
                }

                snprintf(label, sizeof(label), "%s %s", contact->first_name, contact->last_name);
                voip_start_call_to_uri(contact->phone, label);
                return;
            } else if (key == KEY_ESC) {
                return;
            }
        }
    }
}

static void voip_show_configuration(void) {
    char message[2048] = {0};
    char *display_name = voip_get_setting_value("voip_display_name");
    char *server = voip_get_setting_value("voip_sip_server");
    char *domain = voip_get_setting_value("voip_sip_domain");
    char *username = voip_get_setting_value("voip_username");
    char *password = voip_get_setting_value("voip_password");
    char *transport = voip_get_setting_value("voip_transport");
    char masked_password[128];

    voip_mask_password(password, masked_password, sizeof(masked_password));

    voip_append_line(message, sizeof(message),
                     menu_translate("voip_display_name", "Display Name"), display_name);
    voip_append_line(message, sizeof(message),
                     menu_translate("voip_sip_server", "SIP Server"), server);
    voip_append_line(message, sizeof(message),
                     menu_translate("voip_sip_domain", "SIP Domain"), domain);
    voip_append_line(message, sizeof(message),
                     menu_translate("voip_username", "Username"), username);
    voip_append_line(message, sizeof(message),
                     menu_translate("voip_password", "Password"), masked_password);
    voip_append_line(message, sizeof(message),
                     menu_translate("voip_transport", "Transport"),
                     menu_translate(strcmp(transport, "tcp") == 0 ? "voip_transport_tcp" :
                                    strcmp(transport, "tls") == 0 ? "voip_transport_tls" :
                                                                    "voip_transport_udp",
                                    strcmp(transport, "tcp") == 0 ? "TCP" :
                                    strcmp(transport, "tls") == 0 ? "TLS" : "UDP"));

    voip_show_message(menu_translate("voip_show_config", "Show VoIP Config"), message);

    free(display_name);
    free(server);
    free(domain);
    free(username);
    free(password);
    free(transport);
}

static void voip_edit_text_setting(const VoipTextSetting *setting) {
    char current[256];

    if (!setting) {
        return;
    }

    current[0] = '\0';
    {
        char *saved = get_setting(setting->config_key);
        if (saved) {
            snprintf(current, sizeof(current), "%s", saved);
            free(saved);
        }
    }

    get_user_input(current, sizeof(current),
                   menu_translate(setting->prompt_key, setting->prompt_fallback));
    save_setting(setting->config_key, current);
    voip_show_message(menu_translate("voip", "VoIP"),
                      menu_translate("ui_value_saved", "Value saved!"));
}

static void voip_edit_transport(void) {
    char *current = voip_get_setting_value("voip_transport");
    int selected = 0;
    size_t i;

    for (i = 0; i < sizeof(voip_transport_choices) / sizeof(voip_transport_choices[0]); i++) {
        if (strcasecmp(current, voip_transport_choices[i].value) == 0) {
            selected = (int)i;
            break;
        }
    }

    while (1) {
        printf("\033[H\033[J--- %s ---\n",
               menu_translate("voip_transport", "Transport"));
        for (i = 0; i < sizeof(voip_transport_choices) / sizeof(voip_transport_choices[0]); i++) {
            const char *label = menu_translate(voip_transport_choices[i].label_key,
                                               voip_transport_choices[i].label_fallback);
            printf("%s%s\n", (int)i == selected ? "> " : "  ", label);
            if ((int)i == selected) {
                menu_audio_request(label);
            }
        }
        printf("\n%s\n", menu_translate("ui_footer_back", "[Arrows: Navigate | Enter: Select | Esc: Back]"));
        fflush(stdout);

        {
            int key = read_key();
            if (key == KEY_UP) {
                selected = menu_next_index(selected, -1, (int)(sizeof(voip_transport_choices) / sizeof(voip_transport_choices[0])));
            } else if (key == KEY_DOWN) {
                selected = menu_next_index(selected, 1, (int)(sizeof(voip_transport_choices) / sizeof(voip_transport_choices[0])));
            } else if (key == KEY_ENTER) {
                save_setting("voip_transport", voip_transport_choices[selected].value);
                free(current);
                voip_show_message(menu_translate("voip", "VoIP"),
                                  menu_translate("ui_value_saved", "Value saved!"));
                return;
            } else if (key == KEY_ESC) {
                free(current);
                return;
            }
        }
    }
}

char *voip_get_selected_label(const char *menu_key) {
    const VoipTextSetting *setting = voip_find_text_setting(menu_key);

    if (!menu_key) {
        return NULL;
    }

    if (strcmp(menu_key, "voip") == 0) {
        return strdup(menu_translate(voip_has_basic_config() ? "voip_ready" : "voip_not_configured",
                                     voip_has_basic_config() ? "Ready" : "Not configured"));
    }

    if (strcmp(menu_key, "voip_transport") == 0) {
        char *transport = voip_get_setting_value("voip_transport");
        char *label = strdup(menu_translate(strcmp(transport, "tcp") == 0 ? "voip_transport_tcp" :
                                            strcmp(transport, "tls") == 0 ? "voip_transport_tls" :
                                                                            "voip_transport_udp",
                                            strcmp(transport, "tcp") == 0 ? "TCP" :
                                            strcmp(transport, "tls") == 0 ? "TLS" : "UDP"));
        free(transport);
        return label;
    }

    if (setting) {
        char *value = voip_get_setting_value(setting->config_key);
        char *result;

        if (strcmp(menu_key, "voip_password") == 0) {
            char masked[128];
            voip_mask_password(value, masked, sizeof(masked));
            free(value);
            if (masked[0]) {
                return strdup(masked);
            }
            return strdup(menu_translate("ui_not_set", "Not set"));
        }

        if (value[0]) {
            result = value;
        } else {
            free(value);
            result = strdup(menu_translate("ui_not_set", "Not set"));
        }
        return result;
    }

    return NULL;
}

void voip_handle_menu(MenuNode *node) {
    const VoipTextSetting *setting;

    if (!node) {
        return;
    }

    setting = voip_find_text_setting(node->key);
    if (setting) {
        voip_edit_text_setting(setting);
        return;
    }

    if (strcmp(node->key, "voip_dial") == 0) {
        voip_dial_manual();
    } else if (strcmp(node->key, "voip_call_contact") == 0) {
        voip_dial_contact();
    } else if (strcmp(node->key, "voip_hangup") == 0) {
        voip_hang_up();
    } else if (strcmp(node->key, "voip_status") == 0) {
        voip_show_registration_status();
    } else if (strcmp(node->key, "voip_show_config") == 0) {
        voip_show_configuration();
    } else if (strcmp(node->key, "voip_transport") == 0) {
        voip_edit_transport();
    } else {
        ui_feedback_play(UI_FEEDBACK_WARNING);
    }
}
