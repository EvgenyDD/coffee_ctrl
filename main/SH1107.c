#include "sh1107.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "spi_common.h"
#include <string.h>

#define PIN_DC 5

#define NUM_PAGES 16

static char buf_print[256];
static uint8_t disp_buf[8 * 128] = {0};
static bool update_pending = true;
static int upd_phase = -1;

static struct
{
	const uint8_t *font;
	uint8_t x_size;
	uint8_t y_size;
	uint8_t offset;
	uint8_t numchars;
	uint8_t inverse;
} cfont = {0};

static void cmd_write(uint8_t cmd)
{
	gpio_set_level(PIN_DC, 0);
	spi_write_lcd(&cmd, 1);
}

static void data_write(uint8_t *data, uint32_t size)
{
	gpio_set_level(PIN_DC, 1);
	spi_write_lcd(data, size);
}

static void delayMS(int ms) { vTaskDelay(ms / portTICK_PERIOD_MS); }

static void update_continue(void)
{
	if(upd_phase >= NUM_PAGES || upd_phase == -1)
	{
		upd_phase = -1;
		return;
	}
	cmd_write(0x10);			 // set column address
	cmd_write(0x00);			 // set column address
	cmd_write(0xB0 + upd_phase); // set page address

	data_write(&disp_buf[upd_phase * 64], 64);
	upd_phase++;
}

static void update_start(void)
{
	if(upd_phase != -1) return;

	update_pending = false;
	upd_phase = 0;
	update_continue();
}

void sh1107_init(void)
{
	gpio_reset_pin(PIN_DC);
	gpio_set_direction(PIN_DC, GPIO_MODE_OUTPUT);

	cmd_write(0xAE);		 // Turn display off
	cmd_write(0xDC);		 // Set display start line
	cmd_write(0x00);		 // ...value
	cmd_write(0x81);		 // Set display contrast
	cmd_write(0x2F);		 // ...value
	cmd_write(0x20);		 // Set memory mode
	cmd_write(0xA0 + 1 * 0); // Non-rotated display
	cmd_write(0xC0 + 8 * 0); // Non-flipped vertical
	cmd_write(0xA8);		 // Set multiplex ratio
	cmd_write(0x7F);		 // ...value
	cmd_write(0xD3);		 // Set display offset to zero
	cmd_write(0x60);		 // ...value
	cmd_write(0xD5);		 // Set display clock divider
	cmd_write(0x51);		 // ...value
	cmd_write(0xD9);		 // Set pre-charge
	cmd_write(0x22);		 // ...value
	cmd_write(0xDB);		 // Set com detect
	cmd_write(0x35);		 // ...value
	cmd_write(0xB0);		 // Set page address
	cmd_write(0xDA);		 // Set com pins
	cmd_write(0x12);		 // ...value
	cmd_write(0xA4);		 // output ram to display
	cmd_write(0xA6);		 // Non-inverted display
	cmd_write(0xad);		 // external VPP
	cmd_write(0x80);		 // ...value
	delayMS(50);
	cmd_write(0xAF); // Turn display on
	update_pending = true;
}

bool sh1107_is_update_pending(void) { return update_pending && upd_phase == -1; }
void sh1107_set_update_pending(void) { update_pending = true; }

void ssh1107_poll(void)
{
	if(sh1107_is_update_pending())
	{
		update_start();
		while(upd_phase != -1)
			update_continue();
	}
}

void sh1107_buffer_clear(void) { memset(disp_buf, 0x0, sizeof(disp_buf)); }

void sh1107_pixel(uint16_t x, uint16_t y, uint8_t is_filled)
{
	if(x >= 128 || y >= 64) return;
	if(is_filled)
		disp_buf[y + x / 8 * 64] |= (1 << (x % 8));
	else
		disp_buf[y + x / 8 * 64] &= ~(1 << (x % 8));
}

void sh1107_invert_pixel(uint16_t x, uint16_t y)
{
	if(x >= 128 || y >= 64)
		return;
	x = 128 - 1 - x;

	if(disp_buf[y + x / 8 * 64] & (1 << (x % 8)))
		disp_buf[y + x / 8 * 64] &= ~(1 << (x % 8));
	else
		disp_buf[y + x / 8 * 64] |= (1 << (x % 8));
}

