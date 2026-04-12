# Documentation Technique - Medbox Firmware

## Vue d'ensemble

Le firmware Medbox est une application ESP32 basée sur ESP-IDF v5.5.1 qui communique avec un broker MQTT (EMQX) via TLS pour le contrôle et la télémétrie d'une Medbox.

```
┌─────────────────────────────────────────────────────────────────────┐
│                           ESP32 Medbox                               │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────────┐  │
│  │   WiFi      │  │    MQTT     │  │       Application           │  │
│  │  Manager    │──│   Client    │──│       (main_task)           │  │
│  │  (STA+AP)   │  └─────────────┘  └─────────────────────────────┘  │
│  └─────────────┘        │                       │                   │
│         │               │                       │                   │
│         │         ┌─────┴─────┐         ┌───────┴───────┐           │
│         │         │  Error    │         │  Web Server   │           │
│         │         │  Manager  │         │  (HTTP:80)    │           │
│         │         └───────────┘         └───────────────┘           │
│         │               │                       │                   │
│         └───────────────┴───────────────────────┘                   │
│                         │                                           │
│              ┌──────────┴──────────┐                                │
│              │   Secure Storage    │                                │
│              │       (NVS)         │                                │
│              └─────────────────────┘                                │
└─────────────────────────────────────────────────────────────────────┘
          │                                    │
          │ MQTT over TLS (port 8883)          │ HTTP (port 80)
          ▼                                    ▼
┌─────────────────────┐              ┌─────────────────────┐
│    Broker EMQX      │              │  Interface Web      │
│  mqtt.medbox.       │              │  (Configuration)    │
│  theokaszak.fr      │              │                     │
└─────────────────────┘              └─────────────────────┘
```

---

## Sécurité

Le firmware utilise une architecture de sécurité complète :

| Fonctionnalité | État | Description |
|----------------|------|-------------|
| **Secure Boot V1** | Activé | Seul le firmware signé peut démarrer |
| **Flash Encryption** | Activé (dev) | Contenu de la flash chiffré AES-256 |
| **NVS Encryption** | Activé | Données sensibles chiffrées |
| **TLS 1.2+** | Activé | Communication MQTT chiffrée |
| **mTLS** | Activé | Authentification par certificat client |

---

## Structure du Projet

```
MedboxFirmware/
├── main/                      # Code source principal
│   ├── main.c                 # Point d'entrée, orchestration FreeRTOS
│   └── Kconfig.projbuild      # Options de configuration menuconfig
├── components/                # Composants réutilisables
│   ├── mqtt_client/           # Client MQTT avec TLS/mTLS
│   ├── wifi_manager/          # Gestion WiFi STA+AP avec reconnexion auto
│   ├── web_server/            # Serveur HTTP pour interface de configuration
│   ├── error_manager/         # Gestionnaire d'erreurs centralise
│   └── secure_storage/        # Stockage NVS sécurisé
├── scripts/                   # Scripts de sécurité ESP32
│   ├── check_esp32.py         # Vérifier l'état sécurité d'un ESP32
│   ├── generate_key.py        # Générer la clé de chiffrement
│   ├── setup_esp32.py         # Configurer un nouvel ESP32 (burn eFuses)
│   ├── encrypted_flash.py     # Flash pré-chiffré (illimité)
│   ├── flash.bat              # Build + Flash en une commande
│   └── setup.bat              # Setup sécurité ESP32
├── tools/                     # Scripts pour certificats TLS
│   ├── generate_certs.sh      # Génération certificats CA + client
│   ├── setup_emqx_tls.sh      # Configuration TLS pour EMQX
│   └── provision_nvs.py       # Provisioning certificats dans NVS
├── keys/                      # Clés de sécurité (NON VERSIONNÉ)
│   ├── secure_boot_signing_key.pem
│   └── flash_encryption_key.bin
├── certs/                     # Certificats TLS (NON VERSIONNÉ)
├── menuconfig.bat             # Configuration du projet
├── partitions.csv             # Table des partitions
└── sdkconfig.defaults         # Configuration par défaut
```

---

## Composants

### 1. Secure Storage (`components/secure_storage/`)

**Rôle**: Stockage sécurisé des données sensibles dans la partition NVS (Non-Volatile Storage).

**Fichiers**:
- `include/secure_storage.h` - API publique
- `secure_storage.c` - Implémentation

