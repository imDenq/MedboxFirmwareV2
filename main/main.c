#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"
#include "sdkconfig.h"

#include "secure_storage.h"
#include "wifi_manager.h"
#include "mqtt_client_medbox.h"
#include "error_manager.h"
#include "web_server.h"

static const char *TAG = "medbox_main";

// Version du firmware
#define FIRMWARE_VERSION "1.0.0"

// Queues pour la communication inter-tâches
static QueueHandle_t mqtt_rx_queue = NULL;

// Topics MQTT (selon protocole medbox)
// Events: medbox/box/{UUID}/evt/{event_type}
// Commands: medbox/box/{UUID}/cmd/#
static char topic_base_evt[128];      // medbox/box/{UUID}/evt
static char topic_base_cmd[128];      // medbox/box/{UUID}/cmd
static char topic_cmd_subscribe[136]; // medbox/box/{UUID}/cmd/#

// Topics d'événements spécifiques
static char topic_evt_heartbeat[160];
static char topic_evt_wheel_inserted[160];
static char topic_evt_dispense_confirmed[160];

// Topic de télémétrie (niveau box, pas sous /evt)
static char topic_telemetry[160];

// UUID de la Medbox
static char medbox_uuid[MEDBOX_UUID_MAX_LEN];

// Suffixe pour le SSID de l'AP (4 derniers caracteres UUID)
static char ap_uuid_suffix[8] = {0};

// Niveau batterie simulé (à remplacer par vraie lecture)
static uint8_t battery_level = 100;

// Prototypes des tâches
static void mqtt_task(void *pvParameters);
static void main_task(void *pvParameters);
static void send_telemetry(void);

// Callback pour les messages MQTT reçus
static void on_mqtt_message(const mqtt_message_t *message)
{
    ESP_LOGI(TAG, "Message MQTT reçu: topic=%s, len=%d", message->topic, message->data_len);

    // Copier le message dans la queue pour traitement asynchrone
    mqtt_message_t msg_copy = *message;

    // Allouer de la mémoire pour les données
    if (message->data_len > 0) {
        msg_copy.data = malloc(message->data_len + 1);
        if (msg_copy.data != NULL) {
            memcpy(msg_copy.data, message->data, message->data_len);
            msg_copy.data[message->data_len] = '\0';  // Null-terminate pour les strings

            if (xQueueSend(mqtt_rx_queue, &msg_copy, pdMS_TO_TICKS(100)) != pdTRUE) {
                ESP_LOGW(TAG, "Queue pleine, message ignoré");
                free(msg_copy.data);
            }
        }
    }
}

// Callback pour les changements d'état MQTT
static void on_mqtt_state_change(mqtt_client_state_t state)
{
    const char *state_str[] = {"DISCONNECTED", "CONNECTING", "CONNECTED", "ERROR"};
    ESP_LOGI(TAG, "État MQTT: %s", state_str[state]);

    if (state == MQTT_STATE_CONNECTED) {
        // S'abonner aux topics de commandes (QoS 1)
        mqtt_client_medbox_subscribe(topic_cmd_subscribe, 1);

        // Envoyer la télémétrie initiale
        ESP_LOGI(TAG, "Envoi télémétrie initiale...");
        send_telemetry();
    }
}

// Envoie un heartbeat au backend
static void send_heartbeat(void)
{
    if (!mqtt_client_medbox_is_connected()) return;

    uint32_t uptime_sec = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;

    char heartbeat_json[256];
    snprintf(heartbeat_json, sizeof(heartbeat_json),
             "{"
             "\"timestamp\":%lu,"
             "\"box_id\":\"%s\","
             "\"battery_level\":%d,"
             "\"firmware_version\":\"%s\""
             "}",
             (unsigned long)uptime_sec,
             medbox_uuid,
             battery_level,
             FIRMWARE_VERSION);

    int msg_id = mqtt_client_medbox_publish(topic_evt_heartbeat, heartbeat_json,
                                             strlen(heartbeat_json), 0, false);
    if (msg_id >= 0) {
        ESP_LOGI(TAG, "Heartbeat envoyé");
    }
}

