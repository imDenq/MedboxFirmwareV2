#!/usr/bin/env python3
"""
Script de provisioning NVS pour Medbox
Génère une image NVS avec les certificats et l'UUID pour flasher sur l'ESP32
"""

import argparse
import os
import sys
import subprocess
import csv
import tempfile
from pathlib import Path


def read_file(filepath: str) -> str:
    """Lit un fichier et retourne son contenu."""
    with open(filepath, 'r') as f:
        return f.read()


def generate_nvs_csv(uuid: str, ca_cert: str, client_cert: str, client_key: str, output_path: str):
    """
    Génère le fichier CSV pour NVS Partition Generator.
    Format: key,type,encoding,value
    """
    with open(output_path, 'w', newline='') as f:
        writer = csv.writer(f)

        # En-tête
        writer.writerow(['key', 'type', 'encoding', 'value'])

        # Namespace pour les certificats
        writer.writerow(['certs', 'namespace', '', ''])
        writer.writerow(['ca_cert', 'data', 'string', ca_cert])
        writer.writerow(['client_cert', 'data', 'string', client_cert])
        writer.writerow(['client_key', 'data', 'string', client_key])

        # Namespace pour la configuration
        writer.writerow(['config', 'namespace', '', ''])
        writer.writerow(['medbox_uuid', 'data', 'string', uuid])

    print(f"CSV généré: {output_path}")


def generate_nvs_partition(csv_path: str, output_path: str, size: str = "0x6000"):
    """
    Utilise nvs_partition_gen.py pour générer l'image NVS binaire.
    """
    # Chercher le script nvs_partition_gen.py dans IDF_PATH
    idf_path = os.environ.get('IDF_PATH')
    if not idf_path:
        print("ERREUR: IDF_PATH non défini. Sourcez esp-idf/export.sh d'abord.")
        sys.exit(1)

    nvs_gen_script = Path(idf_path) / "components" / "nvs_flash" / "nvs_partition_generator" / "nvs_partition_gen.py"

    if not nvs_gen_script.exists():
        print(f"ERREUR: Script non trouvé: {nvs_gen_script}")
        sys.exit(1)

    cmd = [
        sys.executable,
        str(nvs_gen_script),
        "generate",
        csv_path,
        output_path,
        size
    ]

    print(f"Génération de l'image NVS...")
    print(f"Commande: {' '.join(cmd)}")

    result = subprocess.run(cmd, capture_output=True, text=True)

    if result.returncode != 0:
        print(f"ERREUR lors de la génération:")
        print(result.stderr)
        sys.exit(1)

    print(f"Image NVS générée: {output_path}")
    return output_path


def flash_nvs_partition(partition_path: str, address: str = "0x9000", port: str = None):
    """
    Flashe l'image NVS sur l'ESP32.
    """
    cmd = ["esptool.py"]

    if port:
        cmd.extend(["--port", port])

    cmd.extend([
        "write_flash",
        address,
        partition_path
    ])

    print(f"Flashage de la partition NVS...")
    print(f"Commande: {' '.join(cmd)}")

    result = subprocess.run(cmd)

    if result.returncode != 0:
        print("ERREUR lors du flashage")
        sys.exit(1)

    print("Partition NVS flashée avec succès!")


def main():
    parser = argparse.ArgumentParser(
        description="Provisionne une Medbox avec les certificats et l'UUID",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Exemples:
  # Générer l'image NVS uniquement
  python provision_nvs.py --uuid medbox-001 \\
    --ca-cert certs/ca/ca.crt \\
    --client-cert certs/medbox-001/client.crt \\
    --client-key certs/medbox-001/client.key \\
    --output nvs_medbox-001.bin

  # Générer et flasher directement
  python provision_nvs.py --uuid medbox-001 \\
    --ca-cert certs/ca/ca.crt \\
    --client-cert certs/medbox-001/client.crt \\
    --client-key certs/medbox-001/client.key \\
    --flash --port COM3

  # Avec une adresse de partition personnalisée
  python provision_nvs.py --uuid medbox-001 \\
    --ca-cert certs/ca/ca.crt \\
    --client-cert certs/medbox-001/client.crt \\
    --client-key certs/medbox-001/client.key \\
    --flash --port COM3 --nvs-address 0x9000
        """
    )

    parser.add_argument('--uuid', required=True, help='UUID de la Medbox')
    parser.add_argument('--ca-cert', required=True, help='Chemin vers le certificat CA')
    parser.add_argument('--client-cert', required=True, help='Chemin vers le certificat client')
    parser.add_argument('--client-key', required=True, help='Chemin vers la clé privée client')
    parser.add_argument('--output', default=None, help='Fichier de sortie pour l\'image NVS')
    parser.add_argument('--nvs-size', default='0x6000', help='Taille de la partition NVS (défaut: 0x6000)')
    parser.add_argument('--flash', action='store_true', help='Flasher directement sur l\'ESP32')
    parser.add_argument('--port', default=None, help='Port série (ex: COM3, /dev/ttyUSB0)')
    parser.add_argument('--nvs-address', default='0x9000', help='Adresse de la partition NVS (défaut: 0x9000)')
    parser.add_argument('--keep-csv', action='store_true', help='Conserver le fichier CSV intermédiaire')

    args = parser.parse_args()

    # Vérifier que les fichiers existent
    for filepath, name in [(args.ca_cert, 'CA cert'),
                           (args.client_cert, 'Client cert'),
                           (args.client_key, 'Client key')]:
        if not os.path.exists(filepath):
            print(f"ERREUR: {name} non trouvé: {filepath}")
            sys.exit(1)

    # Lire les certificats
    print(f"\n=== Provisioning Medbox: {args.uuid} ===\n")

    ca_cert = read_file(args.ca_cert)
    client_cert = read_file(args.client_cert)
    client_key = read_file(args.client_key)

    print(f"CA Certificate: {len(ca_cert)} bytes")
    print(f"Client Certificate: {len(client_cert)} bytes")
    print(f"Client Key: {len(client_key)} bytes")

    # Générer le CSV
    csv_path = tempfile.mktemp(suffix='.csv') if not args.keep_csv else f"nvs_{args.uuid}.csv"
    generate_nvs_csv(args.uuid, ca_cert, client_cert, client_key, csv_path)

    # Générer l'image NVS
    output_path = args.output or f"nvs_{args.uuid}.bin"
    generate_nvs_partition(csv_path, output_path, args.nvs_size)

    # Nettoyer le CSV si nécessaire
    if not args.keep_csv and os.path.exists(csv_path):
        os.remove(csv_path)

    # Flasher si demandé
    if args.flash:
        flash_nvs_partition(output_path, args.nvs_address, args.port)

    print(f"\n=== Provisioning terminé ===")
    print(f"UUID: {args.uuid}")
    print(f"Image NVS: {output_path}")

    if not args.flash:
        print(f"\nPour flasher manuellement:")
        port_arg = f"--port {args.port} " if args.port else ""
        print(f"  esptool.py {port_arg}write_flash {args.nvs_address} {output_path}")


if __name__ == '__main__':
    main()
