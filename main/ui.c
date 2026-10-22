#include "ui.h"

#include <stdio.h>
#include "esp_lvgl_port.h"

#define COLOR_BG       0x101418
#define COLOR_CAPTION  0x8A949E
#define COLOR_TEMP     0xFF9F43
#define COLOR_HUMIDITY 0x4AA8FF

// Widgets that change after startup. Static labels (captions) aren't kept.
static lv_obj_t *s_temp_value;
static lv_obj_t *s_humidity_value;
static lv_obj_t *s_humidity_bar;

static lv_obj_t *create_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int y, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
    lv_label_set_text(label, text);
    return label;
}

void ui_init(void)
{
    // LVGL runs in its own task. Every LVGL call from another task must hold this
    // lock, like a synchronized block in Java.
    lvgl_port_lock(0);

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BG), 0);

    create_label(screen, &lv_font_montserrat_14, COLOR_CAPTION, 22, "TEMPERATURE");
    s_temp_value = create_label(screen, &lv_font_montserrat_48, COLOR_TEMP, 44, "--.-°C");

    create_label(screen, &lv_font_montserrat_14, COLOR_CAPTION, 122, "HUMIDITY");
    s_humidity_value = create_label(screen, &lv_font_montserrat_28, COLOR_HUMIDITY, 144, "--.- %");

    s_humidity_bar = lv_bar_create(screen);
    lv_obj_set_size(s_humidity_bar, 180, 12);
    lv_obj_align(s_humidity_bar, LV_ALIGN_TOP_MID, 0, 196);
    lv_bar_set_range(s_humidity_bar, 0, 100);
    lv_obj_set_style_bg_color(s_humidity_bar, lv_color_hex(COLOR_HUMIDITY), LV_PART_INDICATOR);

    lvgl_port_unlock();
}

void ui_show_reading(const dht22_reading_t *reading)
{
    // Format before taking the lock so the LVGL task is blocked as briefly as possible.
    char temp_text[16];
    char humidity_text[16];
    snprintf(temp_text, sizeof(temp_text), "%.1f°C", reading->temperature_c);
    snprintf(humidity_text, sizeof(humidity_text), "%.1f %%", reading->humidity_pct);

    lvgl_port_lock(0);
    lv_label_set_text(s_temp_value, temp_text);
    lv_label_set_text(s_humidity_value, humidity_text);
    lv_bar_set_value(s_humidity_bar, (int32_t)(reading->humidity_pct + 0.5f), LV_ANIM_ON);
    lvgl_port_unlock();
}
