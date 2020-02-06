#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "ds18b20.h"

#define MAX_DEVICES          (8)

// Expose queue handles for passing data between tasks
extern xQueueHandle tempQueue;
extern xQueueHandle hotSideTempQueue;
extern xQueueHandle coldSideTempQueue;
extern xQueueHandle flowRateQueue;

/*
*   --------------------------------------------------------------------  
*   temp_sensor_task
*   --------------------------------------------------------------------
*   Main temperature sensor task. Handles the initialization and reading
*   of the ds18b20 temperature sensors on the onewire bus
*/
void temp_sensor_task(void *pvParameters);

/*
*   --------------------------------------------------------------------  
*   sensor_init
*   --------------------------------------------------------------------
*   Initializes sensors on the one wire bus and the queues to pass data
*   between tasks
*/
esp_err_t sensor_init(uint8_t ds_pin, DS18B20_RESOLUTION res);

int scanTempSensorNetwork(OneWireBus_ROMCode rom_codes[MAX_DEVICES]);

/*
*   --------------------------------------------------------------------  
*   flowmeter_task
*   --------------------------------------------------------------------
*   Main task responsible for reading the flowmeter
*/
void flowmeter_task(void *pvParameters);

/*
*   --------------------------------------------------------------------  
*   flowmeter_ISR
*   --------------------------------------------------------------------
*   Interrupt routine triggered by signals from the flowmeter
*/
void IRAM_ATTR flowmeter_ISR(void* arg);

/*
*   --------------------------------------------------------------------  
*   init_timer
*   --------------------------------------------------------------------
*   Initializes one of the ESP32 timers for measuring flowrate
*/
esp_err_t init_timer(void);

/*
*   --------------------------------------------------------------------  
*   readTemps
*   --------------------------------------------------------------------
*   Reads the temperatures from all connected sensors and loads them
*   into sensorTemps array
*/
void readTemps(float sensorTemps[]);

/*
*   --------------------------------------------------------------------  
*   checkPowerSupply
*   --------------------------------------------------------------------
*   Checks if DS18B20 sensors are connected in powered or parasitic
*   power mode
*/
void checkPowerSupply(void);

#ifdef __cplusplus
}
#endif