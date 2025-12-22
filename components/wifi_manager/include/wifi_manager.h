#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include "esp_wifi_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Nombre maximum de tentatives de reconnexion
#define WIFI_MAX_RETRY              10
#define WIFI_CONNECTED_BIT          BIT0
#define WIFI_FAIL_BIT               BIT1

// États de la connexion WiFi
typedef enum {
    WIFI_STATE_DISCONNECTED = 0,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_FAILED
} wifi_state_t;

// Callback pour les événements WiFi
typedef void (*wifi_event_callback_t)(wifi_state_t state);

/**
 * @brief Initialise le WiFi en mode station
 *
 * @return esp_err_t ESP_OK en cas de succès
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Démarre la connexion WiFi
 *
 * @param ssid SSID du réseau WiFi
 * @param password Mot de passe WiFi
 * @return esp_err_t ESP_OK en cas de succès
 */
esp_err_t wifi_manager_connect(const char *ssid, const char *password);

/**
 * @brief Déconnecte du WiFi
 *
 * @return esp_err_t ESP_OK en cas de succès
 */
esp_err_t wifi_manager_disconnect(void);

/**
 * @brief Attend la connexion WiFi (bloquant)
 *
 * @param timeout_ms Timeout en millisecondes
 * @return esp_err_t ESP_OK si connecté, ESP_ERR_TIMEOUT si timeout
 */
esp_err_t wifi_manager_wait_connected(uint32_t timeout_ms);

/**
 * @brief Vérifie si le WiFi est connecté
 *
 * @return true si connecté
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Récupère l'état actuel du WiFi
 *
 * @return wifi_state_t État actuel
 */
wifi_state_t wifi_manager_get_state(void);

/**
 * @brief Enregistre un callback pour les événements WiFi
 *
 * @param callback Fonction callback
 */
void wifi_manager_set_callback(wifi_event_callback_t callback);

/**
 * @brief Récupère l'adresse IP actuelle
 *
 * @param ip_str Buffer pour stocker l'IP (min 16 bytes)
 * @return esp_err_t ESP_OK en cas de succès
 */
esp_err_t wifi_manager_get_ip(char *ip_str);

/**
 * @brief Récupère le RSSI actuel
 *
 * @return int8_t RSSI en dBm, ou 0 si non connecté
 */
int8_t wifi_manager_get_rssi(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H