// Envoie la télémétrie au backend (topic: medbox/box/{UUID}/telemetry)
static void send_telemetry(void)
{
    if (!mqtt_client_medbox_is_connected()) return;

    // TODO: Récupérer la vraie date via SNTP
    char telemetry_json[256];
    snprintf(telemetry_json, sizeof(telemetry_json),
             "{"
             "\"firmware_version\":\"%s\","
             "\"last_sync\":\"2026-04-03T08:00:00Z\","
             "\"current_date\":\"2026-04-03T20:55:00Z\""
             "}",
             FIRMWARE_VERSION);

    int msg_id = mqtt_client_medbox_publish(topic_telemetry, telemetry_json,
                                             strlen(telemetry_json), 1, false);
    if (msg_id >= 0) {
        ESP_LOGI(TAG, "Télémétrie envoyée");
    }
}

// Envoie un événement wheel_inserted
void medbox_evt_wheel_inserted(const char *wheel_id)
{
    if (!mqtt_client_medbox_is_connected()) return;

    char json[128];
    snprintf(json, sizeof(json),
             "{\"wheel_id\":\"%s\"}",
             wheel_id);

    mqtt_client_medbox_publish(topic_evt_wheel_inserted, json, strlen(json), 1, false);
    ESP_LOGI(TAG, "Event wheel_inserted: %s", wheel_id);
}

// Envoie un événement dispense_confirmed
void medbox_evt_dispense_confirmed(const char *dose_id)
{
    if (!mqtt_client_medbox_is_connected()) return;

    char json[128];
    snprintf(json, sizeof(json),
             "{\"dose_id\":\"%s\"}",
             dose_id);

    mqtt_client_medbox_publish(topic_evt_dispense_confirmed, json, strlen(json), 1, false);
    ESP_LOGI(TAG, "Event dispense_confirmed: %s", dose_id);
}

// Traite une commande reçue
static void process_command(const char *topic, const char *payload, size_t len)
{
    ESP_LOGI(TAG, "Commande reçue: %s", topic);

    // Extraire le type de commande du topic
    // Format: medbox/box/{UUID}/cmd/{command_type}
    const char *cmd_type = strrchr(topic, '/');
    if (cmd_type == NULL) {
        ESP_LOGW(TAG, "Format de topic invalide");
        return;
    }
    cmd_type++; // Skip le '/'

    if (strcmp(cmd_type, "ping") == 0) {
        // Répondre au ping avec un heartbeat
        ESP_LOGI(TAG, "Commande PING reçue");
        send_heartbeat();

    } else if (strcmp(cmd_type, "scheduling_update") == 0) {
        // Mise à jour du planning de distribution
        ESP_LOGI(TAG, "Commande SCHEDULING_UPDATE reçue");
        ESP_LOGI(TAG, "Payload: %.*s", (int)len, payload);
        // TODO: Parser le JSON et mettre à jour le planning

    } else if (strcmp(cmd_type, "wheel_reset") == 0) {
        // Reset de la roue de médicaments
        ESP_LOGI(TAG, "Commande WHEEL_RESET reçue");
        // TODO: Implémenter le reset de la roue

    } else {
        ESP_LOGW(TAG, "Commande inconnue: %s", cmd_type);
    }
}

// Callback pour les changements d'état WiFi
static void on_wifi_state_change(wifi_state_t state)
{
    const char *state_str[] = {"DISCONNECTED", "CONNECTING", "CONNECTED", "FAILED"};
    ESP_LOGI(TAG, "Etat WiFi: %s", state_str[state]);

    // Gestion dynamique de l'AP selon l'etat WiFi
    if (state == WIFI_STATE_CONNECTED) {
        // WiFi connecte -> arreter l'AP si actif
        if (wifi_manager_is_ap_active()) {
            ESP_LOGI(TAG, "WiFi connecte, arret du point d'acces");
            wifi_manager_stop_ap();
        }
    } else if (state == WIFI_STATE_DISCONNECTED || state == WIFI_STATE_FAILED) {
        // WiFi deconnecte -> demarrer l'AP si inactif
        if (!wifi_manager_is_ap_active() && ap_uuid_suffix[0] != '\0') {
            ESP_LOGI(TAG, "WiFi deconnecte, demarrage du point d'acces");
            wifi_manager_start_ap(ap_uuid_suffix);
        }
    }
}

