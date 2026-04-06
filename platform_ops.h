#ifndef PLATFORM_OPS_H
#define PLATFORM_OPS_H

#include <stddef.h>

#define PLATFORM_WIFI_MAX_SCAN_RESULTS 64

typedef enum {
    PLATFORM_MODE_STUB = 0,
    PLATFORM_MODE_DEV = 1,
    PLATFORM_MODE_TINYCORE = 2
} PlatformMode;

typedef struct {
    char ssid[128];
    char security[32];
    char signal[64];
} PlatformWifiScanResult;

typedef struct {
    int success;
    char message[256];
    char interface_name[64];
    char connected[32];
    char ssid[128];
    char ip_address[64];
    PlatformWifiScanResult scans[PLATFORM_WIFI_MAX_SCAN_RESULTS];
    int scan_count;
} PlatformWifiResponse;

PlatformMode platform_ops_get_mode(void);
const char *platform_ops_get_mode_name(void);
int platform_ops_get_device_model(char *buffer, size_t buffer_size);

int platform_ops_wifi_status(PlatformWifiResponse *response);
int platform_ops_wifi_scan(PlatformWifiResponse *response);
int platform_ops_wifi_connect(const char *ssid, const char *password, PlatformWifiResponse *response);
int platform_ops_wifi_disconnect(PlatformWifiResponse *response);

int platform_ops_get_system_volume_percent(int *percent, char *message, size_t message_size);
int platform_ops_set_system_volume_percent(int percent, char *message, size_t message_size);
int platform_ops_set_audio_output(const char *output, char *message, size_t message_size);

int platform_ops_set_timezone(const char *timezone, char *message, size_t message_size);
int platform_ops_set_system_time(int hour, int minute, int second, char *message, size_t message_size);
int platform_ops_set_system_date(int year, int month, int day, char *message, size_t message_size);
int platform_ops_power_off(char *message, size_t message_size);
int platform_ops_record_voice(const char *path, int seconds, char *message, size_t message_size);
int platform_ops_play_mp3(const char *path, char *message, size_t message_size);

#endif /* PLATFORM_OPS_H */