void sh1107_invert(uint16_t x, uint16_t y, uint16_t length, uint16_t height)
{
	for(uint16_t i = 0; i < length; i++)
		for(uint16_t j = 0; j < height; j++)
			sh1107_invert_pixel(x + i, y + j);
}

void sh1107_line_h(uint16_t x, uint16_t y, uint16_t length, uint8_t is_filled)
{
	for(uint16_t i = 0; i < length; i++)
	{
		sh1107_pixel(x + i, y, is_filled);
	}
}

void sh1107_line_v(uint16_t x, uint16_t y, uint16_t length, uint8_t is_filled)
{
	for(uint16_t i = 0; i < length; i++)
	{
		sh1107_pixel(x, y + i, is_filled);
	}
}

void sh1107_set_font(const uint8_t *font)
{
	cfont.font = font;
	cfont.x_size = cfont.font[0];
	cfont.y_size = cfont.font[1];
	cfont.offset = cfont.font[2];
	cfont.numchars = cfont.font[3];
	cfont.inverse = cfont.font[4];
}

#define BitIsSet(x, y) ((x) & (1 << (y)))

void sh1107_char(uint16_t x, uint16_t y, char s, uint8_t is_filled, uint8_t *act_width)
{
#define FONT_WIDTH cfont.x_size
#define FONT_HEIGHT cfont.y_size

	*act_width = 0;
	if(s >= cfont.offset + cfont.numchars) return;

	uint8_t scale = 1;
	uint8_t _transparent = 0;

	uint32_t temp = (uint32_t)((s - cfont.offset) * FONT_WIDTH * (FONT_HEIGHT < 8 ? 8 : FONT_HEIGHT) / 8) + 5;

	if(BitIsSet(cfont.inverse, 7))
	{ // STD FONTS
		for(int16_t j = FONT_HEIGHT * scale - 1; j > -1; j--)
		{
			for(int8_t k = 0; k < FONT_WIDTH / 8; k++)
			{
				char frame = cfont.font[temp++];
				for(uint8_t i = 0; i < 8 * scale; i++)
				{
					if(BitIsSet(frame, cfont.inverse & 0x01 ? (7 - i) : (i)))
					{
						sh1107_pixel(x + k * 8 * scale + 8 * scale - 1 - i, y + j, is_filled);
						if(*act_width < k * 8 * scale + 8 * scale - 2 - i) *act_width = k * 8 * scale + 8 * scale - 2 - i;
					}
					else if(!_transparent)
					{
						sh1107_pixel(x + k * 8 * scale + 8 * scale - 1 - i, y + j, !is_filled);
					}
				}
			}
			if(j % (scale) != 0) temp -= 2;
		}
	}
	else
	{
		for(uint16_t j = 0; j < FONT_WIDTH * (FONT_HEIGHT / 8 + FONT_HEIGHT % 8 ? 1 : 0) * scale; j++)
		{
			char frame = cfont.font[temp];

			for(uint8_t i = 0; i < FONT_HEIGHT * scale; i++)
			{
				if(BitIsSet(frame, cfont.inverse & 0x01 ? (7 - i / scale) : (i / scale)))
				{
					sh1107_pixel(x + j, y + i, is_filled);
					if(*act_width < j + 1) *act_width = j + 1;
				}
				else if(!_transparent)
				{
					sh1107_pixel(x + j, y + i, !is_filled);
				}
			}
			if(j % scale == (scale - 1)) temp++;
		}
	}
}

void sh1107_print_str(uint16_t x, uint16_t y, bool monospace, const char *s)
{
	for(const char *it = s; *it; it++)
	{
		uint8_t act_width;
		sh1107_char(x, y, *it, 1, &act_width);
		x += (monospace ? act_width : cfont.x_size) + 1;
	}
}

void sh1107_print_str_n(uint16_t x, uint16_t y, bool monospace, const char *s, uint32_t len)
{
	for(uint32_t i = 0; i < len; i++)
	{
		uint8_t act_width;
		sh1107_char(x, y, s[i], 1, &act_width);
		x += (monospace ? act_width : cfont.x_size) + 1;
	}
}

void sh1107_print(uint16_t x, uint16_t y, bool monospace, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vsnprintf(buf_print, sizeof(buf_print), format, args);
	va_end(args);
	sh1107_print_str(x, y, monospace, buf_print);
}
