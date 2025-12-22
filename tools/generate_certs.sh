#!/bin/bash
# Script de génération des certificats pour Medbox
# Usage: ./generate_certs.sh <medbox_uuid>

set -e

if [ -z "$1" ]; then
    echo "Usage: $0 <medbox_uuid>"
    echo "Exemple: $0 medbox-001"
    exit 1
fi

MEDBOX_UUID=$1
CERTS_DIR="certs/${MEDBOX_UUID}"
CA_DIR="certs/ca"

# Créer les répertoires
mkdir -p "$CERTS_DIR"
mkdir -p "$CA_DIR"

echo "=== Génération des certificats pour Medbox: ${MEDBOX_UUID} ==="

# 1. Générer la CA si elle n'existe pas
if [ ! -f "$CA_DIR/ca.key" ]; then
    echo ">>> Génération de la Certificate Authority..."

    # Clé privée CA (4096 bits)
    openssl genrsa -out "$CA_DIR/ca.key" 4096

    # Certificat CA auto-signé (valide 10 ans)
    openssl req -new -x509 -days 3650 -key "$CA_DIR/ca.key" \
        -out "$CA_DIR/ca.crt" \
        -subj "/C=FR/ST=France/L=Paris/O=Medbox/OU=IoT/CN=Medbox-CA"

    echo "CA générée: $CA_DIR/ca.crt"
else
    echo ">>> CA existante trouvée"
fi

# 2. Générer le certificat client pour cette Medbox
echo ">>> Génération du certificat client pour ${MEDBOX_UUID}..."

# Clé privée client (2048 bits)
openssl genrsa -out "$CERTS_DIR/client.key" 2048

# Demande de signature (CSR)
openssl req -new -key "$CERTS_DIR/client.key" \
    -out "$CERTS_DIR/client.csr" \
    -subj "/C=FR/ST=France/L=Paris/O=Medbox/OU=Device/CN=${MEDBOX_UUID}"

# Configuration pour les extensions
cat > "$CERTS_DIR/client_ext.cnf" << EOF
[req]
req_extensions = v3_req
distinguished_name = req_distinguished_name

[req_distinguished_name]

[v3_req]
basicConstraints = CA:FALSE
keyUsage = digitalSignature, keyEncipherment
extendedKeyUsage = clientAuth
subjectAltName = @alt_names

[alt_names]
DNS.1 = ${MEDBOX_UUID}
EOF

# Signer le certificat client avec la CA (valide 2 ans)
openssl x509 -req -days 730 -in "$CERTS_DIR/client.csr" \
    -CA "$CA_DIR/ca.crt" -CAkey "$CA_DIR/ca.key" \
    -CAcreateserial \
    -out "$CERTS_DIR/client.crt" \
    -extfile "$CERTS_DIR/client_ext.cnf" -extensions v3_req

# Nettoyer les fichiers temporaires
rm -f "$CERTS_DIR/client.csr" "$CERTS_DIR/client_ext.cnf"

# 3. Afficher les informations
echo ""
echo "=== Certificats générés avec succès ==="
echo ""
echo "Fichiers créés:"
echo "  CA Certificate:     $CA_DIR/ca.crt"
echo "  Client Certificate: $CERTS_DIR/client.crt"
echo "  Client Private Key: $CERTS_DIR/client.key"
echo ""
echo "Pour le broker EMQX:"
echo "  - Copier $CA_DIR/ca.crt vers le serveur EMQX"
echo "  - Configurer EMQX pour utiliser cette CA pour vérifier les clients"
echo ""
echo "Vérification du certificat client:"
openssl x509 -in "$CERTS_DIR/client.crt" -noout -subject -issuer -dates
echo ""
echo "Pour provisionner l'ESP32, utilisez:"
echo "  python tools/provision_nvs.py --uuid ${MEDBOX_UUID} \\"
echo "    --ca-cert $CA_DIR/ca.crt \\"
echo "    --client-cert $CERTS_DIR/client.crt \\"
echo "    --client-key $CERTS_DIR/client.key"
