#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include "esp_wifi_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Configuration reconnexion WiFi STA
#define WIFI_RETRY_DELAY_MS         2000    // Delai entre tentatives (ms)
#define WIFI_RETRY_LOG_INTERVAL     10      // Log un warning toutes les X tentatives
#define WIFI_CONNECTED_BIT          BIT0
#define WIFI_FAIL_BIT               BIT1

// Configuration Access Point
#define WIFI_AP_SSID_PREFIX         "Medbox-"
#define WIFI_AP_PASSWORD            "medbox123"
#define WIFI_AP_CHANNEL             1
#define WIFI_AP_MAX_CONNECTIONS     4
#define WIFI_AP_IP                  "192.168.4.1"

// Etats de la connexion WiFi STA
typedef enum {
    WIFI_STATE_DISCONNECTED = 0,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_FAILED
} wifi_state_t;

// Structure pour infos reseau scanne
typedef struct {
    char ssid[33];
    int8_t rssi;
    wifi_auth_mode_t authmode;
} wifi_scan_result_t;

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
 * @brief Recupere le RSSI actuel
 *
 * @return int8_t RSSI en dBm, ou 0 si non connecte
 */
int8_t wifi_manager_get_rssi(void);

// ============================================
// Fonctions Access Point (AP)
// ============================================

/**
 * @brief Demarre le point d'acces (AP)
 *
 * @param uuid_suffix 4 derniers caracteres de l'UUID pour le SSID
 * @return esp_err_t ESP_OK en cas de succes
 */
esp_err_t wifi_manager_start_ap(const char *uuid_suffix);

/**
 * @brief Arrete le point d'acces
 *
 * @return esp_err_t ESP_OK en cas de succes
 */
esp_err_t wifi_manager_stop_ap(void);

/**
 * @brief Verifie si l'AP est actif
 *
 * @return true si AP actif
 */
bool wifi_manager_is_ap_active(void);

/**
 * @brief Recupere l'IP de l'AP
 *
 * @param ip_str Buffer pour stocker l'IP (min 16 bytes)
 * @return esp_err_t ESP_OK en cas de succes
 */
esp_err_t wifi_manager_get_ap_ip(char *ip_str);

/**
 * @brief Recupere le SSID de l'AP
 *
 * @param ssid_str Buffer pour stocker le SSID (min 32 bytes)
 * @return esp_err_t ESP_OK en cas de succes
 */
esp_err_t wifi_manager_get_ap_ssid(char *ssid_str);

// ============================================
// Fonctions Scan WiFi
// ============================================

/**
 * @brief Lance un scan des reseaux WiFi
 *
 * @param results Tableau pour stocker les resultats
 * @param max_results Taille max du tableau
 * @param num_results Nombre de resultats trouves (sortie)
 * @return esp_err_t ESP_OK en cas de succes
 */
esp_err_t wifi_manager_scan(wifi_scan_result_t *results, uint16_t max_results, uint16_t *num_results);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H
