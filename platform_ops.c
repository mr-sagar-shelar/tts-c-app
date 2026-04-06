#include "platform_ops.h"

#include <ctype.h>
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

static PlatformMode cached_mode = -1;

static int platform_get_linux_volume_percent(int *percent, char *message, size_t message_size);
static int platform_set_linux_volume_percent(int percent, char *message, size_t message_size);
static int platform_set_linux_audio_output(const char *output, char *message, size_t message_size);
static int platform_command_matches_model(const char *needle);
static int platform_is_cm4(void);
static int platform_is_pi_zero(void);
static int platform_read_text_file(const char *path, char *buffer, size_t buffer_size);

static void platform_set_message(char *message, size_t message_size, const char *text) {
    if (!message || message_size == 0) {
        return;
    }

    snprintf(message, message_size, "%s", text ? text : "");
}

static const char *platform_escape_shell_arg(const char *value, char *buffer, size_t buffer_size) {
    size_t pos = 0;

    if (!value || !buffer || buffer_size < 3) {
        return "''";
    }

    buffer[pos++] = '\'';
    while (*value && pos + 5 < buffer_size) {
        if (*value == '\'') {
            buffer[pos++] = '\'';
            buffer[pos++] = '\\';
            buffer[pos++] = '\'';
            buffer[pos++] = '\'';
        } else {
            buffer[pos++] = *value;
        }
        value++;
    }
    buffer[pos++] = '\'';
    buffer[pos] = '\0';
    return buffer;
}

static int platform_run_command_capture(const char *command,
                                        char *output,
                                        size_t output_size,
                                        int *exit_code) {
    FILE *pipe;
    int status;
    size_t used = 0;

    if (output && output_size > 0) {
        output[0] = '\0';
    }

    pipe = popen(command, "r");
    if (!pipe) {
        return 0;
    }

    if (output && output_size > 0) {
        while (fgets(output + used, (int)(output_size - used), pipe)) {
            used = strlen(output);
            if (used >= output_size - 1) {
                break;
            }
        }
    } else {
        char discard[256];
        while (fgets(discard, sizeof(discard), pipe)) {
        }
    }

    status = pclose(pipe);
    if (exit_code) {
        *exit_code = status;
    }

    return 1;
}

static int platform_command_success(const char *command, char *output, size_t output_size) {
    int exit_code = 0;

    if (!platform_run_command_capture(command, output, output_size, &exit_code)) {
        return 0;
    }

    return exit_code == 0;
}

static void platform_trim(char *text) {
    size_t len;

    if (!text) {
        return;
    }

    len = strlen(text);
    while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r' || text[len - 1] == ' ' || text[len - 1] == '\t')) {
        text[len - 1] = '\0';
        len--;
    }
}

#if !defined(__APPLE__)
static int platform_file_contains(const char *path, const char *needle) {
    FILE *file;
    char line[256];

    file = fopen(path, "r");
    if (!file) {
        return 0;
    }

    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, needle) != NULL) {
            fclose(file);
            return 1;
        }
    }

    fclose(file);
    return 0;
}
#endif

int platform_ops_get_device_model(char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return 0;
    }

    buffer[0] = '\0';

#if defined(__APPLE__)
    snprintf(buffer, buffer_size, "%s", "macOS host");
    return 1;
#elif defined(__linux__)
    if (platform_read_text_file("/sys/firmware/devicetree/base/model", buffer, buffer_size) ||
        platform_read_text_file("/proc/device-tree/model", buffer, buffer_size) ||
        platform_read_text_file("/sys/firmware/devicetree/base/compatible", buffer, buffer_size)) {
        return 1;
    }
#endif

    snprintf(buffer, buffer_size, "%s", platform_ops_get_mode_name());
    return 1;
}

static int platform_command_matches_model(const char *needle) {
    char model[256];
    char lower_model[256];
    char lower_needle[128];
    size_t i;

    if (!needle || !needle[0]) {
        return 0;
    }

    if (!platform_ops_get_device_model(model, sizeof(model))) {
        return 0;
    }

    for (i = 0; model[i] && i < sizeof(lower_model) - 1; i++) {
        lower_model[i] = (char)tolower((unsigned char)model[i]);
    }
    lower_model[i] = '\0';

    for (i = 0; needle[i] && i < sizeof(lower_needle) - 1; i++) {
        lower_needle[i] = (char)tolower((unsigned char)needle[i]);
    }
    lower_needle[i] = '\0';

    return strstr(lower_model, lower_needle) != NULL;
}

static int platform_is_cm4(void) {
    return platform_command_matches_model("compute module 4") ||
           platform_command_matches_model("cm4");
}

static int platform_is_pi_zero(void) {
    return platform_command_matches_model("pi zero");
}

static const char *platform_get_dev_state_dir(void) {
    static char path[PATH_MAX];
    static int initialized = 0;

    if (!initialized) {
        snprintf(path, sizeof(path), ".sai-dev");
        mkdir(path, 0777);
        initialized = 1;
    }

    return path;
}

PlatformMode platform_ops_get_mode(void) {
    if (cached_mode != (PlatformMode)-1) {
        return cached_mode;
    }

#if defined(__APPLE__)
    cached_mode = PLATFORM_MODE_STUB;
#else
    if (access("/etc/sysconfig/tcedir", F_OK) == 0 ||
        platform_file_contains("/etc/os-release", "TinyCore")) {
        cached_mode = PLATFORM_MODE_TINYCORE;
    } else {
        cached_mode = PLATFORM_MODE_DEV;
    }
#endif

    return cached_mode;
}

