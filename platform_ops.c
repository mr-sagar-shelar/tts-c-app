#include "platform_ops.h"

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

static int platform_tinycore_wifi_request(const char *command,
                                          const char *ssid,
                                          const char *password,
                                          PlatformWifiResponse *response) {
    char request_template[] = "/tmp/sai-wifi-ipc/req-XXXXXX";
    char path[PATH_MAX];
    char done_path[PATH_MAX];
    int waited_ms = 0;

    platform_init_wifi_response(response, "Wi-Fi service is not available.");
    mkdir("/tmp/sai-wifi-ipc", 0777);
    if (!mkdtemp(request_template)) {
        platform_set_message(response->message, sizeof(response->message), "Unable to prepare Wi-Fi request.");
        return 0;
    }

    snprintf(path, sizeof(path), "%s/command", request_template);
    platform_write_text_file(path, command);
    if (ssid) {
        snprintf(path, sizeof(path), "%s/ssid", request_template);
        platform_write_text_file(path, ssid);
    }
    if (password) {
        snprintf(path, sizeof(path), "%s/password", request_template);
        platform_write_text_file(path, password);
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
    platform_cleanup_dir(request_template);

    return response->success;
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

static int platform_run_privileged_linux(const char *command, char *message, size_t message_size) {
    char full_command[2048];
    char prefix[64];
    int has_privilege_hint = platform_linux_prefix(prefix, sizeof(prefix));

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
        snprintf(command, sizeof(command),
                 "ln -sf /usr/share/zoneinfo/%s /etc/localtime && printf '%s\\n' > /etc/timezone",
                 timezone, timezone);
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
