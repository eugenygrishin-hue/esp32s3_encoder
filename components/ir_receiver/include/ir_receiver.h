#ifndef IR_RECEIVER_H
#define IR_RECEIVER_H

#include <stdint.h>
#include <stddef.h>
#include "driver/rmt_rx.h"

#ifdef __cplusplus
extern "C" {
#endif

// Максимальное количество символов в пакете
#define IR_MAX_SYMBOLS 300

// Структура принятого пакета
typedef struct {
    rmt_symbol_word_t symbols[IR_MAX_SYMBOLS];
    size_t count;
} ir_packet_t;

// Колбэк, который вызывается при приёме пакета
typedef void (*ir_rx_callback_t)(ir_packet_t *packet, void *user_data);

// Инициализация приёмника
// gpio_num  - номер GPIO для приёма
// callback  - функция, вызываемая при получении пакета
// user_data - пользовательские данные, передаваемые в callback
esp_err_t ir_receiver_init(int gpio_num, ir_rx_callback_t callback, void *user_data);

// Деинициализация и освобождение ресурсов
esp_err_t ir_receiver_deinit(void);

// Принудительный перезапуск приёма (если нужно)
esp_err_t ir_receiver_restart(void);

#ifdef __cplusplus
}
#endif

#endif // IR_RECEIVER_H