/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_sleep.h"

// #define DEBUG

// ===== CONFIGURABLE SETTINGS =====
#define MOTOR_DRIVER_PIN         GPIO_NUM_14        // GPIO pin for motor driver control
#define LIGHT_PIN                GPIO_NUM_2        // GPIO pin for built-in LED light
#ifdef DEBUG
#define WATERING_INTERVAL_HOURS  0.1                 // Hours between watering cycles
#define WATERING_DURATION_MIN    1                // Minutes to keep water flowing
#else
#define WATERING_INTERVAL_HOURS  8  
#define WATERING_DURATION_MIN    10                // Minutes to keep water flowing
#endif
#define WATERING_DURATION_MS     (WATERING_DURATION_MIN * 60 * 1000)  // Convert to milliseconds
#define WATERING_INTERVAL_MS     (WATERING_INTERVAL_HOURS * 60 * 60 * 1000)  // Convert to milliseconds

// ===== SYSTEM CONFIGURATION =====
#define TAG "IRRIGATION_SYSTEM"
#define STACK_SIZE               4096
#define PRIORITY                 5
#define TIMER_PERIOD_MS          1000              // Check every second instead of using very long timers

// ===== GLOBAL VARIABLES =====
static TimerHandle_t watering_timer;
static TimerHandle_t check_timer;
static bool is_watering = false;
static uint32_t seconds_since_last_watering = 0;

// ===== FUNCTION DECLARATIONS =====
static void start_watering(void);
static void stop_watering(void);
static void watering_timer_callback(TimerHandle_t xTimer);
static void check_timer_callback(TimerHandle_t xTimer);
static void irrigation_task(void* pvParameters);

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Irrigation System...");
    ESP_LOGI(TAG, "Configuration:");
    ESP_LOGI(TAG, "  Motor Driver Pin: GPIO %d", MOTOR_DRIVER_PIN);
    ESP_LOGI(TAG, "  Built-in Light Pin: GPIO %d", LIGHT_PIN);
    ESP_LOGI(TAG, "  Watering Interval: %d hours", WATERING_INTERVAL_HOURS);
    ESP_LOGI(TAG, "  Watering Duration: %d minutes", WATERING_DURATION_MIN);
    
    // Configure GPIO pins for motor driver and light
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << MOTOR_DRIVER_PIN) | (1ULL << LIGHT_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    // Initialize motor driver pin to OFF state and light to OFF state
    gpio_set_level(MOTOR_DRIVER_PIN, 0);
    gpio_set_level(LIGHT_PIN, 0);
    ESP_LOGI(TAG, "Motor driver pin initialized to OFF state");
    ESP_LOGI(TAG, "Built-in light initialized to OFF state");
    
    // Create timers
    watering_timer = xTimerCreate("watering_timer", 
                                 pdMS_TO_TICKS(WATERING_DURATION_MS),
                                 pdFALSE,  // One-shot timer
                                 NULL, 
                                 watering_timer_callback);
    
    check_timer = xTimerCreate("check_timer",
                              pdMS_TO_TICKS(TIMER_PERIOD_MS),
                              pdTRUE,   // Auto-reload timer
                              NULL,
                              check_timer_callback);
    
    if (watering_timer == NULL || check_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create timers");
        return;
    }
    
    // Create irrigation task
    xTaskCreate(irrigation_task, "irrigation_task", STACK_SIZE, NULL, PRIORITY, NULL);
    
    ESP_LOGI(TAG, "Irrigation system initialized successfully");
}

static void irrigation_task(void* pvParameters)
{
    ESP_LOGI(TAG, "Irrigation task started");
    
    // Start the first watering cycle immediately
    start_watering();
    
    // Start the check timer
    xTimerStart(check_timer, 0);
    
    // Task main loop - just keep the task alive
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Sleep for 1 second
        
        // Log system status every hour
        static uint32_t hour_counter = 0;
        hour_counter++;
        if (hour_counter >= 3600) { // 3600 seconds = 1 hour
            hour_counter = 0;
            uint32_t hours_until_next = (WATERING_INTERVAL_MS - seconds_since_last_watering * 1000) / (60 * 60 * 1000);
            ESP_LOGI(TAG, "System running - Next watering in %d hours", 
                     is_watering ? WATERING_DURATION_MIN : hours_until_next);
        }
    }
}

static void start_watering(void)
{
    if (is_watering) {
        ESP_LOGW(TAG, "Watering already in progress, ignoring start request");
        return;
    }
    
    ESP_LOGI(TAG, "Starting watering cycle - Duration: %d minutes", WATERING_DURATION_MIN);
    
    // Turn on motor driver and light
    gpio_set_level(MOTOR_DRIVER_PIN, 1);
    gpio_set_level(LIGHT_PIN, 1);
    is_watering = true;
    
    // Start timer to stop watering
    xTimerStart(watering_timer, 0);
}

static void stop_watering(void)
{
    if (!is_watering) {
        ESP_LOGW(TAG, "No watering in progress, ignoring stop request");
        return;
    }
    
    ESP_LOGI(TAG, "Stopping watering cycle");
    
    // Turn off motor driver and light
    gpio_set_level(MOTOR_DRIVER_PIN, 0);
    gpio_set_level(LIGHT_PIN, 0);
    is_watering = false;
    
    // Reset the counter for next watering cycle
    seconds_since_last_watering = 0;
}

static void watering_timer_callback(TimerHandle_t xTimer)
{
    stop_watering();
    ESP_LOGI(TAG, "Watering cycle completed");
}

static void check_timer_callback(TimerHandle_t xTimer)
{
    if (!is_watering) {
        seconds_since_last_watering++;
        
        // Check if it's time for the next watering cycle
        if (seconds_since_last_watering * 1000 >= WATERING_INTERVAL_MS) {
            ESP_LOGI(TAG, "Interval reached - Starting new watering cycle");
            start_watering();
        }
    }
}
