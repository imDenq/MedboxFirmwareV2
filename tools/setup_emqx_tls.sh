#!/bin/bash
# Script de configuration TLS pour EMQX
# Ce script génère les certificats serveur pour EMQX et configure le broker

set -e

EMQX_CERTS_DIR="/opt/emqx/certs"
CA_DIR="certs/ca"
SERVER_DIR="certs/server"

echo "=== Configuration TLS pour EMQX ==="

# Vérifier que la CA existe
if [ ! -f "$CA_DIR/ca.crt" ] || [ ! -f "$CA_DIR/ca.key" ]; then
    echo "ERREUR: CA non trouvée. Exécutez d'abord generate_certs.sh"
    exit 1
fi

# Créer le répertoire pour les certificats serveur
mkdir -p "$SERVER_DIR"

# 1. Générer le certificat serveur pour EMQX
echo ">>> Génération du certificat serveur EMQX..."

# Clé privée serveur
openssl genrsa -out "$SERVER_DIR/server.key" 2048

# CSR serveur
openssl req -new -key "$SERVER_DIR/server.key" \
    -out "$SERVER_DIR/server.csr" \
    -subj "/C=FR/ST=France/L=Paris/O=Medbox/OU=Server/CN=mqtt.medbox.theokaszak.fr"

# Configuration pour les extensions serveur
cat > "$SERVER_DIR/server_ext.cnf" << EOF
[req]
req_extensions = v3_req
distinguished_name = req_distinguished_name

[req_distinguished_name]

[v3_req]
basicConstraints = CA:FALSE
keyUsage = digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = @alt_names

[alt_names]
IP.1 = mqtt.medbox.theokaszak.fr
DNS.1 = localhost
EOF

# Signer le certificat serveur
openssl x509 -req -days 730 -in "$SERVER_DIR/server.csr" \
    -CA "$CA_DIR/ca.crt" -CAkey "$CA_DIR/ca.key" \
    -CAcreateserial \
    -out "$SERVER_DIR/server.crt" \
    -extfile "$SERVER_DIR/server_ext.cnf" -extensions v3_req

# Nettoyer
rm -f "$SERVER_DIR/server.csr" "$SERVER_DIR/server_ext.cnf"

echo "Certificat serveur généré: $SERVER_DIR/server.crt"

# 2. Afficher la configuration EMQX
echo ""
echo "=== Configuration EMQX ==="
echo ""
echo "1. Copier les certificats vers le serveur EMQX:"
echo "   scp $CA_DIR/ca.crt user@mqtt.medbox.theokaszak.fr:$EMQX_CERTS_DIR/"
echo "   scp $SERVER_DIR/server.crt user@mqtt.medbox.theokaszak.fr:$EMQX_CERTS_DIR/"
echo "   scp $SERVER_DIR/server.key user@mqtt.medbox.theokaszak.fr:$EMQX_CERTS_DIR/"
echo ""
echo "2. Configurer EMQX via le Dashboard (http://mqtt.medbox.theokaszak.fr:18083):"
echo "   - Aller dans 'Authentication' > 'TLS/SSL'"
echo "   - Ou modifier le fichier emqx.conf:"
echo ""
cat << 'EMQX_CONFIG'
# Configuration TLS EMQX - Ajouter dans emqx.conf

listeners.ssl.default {
  bind = "0.0.0.0:8883"
  ssl_options {
    cacertfile = "/opt/emqx/certs/ca.crt"
    certfile = "/opt/emqx/certs/server.crt"
    keyfile = "/opt/emqx/certs/server.key"
    verify = verify_peer
    fail_if_no_peer_cert = true
  }
}
EMQX_CONFIG
echo ""
echo "3. Redémarrer EMQX:"
echo "   docker restart emqx"
echo ""
echo "4. Tester la connexion TLS:"
echo "   openssl s_client -connect mqtt.medbox.theokaszak.fr:8883 -CAfile $CA_DIR/ca.crt"
