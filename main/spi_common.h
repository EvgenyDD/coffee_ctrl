#ifndef SPI_COMMON_H__
#define SPI_COMMON_H__

#include <stdint.h>

void spi_common_init(void);
void spi_write_lcd(uint8_t *data, uint32_t length);
uint16_t spi_xfer_adc(uint8_t channel);

#endif // SPI_COMMON_H__