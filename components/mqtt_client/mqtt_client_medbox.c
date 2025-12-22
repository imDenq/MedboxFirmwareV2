#include "mqtt_client_medbox.h"
#include "mqtt_client.h"
#include "secure_storage.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "sdkconfig.h"
#include <string.h>

static const char *TAG = "mqtt_medbox";

#define MQTT_CONNECTED_BIT      BIT0
#define MQTT_DISCONNECTED_BIT   BIT1

// Certificats embarqués (mode DEV)
#ifdef CONFIG_MEDBOX_DEV_MODE
extern const uint8_t ca_crt_start[] asm("_binary_ca_crt_start");
extern const uint8_t ca_crt_end[] asm("_binary_ca_crt_end");
extern const uint8_t client_crt_start[] asm("_binary_client_crt_start");
extern const uint8_t client_crt_end[] asm("_binary_client_crt_end");
extern const uint8_t client_key_start[] asm("_binary_client_key_start");
extern const uint8_t client_key_end[] asm("_binary_client_key_end");
#endif

static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static mqtt_client_state_t s_mqtt_state = MQTT_STATE_DISCONNECTED;
static EventGroupHandle_t s_mqtt_event_group = NULL;
static bool s_client_started = false;
static mqtt_message_callback_t s_message_callback = NULL;
static mqtt_state_callback_t s_state_callback = NULL;

// Buffers pour les certificats (chargés depuis NVS en mode production)
#ifndef CONFIG_MEDBOX_DEV_MODE
static char s_ca_cert[MEDBOX_CERT_MAX_LEN];
static char s_client_cert[MEDBOX_CERT_MAX_LEN];
static char s_client_key[MEDBOX_PRIVATE_KEY_MAX_LEN];
#endif
static bool s_certs_loaded = false;

static void notify_state_change(mqtt_client_state_t new_state)
{
    s_mqtt_state = new_state;
    if (s_state_callback != NULL) {
        s_state_callback(new_state);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connecté au broker MQTT");
            xEventGroupSetBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
            xEventGroupClearBits(s_mqtt_event_group, MQTT_DISCONNECTED_BIT);
            notify_state_change(MQTT_STATE_CONNECTED);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Déconnecté du broker MQTT");
            xEventGroupSetBits(s_mqtt_event_group, MQTT_DISCONNECTED_BIT);
            xEventGroupClearBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
            notify_state_change(MQTT_STATE_DISCONNECTED);
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "Abonné, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "Désabonné, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "Message publié, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "Message reçu sur topic: %.*s", event->topic_len, event->topic);
            if (s_message_callback != NULL) {
                mqtt_message_t msg = {0};

                // Copier le topic
                size_t topic_len = event->topic_len < MQTT_TOPIC_MAX_LEN - 1 ?
                                   event->topic_len : MQTT_TOPIC_MAX_LEN - 1;
                memcpy(msg.topic, event->topic, topic_len);
                msg.topic[topic_len] = '\0';

                // Données du message
                msg.data = (uint8_t *)event->data;
                msg.data_len = event->data_len;
                msg.msg_id = event->msg_id;
                msg.qos = event->qos;
                msg.retain = event->retain;

                s_message_callback(&msg);
            }
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Erreur MQTT");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "Transport error: esp_tls_last_esp_err=0x%x",
                         event->error_handle->esp_tls_last_esp_err);
                ESP_LOGE(TAG, "Transport error: tls_stack_err=0x%x",
                         event->error_handle->esp_tls_stack_err);
                ESP_LOGE(TAG, "Transport error: socket_errno=%d",
                         event->error_handle->esp_transport_sock_errno);
            }
            notify_state_change(MQTT_STATE_ERROR);
            break;

        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGI(TAG, "Connexion au broker...");
            notify_state_change(MQTT_STATE_CONNECTING);
            break;

        default:
            ESP_LOGD(TAG, "Événement MQTT non géré: %d", event->event_id);
            break;
    }
}

