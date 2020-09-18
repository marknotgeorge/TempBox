/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include <string>
#include <sstream>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_sntp.h"
#include <esp_log.h>

#include "wifi_manager.h"

#include "ssd1306.h"
#include "ssd1306_draw.h"
#include "ssd1306_font.h"
#include "ssd1306_bitmap.h"
#include "ssd1306_default_if.h"

#include "zones.h"

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

static bool timeSet = false;

static uint8_t logo_bmp[] =
	{0b00000000, 0b11000000,
	 0b00000001, 0b11000000,
	 0b00000001, 0b11000000,
	 0b00000011, 0b11100000,
	 0b11110011, 0b11100000,
	 0b11111110, 0b11111000,
	 0b01111110, 0b11111111,
	 0b00110011, 0b10011111,
	 0b00011111, 0b11111100,
	 0b00001101, 0b01110000,
	 0b00011011, 0b10100000,
	 0b00111111, 0b11100000,
	 0b00111111, 0b11110000,
	 0b01111100, 0b11110000,
	 0b01110000, 0b01110000,
	 0b00000000, 0b00110000};

#define LOGO_WIDTH 16
#define LOGO_HEIGHT 16


void displayTime()
{
	time_t now = 0;
	struct tm timeinfo = {0};
	char timeBuff[16];
	char dateBuff[16];



	if (timeSet)
		{
			time(&now);
			localtime_r(&now, &timeinfo);
			strftime(timeBuff, sizeof(timeBuff), "%H:%M", &timeinfo);
			strftime(dateBuff, sizeof(dateBuff), "%a %b %d", &timeinfo);
		}
		else
		{
			strcpy(timeBuff,"--:--");
			strcpy(dateBuff, "Syncing time..." );
		}
		

		
		SSD1306_Clear(&i2cDisplay, SSD_COLOR_BLACK);
		SSD1306_SetFont(&i2cDisplay, &Font_droid_sans_mono_7x13);
		SSD1306_FontDrawAnchoredString(&i2cDisplay, TextAnchor_North, dateBuff, SSD_COLOR_WHITE);
		SSD1306_SetFont(&i2cDisplay, &Font_droid_sans_fallback_24x28);
		SSD1306_FontDrawAnchoredString(&i2cDisplay, TextAnchor_Center, timeBuff, SSD_COLOR_WHITE);
		SSD1306_Update(&i2cDisplay);
}


/**
 * @brief RTOS task that periodically prints the heap memory available.
 * @note Pure debug information, should not be ever started on production code! This is an example on how you can integrate your code with wifi-manager
 */
void monitoring_task(void *pvParameter)
{
	for (;;)
	{
		xSemaphoreTake(displaySemaphore, portMAX_DELAY);
		displayTime();
		
		xSemaphoreGive(displaySemaphore);
		
		vTaskDelay(pdMS_TO_TICKS(300));
	}
}

void setupDisplay()
{
	// Set up display
	ESP_LOGI(TAG, "Setting up display...");
	assert(SSD1306_I2CMasterInitDefault() == true);
	assert(SSD1306_I2CMasterAttachDisplayDefault(&i2cDisplay, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_ADDRESS, DISPLAY_RESET) == true);
	SSD1306_Clear(&i2cDisplay, SSD_COLOR_BLACK);
	SSD1306_DrawBitmap(&i2cDisplay, (DISPLAY_WIDTH - LOGO_WIDTH) / 2, (DISPLAY_HEIGHT - LOGO_HEIGHT) / 2,
					   logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, SSD_COLOR_WHITE);
	SSD1306_Update(&i2cDisplay);
	vTaskDelay(pdMS_TO_TICKS(3000));
}

static void sntp_sync_cb(struct timeval *tv)
{
	time_t now;
	struct tm timeinfo;

	char strftime_buf[64];

	ESP_LOGI(TAG, "SNTP time sync received!");
	time(&now);

	localtime_r(&now, &timeinfo);

	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);

	timeSet = true;
}

static void initialize_sntp(void)
{
	ESP_LOGI(TAG, "Initializing SNTP");
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, (char *)"pool.ntp.org");
	sntp_setservername(1, (char *)"ntp.virginmedia.org");
	sntp_set_time_sync_notification_cb(sntp_sync_cb);
	sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
	sntp_init();
}

static void obtain_time(void)
{

	time_t now = 0;
	struct tm timeinfo = {0};

	int retry = 0;
	const int retry_count = 10;
	while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count)
	{
		ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
		vTaskDelay(2000 / portTICK_PERIOD_MS);
		time(&now);
		localtime_r(&now, &timeinfo);
	}
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

	//TODO: Get timezone information from Web API
	char posixTimeZone[] = "Europe/London";

	const char * timeZoneString = micro_tz_db_get_posix_str(posixTimeZone);

	ESP_LOGI("TAG", "Timezone data: %s", timeZoneString);
	
	
	setenv("TZ", timeZoneString, 1);
	tzset();

	initialize_sntp();

	// wait for time to be set
	obtain_time();
}

void app_main()
{
	// Set up the display
	setupDisplay();

	/* start the wifi manager */
	wifi_manager_start();

	/* register a callback as an example to how you can integrate your code with the wifi manager */
	wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &cb_connection_ok);

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
