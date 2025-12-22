# Documentation Technique - Medbox Firmware

## Vue d'ensemble

Le firmware Medbox est une application ESP32 basée sur ESP-IDF v5.5 qui communique avec un broker MQTT (EMQX) pour le contrôle et la télémétrie d'une Medbox.

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
                          │ MQTT (port 1883/8883)
                          ▼
              ┌─────────────────────┐
              │    Broker EMQX      │
              │  185.157.245.46     │
              └─────────────────────┘
```

---

## Structure du Projet

```
MedboxFirmware/
├── components/                 # Composants réutilisables
│   ├── secure_storage/         # Gestion NVS (certificats, UUID)
│   ├── wifi_manager/           # Connexion WiFi
│   └── mqtt_client/            # Client MQTT avec TLS
├── main/
│   ├── main.c                  # Point d'entrée, orchestration
│   └── Kconfig.projbuild       # Options de configuration
├── tools/                      # Scripts utilitaires
├── build.bat                   # Script de compilation
├── flash.bat                   # Script de flash (COM5)
├── menuconfig.bat              # Configuration du projet
├── partitions.csv              # Table des partitions
└── sdkconfig.defaults          # Configuration par défaut
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

**Flux de données**:
```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  Provisioning   │───▶│  secure_storage │────▶│   NVS Flash     │
│    (scripts)    │     │      API        │     │   (partition)   │
└─────────────────┘     └─────────────────┘     └─────────────────┘
                               │
                               ▼
                        ┌─────────────────┐
                        │   mqtt_client   │
                        │ (lit les certs) │
                        └─────────────────┘
```

---

### 2. WiFi Manager (`components/wifi_manager/`)

**Rôle**: Gestion de la connexion WiFi avec reconnexion automatique.

**Fichiers**:
- `include/wifi_manager.h` - API publique
- `wifi_manager.c` - Implémentation

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

**Fonctionnement interne**:
1. `wifi_manager_init()` initialise le driver WiFi ESP32
2. `wifi_manager_connect()` démarre la connexion
3. Le handler d'événements gère:
   - `WIFI_EVENT_STA_START` → Lance `esp_wifi_connect()`
   - `WIFI_EVENT_STA_DISCONNECTED` → Retente jusqu'à 10 fois
   - `IP_EVENT_STA_GOT_IP` → Connexion réussie

---

### 3. MQTT Client (`components/mqtt_client/`)

**Rôle**: Communication MQTT avec le broker EMQX, support TLS optionnel.

**Fichiers**:
- `include/mqtt_client_medbox.h` - API publique
- `mqtt_client_medbox.c` - Implémentation

**États MQTT**:
```
DISCONNECTED ──▶ CONNECTING ──▶ CONNECTED
      ▲               │              │
      │               ▼              │
      └─────────── ERROR ◀───────────┘
```

**Configuration**:
```c
typedef struct {
    const char *broker_uri;      // "mqtt://IP:1883" ou "mqtts://IP:8883"
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

**Chargement des certificats (mode TLS)**:
```c
// Dans mqtt_client_medbox_init(), si TLS activé:
1. secure_storage_get_ca_cert()      → Charge le CA
2. secure_storage_get_client_cert()  → Charge le cert client (si mTLS)
3. secure_storage_get_client_key()   → Charge la clé privée (si mTLS)
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

### Flux de démarrage