const char *platform_ops_get_mode_name(void) {
    switch (platform_ops_get_mode()) {
        case PLATFORM_MODE_TINYCORE:
            return "tinycore";
        case PLATFORM_MODE_DEV:
            return "dev";
        case PLATFORM_MODE_STUB:
        default:
            return "stub";
    }
}

static void platform_init_wifi_response(PlatformWifiResponse *response, const char *message) {
    if (!response) {
        return;
    }

    memset(response, 0, sizeof(*response));
    snprintf(response->connected, sizeof(response->connected), "no");
    platform_set_message(response->message, sizeof(response->message), message);
}

static int platform_read_text_file(const char *path, char *buffer, size_t buffer_size) {
    FILE *file;

    if (!buffer || buffer_size == 0) {
        return 0;
    }

    buffer[0] = '\0';
    file = fopen(path, "r");
    if (!file) {
        return 0;
    }

    if (!fgets(buffer, (int)buffer_size, file)) {
        fclose(file);
        return 0;
    }
    fclose(file);
    platform_trim(buffer);
    return 1;
}

static int platform_write_text_file(const char *path, const char *value) {
    FILE *file = fopen(path, "w");

    if (!file) {
        return 0;
    }

    if (value) {
        fputs(value, file);
    }
    fclose(file);
    return 1;
}

static int platform_get_first_line(const char *command, char *buffer, size_t buffer_size) {
    if (!platform_command_success(command, buffer, buffer_size)) {
        if (buffer && buffer_size > 0) {
            buffer[0] = '\0';
        }
        return 0;
    }

    platform_trim(buffer);
    return buffer && buffer[0] != '\0';
}

static int platform_write_alsa_config_file(const char *path,
                                           const char *card_index,
                                           const char *device_index) {
    FILE *file;

    if (!path || !card_index || !card_index[0] || !device_index || !device_index[0]) {
        return 0;
    }

    file = fopen(path, "w");
    if (!file) {
        return 0;
    }

    fprintf(file,
            "pcm.!default {\n"
            "    type plug\n"
            "    slave.pcm \"hw:%s,%s\"\n"
            "}\n\n"
            "ctl.!default {\n"
            "    type hw\n"
            "    card %s\n"
            "}\n",
            card_index,
            device_index,
            card_index);
    fclose(file);
    return 1;
}

static int platform_find_linux_audio_target(const char *output,
                                            char *card_index,
                                            size_t card_index_size,
                                            char *device_index,
                                            size_t device_index_size) {
    char card_command[512];
    char device_command[768];
    const char *card_pattern;
    const char *device_pattern = "";

    if (!output || !card_index || !device_index) {
        return 0;
    }

    card_index[0] = '\0';
    device_index[0] = '\0';

    if (strcmp(output, "hat") == 0) {
        card_pattern = "hifiberry|pirate|iqaudio|dac";
    } else {
        card_pattern = "bcm2835|vc4hdmi|HDMI";
        device_pattern = "&& $0 ~ /HDMI/";
    }

    snprintf(card_command, sizeof(card_command),
             "awk '/%s/i {print $1; exit} /^[[:space:]]*[0-9]+[[:space:]]+\\[/ && first == \"\" { first = $1 } END { if (first != \"\") print first }' /proc/asound/cards 2>/dev/null | tr -d '[:space:]'",
             card_pattern);
    if (!platform_get_first_line(card_command, card_index, card_index_size)) {
        return 0;
    }

    snprintf(device_command, sizeof(device_command),
             "awk -F: -v card='%s' '$1 ~ \"^\" card \"-[0-9]+\" && $0 ~ /playback/ %s { split($1, parts, \"-\"); print parts[2]; exit } $1 ~ \"^\" card \"-[0-9]+\" && $0 ~ /playback/ && first == \"\" { split($1, parts, \"-\"); first = parts[2]; } END { if (first != \"\") print first }' /proc/asound/pcm 2>/dev/null | tr -d '[:space:]'",
             card_index,
             device_pattern);
    if (!platform_get_first_line(device_command, device_index, device_index_size)) {
        snprintf(device_index, device_index_size, "%s", "0");
    }

    return 1;
}

static void platform_cleanup_dir(const char *path) {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(path);
    if (!dir) {
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        char child[PATH_MAX];

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        unlink(child);
    }

    closedir(dir);
    rmdir(path);
}

static int platform_parse_scan_file(const char *path, PlatformWifiResponse *response) {
    FILE *file;
    char line[512];

    if (!response) {
        return 0;
    }

    file = fopen(path, "r");
    if (!file) {
        return 0;
    }

    response->scan_count = 0;
    while (fgets(line, sizeof(line), file) && response->scan_count < PLATFORM_WIFI_MAX_SCAN_RESULTS) {
        char *ssid;
        char *security;
        char *signal;
        PlatformWifiScanResult *scan;

        line[strcspn(line, "\r\n")] = '\0';
        ssid = strtok(line, "\t");
        security = strtok(NULL, "\t");
        signal = strtok(NULL, "\t");
        if (!ssid || ssid[0] == '\0') {
            continue;
        }

        scan = &response->scans[response->scan_count++];
        snprintf(scan->ssid, sizeof(scan->ssid), "%s", ssid);
        snprintf(scan->security, sizeof(scan->security), "%s", security ? security : "secured");
        snprintf(scan->signal, sizeof(scan->signal), "%s", signal ? signal : "?");
    }

    fclose(file);
    return 1;
}

