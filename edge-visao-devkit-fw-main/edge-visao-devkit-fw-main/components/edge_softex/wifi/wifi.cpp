//
// Created by vnrju on 10/10/2022.
//

#include <edge_softex/edge_softex.h>
#include <edge_softex/wifi.h>

#include <esp_wifi.h>
#include <nvs_flash.h>
#include <cstring>
#include <esp_log.h>
#include <mdns.h>

#define SOFTEX_WIFI_MAXIMUM_RETRY 0
#define TAG "softex_wifi"

static int retry_num = 0;

/**
 * @brief Callback que cuida dos eventos de wifi.
 *
 * @param ctx Dado de usuário
 * @param event Evento ocorrido
 *
 * @return
 *      - 0 caso o evento seja tratado com sucesso
 *      - -EINVST caso o WiFi não tenha sido inicializado ou iniciado
 *      - -EINTERNAL caso ocorra algum erro interno
 *      - -EINVAL caso o SSID da rede seja inválido
 *      - -ENETUNREACH caso não consiga conectar à rede
 */
static esp_err_t event_handler(void *ctx, system_event_t *event) {
    esp_err_t err;

    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            err = esp_wifi_connect();

            switch (err) {
                case ESP_ERR_WIFI_NOT_INIT:
                    ESP_LOGE(TAG, "WiFi is not initialized by esp_wifi_init");
                    return -EINVST;
                case ESP_ERR_WIFI_NOT_STARTED:
                    ESP_LOGE(TAG, "WiFi is not started by esp_wifi_start");
                    return -EINVST;
                case ESP_ERR_WIFI_CONN:
                    ESP_LOGE(TAG, "WiFi internal error, station or soft-AP control block wrong");
                    return -EINTERNAL;
                case ESP_ERR_WIFI_SSID:
                    ESP_LOGE(TAG, "SSID of AP which station connects is invalid");
                    return -EINVAL;
                case ESP_OK:
                    break;
            }

            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "got ip:%s", ip4addr_ntoa((const ip4_addr_t *) (&event->event_info.got_ip.ip_info.ip)));
            retry_num = 0;
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED: {
            if (retry_num < SOFTEX_WIFI_MAXIMUM_RETRY) {
                err = esp_wifi_connect();

                switch (err) {
                    case ESP_ERR_WIFI_NOT_INIT:
                        ESP_LOGE(TAG, "WiFi is not initialized by esp_wifi_init");
                        return -EINVST;
                    case ESP_ERR_WIFI_NOT_STARTED:
                        ESP_LOGE(TAG, "WiFi is not started by esp_wifi_start");
                        return -EINVST;
                    case ESP_ERR_WIFI_CONN:
                        ESP_LOGE(TAG, "WiFi internal error, station or soft-AP control block wrong");
                        return -EINTERNAL;
                    case ESP_ERR_WIFI_SSID:
                        ESP_LOGE(TAG, "SSID of AP which station connects is invalid");
                        return -EINVAL;
                    case ESP_OK:
                        break;
                }

                retry_num++;
                ESP_LOGI(TAG, "Retry to connect to the AP");
            }
            else {
                ESP_LOGE(TAG, "Connect to the AP fail. Maximum retry reached");
                return -ENETUNREACH;
            }
            break;
        }
        default:
            break;
    }

    mdns_handle_system_event(ctx, event);

    return 0;
}


int wifi_setup(const char *ssid, const char *password) {
    esp_err_t err;
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    wifi_mode_t mode = WIFI_MODE_STA;
    wifi_config_t wifi_config;

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    ESP_ERROR_CHECK(err);

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(mode));

    memset(&wifi_config, 0, sizeof(wifi_config_t));
    snprintf((char *) wifi_config.sta.ssid, 32, "%s", ssid);
    snprintf((char *) wifi_config.sta.password, 64, "%s", password);

    ESP_ERROR_CHECK(esp_wifi_set_config((wifi_interface_t) ESP_IF_WIFI_STA, &wifi_config));
    ESP_LOGI(TAG, "Connected to %s", ssid);

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    return 0;
}
