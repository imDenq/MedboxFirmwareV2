/**
 * @file console_ws.h
 * @brief Console distante : stream des logs via WebSocket sur le serveur web.
 *
 * Detourne esp_log vers un ring buffer, une tache pousse les lignes aux clients
 * WebSocket connectes. Reutilise le httpd existant (aucune stack crypto en plus).
 * Plain ws:// — pour le debug/test, pas de chiffrement.
 */

#ifndef CONSOLE_WS_H
#define CONSOLE_WS_H

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Installe le hook de logs + demarre la tache d'envoi WebSocket. */
void console_ws_start(void);

/** Enregistre la page /console et l'endpoint WebSocket /ws/console. */
esp_err_t console_ws_register(httpd_handle_t server);

#ifdef __cplusplus
}
#endif

#endif  // CONSOLE_WS_H
