#include "voice_library.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "download_manager.h"
#include "utils.h"

#define VOICE_LIBRARY_DIR "voices"
#define VOICE_LIBRARY_URL "http://cmuflite.org/packed/flite-2.1/voices/"

typedef struct {
    const char *filename;
    const char *label;
} VoiceLibraryEntry;

typedef struct {
    pid_t pid;
    int in_progress;
    int success;
    int completed;
    char filename[PATH_MAX];
    char path[PATH_MAX];
    char action[16];
    char message[256];
} VoiceOperationState;

static const VoiceLibraryEntry voice_entries[] = {
    {"cmu_us_slt.flitevox", "cmu_us_slt.flitevox"},
    {"cmu_us_aup.flitevox", "cmu_us_aup.flitevox"},
    {"cmu_us_fem.flitevox", "cmu_us_fem.flitevox"},
    {"cmu_us_rms.flitevox", "cmu_us_rms.flitevox"},
    {"cmu_indic_ben_rm.flitevox", "cmu_indic_ben_rm.flitevox"},
    {"cmu_indic_guj_ad.flitevox", "cmu_indic_guj_ad.flitevox"},
    {"cmu_indic_guj_dp.flitevox", "cmu_indic_guj_dp.flitevox"},
    {"cmu_indic_guj_kt.flitevox", "cmu_indic_guj_kt.flitevox"},
    {"cmu_indic_hin_ab.flitevox", "cmu_indic_hin_ab.flitevox"},
    {"cmu_indic_kan_plv.flitevox", "cmu_indic_kan_plv.flitevox"},
    {"cmu_indic_mar_aup.flitevox", "cmu_indic_mar_aup.flitevox"},
    {"cmu_indic_mar_slp.flitevox", "cmu_indic_mar_slp.flitevox"},
    {"cmu_indic_pan_amp.flitevox", "cmu_indic_pan_amp.flitevox"},
    {"cmu_indic_tam_sdr.flitevox", "cmu_indic_tam_sdr.flitevox"},
    {"cmu_indic_tel_kpn.flitevox", "cmu_indic_tel_kpn.flitevox"},
    {"cmu_indic_tel_sk.flitevox", "cmu_indic_tel_sk.flitevox"},
    {"cmu_indic_tel_ss.flitevox", "cmu_indic_tel_ss.flitevox"}
};

static VoiceOperationState voice_operation = {0};
static DownloadTask voice_download = {0};

static void build_voice_path(const char *filename, char *buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size, "%s/%s", VOICE_LIBRARY_DIR, filename);
}

