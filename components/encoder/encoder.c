#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "encoder.h"

static const char *TAG = "ENCODER";

// Состояние энкодера
typedef struct {
    int clk_pin;
    int dt_pin;
    int sw_pin;
    int last_state;
    int count;
    bool sw_pressed;
    encoder_callback_t callback;
    encoder_button_callback_t btn_callback;
    void *user_data;
} encoder_state_t;

static encoder_state_t s_encoder = {0};

// ---- ЗАДАЧА ОБРАБОТКИ ЭНКОДЕРА ----
static void encoder_task(void *arg)
{
    while (1) {
        // Читаем состояние CLK и DT
        int clk_state = gpio_get_level(s_encoder.clk_pin);
        int dt_state = gpio_get_level(s_encoder.dt_pin);
        int sw_state = gpio_get_level(s_encoder.sw_pin);

        // Обработка вращения (по изменению CLK)
        if (clk_state != s_encoder.last_state) {
            if (clk_state == 0) {  // Фронт спада
                if (dt_state != clk_state) {
                    s_encoder.count++;
                } else {
                    s_encoder.count--;
                }
                
                if (s_encoder.callback != NULL) {
                    s_encoder.callback((dt_state != clk_state) ? 1 : -1, 
                                      s_encoder.count, 
                                      s_encoder.user_data);
                }
            }
            s_encoder.last_state = clk_state;
        }

        // Обработка нажатия кнопки (антидребезг)
        if (sw_state == 0 && !s_encoder.sw_pressed) {
            s_encoder.sw_pressed = true;
            if (s_encoder.btn_callback != NULL) {
                s_encoder.btn_callback(s_encoder.user_data);
            }
        } else if (sw_state == 1 && s_encoder.sw_pressed) {
            s_encoder.sw_pressed = false;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ---- ПУБЛИЧНЫЕ ФУНКЦИИ ----

esp_err_t encoder_init(int clk_pin, int dt_pin, int sw_pin,
                       encoder_callback_t callback,
                       encoder_button_callback_t btn_callback,
                       void *user_data)
{
    // Сохраняем настройки
    s_encoder.clk_pin = clk_pin;
    s_encoder.dt_pin = dt_pin;
    s_encoder.sw_pin = sw_pin;
    s_encoder.callback = callback;
    s_encoder.btn_callback = btn_callback;
    s_encoder.user_data = user_data;
    s_encoder.count = 0;
    s_encoder.sw_pressed = false;

    // Настройка GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << clk_pin) | (1ULL << dt_pin) | (1ULL << sw_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Запоминаем начальное состояние CLK
    s_encoder.last_state = gpio_get_level(clk_pin);

    // Создаём задачу обработки
    xTaskCreate(encoder_task, "encoder_task", 2048, NULL, 10, NULL);

    ESP_LOGI(TAG, "Encoder initialized: CLK=%d, DT=%d, SW=%d", clk_pin, dt_pin, sw_pin);
    return ESP_OK;
}

int encoder_get_count(void)
{
    return s_encoder.count;
}

void encoder_reset_count(void)
{
    s_encoder.count = 0;
}