idf_component_register(SRCS "main.c" INCLUDE_DIRS "." REQUIRES lighting_core lab_radio lab_neopixel esp_timer nvs_flash esp_driver_gpio esp_wifi spi_bus mcp3202 mux_cd74hc4067 esp_rom)
