# Medbox Firmware - Codes d'erreur

Ce document liste tous les codes d'erreur du firmware Medbox avec leur signification.

## Format des codes

Les codes d'erreur suivent le format `0xCCNN` :
- **CC** : Categorie (2 chiffres hex)
- **NN** : Numero d'erreur dans la categorie (2 chiffres hex)

## Categories

| Code | Categorie | Description |
|------|-----------|-------------|
| 0x00xx | OK | Succes |
| 0x01xx | SYS | Erreurs systeme |
| 0x02xx | WIFI | Erreurs WiFi |
| 0x03xx | MQTT | Erreurs MQTT |
| 0x04xx | NVS | Erreurs stockage |
| 0x05xx | SEC | Erreurs securite |
| 0x06xx | APP | Erreurs metier |

---

## 0x00xx - Succes

| Code | Nom | Message |
|------|-----|---------|
| 0x0000 | MEDBOX_OK | Succes |

---

## 0x01xx - Erreurs Systeme

| Code | Nom | Message | Cause probable |
|------|-----|---------|----------------|
| 0x0101 | MEDBOX_ERR_SYS_INIT_FAILED | Echec d'initialisation du systeme | Probleme materiel ou configuration |
| 0x0102 | MEDBOX_ERR_SYS_NO_MEMORY | Memoire insuffisante | Heap epuise, reduire les buffers |
| 0x0103 | MEDBOX_ERR_SYS_TASK_CREATE_FAILED | Echec de creation de tache | Memoire insuffisante pour la stack |
| 0x0104 | MEDBOX_ERR_SYS_QUEUE_CREATE_FAILED | Echec de creation de file d'attente | Memoire insuffisante |
| 0x0105 | MEDBOX_ERR_SYS_QUEUE_FULL | File d'attente pleine | Traitement trop lent |
| 0x0106 | MEDBOX_ERR_SYS_INVALID_ARG | Argument invalide | Bug dans le code appelant |
| 0x0107 | MEDBOX_ERR_SYS_INVALID_STATE | Etat invalide | Operation non permise dans l'etat actuel |
| 0x0108 | MEDBOX_ERR_SYS_TIMEOUT | Delai d'attente depasse | Operation trop longue |

---

## 0x02xx - Erreurs WiFi

| Code | Nom | Message | Cause probable |
|------|-----|---------|----------------|
| 0x0201 | MEDBOX_ERR_WIFI_INIT_FAILED | Echec d'initialisation WiFi | Probleme materiel ou NVS corrompu |
| 0x0202 | MEDBOX_ERR_WIFI_ALREADY_INIT | WiFi deja initialise | Double appel a wifi_init() |
| 0x0203 | MEDBOX_ERR_WIFI_NOT_INIT | WiFi non initialise | Appel avant initialisation |
| 0x0204 | MEDBOX_ERR_WIFI_CONNECT_FAILED | Echec de connexion WiFi | SSID/password incorrect ou reseau indisponible |
| 0x0205 | MEDBOX_ERR_WIFI_CONNECT_TIMEOUT | Delai de connexion WiFi depasse | Reseau trop lent ou hors de portee |
| 0x0206 | MEDBOX_ERR_WIFI_DISCONNECTED | WiFi deconnecte | Perte de signal ou routeur redémarre |
| 0x0207 | MEDBOX_ERR_WIFI_SSID_INVALID | SSID invalide ou vide | Configuration manquante |
| 0x0208 | MEDBOX_ERR_WIFI_AUTH_FAILED | Echec d'authentification WiFi | Mot de passe incorrect |
| 0x0209 | MEDBOX_ERR_WIFI_MAX_RETRY | Nombre max de tentatives atteint | Reseau instable, verifier la couverture |

---

## 0x03xx - Erreurs MQTT

| Code | Nom | Message | Cause probable |
|------|-----|---------|----------------|
| 0x0301 | MEDBOX_ERR_MQTT_INIT_FAILED | Echec d'initialisation MQTT | Configuration invalide |
| 0x0302 | MEDBOX_ERR_MQTT_ALREADY_INIT | Client MQTT deja initialise | Double appel |
| 0x0303 | MEDBOX_ERR_MQTT_NOT_INIT | Client MQTT non initialise | Appel avant initialisation |
| 0x0304 | MEDBOX_ERR_MQTT_CONNECT_FAILED | Echec de connexion au broker | Broker hors ligne ou config incorrecte |
| 0x0305 | MEDBOX_ERR_MQTT_CONNECT_TIMEOUT | Delai de connexion MQTT depasse | Broker lent ou reseau congestionne |
| 0x0306 | MEDBOX_ERR_MQTT_DISCONNECTED | Deconnecte du broker MQTT | Perte de connexion reseau |
| 0x0307 | MEDBOX_ERR_MQTT_PUBLISH_FAILED | Echec de publication MQTT | Broker deconnecte ou topic invalide |
| 0x0308 | MEDBOX_ERR_MQTT_SUBSCRIBE_FAILED | Echec d'abonnement MQTT | Topic invalide ou droits insuffisants |
| 0x0309 | MEDBOX_ERR_MQTT_TLS_HANDSHAKE_FAILED | Echec du handshake TLS | Certificat invalide ou expire |
| 0x030A | MEDBOX_ERR_MQTT_BROKER_UNREACHABLE | Broker MQTT inaccessible | Verifier IP/port du broker |
| 0x030B | MEDBOX_ERR_MQTT_INVALID_URI | URI du broker MQTT invalide | Format: mqtt(s)://ip:port |