static int voice_file_exists(const char *filename) {
    char path[PATH_MAX];
    struct stat st;

    build_voice_path(filename, path, sizeof(path));
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int ensure_voice_directory(void) {
    struct stat st;

    if (stat(VOICE_LIBRARY_DIR, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }

    return mkdir(VOICE_LIBRARY_DIR, 0777) == 0;
}

static int start_download_voice_file(const char *filename, char *message, size_t message_size) {
    char path[PATH_MAX];
    char url[PATH_MAX];

    if (!ensure_voice_directory()) {
        snprintf(message, message_size, "Unable to create '%s' directory.", VOICE_LIBRARY_DIR);
        return 0;
    }

    if (voice_operation.in_progress) {
        snprintf(message, message_size, "Please wait for the current voice operation to finish.");
        return 0;
    }

    build_voice_path(filename, path, sizeof(path));
    snprintf(url, sizeof(url), "%s%s", VOICE_LIBRARY_URL, filename);
    download_task_init(&voice_download);
    if (!download_task_start(&voice_download, url, path, filename, message, message_size)) {
        return 0;
    }

    memset(&voice_operation, 0, sizeof(voice_operation));
    voice_operation.pid = voice_download.pid;
    voice_operation.in_progress = 1;
    snprintf(voice_operation.filename, sizeof(voice_operation.filename), "%s", filename);
    snprintf(voice_operation.path, sizeof(voice_operation.path), "%s", path);
    snprintf(voice_operation.action, sizeof(voice_operation.action), "download");
    snprintf(voice_operation.message, sizeof(voice_operation.message), "%s", voice_download.status);
    snprintf(message, message_size, "Started download for %s.", filename);
    return 1;
}

static int start_delete_voice_file(const char *filename, char *message, size_t message_size) {
    char path[PATH_MAX];
    pid_t pid;

    if (voice_operation.in_progress) {
        snprintf(message, message_size, "Please wait for the current voice operation to finish.");
        return 0;
    }

    build_voice_path(filename, path, sizeof(path));
    if (!voice_file_exists(filename)) {
        snprintf(message, message_size, "%s is not downloaded.", filename);
        return 0;
    }

    pid = fork();
    if (pid < 0) {
        snprintf(message, message_size, "Unable to start delete for %s.", filename);
        return 0;
    }

    if (pid == 0) {
        _exit(remove(path) == 0 ? 0 : 1);
    }

    memset(&voice_operation, 0, sizeof(voice_operation));
    voice_operation.pid = pid;
    voice_operation.in_progress = 1;
    snprintf(voice_operation.filename, sizeof(voice_operation.filename), "%s", filename);
    snprintf(voice_operation.path, sizeof(voice_operation.path), "%s", path);
    snprintf(voice_operation.action, sizeof(voice_operation.action), "delete");
    snprintf(voice_operation.message, sizeof(voice_operation.message), "Deleting %s...", filename);
    snprintf(message, message_size, "Started delete for %s.", filename);
    return 1;
}

static void update_voice_operation(void) {
    int status;
    pid_t rc;

    if (!voice_operation.in_progress) {
        return;
    }

    if (strcmp(voice_operation.action, "download") == 0) {
        download_task_poll(&voice_download);
        snprintf(voice_operation.message, sizeof(voice_operation.message), "%s", voice_download.status);

        if (!voice_download.active && voice_download.completed) {
            voice_operation.in_progress = 0;
            voice_operation.completed = 1;
            voice_operation.success = voice_download.success;
            if (!voice_operation.success) {
                remove(voice_operation.path);
            }
            download_task_reset(&voice_download);
        }
        return;
    }

    rc = waitpid(voice_operation.pid, &status, WNOHANG);
    if (rc == 0 || rc < 0) {
        return;
    }

    voice_operation.in_progress = 0;
    voice_operation.completed = 1;
    voice_operation.success = WIFEXITED(status) && WEXITSTATUS(status) == 0;

    if (!voice_operation.success && strcmp(voice_operation.action, "download") == 0) {
        remove(voice_operation.path);
    }

    if (voice_operation.success) {
        snprintf(voice_operation.message, sizeof(voice_operation.message),
                 "%s completed for %s.",
                 strcmp(voice_operation.action, "download") == 0 ? "Download" : "Delete",
                 voice_operation.filename);
    } else {
        snprintf(voice_operation.message, sizeof(voice_operation.message),
                 "%s failed for %s.",
                 strcmp(voice_operation.action, "download") == 0 ? "Download" : "Delete",
                 voice_operation.filename);
    }
}

static void show_voice_action_result(const char *voice_name, const char *message) {
    printf("\033[H\033[J--- Voice File ---\n");
    printf("%s\n\n", voice_name);
    printf("%s\n", message);
    printf("\nPress any key to continue...");
    fflush(stdout);
    read_key();
}

static void handle_voice_actions(const VoiceLibraryEntry *entry) {
    int selected_index = 0;
    const char *actions[] = {"Download", "Delete", "Back"};
    static const char spinner[] = "|/-\\";
    static int spinner_index = 0;

    if (!entry) {
        return;
    }

    while (1) {
        int downloaded;
        int current_operation;
        char spin[2];

        update_voice_operation();
        downloaded = voice_file_exists(entry->filename);
        current_operation = voice_operation.in_progress &&
                            strcmp(voice_operation.filename, entry->filename) == 0;
        spin[0] = spinner[spinner_index++ % 4];
        spin[1] = '\0';

        printf("\033[H\033[J--- Voice File ---\n");
        printf("%s\n", entry->label);
        printf("Source: %s%s\n", VOICE_LIBRARY_URL, entry->filename);
        printf("Status: %s\n\n", downloaded ? "Downloaded" : "Not downloaded");

        for (int i = 0; i < 3; i++) {
            if (i == selected_index) {
                printf("> %s\n", actions[i]);
            } else {
                printf("  %s\n", actions[i]);
            }
        }

        if (current_operation || voice_operation.in_progress) {
            printf("\n%s %s\n", spin, voice_operation.message);
            if (strcmp(voice_operation.action, "download") == 0) {
                printf("Progress: %d%%\n", voice_download.progress_percent);
            }
            printf("Navigation stays within this voice menu until the current operation finishes.\n");
            printf("[Arrows: Navigate locally | Enter: Disabled | Esc: Disabled]\n");
        } else {
            printf("\n[Arrows: Navigate | Enter: Select | Esc: Back]\n");
        }
        fflush(stdout);

        if (voice_operation.completed) {
            voice_operation.completed = 0;
            show_voice_action_result(entry->label, voice_operation.message);
            continue;
        }

        int key = voice_operation.in_progress ? read_key_timeout(150) : read_key();
        if (key == KEY_UP && selected_index > 0) {
            selected_index--;
        } else if (key == KEY_DOWN && selected_index < 2) {
            selected_index++;
        } else if (!voice_operation.in_progress && (key == KEY_ESC || (key == KEY_ENTER && selected_index == 2))) {
            return;
        } else if (!voice_operation.in_progress && key == KEY_ENTER) {
            char message[256];

            if (selected_index == 0) {
                if (!start_download_voice_file(entry->filename, message, sizeof(message))) {
                    show_voice_action_result(entry->label, message);
                }
            } else if (selected_index == 1) {
                if (!start_delete_voice_file(entry->filename, message, sizeof(message))) {
                    show_voice_action_result(entry->label, message);
                }
            }
        }
    }
}

void voice_library_show_menu(void) {
    int selected_index = 0;
    const int total = (int)(sizeof(voice_entries) / sizeof(voice_entries[0]));

    while (1) {
        int page_start = (selected_index / 12) * 12;
        int page_end = page_start + 12;

        update_voice_operation();

        if (page_end > total) {
            page_end = total;
        }

        printf("\033[H\033[J--- Download Voice ---\n");
        printf("Official source: %s\n", VOICE_LIBRARY_URL);
        printf("Local folder: %s\n\n", VOICE_LIBRARY_DIR);

        for (int i = page_start; i < page_end; i++) {
            const char *status = voice_file_exists(voice_entries[i].filename) ? "downloaded" : "not downloaded";

            if (i == selected_index) {
                printf("> %s [%s]\n", voice_entries[i].label, status);
            } else {
                printf("  %s [%s]\n", voice_entries[i].label, status);
            }
        }

        if (voice_operation.in_progress) {
            static const char spinner[] = "|/-\\";
            static int spinner_index = 0;
            char spin[2];

            spin[0] = spinner[spinner_index++ % 4];
            spin[1] = '\0';
            printf("\n%s %s\n", spin, voice_operation.message);
            if (strcmp(voice_operation.action, "download") == 0) {
                printf("Progress: %d%%\n", voice_download.progress_percent);
            }
            printf("You can move within Download Voice, but back and new actions stay locked until it finishes.\n");
            printf("[Arrows: Navigate | Enter: Disabled | Esc: Disabled]\n");
        } else {
            printf("\n[Arrows: Navigate | Enter: Open voice actions | Esc: Back]\n");
        }
        fflush(stdout);

        if (voice_operation.completed) {
            voice_operation.completed = 0;
            show_voice_action_result(voice_operation.filename, voice_operation.message);
            continue;
        }

        int key = voice_operation.in_progress ? read_key_timeout(150) : read_key();
        if (key == KEY_UP && selected_index > 0) {
            selected_index--;
        } else if (key == KEY_DOWN && selected_index < total - 1) {
            selected_index++;
        } else if (!voice_operation.in_progress && key == KEY_ENTER) {
            handle_voice_actions(&voice_entries[selected_index]);
        } else if (!voice_operation.in_progress && key == KEY_ESC) {
            return;
        }
    }
}
