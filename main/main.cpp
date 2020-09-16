/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string>
#include <sstream>
#include <esp_wifi.h>
#include <esp_netif.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include <esp_log.h>

#include "wifi_manager.h"

#include "ssd1306.h"
#include "ssd1306_draw.h"
#include "ssd1306_font.h"
#include "ssd1306_default_if.h"

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define DISPLAY_ADDRESS 0x3c
#define DISPLAY_RESET -1

using namespace std;

extern "C"
{
	void app_main();
}

static const char TAG[] = "main";

static struct SSD1306_Device i2cDisplay;

SemaphoreHandle_t displaySemaphore = NULL;

/**
 * @brief RTOS task that periodically prints the heap memory available.
 * @note Pure debug information, should not be ever started on production code! This is an example on how you can integrate your code with wifi-manager
 */
void monitoring_task(void *pvParameter)
{
	int32_t oldFreeHeap = 0L;

	for (;;)
	{

		int32_t freeHeap = esp_get_free_heap_size();
		ESP_LOGD(TAG, "Old freeHeap: %d New freeHeap: %d", oldFreeHeap, freeHeap);
		if (freeHeap != oldFreeHeap)
		{
			
			ESP_LOGD(TAG, "Redrawing display!");
			xSemaphoreTake(displaySemaphore, portMAX_DELAY);
			std::stringstream s;
			s << freeHeap;
			string freeHeapText = s.str();
			SSD1306_Clear(&i2cDisplay, SSD_COLOR_BLACK);
			SSD1306_FontDrawAnchoredString(&i2cDisplay, TextAnchor_Center, freeHeapText.c_str(), SSD_COLOR_WHITE);
			SSD1306_Update(&i2cDisplay);
			xSemaphoreGive(displaySemaphore);			

			oldFreeHeap = freeHeap;
		}
		vTaskDelay(pdMS_TO_TICKS(10000));
	}
}

void setupDisplay()
{
	// Set up display
	ESP_LOGI(TAG, "Setting up display...");
	SSD1306_I2CMasterInitDefault();
	SSD1306_I2CMasterAttachDisplayDefault(&i2cDisplay, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_ADDRESS, DISPLAY_RESET);
	SSD1306_Clear(&i2cDisplay, SSD_COLOR_BLACK);
	SSD1306_SetFont(&i2cDisplay, &Font_droid_sans_fallback_24x28);
	SSD1306_FontDrawAnchoredString(&i2cDisplay, TextAnchor_Center, "TempBox", SSD_COLOR_WHITE);
	SSD1306_Update(&i2cDisplay);
}

/**
 * @brief this is an exemple of a callback that you can setup in your own app to get notified of wifi manager event.
 */
void cb_connection_ok(void *pvParameter)
{
	ip_event_got_ip_t *param = (ip_event_got_ip_t *)pvParameter;

	/* transform IP to human readable string */
	char str_ip[16];
	esp_ip4addr_ntoa(&param->ip_info.ip, str_ip, IP4ADDR_STRLEN_MAX);

	ESP_LOGI(TAG, "I have a connection and my IP is %s!", str_ip);
}

void app_main()
{
	/* start the wifi manager */
	// wifi_manager_start();

	/* register a callback as an example to how you can integrate your code with the wifi manager */
	// wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &cb_connection_ok);

	// Set up the display
	setupDisplay();

	// Create semaphore for display
	vSemaphoreCreateBinary(displaySemaphore);

	if (displaySemaphore != NULL)
	{
		/* your code should go here. Here we simply create a task on core 2 that monitors free heap memory */
		xTaskCreatePinnedToCore(&monitoring_task, "monitoring_task", 2048, NULL, 1, NULL, 1);
	}
	else
	{
		ESP_LOGE(TAG, "Unable to create display semaphore!");
	}
}