---

## 0x04xx - Erreurs Stockage/NVS

| Code | Nom | Message | Cause probable |
|------|-----|---------|----------------|
| 0x0401 | MEDBOX_ERR_NVS_INIT_FAILED | Echec d'initialisation NVS | Partition NVS corrompue |
| 0x0402 | MEDBOX_ERR_NVS_READ_FAILED | Echec de lecture NVS | Cle inexistante ou corrompue |
| 0x0403 | MEDBOX_ERR_NVS_WRITE_FAILED | Echec d'ecriture NVS | Partition pleine |
| 0x0404 | MEDBOX_ERR_NVS_NOT_FOUND | Cle non trouvee dans NVS | Donnee non provisionnee |
| 0x0405 | MEDBOX_ERR_NVS_INVALID_DATA | Donnees NVS invalides | Donnees corrompues |
| 0x0410 | MEDBOX_ERR_UUID_NOT_PROVISIONED | UUID non provisionne | Executer le provisioning |
| 0x0411 | MEDBOX_ERR_UUID_INVALID | UUID invalide | Format UUID incorrect |

---

## 0x05xx - Erreurs Securite

| Code | Nom | Message | Cause probable |
|------|-----|---------|----------------|
| 0x0501 | MEDBOX_ERR_CERT_NOT_PROVISIONED | Certificats non provisionnes | Executer le provisioning TLS |
| 0x0502 | MEDBOX_ERR_CERT_CA_LOAD_FAILED | Echec de chargement du certificat CA | CA manquant ou corrompu |
| 0x0503 | MEDBOX_ERR_CERT_CLIENT_LOAD_FAILED | Echec de chargement du certificat client | Cert client manquant |
| 0x0504 | MEDBOX_ERR_CERT_KEY_LOAD_FAILED | Echec de chargement de la cle privee | Cle privee manquante |
| 0x0505 | MEDBOX_ERR_CERT_INVALID | Certificat invalide | Format PEM incorrect |
| 0x0506 | MEDBOX_ERR_CERT_EXPIRED | Certificat expire | Renouveler le certificat |
| 0x0507 | MEDBOX_ERR_TLS_VERIFY_FAILED | Echec de verification TLS | CA ne correspond pas au serveur |

---

## 0x06xx - Erreurs Metier

| Code | Nom | Message | Cause probable |
|------|-----|---------|----------------|
| 0x0601 | MEDBOX_ERR_CMD_UNKNOWN | Commande inconnue | Commande non implementee |
| 0x0602 | MEDBOX_ERR_CMD_INVALID_FORMAT | Format de commande invalide | JSON mal forme |
| 0x0603 | MEDBOX_ERR_CMD_INVALID_PAYLOAD | Payload de commande invalide | Champs manquants ou incorrects |
| 0x0604 | MEDBOX_ERR_CMD_EXECUTION_FAILED | Echec d'execution de la commande | Erreur interne |
| 0x0610 | MEDBOX_ERR_WHEEL_NOT_DETECTED | Roue non detectee | Roue absente ou capteur defaillant |
| 0x0611 | MEDBOX_ERR_WHEEL_MOTOR_FAILED | Echec du moteur de la roue | Moteur bloque ou defaillant |
| 0x0612 | MEDBOX_ERR_WHEEL_POSITION_ERROR | Erreur de position de la roue | Encodeur defaillant |
| 0x0620 | MEDBOX_ERR_DISPENSE_FAILED | Echec de distribution | Medicament bloque |
| 0x0621 | MEDBOX_ERR_DISPENSE_TIMEOUT | Delai de distribution depasse | Mecanisme lent ou bloque |
| 0x0630 | MEDBOX_ERR_SENSOR_FAILED | Echec du capteur | Capteur deconnecte ou defaillant |
| 0x0640 | MEDBOX_ERR_BATTERY_LOW | Batterie faible | Niveau < 20% |
| 0x0641 | MEDBOX_ERR_BATTERY_CRITICAL | Batterie critique | Niveau < 5%, arret imminent |

---

## Utilisation dans le code

### Logger une erreur

```c
#include "error_manager.h"

// Simple
medbox_log_error(MEDBOX_ERR_WIFI_CONNECT_FAILED, "main", NULL);
// Output: E (1234) main: [WIFI-0x0204] Echec de connexion WiFi

// Avec contexte
medbox_log_error(MEDBOX_ERR_WIFI_CONNECT_FAILED, "main", "SSID: MonWiFi");
// Output: E (1234) main: [WIFI-0x0204] Echec de connexion WiFi (SSID: MonWiFi)
```

### Obtenir le message

```c
const char *msg = medbox_err_to_str(MEDBOX_ERR_WIFI_CONNECT_FAILED);
// msg = "Echec de connexion WiFi"
```

### Verifier et retourner

```c
medbox_err_t my_function(void) {
    esp_err_t ret = some_esp_function();
    MEDBOX_CHECK_ESP_ERROR(ret, 0x02, "my_func", "etape 1");

    // Si on arrive ici, pas d'erreur
    return MEDBOX_OK;
}
```

---

## Ajouter un nouveau code d'erreur

1. Ajouter l'enum dans `error_manager.h` (respecter le format 0xCCNN)
2. Ajouter l'entree dans `s_error_table` dans `error_manager.c`
3. Mettre a jour cette documentation
