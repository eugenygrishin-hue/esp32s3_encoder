#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "iot_knob.h"   // Компонент ручки
#include "iot_button.h" // Компонент кнопки

static const char *TAG = "ENCODER_OFFICIAL";

#define ENCODER_A_GPIO        4
#define ENCODER_B_GPIO        5
#define ENCODER_CLICK_GPIO    6

// 1. ОБРАБОТЧИК КЛИКОВ КНОПКИ
static void button_event_cb(void *arg, void *usr_data)
{
    button_event_t event = iot_button_get_event(arg);
    
    if (event == BUTTON_PRESS_DOWN) {
        ESP_LOGW(TAG, "🛑 Кнопка НАЖАТА!");
    } else if (event == BUTTON_PRESS_UP) {
        ESP_LOGE(TAG, "🟢 Кнопка ОТПУЩЕНА!");
    }
}

// 2. ОБРАБОТЧИК ВРАЩЕНИЯ ЭНКОДЕРА
static void knob_event_cb(void *arg, void *usr_data)
{
    knob_event_t event = iot_knob_get_event(arg);
    knob_handle_t knob_handle = (knob_handle_t)arg;
    
    // Исправлено под ваше API: функция называется iot_knob_get_count_value
    int32_t position = iot_knob_get_count_value(knob_handle);

    if (event == KNOB_LEFT) {
        ESP_LOGI(TAG, "◄◄◄ Шаг ВЛЕВО. Позиция: %ld", position);
    } else if (event == KNOB_RIGHT) {
        ESP_LOGI(TAG, "►►► Шаг ВПРАВО. Позиция: %ld", position);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Старт инициализации компонентов...");

	// Исправлено: точные имена полей для вашей версии библиотеки knob
	knob_config_t knob_cfg = {
	    .gpio_encoder_a = ENCODER_A_GPIO,
	    .gpio_encoder_b = ENCODER_B_GPIO,
	};


    // Создаем объект энкодера (принимает только 1 аргумент)
    knob_handle_t knob_handle = iot_knob_create(&knob_cfg);
    if (knob_handle == NULL) {
        ESP_LOGE(TAG, "Не удалось создать объект Knob!");
        return;
    }

    // Регистрируем события вращения
    iot_knob_register_cb(knob_handle, KNOB_LEFT, knob_event_cb, NULL);
    iot_knob_register_cb(knob_handle, KNOB_RIGHT, knob_event_cb, NULL);

    // Исправлено под ваше API: Кнопка создается абсолютно независимо!
    button_config_t btn_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = ENCODER_CLICK_GPIO,
            .active_level = 0, // 0 = замыкание на GND
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