static int platform_tinycore_request(const char *command,
                                     const char *value1,
                                     const char *value2,
                                     const char *value3,
                                     const char *value4,
                                     PlatformWifiResponse *response,
                                     char *value_out,
                                     size_t value_out_size) {
    char request_template[] = "/tmp/sai-platform-ipc/req-XXXXXX";
    char path[PATH_MAX];
    char done_path[PATH_MAX];
    int waited_ms = 0;

    platform_init_wifi_response(response, "Platform service is not available.");
    mkdir("/tmp/sai-platform-ipc", 0777);
    if (!mkdtemp(request_template)) {
        platform_set_message(response->message, sizeof(response->message), "Unable to prepare Wi-Fi request.");
        return 0;
    }

    snprintf(path, sizeof(path), "%s/command", request_template);
    platform_write_text_file(path, command);
    if (value1) {
        snprintf(path, sizeof(path), "%s/value1", request_template);
        platform_write_text_file(path, value1);
    }
    if (value2) {
        snprintf(path, sizeof(path), "%s/value2", request_template);
        platform_write_text_file(path, value2);
    }
    if (value3) {
        snprintf(path, sizeof(path), "%s/value3", request_template);
        platform_write_text_file(path, value3);
    }
    if (value4) {
        snprintf(path, sizeof(path), "%s/value4", request_template);
        platform_write_text_file(path, value4);
    }

    snprintf(done_path, sizeof(done_path), "%s/done", request_template);
    while (access(done_path, F_OK) != 0) {
        if (waited_ms >= 30000) {
            platform_set_message(response->message, sizeof(response->message), "Wi-Fi operation timed out.");
            platform_cleanup_dir(request_template);
            return 0;
        }
        usleep(100000);
        waited_ms += 100;
    }

    snprintf(path, sizeof(path), "%s/status", request_template);
    response->success = platform_read_text_file(path, response->connected, sizeof(response->connected));
    if (response->success) {
        response->success = strcmp(response->connected, "ok") == 0;
    }

    snprintf(path, sizeof(path), "%s/message", request_template);
    platform_read_text_file(path, response->message, sizeof(response->message));
    snprintf(path, sizeof(path), "%s/interface", request_template);
    platform_read_text_file(path, response->interface_name, sizeof(response->interface_name));
    snprintf(path, sizeof(path), "%s/connected", request_template);
    platform_read_text_file(path, response->connected, sizeof(response->connected));
    snprintf(path, sizeof(path), "%s/current_ssid", request_template);
    platform_read_text_file(path, response->ssid, sizeof(response->ssid));
    snprintf(path, sizeof(path), "%s/ip_address", request_template);
    platform_read_text_file(path, response->ip_address, sizeof(response->ip_address));
    snprintf(path, sizeof(path), "%s/scan.tsv", request_template);
    platform_parse_scan_file(path, response);
    if (value_out && value_out_size > 0) {
        snprintf(path, sizeof(path), "%s/value", request_template);
        platform_read_text_file(path, value_out, value_out_size);
    }
    platform_cleanup_dir(request_template);

    return response->success;
}

static int platform_tinycore_wifi_request(const char *command,
                                          const char *ssid,
                                          const char *password,
                                          PlatformWifiResponse *response) {
    return platform_tinycore_request(command, ssid, password, NULL, NULL, response, NULL, 0);
}

static int platform_linux_prefix(char *buffer, size_t buffer_size) {
    if (geteuid() == 0) {
        snprintf(buffer, buffer_size, "%s", "");
        return 1;
    }

    if (access("/usr/bin/sudo", X_OK) == 0 || access("/bin/sudo", X_OK) == 0) {
        snprintf(buffer, buffer_size, "%s", "sudo -n ");
        return 1;
    }

    snprintf(buffer, buffer_size, "%s", "");
    return 0;
}

static int platform_detect_interface_linux(char *iface, size_t iface_size) {
    char output[256];

    if (!iface || iface_size == 0) {
        return 0;
    }

    iface[0] = '\0';
    if (platform_command_success("iw dev 2>/dev/null | awk '$1==\"Interface\"{print $2; exit}'", output, sizeof(output)) ||
        platform_command_success("iwconfig 2>/dev/null | awk '/IEEE 802\\.11/ {print $1; exit}'", output, sizeof(output))) {
        platform_trim(output);
        if (output[0] != '\0') {
            snprintf(iface, iface_size, "%s", output);
            return 1;
        }
    }

    return 0;
}

static void platform_fill_linux_status(const char *iface, PlatformWifiResponse *response) {
    char cmd[512];
    char output[512];

    if (!iface || !response) {
        return;
    }

    snprintf(response->interface_name, sizeof(response->interface_name), "%s", iface);

    snprintf(cmd, sizeof(cmd),
             "iwconfig %s 2>/dev/null | awk -F 'ESSID:' '/ESSID:/ {ssid=$2; sub(/^\"/, \"\", ssid); sub(/\".*/, \"\", ssid); if (ssid != \"\" && ssid != \"off/any\") print ssid; exit}'",
             iface);
    if (platform_command_success(cmd, output, sizeof(output))) {
        platform_trim(output);
        if (output[0] != '\0') {
            snprintf(response->ssid, sizeof(response->ssid), "%s", output);
            snprintf(response->connected, sizeof(response->connected), "yes");
        }
    }

    snprintf(cmd, sizeof(cmd),
             "ifconfig %s 2>/dev/null | awk '/inet addr:/ {split($2,a,\":\"); print a[2]; exit} /inet / {print $2; exit}'",
             iface);
    if (platform_command_success(cmd, output, sizeof(output))) {
        platform_trim(output);
        if (output[0] != '\0') {
            snprintf(response->ip_address, sizeof(response->ip_address), "%s", output);
        }
    }

    if (strcmp(response->connected, "yes") == 0) {
        platform_set_message(response->message, sizeof(response->message), "Wi-Fi is connected.");
        response->success = 1;
    } else {
        platform_set_message(response->message, sizeof(response->message), "Wi-Fi is not connected.");
        response->success = 1;
    }
}