**Données stockées**:
| Namespace | Clé | Description |
|-----------|-----|-------------|
| `certs` | `ca_cert` | Certificat CA du broker (pour TLS) |
| `certs` | `client_cert` | Certificat client (pour mTLS) |
| `certs` | `client_key` | Clé privée client (pour mTLS) |
| `config` | `medbox_uuid` | Identifiant unique de la Medbox |

**API principale**:
```c
// Initialisation
esp_err_t secure_storage_init(void);

// UUID
esp_err_t secure_storage_set_medbox_uuid(const char *uuid);
esp_err_t secure_storage_get_medbox_uuid(char *uuid, size_t max_len);
bool secure_storage_uuid_provisioned(void);

// Certificats
esp_err_t secure_storage_set_ca_cert(const char *cert, size_t len);
esp_err_t secure_storage_get_ca_cert(char *cert, size_t max_len, size_t *out_len);
esp_err_t secure_storage_set_client_cert(const char *cert, size_t len);
esp_err_t secure_storage_set_client_key(const char *key, size_t len);
bool secure_storage_certs_provisioned(void);
```

---

### 2. WiFi Manager (`components/wifi_manager/`)

**Rôle**: Gestion de la connexion WiFi (STA) et point d'accès (AP) avec reconnexion automatique.

**Mode AP+STA**:
- **STA (Station)**: Connexion au réseau WiFi configuré
- **AP (Access Point)**: Point d'accès `Medbox-XXXX` pour la configuration
- L'AP s'active uniquement quand le WiFi STA est déconnecté
- L'AP se désactive automatiquement quand le WiFi STA se connecte

**États WiFi**:
```
DISCONNECTED ──▶ CONNECTING ──▶ CONNECTED
      ▲               │              │
      │               ▼              │
      │           FAILED ◀───────────┘
      │               │
      └───────────────┘
      (AP actif)         (AP inactif)
```

**API principale**:
```c
// Initialisation et connexion
esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_connect(const char *ssid, const char *password);
esp_err_t wifi_manager_disconnect(void);

// État STA
bool wifi_manager_is_connected(void);
wifi_state_t wifi_manager_get_state(void);
esp_err_t wifi_manager_wait_connected(uint32_t timeout_ms);
esp_err_t wifi_manager_get_ip(char *ip_str);
int8_t wifi_manager_get_rssi(void);

// Mode AP (Access Point)
esp_err_t wifi_manager_start_ap(const char *uuid_suffix);
esp_err_t wifi_manager_stop_ap(void);
bool wifi_manager_is_ap_active(void);
esp_err_t wifi_manager_get_ap_ip(char *ip_str);
esp_err_t wifi_manager_get_ap_ssid(char *ssid_str);

// Scan WiFi
esp_err_t wifi_manager_scan(wifi_scan_result_t *results, uint16_t max, uint16_t *num);

// Callback
void wifi_manager_set_callback(wifi_event_callback_t callback);
```

---

### 3. MQTT Client (`components/mqtt_client/`)

**Rôle**: Communication MQTT avec le broker EMQX via TLS/mTLS.

**Configuration**:
```c
typedef struct {
    const char *broker_uri;      // "mqtts://IP:8883"
    const char *client_id;       // UUID de la Medbox
    const char *username;        // Optionnel
    const char *password;        // Optionnel
    bool use_tls;                // Activer TLS
    bool verify_server;          // Vérifier cert serveur
    bool use_client_cert;        // Authentification mTLS
    mqtt_message_callback_t on_message;
    mqtt_state_callback_t on_state_change;
} mqtt_client_config_t;
```

**API principale**:
```c
// Initialisation
esp_err_t mqtt_client_medbox_init(const mqtt_client_config_t *config);
esp_err_t mqtt_client_medbox_start(void);
esp_err_t mqtt_client_medbox_stop(void);

// Publication/Abonnement
int mqtt_client_medbox_publish(const char *topic, const char *data,
                                size_t len, int qos, bool retain);
int mqtt_client_medbox_subscribe(const char *topic, int qos);
int mqtt_client_medbox_unsubscribe(const char *topic);

// État
bool mqtt_client_medbox_is_connected(void);
mqtt_client_state_t mqtt_client_medbox_get_state(void);
```

---

### 4. Web Server (`components/web_server/`)

**Rôle**: Interface web de configuration et monitoring accessible via HTTP.

**Architecture**:
```
components/web_server/
├── include/
│   └── web_server.h       # API publique
├── web_server.c           # Serveur HTTP principal
├── web_pages.c            # Pages HTML/CSS/JS embarquées
├── web_api.c              # REST API JSON
└── CMakeLists.txt
```

