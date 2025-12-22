/**
 * @file error_manager.h
 * @brief Gestionnaire d'erreurs centralise pour Medbox
 *
 * Ce module fournit :
 * - Des codes d'erreur uniques pour chaque situation
 * - Des messages d'erreur en francais
 * - Une API de logging centralisee
 */

#ifndef ERROR_MANAGER_H
#define ERROR_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Codes d'erreur Medbox
 *
 * Format: 0xCCNN ou CC = Categorie, NN = Numero
 *
 * Categories:
 * - 0x00xx : Succes
 * - 0x01xx : Systeme (init, memoire, taches)
 * - 0x02xx : WiFi
 * - 0x03xx : MQTT
 * - 0x04xx : Stockage/NVS
 * - 0x05xx : Securite (certificats, cles)
 * - 0x06xx : Metier (commandes, roue, etc.)
 */
typedef enum {
    // ========================================
    // 0x00xx - Succes
    // ========================================
    MEDBOX_OK                               = 0x0000,

    // ========================================
    // 0x01xx - Erreurs Systeme
    // ========================================
    MEDBOX_ERR_SYS_INIT_FAILED              = 0x0101,
    MEDBOX_ERR_SYS_NO_MEMORY                = 0x0102,
    MEDBOX_ERR_SYS_TASK_CREATE_FAILED       = 0x0103,
    MEDBOX_ERR_SYS_QUEUE_CREATE_FAILED      = 0x0104,
    MEDBOX_ERR_SYS_QUEUE_FULL               = 0x0105,
    MEDBOX_ERR_SYS_INVALID_ARG              = 0x0106,
    MEDBOX_ERR_SYS_INVALID_STATE            = 0x0107,
    MEDBOX_ERR_SYS_TIMEOUT                  = 0x0108,

    // ========================================
    // 0x02xx - Erreurs WiFi
    // ========================================
    MEDBOX_ERR_WIFI_INIT_FAILED             = 0x0201,
    MEDBOX_ERR_WIFI_ALREADY_INIT            = 0x0202,
    MEDBOX_ERR_WIFI_NOT_INIT                = 0x0203,
    MEDBOX_ERR_WIFI_CONNECT_FAILED          = 0x0204,
    MEDBOX_ERR_WIFI_CONNECT_TIMEOUT         = 0x0205,
    MEDBOX_ERR_WIFI_DISCONNECTED            = 0x0206,
    MEDBOX_ERR_WIFI_SSID_INVALID            = 0x0207,
    MEDBOX_ERR_WIFI_AUTH_FAILED             = 0x0208,
    MEDBOX_ERR_WIFI_MAX_RETRY               = 0x0209,

    // ========================================
    // 0x03xx - Erreurs MQTT
    // ========================================
    MEDBOX_ERR_MQTT_INIT_FAILED             = 0x0301,
    MEDBOX_ERR_MQTT_ALREADY_INIT            = 0x0302,
    MEDBOX_ERR_MQTT_NOT_INIT                = 0x0303,
    MEDBOX_ERR_MQTT_CONNECT_FAILED          = 0x0304,
    MEDBOX_ERR_MQTT_CONNECT_TIMEOUT         = 0x0305,
    MEDBOX_ERR_MQTT_DISCONNECTED            = 0x0306,
    MEDBOX_ERR_MQTT_PUBLISH_FAILED          = 0x0307,
    MEDBOX_ERR_MQTT_SUBSCRIBE_FAILED        = 0x0308,
    MEDBOX_ERR_MQTT_TLS_HANDSHAKE_FAILED    = 0x0309,
    MEDBOX_ERR_MQTT_BROKER_UNREACHABLE      = 0x030A,
    MEDBOX_ERR_MQTT_INVALID_URI             = 0x030B,

    // ========================================
    // 0x04xx - Erreurs Stockage/NVS
    // ========================================
    MEDBOX_ERR_NVS_INIT_FAILED              = 0x0401,
    MEDBOX_ERR_NVS_READ_FAILED              = 0x0402,
    MEDBOX_ERR_NVS_WRITE_FAILED             = 0x0403,
    MEDBOX_ERR_NVS_NOT_FOUND                = 0x0404,
    MEDBOX_ERR_NVS_INVALID_DATA             = 0x0405,
    MEDBOX_ERR_UUID_NOT_PROVISIONED         = 0x0410,
    MEDBOX_ERR_UUID_INVALID                 = 0x0411,

    // ========================================
    // 0x05xx - Erreurs Securite
    // ========================================
    MEDBOX_ERR_CERT_NOT_PROVISIONED         = 0x0501,
    MEDBOX_ERR_CERT_CA_LOAD_FAILED          = 0x0502,
    MEDBOX_ERR_CERT_CLIENT_LOAD_FAILED      = 0x0503,
    MEDBOX_ERR_CERT_KEY_LOAD_FAILED         = 0x0504,
    MEDBOX_ERR_CERT_INVALID                 = 0x0505,
    MEDBOX_ERR_CERT_EXPIRED                 = 0x0506,
    MEDBOX_ERR_TLS_VERIFY_FAILED            = 0x0507,

    // ========================================
    // 0x06xx - Erreurs Metier
    // ========================================
    MEDBOX_ERR_CMD_UNKNOWN                  = 0x0601,
    MEDBOX_ERR_CMD_INVALID_FORMAT           = 0x0602,
    MEDBOX_ERR_CMD_INVALID_PAYLOAD          = 0x0603,
    MEDBOX_ERR_CMD_EXECUTION_FAILED         = 0x0604,
    MEDBOX_ERR_WHEEL_NOT_DETECTED           = 0x0610,
    MEDBOX_ERR_WHEEL_MOTOR_FAILED           = 0x0611,
    MEDBOX_ERR_WHEEL_POSITION_ERROR         = 0x0612,
    MEDBOX_ERR_DISPENSE_FAILED              = 0x0620,
    MEDBOX_ERR_DISPENSE_TIMEOUT             = 0x0621,
    MEDBOX_ERR_SENSOR_FAILED                = 0x0630,
    MEDBOX_ERR_BATTERY_LOW                  = 0x0640,
    MEDBOX_ERR_BATTERY_CRITICAL             = 0x0641,

} medbox_err_t;

