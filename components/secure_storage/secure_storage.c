#include "secure_storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "error_manager.h"
#include <string.h>

static const char *TAG = "secure_storage";

// Handle NVS pour les certificats
static nvs_handle_t nvs_certs_handle = 0;
static nvs_handle_t nvs_config_handle = 0;
static bool is_initialized = false;

esp_err_t secure_storage_init(void)
{
    if (is_initialized) {
        ESP_LOGW(TAG, "Stockage sécurisé déjà initialisé");
        return ESP_OK;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Effacement et réinitialisation NVS");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Ouvrir le namespace pour les certificats
    ret = nvs_open(NVS_NAMESPACE_CERTS, NVS_READWRITE, &nvs_certs_handle);
    if (ret != ESP_OK) {
        medbox_log_error(MEDBOX_ERR_NVS_INIT_FAILED, TAG, "certs namespace");
        return ret;
    }

    // Ouvrir le namespace pour la configuration
    ret = nvs_open(NVS_NAMESPACE_CONFIG, NVS_READWRITE, &nvs_config_handle);
    if (ret != ESP_OK) {
        medbox_log_error(MEDBOX_ERR_NVS_INIT_FAILED, TAG, "config namespace");
        nvs_close(nvs_certs_handle);
        return ret;
    }

    is_initialized = true;
    ESP_LOGI(TAG, "Stockage sécurisé initialisé");
    return ESP_OK;
}

esp_err_t secure_storage_set_ca_cert(const char *cert, size_t len)
{
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (cert == NULL || len == 0 || len > MEDBOX_CERT_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = nvs_set_blob(nvs_certs_handle, NVS_KEY_CA_CERT, cert, len);
    if (ret != ESP_OK) {
        medbox_log_error(MEDBOX_ERR_NVS_WRITE_FAILED, TAG, "CA cert");
        return ret;
    }

    ret = nvs_commit(nvs_certs_handle);
    if (ret != ESP_OK) {
        medbox_log_error(MEDBOX_ERR_NVS_WRITE_FAILED, TAG, "CA cert commit");
        return ret;
    }

    ESP_LOGI(TAG, "CA cert stocké (%d bytes)", len);
    return ESP_OK;
}

esp_err_t secure_storage_get_ca_cert(char *cert, size_t max_len, size_t *out_len)
{
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (cert == NULL || out_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t required_size = 0;
    esp_err_t ret = nvs_get_blob(nvs_certs_handle, NVS_KEY_CA_CERT, NULL, &required_size);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "CA cert non trouvé");
        }
        return ret;
    }

    if (required_size > max_len) {
        medbox_log_error(MEDBOX_ERR_SYS_NO_MEMORY, TAG, "CA cert buffer");
        return ESP_ERR_NO_MEM;
    }

    ret = nvs_get_blob(nvs_certs_handle, NVS_KEY_CA_CERT, cert, &required_size);
    if (ret != ESP_OK) {
        return ret;
    }

    *out_len = required_size;
    return ESP_OK;
}

esp_err_t secure_storage_set_client_cert(const char *cert, size_t len)
{
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (cert == NULL || len == 0 || len > MEDBOX_CERT_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = nvs_set_blob(nvs_certs_handle, NVS_KEY_CLIENT_CERT, cert, len);
    if (ret != ESP_OK) {
        medbox_log_error(MEDBOX_ERR_NVS_WRITE_FAILED, TAG, "client cert");
        return ret;
    }

    ret = nvs_commit(nvs_certs_handle);
    if (ret != ESP_OK) {
        medbox_log_error(MEDBOX_ERR_NVS_WRITE_FAILED, TAG, "client cert commit");
        return ret;
    }

    ESP_LOGI(TAG, "Client cert stocké (%d bytes)", len);
    return ESP_OK;
}

esp_err_t secure_storage_get_client_cert(char *cert, size_t max_len, size_t *out_len)
{
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (cert == NULL || out_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t required_size = 0;
    esp_err_t ret = nvs_get_blob(nvs_certs_handle, NVS_KEY_CLIENT_CERT, NULL, &required_size);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Client cert non trouvé");
        }
        return ret;
    }

    if (required_size > max_len) {
        medbox_log_error(MEDBOX_ERR_SYS_NO_MEMORY, TAG, "client cert buffer");
        return ESP_ERR_NO_MEM;
    }

    ret = nvs_get_blob(nvs_certs_handle, NVS_KEY_CLIENT_CERT, cert, &required_size);
    if (ret != ESP_OK) {
        return ret;
    }

    *out_len = required_size;
    return ESP_OK;
}

