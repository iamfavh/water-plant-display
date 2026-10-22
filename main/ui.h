#pragma once

#include "dht22.h"

// Builds the screen layout. Call once, after display_init().
void ui_init(void);

// Updates the on-screen values. Safe to call from any task.
void ui_show_reading(const dht22_reading_t *reading);