static esp_err_t load_certificates(bool use_tls, bool use_client_cert)
{
    if (!use_tls) {
        s_certs_loaded = true;
        return ESP_OK;
    }

#ifdef CONFIG_MEDBOX_DEV_MODE
    // Mode DEV: utiliser les certificats embarqués
    ESP_LOGI(TAG, "Certificats embarqués (mode DEV)");
    ESP_LOGI(TAG, "  CA: %d bytes", ca_crt_end - ca_crt_start);
    ESP_LOGI(TAG, "  Client cert: %d bytes", client_crt_end - client_crt_start);
    ESP_LOGI(TAG, "  Client key: %d bytes", client_key_end - client_key_start);
    s_certs_loaded = true;
    return ESP_OK;
#else
    // Mode production: charger depuis NVS
    esp_err_t ret;
    size_t len;

    // Charger le certificat CA
    memset(s_ca_cert, 0, sizeof(s_ca_cert));
    ret = secure_storage_get_ca_cert(s_ca_cert, sizeof(s_ca_cert), &len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Impossible de charger le certificat CA: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Certificat CA chargé (%d bytes)", len);

    if (use_client_cert) {
        // Charger le certificat client
        memset(s_client_cert, 0, sizeof(s_client_cert));
        ret = secure_storage_get_client_cert(s_client_cert, sizeof(s_client_cert), &len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Impossible de charger le certificat client: %s", esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(TAG, "Certificat client chargé (%d bytes)", len);

        // Charger la clé privée
        memset(s_client_key, 0, sizeof(s_client_key));
        ret = secure_storage_get_client_key(s_client_key, sizeof(s_client_key), &len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Impossible de charger la clé privée: %s", esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(TAG, "Clé privée chargée (%d bytes)", len);
    }

    s_certs_loaded = true;
    return ESP_OK;
#endif
}

esp_err_t mqtt_client_medbox_init(const mqtt_client_config_t *config)
{
    if (config == NULL || config->broker_uri == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_mqtt_client != NULL) {
        ESP_LOGW(TAG, "Client MQTT déjà initialisé");
        return ESP_OK;
    }

    // Créer le groupe d'événements
    s_mqtt_event_group = xEventGroupCreate();
    if (s_mqtt_event_group == NULL) {
        ESP_LOGE(TAG, "Échec création event group");
        return ESP_ERR_NO_MEM;
    }

    // Charger les certificats si TLS activé
    esp_err_t ret = load_certificates(config->use_tls, config->use_client_cert);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Échec chargement certificats");
        vEventGroupDelete(s_mqtt_event_group);
        s_mqtt_event_group = NULL;
        return ret;
    }

    // Sauvegarder les callbacks
    s_message_callback = config->on_message;
    s_state_callback = config->on_state_change;

    // Configuration du client MQTT
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .uri = config->broker_uri,
            },
        },
        .credentials = {
            .client_id = config->client_id,
            .username = config->username,
            .authentication = {
                .password = config->password,
            },
        },
        .session = {
            .keepalive = MQTT_KEEPALIVE_SEC,
        },
        .buffer = {
            .size = MQTT_BUFFER_SIZE,
        },
    };

    // Configuration TLS si activé
    if (config->use_tls && s_certs_loaded) {
#ifdef CONFIG_MEDBOX_DEV_MODE
        // Mode DEV: certificats embarqués
        mqtt_cfg.broker.verification.certificate = (const char *)ca_crt_start;
        if (config->use_client_cert) {
            mqtt_cfg.credentials.authentication.certificate = (const char *)client_crt_start;
            mqtt_cfg.credentials.authentication.key = (const char *)client_key_start;
        }
#else
        // Mode production: certificats depuis NVS
        mqtt_cfg.broker.verification.certificate = s_ca_cert;
        if (config->use_client_cert) {
            mqtt_cfg.credentials.authentication.certificate = s_client_cert;
            mqtt_cfg.credentials.authentication.key = s_client_key;
        }
#endif
        if (!config->verify_server) {
            mqtt_cfg.broker.verification.skip_cert_common_name_check = true;
        }
    }

    // Créer le client
    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "Échec initialisation client MQTT");
        vEventGroupDelete(s_mqtt_event_group);
        s_mqtt_event_group = NULL;
        return ESP_FAIL;
    }

    // Enregistrer le handler d'événements
    ret = esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID,
                                         mqtt_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Échec enregistrement handler: %s", esp_err_to_name(ret));
        esp_mqtt_client_destroy(s_mqtt_client);
        s_mqtt_client = NULL;
        vEventGroupDelete(s_mqtt_event_group);
        s_mqtt_event_group = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "Client MQTT initialisé (URI: %s, TLS: %s, mTLS: %s)",
             config->broker_uri,
             config->use_tls ? "oui" : "non",
             config->use_client_cert ? "oui" : "non");

    return ESP_OK;
}

esp_err_t mqtt_client_medbox_start(void)
{
    if (s_mqtt_client == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    // Éviter de démarrer plusieurs fois
    if (s_client_started) {
        ESP_LOGD(TAG, "Client MQTT déjà démarré");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Connexion au broker...");
    esp_err_t ret = esp_mqtt_client_start(s_mqtt_client);
    if (ret == ESP_OK) {
        s_client_started = true;
        notify_state_change(MQTT_STATE_CONNECTING);
    } else {
        ESP_LOGE(TAG, "Échec démarrage client MQTT: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t mqtt_client_medbox_stop(void)
{
    if (s_mqtt_client == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_mqtt_client_stop(s_mqtt_client);
    if (ret == ESP_OK) {
        s_client_started = false;
        notify_state_change(MQTT_STATE_DISCONNECTED);
    }
    return ret;
}

int mqtt_client_medbox_publish(const char *topic, const char *data, size_t len, int qos, bool retain)
{
    if (s_mqtt_client == NULL || topic == NULL) {
        return -1;
    }

    int msg_id = esp_mqtt_client_publish(s_mqtt_client, topic, data, len, qos, retain ? 1 : 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Échec publication sur %s", topic);
    } else {
        ESP_LOGD(TAG, "Publié sur %s (msg_id=%d, qos=%d)", topic, msg_id, qos);
    }
    return msg_id;
}

int mqtt_client_medbox_subscribe(const char *topic, int qos)
{
    if (s_mqtt_client == NULL || topic == NULL) {
        return -1;
    }

    int msg_id = esp_mqtt_client_subscribe(s_mqtt_client, topic, qos);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Échec abonnement à %s", topic);
    } else {
        ESP_LOGI(TAG, "Abonnement à %s (msg_id=%d, qos=%d)", topic, msg_id, qos);
    }
    return msg_id;
}

int mqtt_client_medbox_unsubscribe(const char *topic)
{
    if (s_mqtt_client == NULL || topic == NULL) {
        return -1;
    }

    return esp_mqtt_client_unsubscribe(s_mqtt_client, topic);
}

bool mqtt_client_medbox_is_connected(void)
{
    return s_mqtt_state == MQTT_STATE_CONNECTED;
}

mqtt_client_state_t mqtt_client_medbox_get_state(void)
{
    return s_mqtt_state;
}

esp_err_t mqtt_client_medbox_wait_connected(uint32_t timeout_ms)
{
    if (s_mqtt_event_group == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    EventBits_t bits = xEventGroupWaitBits(
        s_mqtt_event_group,
        MQTT_CONNECTED_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(timeout_ms));

    if (bits & MQTT_CONNECTED_BIT) {
        return ESP_OK;
    }

    return ESP_ERR_TIMEOUT;
}
