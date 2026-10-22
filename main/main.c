#include "dht22.h"
#include "display.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ui.h"

#define SENSOR_PIN GPIO_NUM_19

static const char *TAG = "main";

// Latest good sensor reading.
static dht22_reading_t s_latest;

void app_main(void)
{
    ESP_ERROR_CHECK(display_init());
    ui_init();

    ESP_ERROR_CHECK(dht22_init(SENSOR_PIN));

    while (1) {
        // Also covers the sensor's ~2 s startup time before the first read.
        vTaskDelay(pdMS_TO_TICKS(2000));

        dht22_reading_t reading;
        if (dht22_read(&reading) == ESP_OK) {
            s_latest = reading;
            ESP_LOGI(TAG, "Temp: %.1f C  Humidity: %.1f %%", s_latest.temperature_c, s_latest.humidity_pct);
            ui_show_reading(&s_latest);
        }
        // On failure, the screen keeps showing the last good value. Occasional misses are normal.
    }
}
