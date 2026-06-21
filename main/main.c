#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "iot_knob.h"
#include "wifi_manager.h"
#include "ssd1306.h"

static const char *TAG = "MAIN";

#define ENCODER_A_GPIO        4
#define ENCODER_B_GPIO        5
#define ENCODER_CLICK_GPIO    6

// OLED
#define OLED_I2C_PORT         I2C_NUM_0
#define OLED_I2C_ADDR         0x3C
#define OLED_WIDTH            128
#define OLED_HEIGHT           32

// ---- Wi-Fi настройки ----
#define WIFI_SSID     "Elektron-38463"    // <-- ЗАМЕНИТЕ!
#define WIFI_PASS     "138ff5395"  // <-- ЗАМЕНИТЕ!
// Размер шрифта: 8, 16 или 24
#define FONT_SIZE             16   // <-- УВЕЛИЧИЛИ С 8 ДО 16

static struct tm s_timeinfo = {0};
static bool s_time_synced = false;
static ssd1306_handle_t s_oled_handle = NULL;

// ---- КОЛБЭКИ ----
static void wifi_status_cb(bool connected, void *user_data)
{
    if (connected) {
        ESP_LOGI(TAG, "Wi-Fi connected! IP: %s", wifi_manager_get_ip());
    } else {
        ESP_LOGW(TAG, "Wi-Fi disconnected");
    }
}

static void sntp_sync_cb(struct timeval *tv)
{
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    s_timeinfo = *local;
    s_time_synced = true;
    ESP_LOGI(TAG, "Time synced: %02d:%02d:%02d", 
             local->tm_hour, local->tm_min, local->tm_sec);
}

static void init_ntp(void)
{
    ESP_LOGI(TAG, "Initializing NTP...");
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.start = true;
    config.sync_cb = sntp_sync_cb;
    esp_netif_sntp_init(&config);
}

// ---- ОТОБРАЖЕНИЕ ВРЕМЕНИ ----
static void display_time(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
    if (s_oled_handle == NULL) return;
    
    char str[16];
    snprintf(str, sizeof(str), "%02d:%02d:%02d", hours, minutes, seconds);
    
    ssd1306_clear_screen(s_oled_handle, 0x00);
    // ШРИФТ 16 (крупный)
    ssd1306_draw_string(s_oled_handle, 0, 0, (const uint8_t *)"Time:", FONT_SIZE, 0);
    ssd1306_draw_string(s_oled_handle, 0, 2, (const uint8_t *)str, FONT_SIZE, 0);
    ssd1306_refresh_gram(s_oled_handle);
}

// ---- КОЛБЭК ЭНКОДЕРА ----
static void knob_event_cb(void *arg, void *usr_data)
{
    knob_event_t event = iot_knob_get_event(arg);
    knob_handle_t knob_handle = (knob_handle_t)arg;
    int32_t position = iot_knob_get_count_value(knob_handle);

    if (event == KNOB_LEFT) {
        ESP_LOGI(TAG, "Encoder LEFT. Pos: %ld", position);
    } else if (event == KNOB_RIGHT) {
        ESP_LOGI(TAG, "Encoder RIGHT. Pos: %ld", position);
    }
}

static void IRAM_ATTR btn_isr_handler(void *arg)
{
    // Обработка кнопки в основном цикле
}

// ---- MAIN ----
void app_main(void)
{
    ESP_LOGI(TAG, "System starting...");

    // ----- 1. Wi-Fi -----
    wifi_manager_init(WIFI_SSID, WIFI_PASS, wifi_status_cb, NULL);

    int wait = 0;
    while (!wifi_manager_is_connected() && wait < 100) {
        vTaskDelay(pdMS_TO_TICKS(100));
        wait++;
    }

    if (wifi_manager_is_connected()) {
        init_ntp();
    }

    // ----- 2. I2C -----
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 8,
        .scl_io_num = 9,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    ESP_ERROR_CHECK(i2c_param_config(OLED_I2C_PORT, &i2c_config));
    ESP_ERROR_CHECK(i2c_driver_install(OLED_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0));

    // ----- 3. OLED -----
    s_oled_handle = ssd1306_create(OLED_I2C_PORT, OLED_I2C_ADDR);
    if (s_oled_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create OLED device!");
    } else {
        ESP_LOGI(TAG, "OLED created successfully");
        ssd1306_clear_screen(s_oled_handle, 0x00);
        // ШРИФТ 16
        ssd1306_draw_string(s_oled_handle, 0, 0, (const uint8_t *)"ESP32-S3", FONT_SIZE, 0);
        ssd1306_draw_string(s_oled_handle, 0, 2, (const uint8_t *)"Connecting...", FONT_SIZE, 0);
        ssd1306_refresh_gram(s_oled_handle);
    }

    // ----- 4. Энкодер -----
    knob_config_t knob_cfg = {
        .gpio_encoder_a = ENCODER_A_GPIO,
        .gpio_encoder_b = ENCODER_B_GPIO,
    };
    knob_handle_t knob_handle = iot_knob_create(&knob_cfg);
    if (knob_handle != NULL) {
        iot_knob_register_cb(knob_handle, KNOB_LEFT, knob_event_cb, NULL);
        iot_knob_register_cb(knob_handle, KNOB_RIGHT, knob_event_cb, NULL);
    }

    // ----- 5. Кнопка -----
    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << ENCODER_CLICK_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&btn_cfg);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(ENCODER_CLICK_GPIO, btn_isr_handler, NULL);

    ESP_LOGI(TAG, "System ready!");

    // ----- 6. ОСНОВНОЙ ЦИКЛ -----
    uint8_t last_seconds = 0;

    while (1) {
        // Кнопка
        if (gpio_get_level(ENCODER_CLICK_GPIO) == 0) {
            ESP_LOGW(TAG, "Button pressed!");
            if (s_time_synced) {
                ESP_LOGI(TAG, "Time: %02d:%02d:%02d", 
                         s_timeinfo.tm_hour, s_timeinfo.tm_min, s_timeinfo.tm_sec);
            }
            vTaskDelay(pdMS_TO_TICKS(200));
        }

        if (s_time_synced) {
            time_t now = time(NULL);
            struct tm *local = localtime(&now);
            s_timeinfo = *local;

            if (local->tm_sec != last_seconds) {
                last_seconds = local->tm_sec;
                display_time(local->tm_hour, local->tm_min, local->tm_sec);
            }
        } else {
            static int dot_count = 0;
            if (dot_count++ % 10 == 0) {
                ESP_LOGI(TAG, "Waiting for NTP sync...");
                if (s_oled_handle != NULL) {
                    ssd1306_clear_screen(s_oled_handle, 0x00);
                    ssd1306_draw_string(s_oled_handle, 0, 0, (const uint8_t *)"ESP32-S3", FONT_SIZE, 0);
                    ssd1306_draw_string(s_oled_handle, 0, 2, (const uint8_t *)"Connecting...", FONT_SIZE, 0);
                    ssd1306_refresh_gram(s_oled_handle);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}