```
1. app_main()
   │
   ├─▶ secure_storage_init()
   │
   ├─▶ [MODE DEV] strncpy(medbox_uuid, CONFIG_MEDBOX_DEV_UUID)
   │   [MODE PROD] secure_storage_get_medbox_uuid()
   │
   ├─▶ Construire les topics:
   │   - medbox/{uuid}/commands  (subscribe)
   │   - medbox/{uuid}/status    (publish)
   │   - medbox/{uuid}/telemetry (publish)
   │
   ├─▶ wifi_manager_init()
   ├─▶ wifi_manager_connect(SSID, PASSWORD)
   ├─▶ wifi_manager_wait_connected(30s)
   │
   ├─▶ xTaskCreate(mqtt_task, ...)
   └─▶ xTaskCreate(main_task, ...)
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
    "uuid": "medbox-dev-001",
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
| `MEDBOX_WIFI_SSID` | string | "your_ssid" | SSID du réseau |
| `MEDBOX_WIFI_PASSWORD` | string | "your_password" | Mot de passe |

#### MQTT Configuration
| Option | Type | Défaut | Description |
|--------|------|--------|-------------|
| `MEDBOX_MQTT_BROKER_IP` | string | "185.157.245.46" | IP du broker |
| `MEDBOX_MQTT_BROKER_PORT` | int | 1883 | Port (1883=TCP, 8883=TLS) |
| `MEDBOX_MQTT_USE_TLS` | bool | n | Activer TLS |
| `MEDBOX_MQTT_VERIFY_SERVER` | bool | y | Vérifier cert serveur |
| `MEDBOX_MQTT_USE_CLIENT_CERT` | bool | y | Activer mTLS |
| `MEDBOX_MQTT_USERNAME` | string | "" | Username (optionnel) |
| `MEDBOX_MQTT_PASSWORD` | string | "" | Password (optionnel) |
| `MEDBOX_MQTT_TOPIC_BASE` | string | "medbox" | Préfixe des topics |

#### Security Configuration
| Option | Type | Défaut | Description |
|--------|------|--------|-------------|
| `MEDBOX_DEV_MODE` | bool | y | Mode développement |
| `MEDBOX_DEV_UUID` | string | "medbox-dev-001" | UUID de test |
| `MEDBOX_PROVISIONING_MODE` | bool | n | Mode provisioning |

---

## Modes de Fonctionnement

### 1. Mode Développement (actuel)

```
CONFIG_MEDBOX_DEV_MODE=y
```

- UUID hardcodé (`medbox-dev-001`)
- Pas besoin de provisioning
- Pas de TLS (port 1883)
- Pour les tests rapides

### 2. Mode Production

```
CONFIG_MEDBOX_DEV_MODE=n
CONFIG_MEDBOX_MQTT_USE_TLS=y
```

- UUID lu depuis NVS
- Certificats TLS requis dans NVS
- Connexion sécurisée (port 8883)
- mTLS pour authentification

### 3. Mode Provisioning

```
CONFIG_MEDBOX_DEV_MODE=n
CONFIG_MEDBOX_PROVISIONING_MODE=y
```

- Attend l'injection des certificats
- Utilisé lors de la fabrication
- Utiliser `tools/provision_nvs.py`

---

## Partition Flash

Définie dans `partitions.csv`:

```
# Name,    Type, SubType,  Offset,   Size
nvs,       data, nvs,      0x9000,   0x6000   (24 KB)
phy_init,  data, phy,      0xf000,   0x1000   (4 KB)
factory,   app,  factory,  0x10000,  0x1F0000 (1984 KB)
```

```
┌────────────────────────────────────────┐ 0x000000
│            Bootloader (24KB)           │
├────────────────────────────────────────┤ 0x008000
│         Partition Table (4KB)          │
├────────────────────────────────────────┤ 0x009000
│              NVS (24KB)                │ ← Certificats, UUID, WiFi
├────────────────────────────────────────┤ 0x00F000
│           PHY Init (4KB)               │
├────────────────────────────────────────┤ 0x010000
│                                        │
│         Application (1984KB)           │ ← Firmware Medbox
│                                        │
└────────────────────────────────────────┘ 0x200000
```

---

## Flux de Communication MQTT

### Connexion
```
ESP32                                    EMQX Broker
  │                                          │
  │──────── CONNECT (client_id=uuid) ───────▶│
  │                                          │
  │◀─────────── CONNACK ────────────────────│
  │                                          │
  │──── SUBSCRIBE (medbox/uuid/commands) ───▶│
  │                                          │
  │◀─────────── SUBACK ─────────────────────│
```

### Publication Télémétrie (toutes les 30s)
```
ESP32                                    EMQX Broker
  │                                          │
  │── PUBLISH (medbox/uuid/telemetry) ──────▶│
  │   {"uuid":"...","uptime":...}            │
```

### Réception Commande
```
ESP32                                    EMQX Broker
  │                                          │
  │◀── PUBLISH (medbox/uuid/commands) ──────│
  │                                          │
  │          ┌──────────────┐                │
  │          │ on_mqtt_message()             │
  │          │ → mqtt_rx_queue               │
  │          │ → main_task traite            │
  │          └──────────────┘                │
```

---

## Scripts Utilitaires

### build.bat
Double-cliquer pour compiler le projet.

### flash.bat
Double-cliquer pour flasher sur COM5 et ouvrir le moniteur série.

### menuconfig.bat
Double-cliquer pour configurer le projet (WiFi, MQTT, etc.).

### tools/generate_certs.sh
Génère les certificats TLS (CA + client) pour une Medbox.
```bash
./generate_certs.sh medbox-001
# Crée: certs/ca/ca.crt, certs/medbox-001/client.crt, client.key
```

### tools/provision_nvs.py
Injecte les certificats et UUID dans une image NVS.
```bash
python provision_nvs.py --uuid medbox-001 \
    --ca-cert certs/ca/ca.crt \
    --client-cert certs/medbox-001/client.crt \
    --client-key certs/medbox-001/client.key \
    --flash --port COM5
```

---

## Prochaines Étapes

1. **Activer TLS** sur EMQX et le firmware
2. **Implémenter le traitement des commandes** dans `main_task`
3. **Ajouter les capteurs/actionneurs** spécifiques à la Medbox
4. **Provisioning production** avec certificats uniques par appareil
5. **Secure Boot + Flash Encryption** pour la production
