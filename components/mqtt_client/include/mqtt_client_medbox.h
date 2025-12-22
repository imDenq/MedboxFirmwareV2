#ifndef MQTT_CLIENT_MEDBOX_H
#define MQTT_CLIENT_MEDBOX_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Configuration par défaut
#define MQTT_BROKER_PORT_TLS        8883
#define MQTT_BROKER_PORT_TCP        1883
#define MQTT_KEEPALIVE_SEC          60
#define MQTT_BUFFER_SIZE            4096
#define MQTT_TOPIC_MAX_LEN          256

// États du client MQTT
typedef enum {
    MQTT_STATE_DISCONNECTED = 0,
    MQTT_STATE_CONNECTING,
    MQTT_STATE_CONNECTED,
    MQTT_STATE_ERROR
} mqtt_client_state_t;

// Structure pour les messages reçus
typedef struct {
    char topic[MQTT_TOPIC_MAX_LEN];
    uint8_t *data;
    size_t data_len;
    int msg_id;
    int qos;
    bool retain;
} mqtt_message_t;

// Callback pour les messages reçus
typedef void (*mqtt_message_callback_t)(const mqtt_message_t *message);

// Callback pour les changements d'état
typedef void (*mqtt_state_callback_t)(mqtt_client_state_t state);

// Configuration du client MQTT
typedef struct {
    const char *broker_uri;          // URI du broker (ex: "mqtts://185.157.245.46:8883")
    const char *client_id;           // ID du client (utiliser UUID Medbox)
    const char *username;            // Username (optionnel)
    const char *password;            // Password (optionnel)
    bool use_tls;                    // Activer TLS
    bool verify_server;              // Vérifier le certificat serveur
    bool use_client_cert;            // Utiliser certificat client (mTLS)
    mqtt_message_callback_t on_message;
    mqtt_state_callback_t on_state_change;
} mqtt_client_config_t;

/**
 * @brief Initialise le client MQTT avec les certificats du NVS
 *
 * @param config Configuration du client
 * @return esp_err_t ESP_OK en cas de succès
 */
esp_err_t mqtt_client_medbox_init(const mqtt_client_config_t *config);

/**
 * @brief Démarre la connexion au broker
 *
 * @return esp_err_t ESP_OK en cas de succès
 */
esp_err_t mqtt_client_medbox_start(void);

/**
 * @brief Arrête le client MQTT
 *
 * @return esp_err_t ESP_OK en cas de succès
 */
esp_err_t mqtt_client_medbox_stop(void);

/**
 * @brief Publie un message
 *
 * @param topic Topic de publication
 * @param data Données à publier
 * @param len Longueur des données
 * @param qos QoS (0, 1 ou 2)
 * @param retain Flag retain
 * @return int Message ID ou -1 en cas d'erreur
 */
int mqtt_client_medbox_publish(const char *topic, const char *data, size_t len, int qos, bool retain);

/**
 * @brief S'abonne à un topic
 *
 * @param topic Topic (supporte wildcards + et #)
 * @param qos QoS (0, 1 ou 2)
 * @return int Message ID ou -1 en cas d'erreur
 */
int mqtt_client_medbox_subscribe(const char *topic, int qos);

/**
 * @brief Se désabonne d'un topic
 *
 * @param topic Topic
 * @return int Message ID ou -1 en cas d'erreur
 */
int mqtt_client_medbox_unsubscribe(const char *topic);

/**
 * @brief Vérifie si le client est connecté
 *
 * @return true si connecté
 */
bool mqtt_client_medbox_is_connected(void);

/**
 * @brief Récupère l'état actuel du client
 *
 * @return mqtt_client_state_t État actuel
 */
mqtt_client_state_t mqtt_client_medbox_get_state(void);

/**
 * @brief Attend la connexion MQTT (bloquant)
 *
 * @param timeout_ms Timeout en millisecondes
 * @return esp_err_t ESP_OK si connecté
 */
esp_err_t mqtt_client_medbox_wait_connected(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // MQTT_CLIENT_MEDBOX_H
