#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "wifi_manager.h"

static const char *TAG = "WIFI_MGR";

// События Wi-Fi
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// Статическое состояние
static EventGroupHandle_t s_wifi_event_group = NULL;
static bool s_is_connected = false;
static char s_ip_str[16] = {0};
static wifi_status_callback_t s_callback = NULL;
static void *s_user_data = NULL;
static int s_retry_num = 0;
static const int WIFI_MAX_RETRY = 5;

// ---- ОБРАБОТЧИК СОБЫТИЙ ----
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Подключение к Wi-Fi...");
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_is_connected = false;
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "Переподключение... попытка %d/%d", s_retry_num, WIFI_MAX_RETRY);
        } else {
            ESP_LOGE(TAG, "Не удалось подключиться после %d попыток", WIFI_MAX_RETRY);
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            if (s_callback) s_callback(false, s_user_data);
        }
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        
        // ИСПРАВЛЕНО: используем esp_ip4_addr_t вместо ip4_addr_t
        esp_ip4_addr_t ip = event->ip_info.ip;
        
        // Формируем строку IP-адреса
        snprintf(s_ip_str, sizeof(s_ip_str), 
                 IPSTR, 
                 esp_ip4_addr1_16(&ip),
                 esp_ip4_addr2_16(&ip),
                 esp_ip4_addr3_16(&ip),
                 esp_ip4_addr4_16(&ip));
        
        s_is_connected = true;
        s_retry_num = 0;
        ESP_LOGI(TAG, "✅ Подключено! IP: %s", s_ip_str);
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        if (s_callback) s_callback(true, s_user_data);
    }
}

// ---- ПУБЛИЧНЫЕ ФУНКЦИИ ----

esp_err_t wifi_manager_init(const char *ssid, const char *password,
                            wifi_status_callback_t callback, void *user_data)
{
    if (ssid == NULL || password == NULL) {
        ESP_LOGE(TAG, "SSID и пароль обязательны!");
        return ESP_ERR_INVALID_ARG;
    }

    s_callback = callback;
    s_user_data = user_data;
    s_is_connected = false;
    s_retry_num = 0;
    memset(s_ip_str, 0, sizeof(s_ip_str));

    // ---- 1. ИНИЦИАЛИЗАЦИЯ NVS ----
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // ---- 2. СОЗДАЁМ ГРУППУ СОБЫТИЙ ----
    s_wifi_event_group = xEventGroupCreate();

    // ---- 3. ИНИЦИАЛИЗАЦИЯ TCP/IP И СЕТЕВОГО ИНТЕРФЕЙСА ----
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // ---- 4. ИНИЦИАЛИЗАЦИЯ WIFI ----
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Регистрируем обработчики событий
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // ---- 5. НАСТРОЙКА ПОДКЛЮЧЕНИЯ (СКРЫТАЯ СЕТЬ) ----
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .channel = 0,
        },
    };
    strcpy((char *)wifi_config.sta.ssid, ssid);
    strcpy((char *)wifi_config.sta.password, password);

    // ---- 6. ЗАПУСК WIFI ----
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi инициализирован. Подключение к '%s'...", ssid);

    return ESP_OK;
}

bool wifi_manager_is_connected(void)
{
    return s_is_connected;
}

const char* wifi_manager_get_ip(void)
{
    return s_ip_str;
}

esp_err_t wifi_manager_deinit(void)
{
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
    if (s_wifi_event_group) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }
    s_is_connected = false;
    ESP_LOGI(TAG, "Wi-Fi деинициализирован");
    return ESP_OK;
}