static int platform_dev_wifi_status(PlatformWifiResponse *response) {
    char iface[64];

    platform_init_wifi_response(response, "Wi-Fi is not connected.");
    if (!platform_detect_interface_linux(iface, sizeof(iface))) {
        platform_set_message(response->message, sizeof(response->message), "No wireless interface was detected.");
        return 0;
    }

    platform_fill_linux_status(iface, response);
    return response->success;
}

static int platform_dev_wifi_scan(PlatformWifiResponse *response) {
    char iface[64];
    char prefix[64];
    char cmd[1024];
    char output[8192];
    char *line;
    PlatformWifiScanResult *scan = NULL;

    platform_init_wifi_response(response, "No Wi-Fi networks were found.");
    if (!platform_detect_interface_linux(iface, sizeof(iface))) {
        platform_set_message(response->message, sizeof(response->message), "No wireless interface was detected.");
        return 0;
    }
    snprintf(response->interface_name, sizeof(response->interface_name), "%s", iface);

    platform_linux_prefix(prefix, sizeof(prefix));
    snprintf(cmd, sizeof(cmd), "%siwlist %s scan 2>/dev/null", prefix, iface);
    if (!platform_command_success(cmd, output, sizeof(output))) {
        platform_set_message(response->message, sizeof(response->message),
                             prefix[0] ? "Wi-Fi scan failed. Passwordless sudo may be required in dev mode."
                                       : "Wi-Fi scan failed.");
        return 0;
    }

    line = strtok(output, "\n");
    while (line && response->scan_count < PLATFORM_WIFI_MAX_SCAN_RESULTS) {
        if (strstr(line, "Cell ") != NULL) {
            scan = &response->scans[response->scan_count];
            memset(scan, 0, sizeof(*scan));
            snprintf(scan->security, sizeof(scan->security), "%s", "secured");
            snprintf(scan->signal, sizeof(scan->signal), "%s", "?");
        } else if (scan && strstr(line, "ESSID:\"") != NULL) {
            char *ssid = strstr(line, "ESSID:\"") + 7;
            char *end = strchr(ssid, '"');
            if (end) {
                *end = '\0';
            }
            snprintf(scan->ssid, sizeof(scan->ssid), "%s", ssid);
        } else if (scan && strstr(line, "Encryption key:") != NULL) {
            if (strstr(line, "off")) {
                snprintf(scan->security, sizeof(scan->security), "%s", "open");
            }
        } else if (scan && strstr(line, "Quality=") != NULL) {
            char *quality = strstr(line, "Quality=");
            char temp[64];
            int i = 0;

            quality += 8;
            while (*quality && *quality != ' ' && i < (int)sizeof(temp) - 1) {
                temp[i++] = *quality++;
            }
            temp[i] = '\0';
            if (temp[0] != '\0') {
                snprintf(scan->signal, sizeof(scan->signal), "%s", temp);
            }
        }

        if (scan && scan->ssid[0] != '\0') {
            int duplicate = 0;
            int i;

            for (i = 0; i < response->scan_count; i++) {
                if (strcmp(response->scans[i].ssid, scan->ssid) == 0) {
                    duplicate = 1;
                    break;
                }
            }
            if (!duplicate) {
                response->scan_count++;
            }
            scan = NULL;
        }

        line = strtok(NULL, "\n");
    }

    response->success = 1;
    if (response->scan_count > 0) {
        platform_set_message(response->message, sizeof(response->message), "Available Wi-Fi networks loaded.");
    }
    return 1;
}

static int platform_dev_wifi_connect(const char *ssid, const char *password, PlatformWifiResponse *response) {
    char iface[64];
    char prefix[64];
    char cmd[2048];
    char escaped_ssid[256];
    char escaped_password[512];
    char conf_path[PATH_MAX];
    const char *state_dir = platform_get_dev_state_dir();

    platform_init_wifi_response(response, "Wi-Fi connection failed.");
    if (!ssid || ssid[0] == '\0') {
        platform_set_message(response->message, sizeof(response->message), "A Wi-Fi network name is required.");
        return 0;
    }
    if (!platform_detect_interface_linux(iface, sizeof(iface))) {
        platform_set_message(response->message, sizeof(response->message), "No wireless interface was detected.");
        return 0;
    }
    snprintf(response->interface_name, sizeof(response->interface_name), "%s", iface);

    platform_linux_prefix(prefix, sizeof(prefix));
    snprintf(conf_path, sizeof(conf_path), "%s/wpa_supplicant.conf", state_dir);
    if (password && password[0] != '\0') {
        snprintf(cmd, sizeof(cmd),
                 "%swpa_passphrase %s %s > %s",
                 prefix,
                 platform_escape_shell_arg(ssid, escaped_ssid, sizeof(escaped_ssid)),
                 platform_escape_shell_arg(password, escaped_password, sizeof(escaped_password)),
                 conf_path);
    } else {
        FILE *file = fopen(conf_path, "w");
        if (!file) {
            platform_set_message(response->message, sizeof(response->message), "Unable to create Wi-Fi configuration.");
            return 0;
        }
        fprintf(file,
                "ctrl_interface=/var/run/wpa_supplicant\n"
                "update_config=1\n"
                "network={\n"
                "    ssid=\"%s\"\n"
                "    key_mgmt=NONE\n"
                "}\n",
                ssid);
        fclose(file);
        chmod(conf_path, 0600);
        cmd[0] = '\0';
    }

    if (cmd[0] != '\0' && !platform_command_success(cmd, NULL, 0)) {
        platform_set_message(response->message, sizeof(response->message),
                             "Failed to generate Wi-Fi configuration. Passwordless sudo may be required in dev mode.");
        return 0;
    }

    snprintf(cmd, sizeof(cmd),
             "%spkill -f 'wpa_supplicant.*-i%s' >/dev/null 2>&1 || true; "
             "%sifconfig %s up >/dev/null 2>&1 || true; "
             "%swpa_supplicant -B -i %s -c %s >/dev/null 2>&1; "
             "%sudhcpc -b -i %s -p /tmp/udhcpc.%s.pid >/dev/null 2>&1 || true",
             prefix, iface,
             prefix, iface,
             prefix, iface, conf_path,
             prefix, iface, iface);
    if (!platform_command_success(cmd, NULL, 0)) {
        platform_set_message(response->message, sizeof(response->message),
                             prefix[0] ? "Failed to connect. Passwordless sudo may be required in dev mode."
                                       : "Failed to connect.");
        return 0;
    }

    sleep(3);
    platform_fill_linux_status(iface, response);
    if (strcmp(response->connected, "yes") == 0) {
        platform_set_message(response->message, sizeof(response->message), "Wi-Fi connected successfully.");
        return 1;
    }

    platform_set_message(response->message, sizeof(response->message),
                         "Connection command ran, but the interface is not connected yet.");
    return 0;
}

