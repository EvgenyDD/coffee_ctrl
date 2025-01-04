#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/gpio_hal.h"
#include "mcp3004.h"
#include "sdkconfig.h"
#include "sh1107.h"
#include "soc/timer_group_reg.h"
#include "spi_common.h"
#include "web/web_common.h"
#include "web/wifi.h"
#include "web/ws.h"
#include <esp_ota_ops.h>
#include <stdio.h>
#include <string.h>

#if !CONFIG_IDF_TARGET_ESP32C3
#error "Wrong target"
#endif

uint32_t restart_timer = 0;

void app_main(void)
{
	ESP_LOGI("", "Running on core: %d\n", xPortGetCoreID());
	ESP_LOGI("HEAP", "Free heap: %d", xPortGetFreeHeapSize());

	spi_common_init();
	sh1107_init();
	sh1107_set_font(font5x8);

	web_common_init();
	wifi_init();
	ws_init();

	uint32_t prev_systick = esp_log_timestamp();
	esp_ota_mark_app_valid_cancel_rollback();
	while(1)
	{
		const uint32_t systick_now = esp_log_timestamp();
		uint32_t diff_ms = systick_now - prev_systick;
		prev_systick = systick_now;

		static uint32_t tick_ms_acc = 0;
		static uint8_t a = 0;
		tick_ms_acc += diff_ms;
		if(tick_ms_acc >= 10)
		{
			tick_ms_acc = 0;
			for(uint8_t i = 0; i < 4; i++)
				sh1107_print(0, 10 + i * 10, false, "CH %d: %d     ", i, spi_xfer_adc(i));
			sh1107_print(0, 0, false, "--> %d  ", a++);
			sh1107_set_update_pending();
		}

		ssh1107_poll();

		if(restart_timer)
		{
			if(restart_timer <= diff_ms) esp_restart();
			restart_timer -= diff_ms;
		}

		vTaskDelay(1);
	}
}
