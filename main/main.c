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
#include "esp_timer.h"

// ===== CONFIGURABLE SETTINGS =====
#define MOTOR_DRIVER_PIN         GPIO_NUM_2        // GPIO pin for motor driver control
#define WATERING_INTERVAL_HOURS  8                 // Hours between watering cycles
#define WATERING_DURATION_MIN    10                // Minutes to keep water flowing
#define WATERING_DURATION_MS     (WATERING_DURATION_MIN * 60 * 1000)  // Convert to milliseconds
#define WATERING_INTERVAL_MS     (WATERING_INTERVAL_HOURS * 60 * 60 * 1000)  // Convert to milliseconds

// ===== SYSTEM CONFIGURATION =====
#define TAG "IRRIGATION_SYSTEM"
#define STACK_SIZE               4096
#define PRIORITY                 5

// ===== GLOBAL VARIABLES =====
static esp_timer_handle_t watering_timer;
static esp_timer_handle_t interval_timer;
static bool is_watering = false;

// ===== FUNCTION DECLARATIONS =====
static void start_watering(void);
static void stop_watering(void);
static void watering_timer_callback(void* arg);
static void interval_timer_callback(void* arg);
static void irrigation_task(void* pvParameters);

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Irrigation System...");
    ESP_LOGI(TAG, "Configuration:");
    ESP_LOGI(TAG, "  Motor Driver Pin: GPIO %d", MOTOR_DRIVER_PIN);
    ESP_LOGI(TAG, "  Watering Interval: %d hours", WATERING_INTERVAL_HOURS);
    ESP_LOGI(TAG, "  Watering Duration: %d minutes", WATERING_DURATION_MIN);
    
    // Configure GPIO pin for motor driver
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << MOTOR_DRIVER_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    // Initialize motor driver pin to OFF state
    gpio_set_level(MOTOR_DRIVER_PIN, 0);
    ESP_LOGI(TAG, "Motor driver pin initialized to OFF state");
    
    // Create timers
    esp_timer_create_args_t watering_timer_args = {
        .callback = watering_timer_callback,
        .name = "watering_timer"
    };
    esp_timer_create(&watering_timer_args, &watering_timer);
    
    esp_timer_create_args_t interval_timer_args = {
        .callback = interval_timer_callback,
        .name = "interval_timer"
    };
    esp_timer_create(&interval_timer_args, &interval_timer);
    
    // Create irrigation task
    xTaskCreate(irrigation_task, "irrigation_task", STACK_SIZE, NULL, PRIORITY, NULL);
    
    ESP_LOGI(TAG, "Irrigation system initialized successfully");
}

static void irrigation_task(void* pvParameters)
{
    ESP_LOGI(TAG, "Irrigation task started");
    
    // Start the first watering cycle immediately
    start_watering();
    
    // Start the interval timer for subsequent cycles
    esp_timer_start_periodic(interval_timer, WATERING_INTERVAL_MS * 1000); // Convert to microseconds
    
    // Task main loop
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Check every second
        
        // Log system status every hour
        static uint32_t hour_counter = 0;
        hour_counter++;
        if (hour_counter >= 3600) { // 3600 seconds = 1 hour
            hour_counter = 0;
            ESP_LOGI(TAG, "System running - Next watering in %d hours", 
                     is_watering ? WATERING_DURATION_MIN : WATERING_INTERVAL_HOURS);
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
    
    // Turn on motor driver
    gpio_set_level(MOTOR_DRIVER_PIN, 1);
    is_watering = true;
    
    // Start timer to stop watering
    esp_timer_start_once(watering_timer, WATERING_DURATION_MS * 1000); // Convert to microseconds
}

static void stop_watering(void)
{
    if (!is_watering) {
        ESP_LOGW(TAG, "No watering in progress, ignoring stop request");
        return;
    }
    
    ESP_LOGI(TAG, "Stopping watering cycle");
    
    // Turn off motor driver
    gpio_set_level(MOTOR_DRIVER_PIN, 0);
    is_watering = false;
}

static void watering_timer_callback(void* arg)
{
    stop_watering();
    ESP_LOGI(TAG, "Watering cycle completed");
}

static void interval_timer_callback(void* arg)
{
    ESP_LOGI(TAG, "Interval timer triggered - Starting new watering cycle");
    start_watering();
}
