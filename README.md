# Medbox Firmware

Firmware ESP32 sécurisé pour le projet Medbox avec communication MQTT/TLS vers EMQX.

## Fonctionnalités

### Sécurité
- **Secure Boot V1** - Seul le firmware signé peut démarrer
- **Flash Encryption** - Contenu de la flash chiffré (mode développement)
- **NVS Encryption** - Données sensibles chiffrées
- **MQTT over TLS** - Communication sécurisée avec le broker EMQX
- **mTLS** - Authentification mutuelle avec certificats client

### Connectivité
- **WiFi STA+AP** - Mode station + point d'accès simultané
- **Point d'accès** - `Medbox-XXXX` actif quand WiFi déconnecté
- **Reconnexion auto** - Reconnexion WiFi et MQTT automatique

### Interface Web
- **Serveur HTTP** - Interface de configuration sur port 80
- **Design moderne** - Interface responsive style Apple
- **REST API** - API JSON pour intégration
- **Monitoring** - Status, RSSI, uptime, erreurs en temps réel

### Gestion d'erreurs
- **Codes hexadécimaux** - Format `0xCCNN` (ex: `0x0301`)
- **Historique** - 20 dernières erreurs avec timestamp
- **Catégories** - SYS, WIFI, MQTT, NVS, SEC, APP, HTTP

## Prérequis

- **ESP-IDF v5.5.1**
- Python 3.8+
- OpenSSL (pour la génération des certificats)

## Structure du projet

```
MedboxFirmware/
├── main/                    # Code source principal
│   ├── main.c
│   └── Kconfig.projbuild    # Options menuconfig
├── components/
│   ├── mqtt_client/         # Client MQTT avec TLS/mTLS
│   ├── wifi_manager/        # Gestion WiFi STA+AP
│   ├── web_server/          # Serveur HTTP + interface web
│   ├── error_manager/       # Gestion erreurs centralisée
│   └── secure_storage/      # Stockage NVS sécurisé
├── scripts/                 # Scripts de sécurité ESP32
│   ├── check_esp32.py       # Vérifier l'état sécurité d'un ESP32
│   ├── generate_key.py      # Générer la clé de chiffrement
│   ├── setup_esp32.py       # Configurer un nouvel ESP32 (burn eFuses)
│   ├── encrypted_flash.py   # Flash pré-chiffré (illimité)
│   ├── flash.bat            # Build + Flash en une commande
│   └── setup.bat            # Setup sécurité ESP32
├── tools/                   # Scripts pour certificats TLS
│   ├── generate_certs.sh    # Génération certificats CA + client
│   ├── setup_emqx_tls.sh    # Configuration TLS pour EMQX
│   └── provision_nvs.py     # Provisioning certificats dans NVS
├── keys/                    # Clés de sécurité (NE PAS COMMITER)
│   ├── secure_boot_signing_key.pem
│   └── flash_encryption_key.bin
├── certs/                   # Certificats TLS MQTT
│   ├── ca.crt
│   ├── client.crt
│   └── client.key
├── menuconfig.bat           # Raccourci configuration
├── partitions.csv           # Table de partitions
├── sdkconfig.defaults       # Configuration par défaut
└── docs/
    ├── ARCHITECTURE.md      # Architecture du projet
    ├── ERRORS.md            # Codes d'erreur (0x0000 - 0x07FF)
    └── SECURITY.md          # Guide sécurité détaillé
```

## Quick Start (Windows)

### 1. Cloner et configurer l'environnement

```batch
git clone <repo-url>
cd MedboxFirmware

:: Sourcer ESP-IDF
call %USERPROFILE%\esp\v5.5.1\esp-idf\export.bat
```

### 2. Configurer le WiFi

```batch
:: Ouvrir le menu de configuration
menuconfig.bat
```

Dans `Medbox Configuration > WiFi Configuration`:
- WiFi SSID
- WiFi Password

### 3. Build

```batch
:: Build du firmware (signe automatiquement avec la clé)
idf.py build
```

### 4. Setup d'un nouvel ESP32 (une seule fois)

```batch
:: Vérifier l'état actuel de l'ESP32
python scripts/check_esp32.py COM5

:: Si ESP32 vierge, configurer la sécurité
python scripts/setup_esp32.py COM5
```

### 5. Flash

```batch
:: Flash avec chiffrement (illimité)
python scripts/encrypted_flash.py COM5

:: Ou build + flash en une commande
scripts\flash.bat COM5
```

### 6. Monitor

```batch
idf.py -p COM5 monitor
```

## Commandes utiles

| Action | Commande |
|--------|----------|
| Build | `idf.py build` |
| Configuration | `idf.py menuconfig` ou `menuconfig.bat` |
| Flash chiffré | `python scripts/encrypted_flash.py COM5` |
| Flash + Build | `scripts\flash.bat COM5` |
| Monitor | `idf.py -p COM5 monitor` |
| État sécurité ESP32 | `python scripts/check_esp32.py COM5` |
| État eFuses | `espefuse.py -p COM5 summary` |

## Topics MQTT

```
medbox/{uuid}/status      # État de la Medbox (publish)
medbox/{uuid}/telemetry   # Télémétrie périodique (publish)
medbox/{uuid}/commands    # Commandes reçues (subscribe)
```

## Interface Web

### Accès

| Mode | URL | Quand |
|------|-----|-------|
| Point d'accès | `http://192.168.4.1` | WiFi déconnecté, réseau `Medbox-XXXX` |
| Station | `http://<IP>` | WiFi connecté |

### Pages

- `/` - Status système (uptime, mémoire, WiFi, UUID)
- `/wifi` - Configuration WiFi (scan + connexion)
- `/errors` - Historique des erreurs (codes hex)

### API REST

| Endpoint | Méthode | Description |
|----------|---------|-------------|
| `/api/status` | GET | Informations système JSON |
| `/api/wifi/scan` | GET | Liste réseaux disponibles |
| `/api/wifi/config` | GET/POST | Config WiFi |
| `/api/errors` | GET | Historique erreurs |

## Sécurité

Voir [SECURITY.md](docs/SECURITY.md) pour le guide complet.

### Résumé

| Fonctionnalité | État | Description |
|----------------|------|-------------|
| Secure Boot V1 | Activé | Firmware signé avec ECDSA |
| Flash Encryption | Activé (dev) | Chiffrement AES-256 |
| NVS Encryption | Activé | Données sensibles protégées |
| TLS 1.2+ | Activé | Communication MQTT chiffrée |
| mTLS | Activé | Certificat client requis |

### Fichiers sensibles (ne pas commiter)

```
keys/                    # Clés de signature et chiffrement
certs/                   # Certificats TLS
sdkconfig                # Peut contenir des secrets WiFi
```

## Configuration MQTT

Le broker EMQX est configuré dans `sdkconfig.defaults`:

```
Broker: 185.157.245.46
Port: 8883 (TLS)
Topic base: medbox/box
```

## Licence

Propriétaire Medbox - Tous droits réservés.