static int platform_dev_wifi_disconnect(PlatformWifiResponse *response) {
    char iface[64];
    char prefix[64];
    char cmd[1024];

    platform_init_wifi_response(response, "Unable to disconnect Wi-Fi.");
    if (!platform_detect_interface_linux(iface, sizeof(iface))) {
        platform_set_message(response->message, sizeof(response->message), "No wireless interface was detected.");
        return 0;
    }
    snprintf(response->interface_name, sizeof(response->interface_name), "%s", iface);

    platform_linux_prefix(prefix, sizeof(prefix));
    snprintf(cmd, sizeof(cmd),
             "%spkill -f 'wpa_supplicant.*-i%s' >/dev/null 2>&1 || true; "
             "%spkill -f 'udhcpc.*%s' >/dev/null 2>&1 || true",
             prefix, iface, prefix, iface);
    if (!platform_command_success(cmd, NULL, 0)) {
        platform_set_message(response->message, sizeof(response->message),
                             prefix[0] ? "Failed to disconnect. Passwordless sudo may be required in dev mode."
                                       : "Failed to disconnect.");
        return 0;
    }

    response->success = 1;
    snprintf(response->connected, sizeof(response->connected), "%s", "no");
    platform_set_message(response->message, sizeof(response->message), "Wi-Fi disconnected.");
    return 1;
}

int platform_ops_wifi_status(PlatformWifiResponse *response) {
    if (platform_ops_get_mode() == PLATFORM_MODE_TINYCORE) {
        return platform_tinycore_wifi_request("status", NULL, NULL, response);
    }
    if (platform_ops_get_mode() == PLATFORM_MODE_DEV) {
        return platform_dev_wifi_status(response);
    }

    platform_init_wifi_response(response, "Wi-Fi status is stubbed on macOS.");
    response->success = 1;
    return 1;
}

int platform_ops_wifi_scan(PlatformWifiResponse *response) {
    if (platform_ops_get_mode() == PLATFORM_MODE_TINYCORE) {
        return platform_tinycore_wifi_request("scan", NULL, NULL, response);
    }
    if (platform_ops_get_mode() == PLATFORM_MODE_DEV) {
        return platform_dev_wifi_scan(response);
    }

    platform_init_wifi_response(response, "Wi-Fi scan is stubbed on macOS.");
    response->success = 1;
    return 1;
}

int platform_ops_wifi_connect(const char *ssid, const char *password, PlatformWifiResponse *response) {
    if (platform_ops_get_mode() == PLATFORM_MODE_TINYCORE) {
        return platform_tinycore_wifi_request("connect", ssid, password, response);
    }
    if (platform_ops_get_mode() == PLATFORM_MODE_DEV) {
        return platform_dev_wifi_connect(ssid, password, response);
    }

    platform_init_wifi_response(response, "Wi-Fi connect is stubbed on macOS.");
    snprintf(response->ssid, sizeof(response->ssid), "%s", ssid ? ssid : "");
    response->success = 1;
    return 1;
}

int platform_ops_wifi_disconnect(PlatformWifiResponse *response) {
    if (platform_ops_get_mode() == PLATFORM_MODE_TINYCORE) {
        return platform_tinycore_wifi_request("disconnect", NULL, NULL, response);
    }
    if (platform_ops_get_mode() == PLATFORM_MODE_DEV) {
        return platform_dev_wifi_disconnect(response);
    }

    platform_init_wifi_response(response, "Wi-Fi disconnect is stubbed on macOS.");
    response->success = 1;
    return 1;
}

