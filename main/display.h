#pragma once

#include "esp_err.h"

#define DISPLAY_WIDTH  240
#define DISPLAY_HEIGHT 240

// Sets up the ST7789 and hands it to LVGL. Draw with LVGL widgets afterwards (see ui.c).
esp_err_t display_init(void);
