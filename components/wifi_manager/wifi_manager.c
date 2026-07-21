#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "error_manager.h"
#include <string.h>

static const char *TAG = "wifi_manager";

static EventGroupHandle_t s_wifi_event_group = NULL;
static esp_netif_t *s_sta_netif = NULL;
static esp_netif_t *s_ap_netif = NULL;
static int s_retry_num = 0;
static wifi_state_t s_wifi_state = WIFI_STATE_DISCONNECTED;
static wifi_event_callback_t s_user_callback = NULL;
static bool s_initialized = false;
static bool s_ap_active = false;
// esp_wifi_start() ne doit etre appele qu'une fois : la connexion STA comme le
// demarrage de l'AP peuvent en avoir besoin en premier (ex: aucun reseau
// enregistre -> on demarre directement en point d'acces sans passer par connect).
static bool s_wifi_started = false;
static char s_ap_ssid[32] = {0};

static void notify_state_change(wifi_state_t new_state)
{
    s_wifi_state = new_state;
    if (s_user_callback != NULL) {
        s_user_callback(new_state);
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                notify_state_change(WIFI_STATE_CONNECTING);
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                // Reconnexion infinie avec delai
                s_retry_num++;
                if (s_retry_num % WIFI_RETRY_LOG_INTERVAL == 1) {
                    ESP_LOGW(TAG, "WiFi deconnecte - tentative #%d en cours...", s_retry_num);
                }
                vTaskDelay(pdMS_TO_TICKS(WIFI_RETRY_DELAY_MS));
                esp_wifi_connect();
                notify_state_change(WIFI_STATE_CONNECTING);
                break;

            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "AP demarre: %s", s_ap_ssid);
                s_ap_active = true;
                break;

            case WIFI_EVENT_AP_STOP:
                ESP_LOGI(TAG, "AP arrete");
                s_ap_active = false;
                break;

            case WIFI_EVENT_AP_STACONNECTED:
                ESP_LOGI(TAG, "Client connecte au point d'acces");
                break;

            case WIFI_EVENT_AP_STADISCONNECTED:
                ESP_LOGI(TAG, "Client deconnecte du point d'acces");
                break;

            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        if (s_retry_num > 0) {
            ESP_LOGI(TAG, "Reconnecte apres %d tentatives! IP: " IPSTR, s_retry_num, IP2STR(&event->ip_info.ip));
        } else {
            ESP_LOGI(TAG, "Connecte! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        }
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);
        notify_state_change(WIFI_STATE_CONNECTED);
    }
}

esp_err_t wifi_manager_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "WiFi Manager déjà initialisé");
        return ESP_OK;
    }

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        medbox_log_error(MEDBOX_ERR_SYS_NO_MEMORY, TAG, "event_group");
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    s_initialized = true;
    ESP_LOGI(TAG, "WiFi Manager initialisé");
    return ESP_OK;
}

esp_err_t wifi_manager_connect(const char *ssid, const char *password)
{
    if (!s_initialized) {
        medbox_log_error(MEDBOX_ERR_WIFI_NOT_INIT, TAG, NULL);
        return ESP_ERR_INVALID_STATE;
    }

    if (ssid == NULL || strlen(ssid) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // Reset des bits d'événements
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    s_retry_num = 0;

    wifi_config_t wifi_config = {0};

    // Copier SSID et password
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password != NULL && strlen(password) > 0) {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
        // Auto-detect auth mode (plus compatible)
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    } else {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    }

    // Désactiver PMF pour meilleure compatibilité
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_LOGI(TAG, "WiFi config - SSID: %s, Password length: %d", ssid, password ? strlen(password) : 0);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    if (!s_wifi_started) {
        ESP_ERROR_CHECK(esp_wifi_start());
        s_wifi_started = true;
    } else {
        // Driver deja demarre (ex: AP lance avant) -> il suffit de (re)connecter.
        esp_wifi_connect();
    }

    ESP_LOGI(TAG, "Connexion au WiFi SSID: %s", ssid);
    return ESP_OK;
}

esp_err_t wifi_manager_disconnect(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_wifi_disconnect();
    if (ret == ESP_OK) {
        notify_state_change(WIFI_STATE_DISCONNECTED);
    }
    return ret;
}

esp_err_t wifi_manager_wait_connected(uint32_t timeout_ms)
{
    if (!s_initialized || s_wifi_event_group == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(timeout_ms));

    if (bits & WIFI_CONNECTED_BIT) {
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        return ESP_FAIL;
    }

    return ESP_ERR_TIMEOUT;
}

bool wifi_manager_is_connected(void)
{
    return s_wifi_state == WIFI_STATE_CONNECTED;
}

wifi_state_t wifi_manager_get_state(void)
{
    return s_wifi_state;
}

void wifi_manager_set_callback(wifi_event_callback_t callback)
{
    s_user_callback = callback;
}

esp_err_t wifi_manager_get_ip(char *ip_str)
{
    if (ip_str == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!wifi_manager_is_connected()) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_netif_ip_info_t ip_info;
    esp_err_t ret = esp_netif_get_ip_info(s_sta_netif, &ip_info);
    if (ret != ESP_OK) {
        return ret;
    }

    sprintf(ip_str, IPSTR, IP2STR(&ip_info.ip));
    return ESP_OK;
}

int8_t wifi_manager_get_rssi(void)
{
    if (!wifi_manager_is_connected()) {
        return 0;
    }

    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        return ap_info.rssi;
    }
    return 0;
}