int platform_ops_get_system_volume_percent(int *percent, char *message, size_t message_size) {
    PlatformWifiResponse response;
    char value[64] = {0};

    if (platform_ops_get_mode() == PLATFORM_MODE_STUB) {
        if (percent) {
            *percent = 0;
        }
        platform_set_message(message, message_size, "macOS stub mode: volume read skipped.");
        return 1;
    }

    if (platform_ops_get_mode() == PLATFORM_MODE_TINYCORE) {
        if (!platform_tinycore_request("get_volume", NULL, NULL, NULL, NULL, &response, value, sizeof(value))) {
            platform_set_message(message, message_size,
                                 response.message[0] ? response.message : "Unable to read system volume.");
            return 0;
        }
        if (percent) {
            *percent = atoi(value);
        }
        platform_set_message(message, message_size,
                             response.message[0] ? response.message : "System volume loaded.");
        return 1;
    }

    return platform_get_linux_volume_percent(percent, message, message_size);
}

int platform_ops_set_system_volume_percent(int percent, char *message, size_t message_size) {
    PlatformWifiResponse response;
    char value[32];

    if (platform_ops_get_mode() == PLATFORM_MODE_STUB) {
        char text[128];

        snprintf(text, sizeof(text), "macOS stub mode: volume change skipped for %d%%.", percent);
        platform_set_message(message, message_size, text);
        return 1;
    }

    if (platform_ops_get_mode() == PLATFORM_MODE_TINYCORE) {
        snprintf(value, sizeof(value), "%d", percent);
        if (!platform_tinycore_request("set_volume", value, NULL, NULL, NULL, &response, NULL, 0)) {
            platform_set_message(message, message_size,
                                 response.message[0] ? response.message : "Unable to update system volume.");
            return 0;
        }
        platform_set_message(message, message_size,
                             response.message[0] ? response.message : "System volume updated.");
        return 1;
    }

    return platform_set_linux_volume_percent(percent, message, message_size);
}

int platform_ops_set_audio_output(const char *output, char *message, size_t message_size) {
    PlatformWifiResponse response;
    const char *label;

    if (!output || (strcmp(output, "hdmi") != 0 && strcmp(output, "hat") != 0)) {
        platform_set_message(message, message_size, "Audio output must be HDMI or Audio HAT.");
        return 0;
    }

    label = strcmp(output, "hat") == 0 ? "Audio HAT" : "HDMI";

    if (platform_ops_get_mode() == PLATFORM_MODE_STUB) {
        char text[128];

        snprintf(text, sizeof(text), "macOS stub mode: audio output set to %s.", label);
        platform_set_message(message, message_size, text);
        return 1;
    }

    if (platform_ops_get_mode() == PLATFORM_MODE_TINYCORE) {
        if (!platform_tinycore_request("set_audio_output", output, NULL, NULL, NULL, &response, NULL, 0)) {
            platform_set_message(message, message_size,
                                 response.message[0] ? response.message : "Unable to update audio output.");
            return 0;
        }
        setenv("ALSA_CONFIG_PATH", "/tmp/sai-asound.conf", 1);
        platform_set_message(message, message_size,
                             response.message[0] ? response.message : "Audio output updated.");
        return 1;
    }

    return platform_set_linux_audio_output(output, message, message_size);
}

static int platform_run_privileged_linux(const char *command, char *message, size_t message_size) {
    char full_command[2048];
    char prefix[64];
    int has_privilege_hint = platform_linux_prefix(prefix, sizeof(prefix));

    if (platform_command_success(command, NULL, 0)) {
        return 1;
    }

    snprintf(full_command, sizeof(full_command), "%s%s", prefix, command);
    if (platform_command_success(full_command, NULL, 0)) {
        return 1;
    }

    if (!has_privilege_hint) {
        platform_set_message(message, message_size, "Operation failed.");
    } else {
        platform_set_message(message, message_size,
                             "Operation failed. Passwordless sudo may be required in dev mode.");
    }
    return 0;
}

int platform_ops_power_off(char *message, size_t message_size) {
    PlatformWifiResponse response;

    if (platform_ops_get_mode() == PLATFORM_MODE_STUB) {
        platform_set_message(message, message_size, "macOS stub mode: power off skipped.");
        return 1;
    }

    if (platform_ops_get_mode() == PLATFORM_MODE_TINYCORE && platform_is_cm4()) {
        if (platform_tinycore_request("power_off", NULL, NULL, NULL, NULL, &response, NULL, 0)) {
            platform_set_message(message, message_size,
                                 response.message[0] ? response.message : "Power off request sent.");
            return 1;
        }
        platform_set_message(message, message_size,
                             response.message[0] ? response.message : "Unable to power off in TinyCore CM4 mode.");
        return 0;
    }

    if (platform_run_privileged_linux("poweroff || shutdown -h now", message, message_size)) {
        if (platform_is_pi_zero()) {
            platform_set_message(message, message_size, "Pi Zero power off command executed.");
        } else {
            platform_set_message(message, message_size, "Power off command executed.");
        }
        return 1;
    }

    return 0;
}

