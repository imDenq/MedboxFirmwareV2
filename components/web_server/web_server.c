/**
 * @file web_server.c
 * @brief Implementation du serveur web Medbox
 */

#include "web_server.h"
#include "console_ws.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "wifi_manager.h"
#include "error_manager.h"
#include <string.h>

static const char *TAG = "web_server";

static httpd_handle_t s_server = NULL;

// Declarations externes (definies dans web_pages.c et web_api.c)
extern esp_err_t web_pages_register_handlers(httpd_handle_t server);
extern esp_err_t web_api_register_handlers(httpd_handle_t server);

esp_err_t web_server_start(void)
{
    if (s_server != NULL) {
        ESP_LOGW(TAG, "Serveur deja demarre");
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = WEB_SERVER_PORT;
    config.max_uri_handlers = 16;
    config.stack_size = 8192;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Demarrage serveur HTTP sur port %d", WEB_SERVER_PORT);

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        medbox_log_error(MEDBOX_ERR_SYS_INIT_FAILED, TAG, "httpd_start");
        return ret;
    }

    // Enregistrer les handlers de pages
    ret = web_pages_register_handlers(s_server);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur enregistrement pages");
        httpd_stop(s_server);
        s_server = NULL;
        return ret;
    }

    // Enregistrer les handlers API
    ret = web_api_register_handlers(s_server);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur enregistrement API");
        httpd_stop(s_server);
        s_server = NULL;
        return ret;
    }

    // Console live via WebSocket (/console) — debug/test, non bloquant si echec
    console_ws_start();
    if (console_ws_register(s_server) != ESP_OK) {
        ESP_LOGW(TAG, "Console WebSocket non enregistree");
    }

    ESP_LOGI(TAG, "Serveur HTTP demarre avec succes");
    return ESP_OK;
}

esp_err_t web_server_stop(void)
{
    if (s_server == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = httpd_stop(s_server);
    if (ret == ESP_OK) {
        s_server = NULL;
        ESP_LOGI(TAG, "Serveur HTTP arrete");
    }
    return ret;
}

bool web_server_is_running(void)
{
    return s_server != NULL;
}