esp_err_t secure_storage_set_client_key(const char *key, size_t len)
{
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (key == NULL || len == 0 || len > MEDBOX_PRIVATE_KEY_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = nvs_set_blob(nvs_certs_handle, NVS_KEY_CLIENT_KEY, key, len);
    if (ret != ESP_OK) {
        medbox_log_error(MEDBOX_ERR_NVS_WRITE_FAILED, TAG, "client key");
        return ret;
    }

    ret = nvs_commit(nvs_certs_handle);
    if (ret != ESP_OK) {
        medbox_log_error(MEDBOX_ERR_NVS_WRITE_FAILED, TAG, "client key commit");
        return ret;
    }

    ESP_LOGI(TAG, "Client key stocké (%d bytes)", len);
    return ESP_OK;
}

esp_err_t secure_storage_get_client_key(char *key, size_t max_len, size_t *out_len)
{
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (key == NULL || out_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t required_size = 0;
    esp_err_t ret = nvs_get_blob(nvs_certs_handle, NVS_KEY_CLIENT_KEY, NULL, &required_size);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Client key non trouvée");
        }
        return ret;
    }

    if (required_size > max_len) {
        medbox_log_error(MEDBOX_ERR_SYS_NO_MEMORY, TAG, "client key buffer");
        return ESP_ERR_NO_MEM;
    }

    ret = nvs_get_blob(nvs_certs_handle, NVS_KEY_CLIENT_KEY, key, &required_size);
    if (ret != ESP_OK) {
        return ret;
    }

    *out_len = required_size;
    return ESP_OK;
}

esp_err_t secure_storage_set_medbox_uuid(const char *uuid)
{
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (uuid == NULL || strlen(uuid) == 0 || strlen(uuid) >= MEDBOX_UUID_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = nvs_set_str(nvs_config_handle, NVS_KEY_MEDBOX_UUID, uuid);
    if (ret != ESP_OK) {
        medbox_log_error(MEDBOX_ERR_NVS_WRITE_FAILED, TAG, "UUID");
        return ret;
    }

    ret = nvs_commit(nvs_config_handle);
    if (ret != ESP_OK) {
        medbox_log_error(MEDBOX_ERR_NVS_WRITE_FAILED, TAG, "UUID commit");
        return ret;
    }

    ESP_LOGI(TAG, "UUID Medbox stocké: %s", uuid);
    return ESP_OK;
}

esp_err_t secure_storage_get_medbox_uuid(char *uuid, size_t max_len)
{
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (uuid == NULL || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t required_size = max_len;
    esp_err_t ret = nvs_get_str(nvs_config_handle, NVS_KEY_MEDBOX_UUID, uuid, &required_size);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "UUID non trouvé");
        }
        return ret;
    }

    return ESP_OK;
}

esp_err_t secure_storage_get_all_certs(medbox_tls_certs_t *certs)
{
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (certs == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(certs, 0, sizeof(medbox_tls_certs_t));

    esp_err_t ret = secure_storage_get_ca_cert(certs->ca_cert, MEDBOX_CERT_MAX_LEN, &certs->ca_cert_len);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        return ret;
    }

    ret = secure_storage_get_client_cert(certs->client_cert, MEDBOX_CERT_MAX_LEN, &certs->client_cert_len);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        return ret;
    }

    ret = secure_storage_get_client_key(certs->client_key, MEDBOX_PRIVATE_KEY_MAX_LEN, &certs->client_key_len);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        return ret;
    }

    return ESP_OK;
}

bool secure_storage_certs_provisioned(void)
{
    if (!is_initialized) {
        return false;
    }

    size_t size = 0;

    // Vérifier CA cert
    if (nvs_get_blob(nvs_certs_handle, NVS_KEY_CA_CERT, NULL, &size) != ESP_OK) {
        return false;
    }

    // Vérifier client cert
    if (nvs_get_blob(nvs_certs_handle, NVS_KEY_CLIENT_CERT, NULL, &size) != ESP_OK) {
        return false;
    }

    // Vérifier client key
    if (nvs_get_blob(nvs_certs_handle, NVS_KEY_CLIENT_KEY, NULL, &size) != ESP_OK) {
        return false;
    }

    return true;
}

bool secure_storage_uuid_provisioned(void)
{
    if (!is_initialized) {
        return false;
    }

    size_t size = 0;
    return nvs_get_str(nvs_config_handle, NVS_KEY_MEDBOX_UUID, NULL, &size) == ESP_OK;
}

esp_err_t secure_storage_erase_all(void)
{
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGW(TAG, "Effacement de toutes les données sécurisées!");

    esp_err_t ret = nvs_erase_all(nvs_certs_handle);
    if (ret != ESP_OK) {
        medbox_log_error(MEDBOX_ERR_NVS_WRITE_FAILED, TAG, "erase certs");
        return ret;
    }

    ret = nvs_erase_all(nvs_config_handle);
    if (ret != ESP_OK) {
        medbox_log_error(MEDBOX_ERR_NVS_WRITE_FAILED, TAG, "erase config");
        return ret;
    }

    ret = nvs_commit(nvs_certs_handle);
    if (ret != ESP_OK) return ret;

    ret = nvs_commit(nvs_config_handle);
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "Toutes les données effacées");
    return ESP_OK;
}
