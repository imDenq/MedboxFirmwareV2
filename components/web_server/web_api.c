/**
 * @file web_api.c
 * @brief REST API handlers pour le serveur web Medbox
 */

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "wifi_manager.h"
#include "error_manager.h"
#include "sdkconfig.h"
#include <string.h>
#include <time.h>

static const char *TAG = "web_api";

// Version firmware
#define FIRMWARE_VERSION "1.0.0"

// ============================================
// GET /api/status - Infos systeme
// ============================================
static esp_err_t api_status_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Infos firmware
    cJSON_AddStringToObject(root, "firmware_version", FIRMWARE_VERSION);
    cJSON_AddStringToObject(root, "idf_version", esp_get_idf_version());

    // UUID (depuis config ou variable globale)
#ifdef CONFIG_MEDBOX_DEV_UUID
    cJSON_AddStringToObject(root, "uuid", CONFIG_MEDBOX_DEV_UUID);
#else
    cJSON_AddStringToObject(root, "uuid", "unknown");
#endif

    // Uptime
    uint32_t uptime_sec = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    cJSON_AddNumberToObject(root, "uptime_sec", uptime_sec);

    // Memoire libre
    cJSON_AddNumberToObject(root, "free_heap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(root, "min_free_heap", esp_get_minimum_free_heap_size());

    // Heure systeme (reglee via la commande MQTT sync_time)
    time_t now_ts = time(NULL);
    cJSON *time_obj = cJSON_CreateObject();
    bool time_synced = now_ts > 1600000000;  // > sept 2020 => horloge reglee
    cJSON_AddBoolToObject(time_obj, "synced", time_synced);
    if (time_synced) {
        struct tm ti;
        gmtime_r(&now_ts, &ti);
        char timebuf[24];
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &ti);
        cJSON_AddStringToObject(time_obj, "utc", timebuf);
    }
    cJSON_AddItemToObject(root, "time", time_obj);

    // Etat WiFi STA
    cJSON *wifi_sta = cJSON_CreateObject();
    cJSON_AddBoolToObject(wifi_sta, "connected", wifi_manager_is_connected());
    if (wifi_manager_is_connected()) {
        char ip_str[16];
        if (wifi_manager_get_ip(ip_str) == ESP_OK) {
            cJSON_AddStringToObject(wifi_sta, "ip", ip_str);
        }
        cJSON_AddNumberToObject(wifi_sta, "rssi", wifi_manager_get_rssi());
    }
    cJSON_AddItemToObject(root, "wifi_sta", wifi_sta);

    // Etat WiFi AP
    cJSON *wifi_ap = cJSON_CreateObject();
    cJSON_AddBoolToObject(wifi_ap, "active", wifi_manager_is_ap_active());
    if (wifi_manager_is_ap_active()) {
        char ssid[32];
        if (wifi_manager_get_ap_ssid(ssid) == ESP_OK) {
            cJSON_AddStringToObject(wifi_ap, "ssid", ssid);
        }
        char ip_str[16];
        if (wifi_manager_get_ap_ip(ip_str) == ESP_OK) {
            cJSON_AddStringToObject(wifi_ap, "ip", ip_str);
        }
    }
    cJSON_AddItemToObject(root, "wifi_ap", wifi_ap);

    // Envoyer la reponse
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);

    return ESP_OK;
}

// ============================================
// GET /api/wifi/scan - Scanner les reseaux
// ============================================
static esp_err_t api_wifi_scan_handler(httpd_req_t *req)
{
    wifi_scan_result_t results[20];
    uint16_t num_results = 0;

    esp_err_t ret = wifi_manager_scan(results, 20, &num_results);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Scan failed");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *networks = cJSON_CreateArray();

    for (uint16_t i = 0; i < num_results; i++) {
        cJSON *network = cJSON_CreateObject();
        cJSON_AddStringToObject(network, "ssid", results[i].ssid);
        cJSON_AddNumberToObject(network, "rssi", results[i].rssi);

        const char *auth_str;
        switch (results[i].authmode) {
            case WIFI_AUTH_OPEN: auth_str = "OPEN"; break;
            case WIFI_AUTH_WEP: auth_str = "WEP"; break;
            case WIFI_AUTH_WPA_PSK: auth_str = "WPA"; break;
            case WIFI_AUTH_WPA2_PSK: auth_str = "WPA2"; break;
            case WIFI_AUTH_WPA_WPA2_PSK: auth_str = "WPA/WPA2"; break;
            case WIFI_AUTH_WPA3_PSK: auth_str = "WPA3"; break;
            default: auth_str = "UNKNOWN"; break;
        }
        cJSON_AddStringToObject(network, "security", auth_str);

        cJSON_AddItemToArray(networks, network);
    }

    cJSON_AddItemToObject(root, "networks", networks);
    cJSON_AddNumberToObject(root, "count", num_results);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);

    return ESP_OK;
}

