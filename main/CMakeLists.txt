idf_component_register(SRCS "main.c" "display.c" "dht22.c" "ui.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_lcd esp_driver_spi esp_driver_gpio esp_timer)