/**
 * @brief Informations sur une erreur
 */
typedef struct {
    medbox_err_t code;          // Code d'erreur
    const char *message;        // Message en francais
    const char *category;       // Categorie (SYS, WIFI, MQTT, etc.)
} medbox_error_info_t;

// ============================================
// API Publique
// ============================================

/**
 * @brief Initialise le gestionnaire d'erreurs
 * @return MEDBOX_OK en cas de succes
 */
medbox_err_t error_manager_init(void);

/**
 * @brief Obtient le message d'erreur
 * @param err Code d'erreur
 * @return Message d'erreur (ne pas liberer)
 */
const char* medbox_err_to_str(medbox_err_t err);

/**
 * @brief Obtient les informations completes sur une erreur
 * @param err Code d'erreur
 * @return Pointeur vers les infos (ne pas liberer), NULL si inconnu
 */
const medbox_error_info_t* medbox_err_get_info(medbox_err_t err);

/**
 * @brief Obtient la categorie d'une erreur
 * @param err Code d'erreur
 * @return Nom de la categorie (SYS, WIFI, MQTT, NVS, SEC, APP)
 */
const char* medbox_err_get_category(medbox_err_t err);

/**
 * @brief Log une erreur dans la console
 * @param err Code d'erreur Medbox
 * @param tag Tag pour le log (ex: "main", "wifi")
 * @param context Contexte additionnel (peut etre NULL)
 */
void medbox_log_error(medbox_err_t err, const char *tag, const char *context);

/**
 * @brief Log un warning dans la console
 * @param tag Tag pour le log
 * @param format Format printf
 * @param ... Arguments
 */
void medbox_log_warn(const char *tag, const char *format, ...);

/**
 * @brief Convertit un esp_err_t en medbox_err_t
 * @param esp_err Code d'erreur ESP-IDF
 * @param category Categorie de l'erreur (0x01=SYS, 0x02=WIFI, etc.)
 * @return Code d'erreur Medbox correspondant
 */
medbox_err_t medbox_err_from_esp(esp_err_t esp_err, uint8_t category);

/**
 * @brief Macro pour verifier et logger une erreur
 */
#define MEDBOX_CHECK_ERROR(err, tag, context) \
    do { \
        medbox_err_t _err = (err); \
        if (_err != MEDBOX_OK) { \
            medbox_log_error(_err, tag, context); \
            return _err; \
        } \
    } while(0)

/**
 * @brief Macro pour verifier un esp_err_t et le convertir
 */
#define MEDBOX_CHECK_ESP_ERROR(esp_err, category, tag, context) \
    do { \
        esp_err_t _esp_err = (esp_err); \
        if (_esp_err != ESP_OK) { \
            medbox_err_t _merr = medbox_err_from_esp(_esp_err, category); \
            medbox_log_error(_merr, tag, context); \
            return _merr; \
        } \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif // ERROR_MANAGER_H
