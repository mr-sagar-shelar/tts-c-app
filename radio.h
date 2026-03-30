#ifndef RADIO_H
#define RADIO_H

#include "utils.h"

typedef struct {
    char name[128];
    char url[256];
} RadioStation;

/**
 * UI handler for internet radio.
 */
void handle_internet_radio();

#endif /* RADIO_H */