// Tâche MQTT - Gère la connexion et la réception des messages
static void mqtt_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Démarrage tâche MQTT");

    // Attendre que le WiFi soit connecté
    while (!wifi_manager_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // Construire l'URI du broker
    char broker_uri[128];
#if CONFIG_MEDBOX_MQTT_USE_TLS
    snprintf(broker_uri, sizeof(broker_uri), "mqtts://%s:%d",
             CONFIG_MEDBOX_MQTT_BROKER_IP, CONFIG_MEDBOX_MQTT_BROKER_PORT);
#else
    snprintf(broker_uri, sizeof(broker_uri), "mqtt://%s:%d",
             CONFIG_MEDBOX_MQTT_BROKER_IP, CONFIG_MEDBOX_MQTT_BROKER_PORT);
#endif

    // Configuration MQTT
    mqtt_client_config_t mqtt_config = {
        .broker_uri = broker_uri,
        .client_id = medbox_uuid,
#ifdef CONFIG_MEDBOX_MQTT_USERNAME
        .username = strlen(CONFIG_MEDBOX_MQTT_USERNAME) > 0 ? CONFIG_MEDBOX_MQTT_USERNAME : NULL,
#endif
#ifdef CONFIG_MEDBOX_MQTT_PASSWORD
        .password = strlen(CONFIG_MEDBOX_MQTT_PASSWORD) > 0 ? CONFIG_MEDBOX_MQTT_PASSWORD : NULL,
#endif
#if CONFIG_MEDBOX_MQTT_USE_TLS
        .use_tls = true,
        .verify_server = CONFIG_MEDBOX_MQTT_VERIFY_SERVER,
        .use_client_cert = CONFIG_MEDBOX_MQTT_USE_CLIENT_CERT,
#else
        .use_tls = false,
        .verify_server = false,
        .use_client_cert = false,
#endif
        .on_message = on_mqtt_message,
        .on_state_change = on_mqtt_state_change,
    };

    // Initialiser et démarrer le client MQTT
    esp_err_t ret = mqtt_client_medbox_init(&mqtt_config);
    if (ret != ESP_OK) {
        medbox_log_error(MEDBOX_ERR_MQTT_INIT_FAILED, TAG, esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }

    ret = mqtt_client_medbox_start();
    if (ret != ESP_OK) {
        medbox_log_error(MEDBOX_ERR_MQTT_CONNECT_FAILED, TAG, esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }

    // Boucle principale de la tâche MQTT
    while (1) {
        // Vérifier la reconnexion si nécessaire (seulement si vraiment déconnecté, pas en cours de connexion)
        mqtt_client_state_t mqtt_state = mqtt_client_medbox_get_state();
        if (mqtt_state == MQTT_STATE_DISCONNECTED && wifi_manager_is_connected()) {
            ESP_LOGW(TAG, "MQTT déconnecté, tentative de reconnexion...");
            mqtt_client_medbox_start();
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// Tâche principale - Traitement des messages et logique métier
static void main_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Démarrage tâche principale");

    mqtt_message_t rx_message;
    TickType_t last_telemetry = 0;
    const TickType_t telemetry_interval = pdMS_TO_TICKS(60000);  // 60 secondes

    while (1) {
        // Traiter les messages MQTT reçus (commandes)
        if (xQueueReceive(mqtt_rx_queue, &rx_message, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "Traitement commande: %s", rx_message.topic);

            // Traiter la commande
            process_command(rx_message.topic, (const char *)rx_message.data, rx_message.data_len);

            // Libérer la mémoire allouée pour les données
            if (rx_message.data != NULL) {
                free(rx_message.data);
            }
        }

        // Envoyer la télémétrie périodiquement (QoS 1)
        TickType_t now = xTaskGetTickCount();
        if (mqtt_client_medbox_is_connected() &&
            (now - last_telemetry) >= telemetry_interval) {

            last_telemetry = now;
            send_telemetry();
        }
    }
}

#ifdef CONFIG_MEDBOX_PROVISIONING_MODE
// Mode provisioning pour injecter les certificats via UART
static void provisioning_mode(void)
{
    ESP_LOGW(TAG, "=== MODE PROVISIONING ACTIF ===");
    ESP_LOGW(TAG, "ATTENTION: Ce mode ne doit pas être utilisé en production!");
    ESP_LOGW(TAG, "Utilisez le script de provisioning pour injecter:");
    ESP_LOGW(TAG, "  - Certificat CA");
    ESP_LOGW(TAG, "  - Certificat client");
    ESP_LOGW(TAG, "  - Clé privée client");
    ESP_LOGW(TAG, "  - UUID Medbox");

    // TODO: Implémenter le protocole de provisioning série
    // Pour l'instant, on peut utiliser les outils NVS partition gen

    while (1) {
        ESP_LOGI(TAG, "En attente de provisioning...");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
#endif

void app_main(void)
{
    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "    Medbox Firmware v1.0.0");
    ESP_LOGI(TAG, "    ESP-IDF: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "===========================================");

    // Initialiser le gestionnaire d'erreurs
    error_manager_init();

    // Initialiser le stockage sécurisé
    esp_err_t ret = secure_storage_init();
    if (ret != ESP_OK) {
        medbox_log_error(MEDBOX_ERR_NVS_INIT_FAILED, TAG, esp_err_to_name(ret));
        return;
    }

#ifdef CONFIG_MEDBOX_DEV_MODE
    strncpy(medbox_uuid, CONFIG_MEDBOX_DEV_UUID, sizeof(medbox_uuid) - 1);
    ESP_LOGW(TAG, "=== Medbox %s ===", medbox_uuid);
#else
    #ifdef CONFIG_MEDBOX_PROVISIONING_MODE
    provisioning_mode();
    return;
    #endif

    // Vérifier si l'appareil est provisionné
    if (!secure_storage_uuid_provisioned()) {
        medbox_log_error(MEDBOX_ERR_UUID_NOT_PROVISIONED, TAG, "Activez CONFIG_MEDBOX_PROVISIONING_MODE");
        return;
    }

    // Récupérer l'UUID
    ret = secure_storage_get_medbox_uuid(medbox_uuid, sizeof(medbox_uuid));
    if (ret != ESP_OK) {
        medbox_log_error(MEDBOX_ERR_NVS_READ_FAILED, TAG, "UUID");
        return;
    }
    ESP_LOGI(TAG, "UUID Medbox: %s", medbox_uuid);

    #if CONFIG_MEDBOX_MQTT_USE_TLS
    // Vérifier si les certificats sont provisionnés
    if (!secure_storage_certs_provisioned()) {
        medbox_log_error(MEDBOX_ERR_CERT_NOT_PROVISIONED, TAG, "Activez CONFIG_MEDBOX_PROVISIONING_MODE");
        return;
    }
    ESP_LOGI(TAG, "Certificats TLS trouvés dans NVS");
    #endif
#endif

    // Construire les topics basés sur l'UUID (selon protocole MQTT Medbox)
    // Format: medbox/box/{UUID}/evt/{event} et medbox/box/{UUID}/cmd/{command}
    snprintf(topic_base_evt, sizeof(topic_base_evt),
             "%s/%s/evt", CONFIG_MEDBOX_MQTT_TOPIC_BASE, medbox_uuid);
    snprintf(topic_base_cmd, sizeof(topic_base_cmd),
             "%s/%s/cmd", CONFIG_MEDBOX_MQTT_TOPIC_BASE, medbox_uuid);
    snprintf(topic_cmd_subscribe, sizeof(topic_cmd_subscribe),
             "%s/+", topic_base_cmd);

    // Topics d'événements spécifiques
    snprintf(topic_evt_heartbeat, sizeof(topic_evt_heartbeat),
             "%s/heartbeat", topic_base_evt);
    snprintf(topic_evt_wheel_inserted, sizeof(topic_evt_wheel_inserted),
             "%s/wheel_inserted", topic_base_evt);
    snprintf(topic_evt_dispense_confirmed, sizeof(topic_evt_dispense_confirmed),
             "%s/dispense_confirmed", topic_base_evt);

    // Topic de télémétrie (au niveau box, pas sous /evt)
    snprintf(topic_telemetry, sizeof(topic_telemetry),
             "%s/%s/telemetry", CONFIG_MEDBOX_MQTT_TOPIC_BASE, medbox_uuid);

    ESP_LOGI(TAG, "Topics MQTT configurés:");
    ESP_LOGI(TAG, "  Events base:   %s", topic_base_evt);
    ESP_LOGI(TAG, "  Commands sub:  %s", topic_cmd_subscribe);
    ESP_LOGI(TAG, "  Telemetry:     %s", topic_telemetry);
    ESP_LOGI(TAG, "  Heartbeat:     %s", topic_evt_heartbeat);

    // Créer la queue pour les messages MQTT
    mqtt_rx_queue = xQueueCreate(10, sizeof(mqtt_message_t));
    if (mqtt_rx_queue == NULL) {
        medbox_log_error(MEDBOX_ERR_SYS_QUEUE_CREATE_FAILED, TAG, "mqtt_rx_queue");
        return;
    }

    // Initialiser le WiFi Manager
    ret = wifi_manager_init();
    if (ret != ESP_OK) {
        medbox_log_error(MEDBOX_ERR_WIFI_INIT_FAILED, TAG, esp_err_to_name(ret));
        return;
    }

    wifi_manager_set_callback(on_wifi_state_change);

    // Démarrer la connexion WiFi
    ESP_LOGI(TAG, "Connexion au WiFi: %s", CONFIG_MEDBOX_WIFI_SSID);
    ret = wifi_manager_connect(CONFIG_MEDBOX_WIFI_SSID, CONFIG_MEDBOX_WIFI_PASSWORD);
    if (ret != ESP_OK) {
        medbox_log_error(MEDBOX_ERR_WIFI_CONNECT_FAILED, TAG, esp_err_to_name(ret));
        return;
    }

    // Preparer le suffixe pour l'AP (4 derniers caracteres de l'UUID)
    size_t uuid_len = strlen(medbox_uuid);
    if (uuid_len > 4) {
        strncpy(ap_uuid_suffix, medbox_uuid + uuid_len - 4, sizeof(ap_uuid_suffix) - 1);
    } else {
        strncpy(ap_uuid_suffix, medbox_uuid, sizeof(ap_uuid_suffix) - 1);
    }

    // Attendre la connexion WiFi initiale (timeout 30s)
    // Si timeout, on continue quand même - le WiFi manager retente en arrière-plan
    ret = wifi_manager_wait_connected(30000);
    if (ret == ESP_OK) {
        char ip_str[16];
        wifi_manager_get_ip(ip_str);
        ESP_LOGI(TAG, "Connecte au WiFi! IP: %s", ip_str);
        // AP non demarre car WiFi connecte
    } else {
        ESP_LOGW(TAG, "WiFi non connecte apres 30s - demarrage point d'acces");
        // Demarrer le point d'acces car WiFi non connecte
        ret = wifi_manager_start_ap(ap_uuid_suffix);
        if (ret == ESP_OK) {
            char ap_ssid[32];
            char ap_ip[16];
            wifi_manager_get_ap_ssid(ap_ssid);
            wifi_manager_get_ap_ip(ap_ip);
            ESP_LOGI(TAG, "Point d'acces demarre: %s (%s)", ap_ssid, ap_ip);
        } else {
            ESP_LOGW(TAG, "Echec demarrage point d'acces");
        }
    }

    // Demarrer le serveur web
    ret = web_server_start();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Serveur web demarre sur port %d", WEB_SERVER_PORT);
    } else {
        ESP_LOGW(TAG, "Echec demarrage serveur web");
    }

    // Créer les tâches FreeRTOS
    BaseType_t task_ret;

    task_ret = xTaskCreate(mqtt_task, "mqtt_task", 8192, NULL, 5, NULL);
    if (task_ret != pdPASS) {
        medbox_log_error(MEDBOX_ERR_SYS_TASK_CREATE_FAILED, TAG, "mqtt_task");
        return;
    }

    task_ret = xTaskCreate(main_task, "main_task", 4096, NULL, 4, NULL);
    if (task_ret != pdPASS) {
        medbox_log_error(MEDBOX_ERR_SYS_TASK_CREATE_FAILED, TAG, "main_task");
        return;
    }

    ESP_LOGI(TAG, "Système démarré avec succès!");

    // app_main peut retourner, les tâches continuent de tourner
}