// ============================================
// Fonctions Access Point (AP)
// ============================================

esp_err_t wifi_manager_start_ap(const char *uuid_suffix)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_ap_active) {
        ESP_LOGW(TAG, "AP deja actif");
        return ESP_OK;
    }

    // Creer netif AP si pas encore fait
    if (s_ap_netif == NULL) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
        if (s_ap_netif == NULL) {
            medbox_log_error(MEDBOX_ERR_SYS_NO_MEMORY, TAG, "ap_netif");
            return ESP_ERR_NO_MEM;
        }
    }

    // Passer en mode AP+STA
    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur set_mode APSTA: %s", esp_err_to_name(ret));
        return ret;
    }

    // Construire le SSID
    if (uuid_suffix != NULL && strlen(uuid_suffix) >= 4) {
        snprintf(s_ap_ssid, sizeof(s_ap_ssid), "%s%.4s", WIFI_AP_SSID_PREFIX, uuid_suffix);
    } else {
        snprintf(s_ap_ssid, sizeof(s_ap_ssid), "%sXXXX", WIFI_AP_SSID_PREFIX);
    }

    // Configuration AP
    wifi_config_t ap_config = {
        .ap = {
            .channel = WIFI_AP_CHANNEL,
            .max_connection = WIFI_AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };
    strncpy((char *)ap_config.ap.ssid, s_ap_ssid, sizeof(ap_config.ap.ssid) - 1);
    strncpy((char *)ap_config.ap.password, WIFI_AP_PASSWORD, sizeof(ap_config.ap.password) - 1);
    ap_config.ap.ssid_len = strlen(s_ap_ssid);

    ret = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur config AP: %s", esp_err_to_name(ret));
        return ret;
    }

    // Sans esp_wifi_start(), l'AP est configure mais n'emet aucune balise : le
    // SSID reste invisible. Cas atteint quand aucun reseau n'est enregistre et
    // qu'on n'est donc jamais passe par wifi_manager_connect().
    if (!s_wifi_started) {
        ret = esp_wifi_start();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Erreur demarrage WiFi: %s", esp_err_to_name(ret));
            return ret;
        }
        s_wifi_started = true;
    }

    ESP_LOGI(TAG, "AP configure: SSID=%s, Password=%s", s_ap_ssid, WIFI_AP_PASSWORD);
    return ESP_OK;
}

esp_err_t wifi_manager_stop_ap(void)
{
    if (!s_initialized || !s_ap_active) {
        return ESP_ERR_INVALID_STATE;
    }

    // Repasser en mode STA uniquement
    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        return ret;
    }

    s_ap_active = false;
    ESP_LOGI(TAG, "AP arrete");
    return ESP_OK;
}

bool wifi_manager_is_ap_active(void)
{
    return s_ap_active;
}

esp_err_t wifi_manager_get_ap_ip(char *ip_str)
{
    if (ip_str == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_ap_active || s_ap_netif == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_netif_ip_info_t ip_info;
    esp_err_t ret = esp_netif_get_ip_info(s_ap_netif, &ip_info);
    if (ret != ESP_OK) {
        return ret;
    }

    sprintf(ip_str, IPSTR, IP2STR(&ip_info.ip));
    return ESP_OK;
}

esp_err_t wifi_manager_get_ap_ssid(char *ssid_str)
{
    if (ssid_str == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (strlen(s_ap_ssid) == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    strcpy(ssid_str, s_ap_ssid);
    return ESP_OK;
}

// ============================================
// Fonctions Scan WiFi
// ============================================

esp_err_t wifi_manager_scan(wifi_scan_result_t *results, uint16_t max_results, uint16_t *num_results)
{
    if (!s_initialized || results == NULL || num_results == NULL || max_results == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // Configurer et lancer le scan
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = {
                .min = 100,
                .max = 300,
            },
        },
    };

    ESP_LOGI(TAG, "Scan WiFi en cours...");
    esp_err_t ret = esp_wifi_scan_start(&scan_config, true);  // Bloquant
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur scan: %s", esp_err_to_name(ret));
        return ret;
    }

    // Recuperer les resultats
    uint16_t ap_count = 0;
    ret = esp_wifi_scan_get_ap_num(&ap_count);
    if (ret != ESP_OK) {
        return ret;
    }

    if (ap_count == 0) {
        *num_results = 0;
        ESP_LOGI(TAG, "Aucun reseau trouve");
        return ESP_OK;
    }

    // Limiter au max demande
    uint16_t to_fetch = (ap_count < max_results) ? ap_count : max_results;

    wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * to_fetch);
    if (ap_records == NULL) {
        esp_wifi_scan_get_ap_records(&to_fetch, NULL);  // Liberer les resultats internes
        return ESP_ERR_NO_MEM;
    }

    ret = esp_wifi_scan_get_ap_records(&to_fetch, ap_records);
    if (ret != ESP_OK) {
        free(ap_records);
        return ret;
    }

    // Copier vers notre structure
    for (uint16_t i = 0; i < to_fetch; i++) {
        strncpy(results[i].ssid, (char *)ap_records[i].ssid, sizeof(results[i].ssid) - 1);
        results[i].ssid[sizeof(results[i].ssid) - 1] = '\0';
        results[i].rssi = ap_records[i].rssi;
        results[i].authmode = ap_records[i].authmode;
    }

    *num_results = to_fetch;
    free(ap_records);

    ESP_LOGI(TAG, "Scan termine: %d reseaux trouves", to_fetch);
    return ESP_OK;
}
