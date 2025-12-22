# Medbox Firmware - Security Guide

Ce guide explique comment configurer et utiliser le Secure Boot et le Flash Encryption.

## Architecture de Sécurité

| Fonctionnalité | Protection | Description |
|----------------|------------|-------------|
| **Secure Boot V2** | Écriture | Seul le firmware signé peut démarrer |
| **Flash Encryption** | Lecture | Le contenu de la flash est chiffré |
| **NVS Encryption** | Données | Les données sensibles sont chiffrées |

## Fichiers Clés

```
keys/
├── secure_boot_signing_key.pem   # Clé de signature (ECDSA 256-bit)
└── flash_encryption_key.bin      # Clé de chiffrement flash (256-bit)
```

> **IMPORTANT**: Sauvegardez ces fichiers en lieu sûr ! Sans eux, vous ne pourrez plus flasher vos ESP32.

## Scripts Disponibles

| Script | Usage |
|--------|-------|
| `scripts/generate_key.py` | Générer une nouvelle clé de chiffrement |
| `scripts/setup_esp32.py COM5` | Configurer un NOUVEL ESP32 |
| `scripts/encrypted_flash.py COM5` | Flasher avec chiffrement (illimité) |
| `scripts/check_esp32.py COM5` | Vérifier l'état d'un ESP32 |
| `scripts/flash.bat COM5` | Build + Flash en une commande |

---

## Configuration d'un NOUVEL ESP32

### Étape 1: Générer la clé de chiffrement (une seule fois)

```bash
python scripts/generate_key.py
```

Cela crée `keys/flash_encryption_key.bin`.

### Étape 2: Configurer l'ESP32

```bash
python scripts/setup_esp32.py COM5
```

Cela grave dans les eFuses :
- La clé de chiffrement
- FLASH_CRYPT_CNT = 1 (active le chiffrement)
- FLASH_CRYPT_CONFIG = 0xF

### Étape 3: Build et Flash

```bash
idf.py build
python scripts/encrypted_flash.py COM5
```

Ou en une commande :
```bash
scripts\flash.bat COM5
```

### Étape 4: Monitor

```bash
idf.py -p COM5 monitor
```

---

## Workflow de Développement

### Avec TA propre clé (RECOMMANDÉ)

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   idf.py build  │────▶│ encrypted_flash │────▶│   ESP32 reçoit  │
│                 │     │    (chiffre)    │     │  binaire chiffré│
└─────────────────┘     └─────────────────┘     └─────────────────┘
        │                       │                       │
        │                       │                       │
    Firmware              Ta clé .bin             Pas de limite !
     signé                                       FLASH_CRYPT_CNT
                                                  non consommé
```

**Avantages :**
- Flashs illimités
- Clé sous ton contrôle
- Même workflow pour DEV et PROD

### Avec clé auto-générée (ANCIEN)

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   idf.py build  │────▶│  idf.py flash   │────▶│  ESP32 chiffre  │
│                 │     │   (plaintext)   │     │   lui-même      │
└─────────────────┘     └─────────────────┘     └─────────────────┘
        │                       │                       │
        │                       │                       │
    Firmware              Binaire clair          FLASH_CRYPT_CNT
     signé                                        +1 à chaque flash
                                                  (max 4 flashs)
```

**Inconvénients :**
- Limité à 4 flashs
- Clé non récupérable

---

## Vérifier l'état d'un ESP32

```bash
python scripts/check_esp32.py COM5
```

Résultat exemple :
```
  Security Status:
  ----------------------------------------
  Flash Encryption: ENABLED
  FLASH_CRYPT_CNT:  1 (0b0000001)
  Plaintext flashes remaining: 3

  Secure Boot V1:   ENABLED
  Secure Boot V2:   NOT ENABLED

  Key Read Protect: YES (keys cannot be extracted)
```

---

## Mode Production

Pour passer en production, modifiez `sdkconfig.defaults` :

```ini
# Changer DEVELOPMENT en RELEASE
CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE=y

# Désactiver les logs de debug
CONFIG_LOG_DEFAULT_LEVEL_WARN=y
```

Et protégez les clés en lecture :
```bash
python -m espefuse --port COM5 burn_efuse RD_DIS 3
```

---

## Dépannage

### "invalid header" au boot
La flash contient des données non chiffrées mais l'ESP32 attend du chiffré.
Solution : Reflasher tout avec `encrypted_flash.py`.

### "secure boot verification failed"
Le firmware n'est pas signé avec la bonne clé.
Solution : Vérifiez que `keys/secure_boot_signing_key.pem` est correct.

### "FLASH_CRYPT_CNT exhausted"
Plus de flashs plaintext disponibles.
Solution : Utilisez `encrypted_flash.py` ou un nouvel ESP32.

---

## Références

- [ESP-IDF Flash Encryption](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/flash-encryption.html)
- [ESP-IDF Secure Boot V2](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/secure-boot-v2.html)
