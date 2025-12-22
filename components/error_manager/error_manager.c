/**
 * @file error_manager.c
 * @brief Implementation du gestionnaire d'erreurs Medbox
 */

#include "error_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "error_mgr";

// ============================================
// Historique des erreurs (buffer circulaire)
// ============================================
static medbox_error_history_entry_t s_error_history[ERROR_HISTORY_SIZE];
static uint16_t s_history_head = 0;      // Prochain index d'ecriture
static uint16_t s_history_count = 0;     // Nombre d'entrees valides

// ============================================
// Table des erreurs avec messages en francais
// ============================================

static const medbox_error_info_t s_error_table[] = {
    // Succes
    {MEDBOX_OK, "Succes", "OK"},

    // Erreurs Systeme (0x01xx)
    {MEDBOX_ERR_SYS_INIT_FAILED,        "Echec d'initialisation du systeme",       "SYS"},
    {MEDBOX_ERR_SYS_NO_MEMORY,          "Memoire insuffisante",                    "SYS"},
    {MEDBOX_ERR_SYS_TASK_CREATE_FAILED, "Echec de creation de tache",              "SYS"},
    {MEDBOX_ERR_SYS_QUEUE_CREATE_FAILED,"Echec de creation de file d'attente",     "SYS"},
    {MEDBOX_ERR_SYS_QUEUE_FULL,         "File d'attente pleine",                   "SYS"},
    {MEDBOX_ERR_SYS_INVALID_ARG,        "Argument invalide",                       "SYS"},
    {MEDBOX_ERR_SYS_INVALID_STATE,      "Etat invalide",                           "SYS"},
    {MEDBOX_ERR_SYS_TIMEOUT,            "Delai d'attente depasse",                 "SYS"},

    // Erreurs WiFi (0x02xx)
    {MEDBOX_ERR_WIFI_INIT_FAILED,       "Echec d'initialisation WiFi",             "WIFI"},
    {MEDBOX_ERR_WIFI_ALREADY_INIT,      "WiFi deja initialise",                    "WIFI"},
    {MEDBOX_ERR_WIFI_NOT_INIT,          "WiFi non initialise",                     "WIFI"},
    {MEDBOX_ERR_WIFI_CONNECT_FAILED,    "Echec de connexion WiFi",                 "WIFI"},
    {MEDBOX_ERR_WIFI_CONNECT_TIMEOUT,   "Delai de connexion WiFi depasse",         "WIFI"},
    {MEDBOX_ERR_WIFI_DISCONNECTED,      "WiFi deconnecte",                         "WIFI"},
    {MEDBOX_ERR_WIFI_SSID_INVALID,      "SSID invalide ou vide",                   "WIFI"},
    {MEDBOX_ERR_WIFI_AUTH_FAILED,       "Echec d'authentification WiFi",           "WIFI"},
    {MEDBOX_ERR_WIFI_MAX_RETRY,         "Nombre max de tentatives WiFi atteint",   "WIFI"},

    // Erreurs MQTT (0x03xx)
    {MEDBOX_ERR_MQTT_INIT_FAILED,       "Echec d'initialisation MQTT",             "MQTT"},
    {MEDBOX_ERR_MQTT_ALREADY_INIT,      "Client MQTT deja initialise",             "MQTT"},
    {MEDBOX_ERR_MQTT_NOT_INIT,          "Client MQTT non initialise",              "MQTT"},
    {MEDBOX_ERR_MQTT_CONNECT_FAILED,    "Echec de connexion au broker MQTT",       "MQTT"},
    {MEDBOX_ERR_MQTT_CONNECT_TIMEOUT,   "Delai de connexion MQTT depasse",         "MQTT"},
    {MEDBOX_ERR_MQTT_DISCONNECTED,      "Deconnecte du broker MQTT",               "MQTT"},
    {MEDBOX_ERR_MQTT_PUBLISH_FAILED,    "Echec de publication MQTT",               "MQTT"},
    {MEDBOX_ERR_MQTT_SUBSCRIBE_FAILED,  "Echec d'abonnement MQTT",                 "MQTT"},
    {MEDBOX_ERR_MQTT_TLS_HANDSHAKE_FAILED, "Echec du handshake TLS",               "MQTT"},
    {MEDBOX_ERR_MQTT_BROKER_UNREACHABLE,"Broker MQTT inaccessible",                "MQTT"},
    {MEDBOX_ERR_MQTT_INVALID_URI,       "URI du broker MQTT invalide",             "MQTT"},

    // Erreurs Stockage/NVS (0x04xx)
    {MEDBOX_ERR_NVS_INIT_FAILED,        "Echec d'initialisation NVS",              "NVS"},
    {MEDBOX_ERR_NVS_READ_FAILED,        "Echec de lecture NVS",                    "NVS"},
    {MEDBOX_ERR_NVS_WRITE_FAILED,       "Echec d'ecriture NVS",                    "NVS"},
    {MEDBOX_ERR_NVS_NOT_FOUND,          "Cle non trouvee dans NVS",                "NVS"},
    {MEDBOX_ERR_NVS_INVALID_DATA,       "Donnees NVS invalides",                   "NVS"},
    {MEDBOX_ERR_UUID_NOT_PROVISIONED,   "UUID non provisionne",                    "NVS"},
    {MEDBOX_ERR_UUID_INVALID,           "UUID invalide",                           "NVS"},

    // Erreurs Securite (0x05xx)
    {MEDBOX_ERR_CERT_NOT_PROVISIONED,   "Certificats non provisionnes",            "SEC"},
    {MEDBOX_ERR_CERT_CA_LOAD_FAILED,    "Echec de chargement du certificat CA",    "SEC"},
    {MEDBOX_ERR_CERT_CLIENT_LOAD_FAILED,"Echec de chargement du certificat client","SEC"},
    {MEDBOX_ERR_CERT_KEY_LOAD_FAILED,   "Echec de chargement de la cle privee",    "SEC"},
    {MEDBOX_ERR_CERT_INVALID,           "Certificat invalide",                     "SEC"},
    {MEDBOX_ERR_CERT_EXPIRED,           "Certificat expire",                       "SEC"},
    {MEDBOX_ERR_TLS_VERIFY_FAILED,      "Echec de verification TLS",               "SEC"},

    // Erreurs Metier (0x06xx)
    {MEDBOX_ERR_CMD_UNKNOWN,            "Commande inconnue",                       "APP"},
    {MEDBOX_ERR_CMD_INVALID_FORMAT,     "Format de commande invalide",             "APP"},
    {MEDBOX_ERR_CMD_INVALID_PAYLOAD,    "Payload de commande invalide",            "APP"},
    {MEDBOX_ERR_CMD_EXECUTION_FAILED,   "Echec d'execution de la commande",        "APP"},
    {MEDBOX_ERR_WHEEL_NOT_DETECTED,     "Roue non detectee",                       "APP"},
    {MEDBOX_ERR_WHEEL_MOTOR_FAILED,     "Echec du moteur de la roue",              "APP"},
    {MEDBOX_ERR_WHEEL_POSITION_ERROR,   "Erreur de position de la roue",           "APP"},
    {MEDBOX_ERR_DISPENSE_FAILED,        "Echec de distribution",                   "APP"},
    {MEDBOX_ERR_DISPENSE_TIMEOUT,       "Delai de distribution depasse",           "APP"},
    {MEDBOX_ERR_SENSOR_FAILED,          "Echec du capteur",                        "APP"},
    {MEDBOX_ERR_BATTERY_LOW,            "Batterie faible",                         "APP"},
    {MEDBOX_ERR_BATTERY_CRITICAL,       "Batterie critique",                       "APP"},

    // Erreurs Serveur Web/HTTP (0x07xx)
    {MEDBOX_ERR_HTTP_INIT_FAILED,       "Echec d'initialisation du serveur HTTP",  "HTTP"},
    {MEDBOX_ERR_HTTP_START_FAILED,      "Echec de demarrage du serveur HTTP",      "HTTP"},
    {MEDBOX_ERR_HTTP_HANDLER_FAILED,    "Echec d'enregistrement du handler HTTP",  "HTTP"},
    {MEDBOX_ERR_HTTP_RESPONSE_FAILED,   "Echec d'envoi de la reponse HTTP",        "HTTP"},
    {MEDBOX_ERR_HTTP_INVALID_REQUEST,   "Requete HTTP invalide",                   "HTTP"},
    {MEDBOX_ERR_HTTP_JSON_PARSE_ERROR,  "Echec de parsing JSON",                   "HTTP"},
    {MEDBOX_ERR_HTTP_JSON_CREATE_ERROR, "Echec de creation JSON",                  "HTTP"},
    {MEDBOX_ERR_HTTP_BUFFER_OVERFLOW,   "Depassement de buffer HTTP",              "HTTP"},
};

