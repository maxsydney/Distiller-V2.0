#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_timer.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/timer.h"
#include "ds18b20.h" 
#include "sensors.h"
#include "controlLoop.h"
#include "main.h"
#include "pinDefs.h"
#include "owb.h"
#include "owb_rmt.h"
#include "ds18b20.h"

#define SAMPLE_PERIOD        (400)   // milliseconds

static const char* tag = "Sensors";
static volatile double timeVal;

static OneWireBus_ROMCode device_rom_codes[MAX_DEVICES] = {0};
static DS18B20_Info * devices[MAX_DEVICES] = {0};
static OneWireBus * owb;
static owb_rmt_driver_info rmt_driver_info;
int num_devices = 0;

xQueueHandle tempQueue;
xQueueHandle flowRateQueue;

esp_err_t sensor_init(uint8_t ds_pin, DS18B20_RESOLUTION res)
{
    // Create a 1-Wire bus, using the RMT timeslot driver
    owb = owb_rmt_initialize(&rmt_driver_info, ds_pin, RMT_CHANNEL_1, RMT_CHANNEL_0);
    owb_use_crc(owb, true);  // enable CRC check for ROM code

    num_devices = scanTempSensorNetwork(device_rom_codes);
    printf("Found %d device%s\n", num_devices, num_devices == 1 ? "" : "s");

    // Create DS18B20 devices on the 1-Wire bus
    for (int i = 0; i < num_devices; ++i) {
        DS18B20_Info * ds18b20_info = ds18b20_malloc();  // heap allocation
        devices[i] = ds18b20_info;

        if (num_devices == 1) {
            printf("Single device optimisations enabled\n");
            ds18b20_init_solo(ds18b20_info, owb);          // only one device on bus
        } else {
            ds18b20_init(ds18b20_info, owb, device_rom_codes[i]); // associate with bus and device
        }
        ds18b20_use_crc(ds18b20_info, true);           // enable CRC check for temperature readings
        ds18b20_set_resolution(ds18b20_info, res);
    }

    tempQueue = xQueueCreate(10, sizeof(float[5]));
    flowRateQueue = xQueueCreate(10, sizeof(float));

    return ESP_OK;
}

int scanTempSensorNetwork(OneWireBus_ROMCode rom_codes[MAX_DEVICES])
{
    OneWireBus_SearchState search_state = {0};
    bool found = false;
    int n_devices = 0;
    owb_search_first(owb, &search_state, &found);
    while (found) {
        char rom_code_s[17];
        owb_string_from_rom_code(search_state.rom_code, rom_code_s, sizeof(rom_code_s));
        printf("  %d : %s\n", n_devices, rom_code_s);
        rom_codes[n_devices] = search_state.rom_code;
        ++n_devices;
        owb_search_next(owb, &search_state, &found);
    }

    return n_devices;
}

esp_err_t init_timer(void)
{
    timer_config_t config;
    config.divider = 2;
    config.counter_dir = TIMER_COUNT_UP;
    config.alarm_en = TIMER_ALARM_DIS;
    timer_init(TIMER_GROUP_0, TIMER_0, &config);

    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x00000000ULL);
    timer_start(TIMER_GROUP_0, TIMER_0);

    return ESP_OK;
}

void temp_sensor_task(void *pvParameters) 
{
    float sensorTemps[5] = {0};
    portTickType xLastWakeTime = xTaskGetTickCount();
    BaseType_t ret;

    while (1) 
    {
        readTemps(sensorTemps);
        ret = xQueueSend(tempQueue, sensorTemps, 100 / portTICK_PERIOD_MS);
        if (ret == errQUEUE_FULL) {
            ESP_LOGI(tag, "Flow rate queue full");
        }
        
        vTaskDelayUntil(&xLastWakeTime, SAMPLE_PERIOD / portTICK_PERIOD_MS);
    }
}

void flowmeter_task(void *pvParameters) 
{
    float flowRate;
    BaseType_t ret;
    portTickType xLastWakeTime = xTaskGetTickCount();
    double currTime;
    ESP_LOGI(tag, "Running flowmeter task");

    while (true) {
        timer_get_counter_time_sec(TIMER_GROUP_0, TIMER_0, &currTime);
        if (currTime > 1) {
            flowRate = 0;
        } else {
            flowRate = (float) 1 / (timeVal * 7.5);
            printf("Flowrate: %.2f\n", flowRate);
        }
        ret = xQueueSend(flowRateQueue, &flowRate, 100 / portTICK_PERIOD_MS);
        if (ret == errQUEUE_FULL) {
            ESP_LOGI(tag, "Flow rate queue full");
        }
        vTaskDelayUntil(&xLastWakeTime, 500 / portTICK_PERIOD_MS);
    }
}

void IRAM_ATTR flowmeter_ISR(void* arg)
{
    double timeTemp;
    timer_get_counter_time_sec(TIMER_GROUP_0, TIMER_0, &timeTemp);
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x00000000ULL);
    timeVal = timeTemp;
}

void readTemps(float sensorTemps[])
{
    // Read temperatures more efficiently by starting conversions on all devices at the same time
    // int errors_count[MAX_DEVICES] = {0};
    // int sample_count = 0;
    if (num_devices > 0) {
        ds18b20_convert_all(owb);

        // In this application all devices use the same resolution,
        // so use the first device to determine the delay
        ds18b20_wait_for_conversion(devices[0]);

        for (int i = 0; i < num_devices; ++i) {
            ds18b20_read_temp(devices[i], &sensorTemps[i]);
        }
    }
}

#ifdef __cplusplus
}
#endif