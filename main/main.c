#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "iot_knob.h"   
#include "iot_button.h" 

static const char *TAG = "ENCODER_OFFICIAL";

#define ENCODER_A_GPIO        4
#define ENCODER_B_GPIO        5
#define ENCODER_CLICK_GPIO    6

static int32_t last_raw_position = 0;
static int32_t offset_position = 0;

// ---- КОЛБЭК КНОПКИ ----
static void button_event_cb(void *arg, void *usr_data)
{
    button_event_t event = iot_button_get_event(arg);
    
    if (event == BUTTON_PRESS_DOWN) {
        ESP_LOGW(TAG, "Кнопка НАЖАТА!");
        offset_position = last_raw_position;
        ESP_LOGI(TAG, "Счётчик сброшен в 0");
    } else if (event == BUTTON_PRESS_UP) {
        ESP_LOGI(TAG, "Кнопка ОТПУЩЕНА!");
    }
}

// ---- КОЛБЭК ЭНКОДЕРА ----
static void knob_event_cb(void *arg, void *usr_data)
{
    knob_event_t event = iot_knob_get_event(arg);
    knob_handle_t knob_handle = (knob_handle_t)arg;
    
    last_raw_position = iot_knob_get_count_value(knob_handle);
    int32_t virtual_position = last_raw_position - offset_position;

    if (event == KNOB_LEFT) {
        ESP_LOGI(TAG, "◄ Шаг ВЛЕВО. Позиция: %ld", virtual_position);
    } else if (event == KNOB_RIGHT) {
        ESP_LOGI(TAG, "► Шаг ВПРАВО. Позиция: %ld", virtual_position);
    }
}

// ---- MAIN ----
void app_main(void)
{
    ESP_LOGI(TAG, "Старт инициализации компонентов...");

    // ----- ИНИЦИАЛИЗАЦИЯ ЭНКОДЕРА -----
    knob_config_t knob_cfg = {
        .gpio_encoder_a = ENCODER_A_GPIO,
        .gpio_encoder_b = ENCODER_B_GPIO,
    };

    knob_handle_t knob_handle = iot_knob_create(&knob_cfg);
    if (knob_handle == NULL) {
        ESP_LOGE(TAG, "Не удалось создать объект Knob!");
        return;
    }

    iot_knob_register_cb(knob_handle, KNOB_LEFT, knob_event_cb, NULL);
    iot_knob_register_cb(knob_handle, KNOB_RIGHT, knob_event_cb, NULL);

    // ----- ИНИЦИАЛИЗАЦИЯ КНОПКИ -----
    button_config_t btn_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = ENCODER_CLICK_GPIO,
            .active_level = 0,   // LOW = нажата
        },
    };

    button_handle_t btn_handle = iot_button_create(&btn_cfg);
    if (btn_handle != NULL) {
        iot_button_register_cb(btn_handle, BUTTON_PRESS_DOWN, button_event_cb, NULL);
        iot_button_register_cb(btn_handle, BUTTON_PRESS_UP, button_event_cb, NULL);
    } else {
        ESP_LOGE(TAG, "Не удалось создать объект Button!");
    }

    ESP_LOGI(TAG, "Система полностью готова!");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}