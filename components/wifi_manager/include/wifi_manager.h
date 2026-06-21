#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Колбэк, вызываемый при изменении статуса Wi-Fi
typedef void (*wifi_status_callback_t)(bool connected, void *user_data);

// Инициализация Wi-Fi и подключение к сети
// ssid       - имя сети (скрытая поддерживается)
// password   - пароль
// callback   - функция для уведомления о статусе (может быть NULL)
// user_data  - пользовательские данные для callback
esp_err_t wifi_manager_init(const char *ssid, const char *password, 
                            wifi_status_callback_t callback, void *user_data);

// Проверить, подключены ли мы к Wi-Fi
bool wifi_manager_is_connected(void);

// Получить IP-адрес (строковое представление)
const char* wifi_manager_get_ip(void);

// Деинициализация (отключиться и освободить ресурсы)
esp_err_t wifi_manager_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H