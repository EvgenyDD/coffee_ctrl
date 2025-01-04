#include "spi_common.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "sh1107.h"
#include <string.h>

#define PIN_SPI_CLK 6
#define PIN_SPI_MISO 2
#define PIN_SPI_MOSI 7

#define PIN_CS_LCD 4
#define PIN_CS_ADC 3

spi_device_handle_t spi_lcd;
spi_device_handle_t spi_adc;

void spi_common_init(void)
{
	spi_bus_config_t bus_conf = {
		.mosi_io_num = PIN_SPI_MOSI,
		.miso_io_num = PIN_SPI_MISO,
		.sclk_io_num = PIN_SPI_CLK,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = 120000,
	};
	ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_conf, SPI_DMA_CH_AUTO));
	static const spi_device_interface_config_t cfg_lcd = {
		.clock_speed_hz = 8000000,
		.mode = 0,
		.spics_io_num = PIN_CS_LCD,
		.queue_size = 64,
	};
	static const spi_device_interface_config_t cfg_adc = {
		.clock_speed_hz = 1350000,
		.mode = 0,
		.spics_io_num = PIN_CS_ADC,
		.queue_size = 64,
	};
	ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &cfg_lcd, &spi_lcd));
	ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &cfg_adc, &spi_adc));
}

void spi_write_lcd(uint8_t *data, uint32_t length)
{
	static spi_transaction_t SPITransaction;
	memset(&SPITransaction, 0, sizeof(spi_transaction_t));
	SPITransaction.length = length * 8;
	SPITransaction.tx_buffer = data;
	ESP_ERROR_CHECK(spi_device_transmit(spi_lcd, &SPITransaction));
}

uint16_t spi_xfer_adc(uint8_t channel)
{
	static spi_transaction_t SPITransaction;
	memset(&SPITransaction, 0, sizeof(spi_transaction_t));
	uint8_t rx[3];
	uint8_t tx[3] = {1, (1 << 7) | ((channel & 3) << 4), 0};
	SPITransaction.length = 3 * 8;
	SPITransaction.tx_buffer = tx;
	SPITransaction.rx_buffer = rx;
	ESP_ERROR_CHECK(spi_device_transmit(spi_adc, &SPITransaction));
	return (((rx[1] & 0x03) << 8) | (rx[2]));
}
