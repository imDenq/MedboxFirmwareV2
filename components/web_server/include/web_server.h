/**
 * @file web_server.h
 * @brief Serveur web pour configuration et monitoring Medbox
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Configuration serveur
#define WEB_SERVER_PORT             80
#define WEB_SERVER_MAX_URI_LEN      512
#define WEB_SERVER_MAX_RESP_SIZE    4096

/**
 * @brief Initialise et demarre le serveur web
 *
 * @return esp_err_t ESP_OK en cas de succes
 */
esp_err_t web_server_start(void);

/**
 * @brief Arrete le serveur web
 *
 * @return esp_err_t ESP_OK en cas de succes
 */
esp_err_t web_server_stop(void);

/**
 * @brief Verifie si le serveur est actif
 *
 * @return true si actif
 */
bool web_server_is_running(void);

#ifdef __cplusplus
}
#endif

#endif // WEB_SERVER_H