#define ERROR_TABLE_SIZE (sizeof(s_error_table) / sizeof(s_error_table[0]))

// Message par defaut pour erreur inconnue
static const char *s_unknown_error = "Erreur inconnue";
static const char *s_unknown_category = "???";

// ============================================
// Implementation
// ============================================

medbox_err_t error_manager_init(void)
{
    // Initialiser l'historique
    memset(s_error_history, 0, sizeof(s_error_history));
    s_history_head = 0;
    s_history_count = 0;

    ESP_LOGI(TAG, "Gestionnaire d'erreurs initialise (%d codes, historique %d)",
             ERROR_TABLE_SIZE, ERROR_HISTORY_SIZE);
    return MEDBOX_OK;
}

// Ajoute une erreur a l'historique
static void error_history_add(medbox_err_t err, const char *context)
{
    medbox_error_history_entry_t *entry = &s_error_history[s_history_head];

    entry->code = err;
    entry->timestamp = (uint32_t)(esp_timer_get_time() / 1000000ULL);

    if (context != NULL) {
        strncpy(entry->context, context, sizeof(entry->context) - 1);
        entry->context[sizeof(entry->context) - 1] = '\0';
    } else {
        entry->context[0] = '\0';
    }

    // Avancer la tete du buffer circulaire
    s_history_head = (s_history_head + 1) % ERROR_HISTORY_SIZE;
    if (s_history_count < ERROR_HISTORY_SIZE) {
        s_history_count++;
    }
}

