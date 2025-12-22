#ifndef SECURE_STORAGE_H
#define SECURE_STORAGE_H

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Tailles maximales des données
#define MEDBOX_UUID_MAX_LEN         64
#define MEDBOX_CERT_MAX_LEN         4096
#define MEDBOX_PRIVATE_KEY_MAX_LEN  2048

// Namespace NVS pour le stockage sécurisé
#define NVS_NAMESPACE_CERTS         "certs"
#define NVS_NAMESPACE_CONFIG        "config"

// Clés NVS
#define NVS_KEY_CA_CERT             "ca_cert"
#define NVS_KEY_CLIENT_CERT         "client_cert"
#define NVS_KEY_CLIENT_KEY          "client_key"
#define NVS_KEY_MEDBOX_UUID         "medbox_uuid"

/**
 * @brief Structure contenant les certificats TLS
 */
typedef struct {
    char ca_cert[MEDBOX_CERT_MAX_LEN];
    char client_cert[MEDBOX_CERT_MAX_LEN];
    char client_key[MEDBOX_PRIVATE_KEY_MAX_LEN];
    size_t ca_cert_len;
    size_t client_cert_len;
    size_t client_key_len;
} medbox_tls_certs_t;

/**
 * @brief Initialise le stockage sécurisé NVS
 *
 * @return esp_err_t ESP_OK en cas de succès
 */
esp_err_t secure_storage_init(void);

/**
 * @brief Stocke le certificat CA du broker MQTT
 *
 * @param cert Certificat CA en PEM (null-terminated)
 * @param len Longueur du certificat
 * @return esp_err_t ESP_OK en cas de succès
 */
esp_err_t secure_storage_set_ca_cert(const char *cert, size_t len);

/**
 * @brief Récupère le certificat CA du broker MQTT
 *
 * @param cert Buffer pour stocker le certificat
 * @param max_len Taille maximale du buffer
 * @param out_len Longueur réelle du certificat
 * @return esp_err_t ESP_OK en cas de succès
 */
esp_err_t secure_storage_get_ca_cert(char *cert, size_t max_len, size_t *out_len);

/**
 * @brief Stocke le certificat client
 *
 * @param cert Certificat client en PEM (null-terminated)
 * @param len Longueur du certificat
 * @return esp_err_t ESP_OK en cas de succès
 */
esp_err_t secure_storage_set_client_cert(const char *cert, size_t len);

/**
 * @brief Récupère le certificat client
 *
 * @param cert Buffer pour stocker le certificat
 * @param max_len Taille maximale du buffer
 * @param out_len Longueur réelle du certificat
 * @return esp_err_t ESP_OK en cas de succès
 */
esp_err_t secure_storage_get_client_cert(char *cert, size_t max_len, size_t *out_len);

/**
 * @brief Stocke la clé privée du client
 *
 * @param key Clé privée en PEM (null-terminated)
 * @param len Longueur de la clé
 * @return esp_err_t ESP_OK en cas de succès
 */
esp_err_t secure_storage_set_client_key(const char *key, size_t len);

/**
 * @brief Récupère la clé privée du client
 *
 * @param key Buffer pour stocker la clé
 * @param max_len Taille maximale du buffer
 * @param out_len Longueur réelle de la clé
 * @return esp_err_t ESP_OK en cas de succès
 */
esp_err_t secure_storage_get_client_key(char *key, size_t max_len, size_t *out_len);

/**
 * @brief Stocke l'UUID de la Medbox
 *
 * @param uuid UUID (null-terminated)
 * @return esp_err_t ESP_OK en cas de succès
 */
esp_err_t secure_storage_set_medbox_uuid(const char *uuid);

/**
 * @brief Récupère l'UUID de la Medbox
 *
 * @param uuid Buffer pour stocker l'UUID
 * @param max_len Taille maximale du buffer
 * @return esp_err_t ESP_OK en cas de succès
 */
esp_err_t secure_storage_get_medbox_uuid(char *uuid, size_t max_len);

/**
 * @brief Récupère tous les certificats TLS
 *
 * @param certs Structure pour stocker les certificats
 * @return esp_err_t ESP_OK en cas de succès
 */
esp_err_t secure_storage_get_all_certs(medbox_tls_certs_t *certs);

/**
 * @brief Vérifie si les certificats sont provisionnés
 *
 * @return true si tous les certificats sont présents
 */
bool secure_storage_certs_provisioned(void);

/**
 * @brief Vérifie si l'UUID est provisionné
 *
 * @return true si l'UUID est présent
 */
bool secure_storage_uuid_provisioned(void);

/**
 * @brief Efface toutes les données sécurisées (utiliser avec précaution)
 *
 * @return esp_err_t ESP_OK en cas de succès
 */
esp_err_t secure_storage_erase_all(void);

#ifdef __cplusplus
}
#endif

#endif // SECURE_STORAGE_H