**Pages Web**:
| Page | URL | Description |
|------|-----|-------------|
| Status | `/` | Uptime, WiFi RSSI, version firmware, UUID |
| WiFi Config | `/wifi` | Scanner réseaux, configurer SSID/password |
| Erreurs | `/errors` | Historique des 20 dernières erreurs |

**REST API**:
| Endpoint | Méthode | Description |
|----------|---------|-------------|
| `/api/status` | GET | Informations système (JSON) |
| `/api/wifi/scan` | GET | Liste réseaux WiFi disponibles |
| `/api/wifi/config` | GET | Configuration WiFi actuelle |
| `/api/wifi/config` | POST | Configurer nouveau WiFi |
| `/api/errors` | GET | Liste erreurs récentes |

**API principale**:
```c
esp_err_t web_server_start(void);
esp_err_t web_server_stop(void);
bool web_server_is_running(void);
```

**Accès**:
- Via AP: `http://192.168.4.1`
- Via STA: `http://<IP_attribuée>`

---

### 5. Error Manager (`components/error_manager/`)

**Rôle**: Gestion centralisée des erreurs avec codes hexadécimaux et historique.

**Format des codes**: `0xCCNN`
- CC = Catégorie (01=SYS, 02=WIFI, 03=MQTT, 04=NVS, 05=SEC, 06=APP, 07=HTTP)
- NN = Numéro d'erreur

**Historique**: Buffer circulaire de 20 erreurs avec timestamp.

**API principale**:
```c
// Initialisation
medbox_err_t error_manager_init(void);

// Logging
void medbox_log_error(medbox_err_t err, const char *tag, const char *context);

// Conversion
const char* medbox_err_to_str(medbox_err_t err);
const char* medbox_err_get_category(medbox_err_t err);

// Historique
medbox_err_t error_manager_get_history(medbox_error_history_entry_t *entries,
                                       uint16_t max_entries, uint16_t *num_entries);
medbox_err_t error_manager_clear_history(void);
```

Voir [ERRORS.md](ERRORS.md) pour la liste complète des codes.

---

## Application Principale (`main/main.c`)

### Architecture FreeRTOS

```
┌─────────────────────────────────────────────────────────────┐
│                        app_main()                           │
│  1. Initialise secure_storage + error_manager               │
│  2. Charge UUID (dev ou NVS)                                │
│  3. Initialise wifi_manager                                 │
│  4. Connecte au WiFi (timeout 30s)                          │
│  5. Si WiFi OK: continue / Si echec: demarre AP             │
│  6. Demarre web_server (HTTP:80)                            │
│  7. Crée les tâches FreeRTOS                                │
└─────────────────────────────────────────────────────────────┘
                    │
    ┌───────────────┼───────────────┐
    ▼               ▼               ▼
┌─────────────┐ ┌─────────────┐ ┌─────────────┐
│ mqtt_task   │ │ main_task   │ │ httpd_task  │
│ Priorité: 5 │ │ Priorité: 4 │ │ (interne)   │
│ Stack: 8KB  │ │ Stack: 4KB  │ │ Stack: 8KB  │
├─────────────┤ ├─────────────┤ ├─────────────┤
│ Init MQTT   │ │ Messages    │ │ Requetes    │
│ Connexion   │ │ Télémétrie  │ │ HTTP        │
│ Reconnexion │ │             │ │ REST API    │
└─────────────┘ └─────────────┘ └─────────────┘
       │               │
       └───────┬───────┘
               ▼
      ┌─────────────────┐
      │  mqtt_rx_queue  │
      │  (10 messages)  │
      └─────────────────┘
```

### Topics MQTT

| Topic | Direction | QoS | Description |
|-------|-----------|-----|-------------|
| `medbox/box/{uuid}/cmd/+` | Subscribe | 1 | Commandes reçues du serveur |
| `medbox/box/{uuid}/telemetry` | Publish | 1 | Télémétrie périodique (60s) |

**Format de la télémétrie** (JSON):
```json
{
    "firmware_version": "1.0.0",
    "last_sync": "2026-04-03T08:00:00Z",
    "current_date": "2026-04-03T20:55:00Z"
}
```

---

## Configuration (Kconfig)

Accessible via `idf.py menuconfig` ou `menuconfig.bat`.

### Menu "Medbox Configuration"

