#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_rom_sys.h"

#define PIN_SCK 19
#define PIN_MISO 20
#define PIN_MOSI 18
#define PIN_CS 22
static const gpio_num_t mux_pins[] = {GPIO_NUM_0, GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_21};

static void mux_select(unsigned channel) {
    for (unsigned bit = 0; bit < 4; bit++)
        gpio_set_level(mux_pins[bit], (channel >> bit) & 1);
    // Allow the mux and ADC input to settle after switching channels.
    esp_rom_delay_us(20);
}

static uint16_t adc_read(spi_device_handle_t adc) {
    // MCP3202: start, single-ended, CH0, MSB first; mode 0, 24 clocks.
    spi_transaction_t t = { .length = 24, .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA };
    t.tx_data[0] = 0x01;
    t.tx_data[1] = 0xA0;
    t.tx_data[2] = 0x00;
    ESP_ERROR_CHECK(spi_device_polling_transmit(adc, &t));
    return ((t.rx_data[1] & 0x0f) << 8) | t.rx_data[2];
}

void app_main(void) {
    gpio_config_t mux = { .pin_bit_mask = (1ULL<<0)|(1ULL<<1)|(1ULL<<2)|(1ULL<<21), .mode = GPIO_MODE_OUTPUT };
    ESP_ERROR_CHECK(gpio_config(&mux));
    spi_bus_config_t bus = { .sclk_io_num = PIN_SCK, .miso_io_num = PIN_MISO, .mosi_io_num = PIN_MOSI, .quadwp_io_num = -1, .quadhd_io_num = -1, .max_transfer_sz = 3 };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_DISABLED));
    spi_device_interface_config_t config = { .clock_speed_hz = 1000000, .mode = 0, .spics_io_num = PIN_CS, .queue_size = 1 };
    spi_device_handle_t adc;
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &config, &adc));
    setvbuf(stdout, NULL, _IONBF, 0);
    uint32_t sequence = 0;
    while (1) {
        uint16_t readings[12];
        for (unsigned ch = 0; ch < 12; ch++) {
            mux_select(ch);
            // Discard the first conversion after a mux change.
            (void)adc_read(adc);
            unsigned sum = 0;
            for (unsigned n = 0; n < 4; n++) sum += adc_read(adc);
            readings[ch] = sum / 4;
        }
        printf("K,%lu", (unsigned long)sequence++);
        for (int ch = 0; ch < 12; ch++) printf(",%u", (unsigned)readings[ch]);
        printf("\n");
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