int platform_ops_record_voice(const char *path, int seconds, char *message, size_t message_size) {
    PlatformWifiResponse response;
    char escaped_path[PATH_MAX * 2];
    char command[4096];
    char seconds_text[32];

    if (!path || path[0] == '\0') {
        platform_set_message(message, message_size, "A recording path is required.");
        return 0;
    }

    if (seconds <= 0) {
        seconds = 10;
    }

    if (platform_ops_get_mode() == PLATFORM_MODE_STUB) {
        snprintf(command, sizeof(command),
                 "macOS stub mode: voice recording skipped for %s.", path);
        platform_set_message(message, message_size, command);
        return 1;
    }

    if (platform_ops_get_mode() == PLATFORM_MODE_TINYCORE && platform_is_cm4()) {
        snprintf(seconds_text, sizeof(seconds_text), "%d", seconds);
        if (platform_tinycore_request("record_voice", path, seconds_text, NULL, NULL, &response, NULL, 0)) {
            platform_set_message(message, message_size,
                                 response.message[0] ? response.message : "Voice recording finished.");
            return 1;
        }
        platform_set_message(message, message_size,
                             response.message[0] ? response.message : "Unable to record voice in TinyCore CM4 mode.");
        return 0;
    }

    snprintf(command, sizeof(command),
             "(command -v arecord >/dev/null 2>&1 && arecord -q -f cd -d %d %s) || "
             "(command -v rec >/dev/null 2>&1 && rec -q %s trim 0.0 %d)",
             seconds,
             platform_escape_shell_arg(path, escaped_path, sizeof(escaped_path)),
             escaped_path,
             seconds);
    if (platform_command_success(command, NULL, 0)) {
        if (platform_is_pi_zero()) {
            snprintf(command, sizeof(command), "Pi Zero recording saved to %s.", path);
        } else {
            snprintf(command, sizeof(command), "Recording saved to %s.", path);
        }
        platform_set_message(message, message_size, command);
        return 1;
    }

    platform_set_message(message, message_size,
                         "Voice recording failed. Install `arecord` or `rec` and verify the microphone.");
    return 0;
}

int platform_ops_play_mp3(const char *path, char *message, size_t message_size) {
    PlatformWifiResponse response;
    char escaped_path[PATH_MAX * 2];
    char command[4096];

    if (!path || path[0] == '\0') {
        platform_set_message(message, message_size, "An MP3 path is required.");
        return 0;
    }

    if (platform_ops_get_mode() == PLATFORM_MODE_STUB) {
        snprintf(command, sizeof(command),
                 "macOS stub mode: MP3 playback skipped for %s.", path);
        platform_set_message(message, message_size, command);
        return 1;
    }

    if (platform_ops_get_mode() == PLATFORM_MODE_TINYCORE && platform_is_cm4()) {
        if (platform_tinycore_request("play_mp3", path, NULL, NULL, NULL, &response, NULL, 0)) {
            platform_set_message(message, message_size,
                                 response.message[0] ? response.message : "MP3 playback completed.");
            return 1;
        }
        platform_set_message(message, message_size,
                             response.message[0] ? response.message : "Unable to play MP3 in TinyCore CM4 mode.");
        return 0;
    }

    snprintf(command, sizeof(command),
             "(command -v mpg123 >/dev/null 2>&1 && mpg123 %s) || "
             "(command -v madplay >/dev/null 2>&1 && madplay %s) || "
             "(command -v ffplay >/dev/null 2>&1 && ffplay -nodisp -autoexit %s) || "
             "(command -v play >/dev/null 2>&1 && play %s)",
             platform_escape_shell_arg(path, escaped_path, sizeof(escaped_path)),
             escaped_path,
             escaped_path,
             escaped_path);
    if (platform_command_success(command, NULL, 0)) {
        if (platform_is_pi_zero()) {
            snprintf(command, sizeof(command), "Pi Zero played %s.", path);
        } else {
            snprintf(command, sizeof(command), "Playback finished for %s.", path);
        }
        platform_set_message(message, message_size, command);
        return 1;
    }

    platform_set_message(message, message_size,
                         "MP3 playback failed. Install `mpg123`, `madplay`, `ffplay`, or `play`.");
    return 0;
}

static int platform_get_linux_volume_percent(int *percent, char *message, size_t message_size) {
    char output[512];
    int value = -1;
    char *cursor;

    if (!percent) {
        return 0;
    }

    if (!(platform_command_success("amixer sget Master 2>/dev/null", output, sizeof(output)) ||
          platform_command_success("amixer sget PCM 2>/dev/null", output, sizeof(output)))) {
        platform_set_message(message, message_size, "Unable to read system volume.");
        return 0;
    }

    cursor = strchr(output, '[');
    while (cursor) {
        if (sscanf(cursor, "[%d%%]", &value) == 1) {
            *percent = value;
            if (message && message_size > 0) {
                snprintf(message, message_size, "System volume is %d%%.", value);
            }
            return 1;
        }
        cursor = strchr(cursor + 1, '[');
    }

    platform_set_message(message, message_size, "Unable to parse system volume.");
    return 0;
}

static int platform_set_linux_volume_percent(int percent, char *message, size_t message_size) {
    char command[256];

    if (percent < 0) {
        percent = 0;
    } else if (percent > 100) {
        percent = 100;
    }

    snprintf(command, sizeof(command),
             "(amixer -q sset Master %d%% unmute) || (amixer -q sset PCM %d%% unmute)",
             percent, percent);
    if (platform_run_privileged_linux(command, message, message_size)) {
        snprintf(command, sizeof(command), "System volume updated to %d%%.", percent);
        platform_set_message(message, message_size, command);
        return 1;
    }

    return 0;
}

static int platform_set_linux_audio_output(const char *output, char *message, size_t message_size) {
    char card_index[32];
    char device_index[32];
    char config_path[PATH_MAX];
    const char *state_dir;
    const char *label;

    label = strcmp(output, "hat") == 0 ? "Audio HAT" : "HDMI";
    if (!platform_find_linux_audio_target(output, card_index, sizeof(card_index), device_index, sizeof(device_index))) {
        snprintf(message, message_size, "Unable to find an ALSA device for %s.", label);
        return 0;
    }

    state_dir = platform_get_dev_state_dir();
    snprintf(config_path, sizeof(config_path), "%s/asound.conf", state_dir);
    if (!platform_write_alsa_config_file(config_path, card_index, device_index)) {
        platform_set_message(message, message_size, "Unable to write ALSA config for the selected audio output.");
        return 0;
    }

    setenv("ALSA_CONFIG_PATH", config_path, 1);
    snprintf(message, message_size, "Audio output set to %s using ALSA device hw:%s,%s.", label, card_index, device_index);
    return 1;
}

