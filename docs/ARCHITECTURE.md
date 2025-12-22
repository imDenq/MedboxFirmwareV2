# Documentation Technique - Medbox Firmware

## Vue d'ensemble

Le firmware Medbox est une application ESP32 basée sur ESP-IDF v5.5.1 qui communique avec un broker MQTT (EMQX) via TLS pour le contrôle et la télémétrie d'une Medbox.

```
┌─────────────────────────────────────────────────────────────────┐
│                         ESP32 Medbox                            │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐  │
│  │   WiFi      │  │    MQTT     │  │    Application          │  │
│  │  Manager    │──│   Client    │──│    (main_task)          │  │
│  └─────────────┘  └─────────────┘  └─────────────────────────┘  │
│         │               │                     │                 │
│         └───────────────┴─────────────────────┘                 │
│                         │                                       │
│              ┌──────────┴──────────┐                            │
│              │   Secure Storage    │                            │
│              │       (NVS)         │                            │
│              └─────────────────────┘                            │
└─────────────────────────────────────────────────────────────────┘
                          │
                          │ MQTT over TLS (port 8883)
                          ▼
              ┌─────────────────────┐
              │    Broker EMQX      │
              │  185.157.245.46     │
              └─────────────────────┘
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
│   ├── wifi_manager/          # Gestion WiFi avec reconnexion auto
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

**Rôle**: Gestion de la connexion WiFi avec reconnexion automatique.

**États WiFi**:
```
DISCONNECTED ──▶ CONNECTING ──▶ CONNECTED
      ▲               │              │
      │               ▼              │
      │           FAILED ◀───────────┘
      │               │         (après 10 tentatives)
      └───────────────┘
```

**API principale**:
```c
// Initialisation et connexion
esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_connect(const char *ssid, const char *password);
esp_err_t wifi_manager_disconnect(void);

// État
bool wifi_manager_is_connected(void);
wifi_state_t wifi_manager_get_state(void);
esp_err_t wifi_manager_wait_connected(uint32_t timeout_ms);

// Informations
esp_err_t wifi_manager_get_ip(char *ip_str);
int8_t wifi_manager_get_rssi(void);

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

## Application Principale (`main/main.c`)

### Architecture FreeRTOS

```
┌─────────────────────────────────────────────────────────────┐
│                        app_main()                           │
│  1. Initialise secure_storage                               │
│  2. Charge UUID (dev ou NVS)                                │
│  3. Initialise wifi_manager                                 │
│  4. Connecte au WiFi                                        │
│  5. Crée les tâches FreeRTOS                                │
└─────────────────────────────────────────────────────────────┘
                    │
         ┌─────────┴─────────┐
         ▼                   ▼
┌─────────────────┐  ┌─────────────────┐
│   mqtt_task     │  │   main_task     │
│   Priorité: 5   │  │   Priorité: 4   │
│   Stack: 8KB    │  │   Stack: 4KB    │
├─────────────────┤  ├─────────────────┤
│ - Init MQTT     │  │ - Traite les    │
│ - Gère connexion│  │   messages MQTT │
│ - Reconnexion   │  │ - Envoie        │
│   automatique   │  │   télémétrie    │
└─────────────────┘  └─────────────────┘
         │                   │
         └─────────┬─────────┘
                   ▼
         ┌─────────────────┐
         │  mqtt_rx_queue  │
         │  (10 messages)  │
         └─────────────────┘
```

### Topics MQTT

| Topic | Direction | QoS | Description |
|-------|-----------|-----|-------------|
| `medbox/{uuid}/commands` | Subscribe | 1 | Commandes reçues du serveur |
| `medbox/{uuid}/status` | Publish | 1 | État de la Medbox |
| `medbox/{uuid}/telemetry` | Publish | 0 | Données périodiques (30s) |

**Format de la télémétrie** (JSON):
```json
{
    "uuid": "945c1ffc-3801-4af1-bb51-bb1e1710abb1",
    "uptime": 307,
    "heap_free": 245678,
    "rssi": -66
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
| `MEDBOX_MQTT_BROKER_IP` | string | "185.157.245.46" | IP du broker |
| `MEDBOX_MQTT_BROKER_PORT` | int | 8883 | Port TLS |
| `MEDBOX_MQTT_USE_TLS` | bool | y | Activer TLS |
| `MEDBOX_MQTT_VERIFY_SERVER` | bool | y | Vérifier cert serveur |
| `MEDBOX_MQTT_USE_CLIENT_CERT` | bool | y | Activer mTLS |
| `MEDBOX_MQTT_TOPIC_BASE` | string | "medbox/box" | Préfixe des topics |

#### Security Configuration
| Option | Type | Défaut | Description |
|--------|------|--------|-------------|
| `MEDBOX_DEV_MODE` | bool | y | Mode développement |
| `MEDBOX_DEV_UUID` | string | "945c1ffc-..." | UUID de dev |
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
