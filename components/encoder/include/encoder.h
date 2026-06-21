#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Колбэк, вызываемый при изменении положения энкодера
// direction: 1 = по часовой стрелке, -1 = против
// count: текущее значение счётчика
typedef void (*encoder_callback_t)(int direction, int count, void *user_data);

// Колбэк, вызываемый при нажатии кнопки энкодера
typedef void (*encoder_button_callback_t)(void *user_data);

// Инициализация энкодера
// clk_pin     - пин CLK (обычно DT)
// dt_pin      - пин DT (обычно CLK)
// sw_pin      - пин кнопки (SW)
// callback    - функция для обработки вращения
// btn_callback - функция для обработки нажатия кнопки
// user_data   - пользовательские данные
esp_err_t encoder_init(int clk_pin, int dt_pin, int sw_pin,
                       encoder_callback_t callback,
                       encoder_button_callback_t btn_callback,
                       void *user_data);

// Получить текущее значение счётчика
int encoder_get_count(void);

// Сбросить счётчик
void encoder_reset_count(void);

#ifdef __cplusplus
}
#endif

#endif // ENCODER_H