// ============================================
// GET /api/wifi/config - Config WiFi actuelle
// ============================================
static esp_err_t api_wifi_config_get_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();

    wifi_config_t wifi_config;
    if (esp_wifi_get_config(WIFI_IF_STA, &wifi_config) == ESP_OK) {
        cJSON_AddStringToObject(root, "ssid", (char *)wifi_config.sta.ssid);
        // Ne pas renvoyer le mot de passe pour la securite
        cJSON_AddBoolToObject(root, "has_password", strlen((char *)wifi_config.sta.password) > 0);
    }

    cJSON_AddBoolToObject(root, "connected", wifi_manager_is_connected());

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);

    return ESP_OK;
}

// ============================================
// POST /api/wifi/config - Configurer WiFi
// ============================================
static esp_err_t api_wifi_config_post_handler(httpd_req_t *req)
{
    char content[256];
    int received = httpd_req_recv(req, content, sizeof(content) - 1);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    content[received] = '\0';

    ESP_LOGI(TAG, "Config WiFi recue: %s", content);

    cJSON *root = cJSON_Parse(content);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *ssid_json = cJSON_GetObjectItem(root, "ssid");
    cJSON *password_json = cJSON_GetObjectItem(root, "password");

    if (!cJSON_IsString(ssid_json) || strlen(ssid_json->valuestring) == 0) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID required");
        return ESP_FAIL;
    }

    const char *ssid = ssid_json->valuestring;
    const char *password = cJSON_IsString(password_json) ? password_json->valuestring : "";

    ESP_LOGI(TAG, "Configuration WiFi: SSID=%s", ssid);

    // Deconnecter et reconnecter avec nouveaux parametres
    wifi_manager_disconnect();

    // Configurer et connecter
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (strlen(password) > 0) {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }

    esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    cJSON_Delete(root);

    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Config failed");
        return ESP_FAIL;
    }

    // Reconnecter
    esp_wifi_connect();

    // Reponse succes
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "message", "Configuration saved, connecting...");

    char *json_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);

    return ESP_OK;
}

// ============================================
// GET /api/errors - Liste des erreurs
// ============================================
static esp_err_t api_errors_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *errors_array = cJSON_CreateArray();

    // Recuperer l'historique des erreurs
    medbox_error_history_entry_t entries[ERROR_HISTORY_SIZE];
    uint16_t num_entries = 0;

    if (error_manager_get_history(entries, ERROR_HISTORY_SIZE, &num_entries) == MEDBOX_OK) {
        for (uint16_t i = 0; i < num_entries; i++) {
            cJSON *error_obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(error_obj, "code", entries[i].code);
            cJSON_AddStringToObject(error_obj, "message", medbox_err_to_str(entries[i].code));
            cJSON_AddNumberToObject(error_obj, "timestamp", entries[i].timestamp);
            if (entries[i].context[0] != '\0') {
                cJSON_AddStringToObject(error_obj, "context", entries[i].context);
            }
            cJSON_AddItemToArray(errors_array, error_obj);
        }
    }

    cJSON_AddItemToObject(root, "errors", errors_array);
    cJSON_AddNumberToObject(root, "count", num_entries);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);

    return ESP_OK;
}

// ============================================
// Enregistrement des handlers API
// ============================================
esp_err_t web_api_register_handlers(httpd_handle_t server)
{
    httpd_uri_t uri_status = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = api_status_handler,
    };

    httpd_uri_t uri_wifi_scan = {
        .uri = "/api/wifi/scan",
        .method = HTTP_GET,
        .handler = api_wifi_scan_handler,
    };

    httpd_uri_t uri_wifi_config_get = {
        .uri = "/api/wifi/config",
        .method = HTTP_GET,
        .handler = api_wifi_config_get_handler,
    };

    httpd_uri_t uri_wifi_config_post = {
        .uri = "/api/wifi/config",
        .method = HTTP_POST,
        .handler = api_wifi_config_post_handler,
    };

    httpd_uri_t uri_errors = {
        .uri = "/api/errors",
        .method = HTTP_GET,
        .handler = api_errors_handler,
    };

    httpd_register_uri_handler(server, &uri_status);
    httpd_register_uri_handler(server, &uri_wifi_scan);
    httpd_register_uri_handler(server, &uri_wifi_config_get);
    httpd_register_uri_handler(server, &uri_wifi_config_post);
    httpd_register_uri_handler(server, &uri_errors);

    ESP_LOGI(TAG, "API endpoints enregistres");
    return ESP_OK;
}
