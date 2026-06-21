#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/rmt_rx.h"
#include "ir_receiver.h"

static const char *TAG = "IR_RX";

// Статические переменные для хранения состояния
static rmt_channel_handle_t s_rx_channel = NULL;
static rmt_symbol_word_t *s_rx_buffer = NULL;
static ir_rx_callback_t s_callback = NULL;
static void *s_user_data = NULL;
static QueueHandle_t s_packet_queue = NULL;

// Структура, передаваемая в очередь (указатель на пакет)
typedef struct {
    ir_packet_t *packet;
} ir_queue_item_t;

// ---- КОЛБЭК ПРЕРЫВАНИЯ ----
static bool IRAM_ATTR rx_done_callback(rmt_channel_handle_t rx_chan, 
                                       const rmt_rx_done_event_data_t *edata, 
                                       void *user_data)
{
    BaseType_t high_task_wakeup = pdFALSE;

    // Выделяем память для пакета
    ir_packet_t *packet = malloc(sizeof(ir_packet_t));
    if (packet == NULL) {
        return false;
    }

    packet->count = (edata->num_symbols < IR_MAX_SYMBOLS) ? 
                    edata->num_symbols : IR_MAX_SYMBOLS;
    memcpy(packet->symbols, edata->received_symbols, 
           packet->count * sizeof(rmt_symbol_word_t));

    // Отправляем указатель в очередь
    ir_queue_item_t item = { .packet = packet };
    xQueueSendFromISR(s_packet_queue, &item, &high_task_wakeup);

    return high_task_wakeup == pdTRUE;
}

// ---- ЗАДАЧА ОБРАБОТЧИКА ПАКЕТОВ ----
static void packet_processor_task(void *arg)
{
    ir_queue_item_t item;

    while (1) {
        if (xQueueReceive(s_packet_queue, &item, portMAX_DELAY)) {
            // Вызываем пользовательский callback
            if (s_callback != NULL) {
                s_callback(item.packet, s_user_data);
            }
            
            // Освобождаем память, выделенную в колбэке прерывания
            free(item.packet);

            // Перезапускаем приёмник
            ir_receiver_restart();
        }
    }
}

// ---- ПУБЛИЧНЫЕ ФУНКЦИИ ----

esp_err_t ir_receiver_init(int gpio_num, ir_rx_callback_t callback, void *user_data)
{
    if (s_rx_channel != NULL) {
        ESP_LOGW(TAG, "Receiver already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    s_callback = callback;
    s_user_data = user_data;

    // Создаём очередь для пакетов
    s_packet_queue = xQueueCreate(10, sizeof(ir_queue_item_t));
    if (s_packet_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create queue");
        return ESP_ERR_NO_MEM;
    }

    // Настройка канала приёма
    rmt_rx_channel_config_t rx_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = gpio_num,
        .mem_block_symbols = 64,
        .resolution_hz = 1000000,    // 1 мкс
        .flags.invert_in = false,
    };

    ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_cfg, &s_rx_channel));

    // Регистрация колбэка
    rmt_rx_event_callbacks_t cbs = {
        .on_recv_done = rx_done_callback,
    };
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(s_rx_channel, &cbs, NULL));

    // Включаем канал
    ESP_ERROR_CHECK(rmt_enable(s_rx_channel));

    // Выделяем буфер для приёма
    s_rx_buffer = malloc(sizeof(rmt_symbol_word_t) * IR_MAX_SYMBOLS);
    if (s_rx_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate RX buffer");
        rmt_disable(s_rx_channel);
        rmt_del_channel(s_rx_channel);
        s_rx_channel = NULL;
        return ESP_ERR_NO_MEM;
    }

    // Создаём задачу для обработки пакетов
    xTaskCreate(packet_processor_task, "ir_rx_proc", 4096, NULL, 10, NULL);

    // Запускаем приём
    ir_receiver_restart();

    ESP_LOGI(TAG, "IR Receiver initialized on GPIO %d", gpio_num);
    return ESP_OK;
}

esp_err_t ir_receiver_deinit(void)
{
    if (s_rx_channel == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    rmt_disable(s_rx_channel);
    rmt_del_channel(s_rx_channel);
    s_rx_channel = NULL;

    if (s_rx_buffer != NULL) {
        free(s_rx_buffer);
        s_rx_buffer = NULL;
    }

    if (s_packet_queue != NULL) {
        vQueueDelete(s_packet_queue);
        s_packet_queue = NULL;
    }

    s_callback = NULL;
    s_user_data = NULL;

    ESP_LOGI(TAG, "IR Receiver deinitialized");
    return ESP_OK;
}

esp_err_t ir_receiver_restart(void)
{
    if (s_rx_channel == NULL || s_rx_buffer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    rmt_receive_config_t rx_cfg = {
        .signal_range_min_ns = 0,
        .signal_range_max_ns = 30000 * 1000,   // 30 мс
    };

    return rmt_receive(s_rx_channel, s_rx_buffer, 
                      sizeof(rmt_symbol_word_t) * IR_MAX_SYMBOLS, 
                      &rx_cfg);
}