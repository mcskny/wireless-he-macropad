idf_component_register(SRCS "main.c" "usb_composite.c" "hid_engine.c" "hid_profile.c" INCLUDE_DIRS "." "../../shared" REQUIRES esp_timer esp_wifi esp_event nvs_flash espressif__esp_tinyusb)