int platform_ops_set_timezone(const char *timezone, char *message, size_t message_size) {
    char escaped[512];
    char command[1024];

    if (!timezone || timezone[0] == '\0') {
        platform_set_message(message, message_size, "Timezone is required.");
        return 0;
    }

    if (platform_ops_get_mode() == PLATFORM_MODE_STUB) {
        snprintf(command, sizeof(command), "macOS stub mode: timezone change skipped for %s.", timezone);
        platform_set_message(message, message_size, command);
        return 1;
    }

    if (platform_ops_get_mode() == PLATFORM_MODE_TINYCORE) {
        PlatformWifiResponse response;

        if (platform_tinycore_request("set_timezone", timezone, NULL, NULL, NULL, &response, NULL, 0)) {
            platform_set_message(message, message_size,
                                 response.message[0] ? response.message : "Timezone updated.");
            return 1;
        }
        platform_set_message(message, message_size,
                             response.message[0] ? response.message : "Unable to update timezone.");
        return 0;
    } else {
        snprintf(command, sizeof(command),
                 "(command -v timedatectl >/dev/null 2>&1 && timedatectl set-timezone %s) || "
                 "(ln -sf /usr/share/zoneinfo/%s /etc/localtime && printf '%s\\n' > /etc/timezone)",
                 platform_escape_shell_arg(timezone, escaped, sizeof(escaped)),
                 timezone,
                 timezone);
    }

    if (platform_run_privileged_linux(command, message, message_size)) {
        snprintf(command, sizeof(command), "Timezone updated to %s.", timezone);
        platform_set_message(message, message_size, command);
        return 1;
    }

    return 0;
}

int platform_ops_set_system_time(int hour, int minute, int second, char *message, size_t message_size) {
    char command[1024];
    time_t now;
    struct tm local_tm;

    if (platform_ops_get_mode() == PLATFORM_MODE_STUB) {
        snprintf(command, sizeof(command),
                 "macOS stub mode: time change skipped for %02d:%02d:%02d.",
                 hour, minute, second);
        platform_set_message(message, message_size, command);
        return 1;
    }

    if (platform_ops_get_mode() == PLATFORM_MODE_TINYCORE) {
        PlatformWifiResponse response;
        char hour_buffer[16];
        char minute_buffer[16];
        char second_buffer[16];

        snprintf(hour_buffer, sizeof(hour_buffer), "%d", hour);
        snprintf(minute_buffer, sizeof(minute_buffer), "%d", minute);
        snprintf(second_buffer, sizeof(second_buffer), "%d", second);
        if (platform_tinycore_request("set_time", hour_buffer, minute_buffer, second_buffer, NULL, &response, NULL, 0)) {
            platform_set_message(message, message_size,
                                 response.message[0] ? response.message : "System time updated.");
            return 1;
        }
        platform_set_message(message, message_size,
                             response.message[0] ? response.message : "Unable to update system time.");
        return 0;
    }

    now = time(NULL);
    local_tm = *localtime(&now);
    local_tm.tm_hour = hour;
    local_tm.tm_min = minute;
    local_tm.tm_sec = second;

    strftime(command, sizeof(command), "date -s '%Y-%m-%d %H:%M:%S'", &local_tm);
    if (platform_run_privileged_linux(command, message, message_size)) {
        snprintf(command, sizeof(command), "System time updated to %02d:%02d:%02d.", hour, minute, second);
        platform_set_message(message, message_size, command);
        return 1;
    }

    return 0;
}

int platform_ops_set_system_date(int year, int month, int day, char *message, size_t message_size) {
    char command[1024];
    time_t now;
    struct tm local_tm;

    if (platform_ops_get_mode() == PLATFORM_MODE_STUB) {
        snprintf(command, sizeof(command),
                 "macOS stub mode: date change skipped for %04d-%02d-%02d.",
                 year, month, day);
        platform_set_message(message, message_size, command);
        return 1;
    }

    if (platform_ops_get_mode() == PLATFORM_MODE_TINYCORE) {
        PlatformWifiResponse response;
        char year_buffer[16];
        char month_buffer[16];
        char day_buffer[16];

        snprintf(year_buffer, sizeof(year_buffer), "%d", year);
        snprintf(month_buffer, sizeof(month_buffer), "%d", month);
        snprintf(day_buffer, sizeof(day_buffer), "%d", day);
        if (platform_tinycore_request("set_date", year_buffer, month_buffer, day_buffer, NULL, &response, NULL, 0)) {
            platform_set_message(message, message_size,
                                 response.message[0] ? response.message : "System date updated.");
            return 1;
        }
        platform_set_message(message, message_size,
                             response.message[0] ? response.message : "Unable to update system date.");
        return 0;
    }

    now = time(NULL);
    local_tm = *localtime(&now);
    local_tm.tm_year = year - 1900;
    local_tm.tm_mon = month - 1;
    local_tm.tm_mday = day;

    strftime(command, sizeof(command), "date -s '%Y-%m-%d %H:%M:%S'", &local_tm);
    if (platform_run_privileged_linux(command, message, message_size)) {
        snprintf(command, sizeof(command), "System date updated to %04d-%02d-%02d.", year, month, day);
        platform_set_message(message, message_size, command);
        return 1;
    }

    return 0;
}
