#ifndef DICTIONARY_H
#define DICTIONARY_H

#include "utils.h"
#include "config.h"
#include "cJSON.h"

/**
 * Handles the local Sense Dictionary.
 */
void handle_dictionary();

/**
 * Handles the English Only Dictionary (via API).
 */
void handle_english_only_dictionary();

/**
 * Handles the Multi Language Dictionary (via API).
 */
void handle_multi_lang_dictionary();

#endif /* DICTIONARY_H */
