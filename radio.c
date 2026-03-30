#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "radio.h"
#include "utils.h"

static RadioStation stations[] = {
    {"Chillout Lounge", "http://icecast.vrt.be/mnm_chill-high.mp3"},
    {"Global Radio (News)", "http://stream.live.vc.bbcmedia.co.uk/bbc_world_service"},
    {"Smooth Jazz", "http://jazz.stream.publicradio.org/jazz.mp3"},
    {"Hindi Hits", "http://ice4.as34763.net/2000_h_mp3_128"},
    {"Classic Rock", "http://stream.rockantenne.de/classic-perlen/stream/mp3"}
};

static int station_count = sizeof(stations) / sizeof(stations[0]);

static void stop_playback() {
    // Kill any existing mpg123 process
    system("pkill -9 mpg123 > /dev/null 2>&1");
    system("pkill -9 mpv > /dev/null 2>&1");
}

static void play_station(int index) {
    stop_playback();
    
    char cmd[512];
    // We try mpg123 as it was found on the system
    snprintf(cmd, sizeof(cmd), "mpg123 -q \"%s\" > /dev/null 2>&1 &", stations[index].url);
    
    // If you prefer mpv or other player, you can change it here.
    // Using & to run in background.
    int res = system(cmd);
    if (res != 0) {
        // Fallback or error reporting
    }
}

void handle_internet_radio() {
    int current_sel = 0;
    int is_playing = 0;
    int playing_idx = -1;

    while (1) {
        printf("\033[H\033[J--- Internet Radio ---\n");
        for (int i = 0; i < station_count; i++) {
            if (i == current_sel) {
                printf("> %s %s\n", stations[i].name, (is_playing && playing_idx == i) ? "(Playing)" : "");
            } else {
                printf("  %s %s\n", stations[i].name, (is_playing && playing_idx == i) ? "(Playing)" : "");
            }
        }

        printf("\n------------------------------------------\n");
        printf("[Enter: Play/Pause | N: Next | P: Previous | Esc: Stop & Back]\n");
        if (is_playing) {
            printf("Currently Streaming: %s\n", stations[playing_idx].name);
        }
        fflush(stdout);

        int key = read_key();
        if (key == KEY_ESC) {
            stop_playback();
            break;
        } else if (key == KEY_ENTER) {
            if (is_playing && playing_idx == current_sel) {
                stop_playback();
                is_playing = 0;
                playing_idx = -1;
            } else {
                play_station(current_sel);
                is_playing = 1;
                playing_idx = current_sel;
            }
        } else if (key == 'n' || key == 'N' || key == KEY_DOWN) {
            current_sel = (current_sel + 1) % station_count;
            if (key == 'n' || key == 'N') {
                 if (is_playing) {
                    play_station(current_sel);
                    playing_idx = current_sel;
                 }
            }
        } else if (key == 'p' || key == 'P' || key == KEY_UP) {
            current_sel = (current_sel + station_count - 1) % station_count;
            if (key == 'p' || key == 'P') {
                if (is_playing) {
                    play_station(current_sel);
                    playing_idx = current_sel;
                }
            }
        }
    }
}
