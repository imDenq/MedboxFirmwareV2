# Medbox Firmware - Security Guide

Ce guide explique comment configurer et utiliser le Secure Boot et le Flash Encryption.

## Architecture de Sécurité

| Fonctionnalité | Protection | Description |
|----------------|------------|-------------|
| **Secure Boot V1** | Intégrité | Seul le firmware signé peut démarrer |
| **Flash Encryption** | Confidentialité | Le contenu de la flash est chiffré AES-256 |
| **NVS Encryption** | Données | Les données sensibles sont chiffrées |
| **TLS 1.2+** | Communication | MQTT chiffré avec le broker |
| **mTLS** | Authentification | Certificat client requis |

---

## Sécurité de l'Interface Web

### Accès au Serveur HTTP

| Mode | URL | Sécurité |
|------|-----|----------|
| AP (Point d'accès) | `http://192.168.4.1` | Réseau isolé, mot de passe WPA2 |
| STA (Connecté) | `http://<IP>` | Réseau local uniquement |

### Considérations

**Actuellement implémenté:**
- Accès limité au réseau local (pas d'accès Internet)
- Point d'accès protégé par mot de passe WPA2 (`medbox123`)
- Validation des entrées JSON côté serveur
- Pas de stockage des mots de passe WiFi dans les logs

**Recommandations pour la production:**
- Changer le mot de passe AP par défaut
- Ajouter authentification HTTP (Basic Auth ou session)
- Considérer HTTPS avec certificat auto-signé
- Limiter les tentatives de connexion

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

### Etape 1: Générer les clés (une seule fois par projet)

```bash
# Clé de chiffrement flash
python scripts/generate_key.py

# Clé de signature Secure Boot (si pas déjà générée)
espsecure.py generate_signing_key keys/secure_boot_signing_key.pem
```

### Etape 2: Vérifier l'état de l'ESP32

```bash
python scripts/check_esp32.py COM5
```

Si l'ESP32 est vierge, vous verrez :
```
[VIRGIN ESP32]
This ESP32 has no security configured.
```

### Etape 3: Configurer l'ESP32

```bash
python scripts/setup_esp32.py COM5
```

Cela grave dans les eFuses :
- La clé de chiffrement (BLOCK1)
- FLASH_CRYPT_CNT = 1 (active le chiffrement)
- FLASH_CRYPT_CONFIG = 0xF

**ATTENTION** : Cette opération est **IRRÉVERSIBLE** !

### Etape 4: Build et Flash

```bash
idf.py build
python scripts/encrypted_flash.py COM5
```

Ou en une commande :
```bash
scripts\flash.bat COM5
```

### Etape 5: Monitor

```bash
idf.py -p COM5 monitor
```

---

## Workflow de Développement

### Avec TA propre clé (RECOMMANDÉ)

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   idf.py build  │────>│ encrypted_flash │────>│   ESP32 reçoit  │
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

### Avec clé auto-générée (ANCIEN - à éviter)

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   idf.py build  │────>│  idf.py flash   │────>│  ESP32 chiffre  │
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
- Clé générée par l'ESP32, non récupérable

---

## Commandes de Diagnostic

### Vérifier l'état complet d'un ESP32

```bash
# État sécurité (script Medbox)
python scripts/check_esp32.py COM5

# Dump complet des eFuses
espefuse.py -p COM5 summary

# Voir les valeurs brutes
espefuse.py -p COM5 dump
```

### Résultat type (ESP32 configuré)

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

## Secure Boot V1 vs V2

Ce projet utilise **Secure Boot V1** :

| Aspect | V1 (utilisé) | V2 |
|--------|--------------|-----|
| Algorithme | AES-256 + ECDSA | RSA-3072 ou ECDSA-256 |
| Clés de signature | 1 clé | Jusqu'à 3 clés |
| Révocation de clé | Non | Oui |
| Rollback protection | Non | Oui |
| Chips supportés | ESP32 | ESP32 ECO3+, S2, S3, C3... |

**Pourquoi V1 ?** Suffisant pour la plupart des cas, plus simple à configurer.

---

## Mode Production

Pour passer en production, modifiez `sdkconfig.defaults` :

```ini
# Changer DEVELOPMENT en RELEASE
CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE=y

# Désactiver le mode dev
CONFIG_MEDBOX_DEV_MODE=n

# Désactiver les logs de debug
CONFIG_LOG_DEFAULT_LEVEL_WARN=y
```

Et protégez les clés en lecture (optionnel mais recommandé) :
```bash
espefuse.py -p COM5 burn_efuse RD_DIS 3
```

**ATTENTION** : En mode RELEASE, les eFuses sont verrouillés définitivement !

---

## Dépannage

### "invalid header" au boot

**Cause** : La flash contient des données non chiffrées mais l'ESP32 attend du chiffré.

**Solution** :
```bash
python scripts/encrypted_flash.py COM5
```

### "secure boot verification failed"

**Cause** : Le firmware n'est pas signé avec la bonne clé.

**Solution** : Vérifiez que `keys/secure_boot_signing_key.pem` est la clé utilisée lors du setup initial.

### "FLASH_CRYPT_CNT exhausted"

**Cause** : Plus de flashs plaintext disponibles (max 4).

**Solution** :
1. Utilisez `encrypted_flash.py` (pas de limite)
2. Ou utilisez un nouvel ESP32 configuré avec votre clé

### "Failed to connect to ESP32"

**Cause** : Problème de communication série.

**Solutions** :
1. Vérifiez le port COM
2. Maintenez le bouton BOOT enfoncé pendant le flash
3. Vérifiez les drivers USB

---

## Sécurité des Clés

### Ce qu'il faut protéger

| Fichier | Importance | Conséquence si perdu |
|---------|------------|---------------------|
| `secure_boot_signing_key.pem` | CRITIQUE | Impossible de signer de nouvelles versions |
| `flash_encryption_key.bin` | CRITIQUE | Impossible de flasher les ESP32 configurés |

### Bonnes pratiques

1. **Ne jamais commiter** les clés dans git
2. **Sauvegarder** sur un support externe sécurisé
3. **Chiffrer** les sauvegardes (ex: VeraCrypt, 7zip avec mot de passe)
4. **Limiter l'accès** aux personnes autorisées

### Pour les collaborateurs

Chaque développeur doit :
1. Demander les clés au responsable du projet
2. Les stocker dans `keys/` (ignoré par git)
3. Ne jamais les partager par email/chat non chiffré

---

## Références

- [ESP-IDF Flash Encryption](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/flash-encryption.html)
- [ESP-IDF Secure Boot V1](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/secure-boot-v1.html)
- [ESP32 eFuse](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/efuse.html)