#### WiFi Configuration
| Option | Type | Défaut | Description |
|--------|------|--------|-------------|
| `MEDBOX_WIFI_SSID` | string | "" | SSID du réseau |
| `MEDBOX_WIFI_PASSWORD` | string | "" | Mot de passe |

#### MQTT Configuration
| Option | Type | Défaut | Description |
|--------|------|--------|-------------|
| `MEDBOX_MQTT_BROKER_IP` | string | "mqtt.medbox.theokaszak.fr" | Hôte du broker |
| `MEDBOX_MQTT_BROKER_PORT` | int | 8883 | Port TLS |
| `MEDBOX_MQTT_USE_TLS` | bool | y | Activer TLS |
| `MEDBOX_MQTT_VERIFY_SERVER` | bool | y | Vérifier cert serveur |
| `MEDBOX_MQTT_USE_CLIENT_CERT` | bool | y | Activer mTLS |
| `MEDBOX_MQTT_TOPIC_BASE` | string | "medbox/box" | Préfixe des topics |

#### Security Configuration
| Option | Type | Défaut | Description |
|--------|------|--------|-------------|
| `MEDBOX_DEV_MODE` | bool | y | Mode développement |
| `MEDBOX_DEV_UUID` | string | "b9c8d7e6-..." | UUID de dev |
| `MEDBOX_PROVISIONING_MODE` | bool | n | Mode provisioning |

---

## Partition Flash

Définie dans `partitions.csv` (offset 0x10000 pour bootloader étendu) :

```
# Name,       Type, SubType,  Offset,   Size,     Flags
nvs,          data, nvs,      0x11000,  0x4000,
nvs_keys,     data, nvs_keys, 0x15000,  0x1000,   encrypted
phy_init,     data, phy,      0x16000,  0x1000,
factory,      app,  factory,  0x20000,  0x1E0000, encrypted
```

```
┌────────────────────────────────────────┐ 0x000000
│         Bootloader (64KB)              │ ← Signé (Secure Boot)
├────────────────────────────────────────┤ 0x010000
│         Partition Table (4KB)          │
├────────────────────────────────────────┤ 0x011000
│              NVS (16KB)                │ ← Certificats, UUID
├────────────────────────────────────────┤ 0x015000
│           NVS Keys (4KB)               │ ← Clés NVS (encrypted)
├────────────────────────────────────────┤ 0x016000
│           PHY Init (4KB)               │
├────────────────────────────────────────┤ 0x020000
│                                        │
│      Application (~1.9MB)              │ ← Firmware (encrypted)
│                                        │
└────────────────────────────────────────┘ 0x200000
```

---

## Scripts

### Scripts de sécurité (`scripts/`)

| Script | Usage |
|--------|-------|
| `check_esp32.py` | Vérifier l'état sécurité d'un ESP32 |
| `generate_key.py` | Générer la clé de chiffrement flash |
| `setup_esp32.py` | Configurer un nouvel ESP32 (burn eFuses) |
| `encrypted_flash.py` | Flash pré-chiffré (illimité) |
| `flash.bat` | Build + Flash en une commande |

### Scripts TLS (`tools/`)

| Script | Usage |
|--------|-------|
| `generate_certs.sh` | Génère CA + certificats client |
| `setup_emqx_tls.sh` | Configure TLS sur EMQX |
| `provision_nvs.py` | Injecte certificats dans NVS |

---

## Workflow de développement

```
1. Configurer WiFi
   └── menuconfig.bat

2. Build
   └── idf.py build

3. Setup ESP32 (une seule fois par chip)
   └── python scripts/setup_esp32.py COM5

4. Flash
   └── python scripts/encrypted_flash.py COM5
   └── ou: scripts\flash.bat COM5

5. Monitor
   └── idf.py -p COM5 monitor
```

---

## Modes de Fonctionnement

### Mode Développement (actuel)

```
CONFIG_MEDBOX_DEV_MODE=y
CONFIG_MEDBOX_MQTT_USE_TLS=y
CONFIG_SECURE_FLASH_ENCRYPTION_MODE_DEVELOPMENT=y
```

- UUID hardcodé dans sdkconfig
- TLS activé (port 8883)
- Flash Encryption en mode développement (flashs illimités avec script)

### Mode Production

```
CONFIG_MEDBOX_DEV_MODE=n
CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE=y
```

- UUID lu depuis NVS
- Certificats TLS requis dans NVS
- Flash Encryption verrouillé
- Secure Boot permanent
