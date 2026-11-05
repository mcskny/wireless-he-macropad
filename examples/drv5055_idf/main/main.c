#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"

void app_main(void) {
    adc_oneshot_unit_handle_t adc;
    adc_oneshot_unit_init_cfg_t unit = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit, &adc));
    adc_oneshot_chan_cfg_t cfg = { .bitwidth = ADC_BITWIDTH_12, .atten = ADC_ATTEN_DB_12 };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, ADC_CHANNEL_6, &cfg)); // GPIO34
    gpio_config_t gpio = { .pin_bit_mask = 1ULL << GPIO_NUM_27, .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE };
    ESP_ERROR_CHECK(gpio_config(&gpio));
    setvbuf(stdout, NULL, _IONBF, 0);
    while (1) {
        int sum = 0;
        for (int i = 0; i < 8; i++) {
            int raw;
            ESP_ERROR_CHECK(adc_oneshot_read(adc, ADC_CHANNEL_6, &raw));
            sum += raw;
        }
        printf("S,%d,%d\n", sum / 8, gpio_get_level(GPIO_NUM_27));
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