const medbox_error_info_t* medbox_err_get_info(medbox_err_t err)
{
    for (size_t i = 0; i < ERROR_TABLE_SIZE; i++) {
        if (s_error_table[i].code == err) {
            return &s_error_table[i];
        }
    }
    return NULL;
}

const char* medbox_err_to_str(medbox_err_t err)
{
    const medbox_error_info_t *info = medbox_err_get_info(err);
    if (info != NULL) {
        return info->message;
    }
    return s_unknown_error;
}

const char* medbox_err_get_category(medbox_err_t err)
{
    const medbox_error_info_t *info = medbox_err_get_info(err);
    if (info != NULL) {
        return info->category;
    }

    // Fallback basé sur le code
    uint8_t cat = (err >> 8) & 0xFF;
    switch (cat) {
        case 0x00: return "OK";
        case 0x01: return "SYS";
        case 0x02: return "WIFI";
        case 0x03: return "MQTT";
        case 0x04: return "NVS";
        case 0x05: return "SEC";
        case 0x06: return "APP";
        case 0x07: return "HTTP";
        default:   return s_unknown_category;
    }
}

void medbox_log_error(medbox_err_t err, const char *tag, const char *context)
{
    const char *msg = medbox_err_to_str(err);
    const char *cat = medbox_err_get_category(err);

    if (context != NULL && strlen(context) > 0) {
        ESP_LOGE(tag, "[%s-0x%04X] %s (%s)", cat, err, msg, context);
    } else {
        ESP_LOGE(tag, "[%s-0x%04X] %s", cat, err, msg);
    }

    // Ajouter a l'historique
    error_history_add(err, context);
}

void medbox_log_warn(const char *tag, const char *format, ...)
{
    va_list args;
    va_start(args, format);

    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    ESP_LOGW(tag, "%s", buffer);

    va_end(args);
}

medbox_err_t medbox_err_from_esp(esp_err_t esp_err, uint8_t category)
{
    if (esp_err == ESP_OK) {
        return MEDBOX_OK;
    }

    // Mapping generique base sur le type d'erreur ESP-IDF
    switch (esp_err) {
        case ESP_ERR_NO_MEM:
            return MEDBOX_ERR_SYS_NO_MEMORY;

        case ESP_ERR_INVALID_ARG:
            return MEDBOX_ERR_SYS_INVALID_ARG;

        case ESP_ERR_INVALID_STATE:
            return MEDBOX_ERR_SYS_INVALID_STATE;

        case ESP_ERR_TIMEOUT:
            // Timeout specifique selon la categorie
            switch (category) {
                case 0x02: return MEDBOX_ERR_WIFI_CONNECT_TIMEOUT;
                case 0x03: return MEDBOX_ERR_MQTT_CONNECT_TIMEOUT;
                default:   return MEDBOX_ERR_SYS_TIMEOUT;
            }

        case ESP_ERR_NOT_FOUND:
            return MEDBOX_ERR_NVS_NOT_FOUND;

        case ESP_FAIL:
        default:
            // Erreur generique selon la categorie
            switch (category) {
                case 0x01: return MEDBOX_ERR_SYS_INIT_FAILED;
                case 0x02: return MEDBOX_ERR_WIFI_CONNECT_FAILED;
                case 0x03: return MEDBOX_ERR_MQTT_CONNECT_FAILED;
                case 0x04: return MEDBOX_ERR_NVS_READ_FAILED;
                case 0x05: return MEDBOX_ERR_CERT_CA_LOAD_FAILED;
                case 0x06: return MEDBOX_ERR_CMD_EXECUTION_FAILED;
                default:   return MEDBOX_ERR_SYS_INIT_FAILED;
            }
    }
}

// ============================================
// API Historique des erreurs
// ============================================

medbox_err_t error_manager_get_history(medbox_error_history_entry_t *entries,
                                       uint16_t max_entries,
                                       uint16_t *num_entries)
{
    if (entries == NULL || num_entries == NULL) {
        return MEDBOX_ERR_SYS_INVALID_ARG;
    }

    uint16_t count = (s_history_count < max_entries) ? s_history_count : max_entries;
    *num_entries = count;

    if (count == 0) {
        return MEDBOX_OK;
    }

    // Lire les entrees du plus recent au plus ancien
    for (uint16_t i = 0; i < count; i++) {
        // Calculer l'index dans le buffer circulaire
        int16_t idx = (int16_t)s_history_head - 1 - (int16_t)i;
        if (idx < 0) {
            idx += ERROR_HISTORY_SIZE;
        }
        memcpy(&entries[i], &s_error_history[idx], sizeof(medbox_error_history_entry_t));
    }

    return MEDBOX_OK;
}

medbox_err_t error_manager_clear_history(void)
{
    memset(s_error_history, 0, sizeof(s_error_history));
    s_history_head = 0;
    s_history_count = 0;
    ESP_LOGI(TAG, "Historique des erreurs efface");
    return MEDBOX_OK;
}

uint16_t error_manager_get_history_count(void)
{
    return s_history_count;
}
