#!/usr/bin/env python3
"""
Medbox - Encrypted Flash Tool
Pre-encrypts binaries and flashes them to an ESP32 with flash encryption enabled.

This allows UNLIMITED flashing because the ESP32 doesn't need to encrypt
(FLASH_CRYPT_CNT is not consumed).
"""

import os
import sys
import subprocess
import tempfile
import shutil
from pathlib import Path

# Use the same Python that's running this script
PYTHON = sys.executable

# Paths
PROJECT_DIR = Path(__file__).parent.parent
BUILD_DIR = PROJECT_DIR / "build"
KEYS_DIR = PROJECT_DIR / "keys"
FLASH_KEY_FILE = KEYS_DIR / "flash_encryption_key.bin"

# Binary locations and flash addresses
BINARIES = [
    {
        "name": "bootloader",
        "source": BUILD_DIR / "bootloader" / "bootloader.bin",
        "address": "0x1000"
    },
    {
        "name": "partition-table",
        "source": BUILD_DIR / "partition_table" / "partition-table.bin",
        "address": "0x10000"
    },
    {
        "name": "firmware",
        "source": BUILD_DIR / "MedboxFirmware.bin",
        "address": "0x20000"
    }
]

def encrypt_binary(source_path, address, key_file, output_path):
    """Encrypt a binary file using the flash encryption key."""
    cmd = [
        PYTHON, "-m", "espsecure",
        "encrypt_flash_data",
        "--keyfile", str(key_file),
        "--address", address,
        "--output", str(output_path),
        str(source_path)
    ]

    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"[!] Encryption failed: {result.stderr}")
        return False
    return True

def flash_binaries(port, encrypted_files):
    """Flash all encrypted binaries to the ESP32."""
    cmd = [
        PYTHON, "-m", "esptool",
        "--chip", "esp32",
        "--port", port,
        "--baud", "460800",
        "--before=default_reset",
        "--after=hard_reset",
        "write_flash",
        "--flash_mode", "dio",
        "--flash_freq", "40m",
        "--flash_size", "2MB"
    ]

    # Add each binary with its address
    for item in encrypted_files:
        cmd.extend([item["address"], str(item["encrypted_path"])])

    print(f"\n[>] {' '.join(cmd[:10])}...")
    result = subprocess.run(cmd)
    return result.returncode == 0

def encrypted_flash(port, skip_bootloader=False):
    """Main function to encrypt and flash all binaries."""

    print("=" * 60)
    print("  Medbox Encrypted Flash Tool")
    print("=" * 60)
    print()

    # Check for flash encryption key
    if not FLASH_KEY_FILE.exists():
        print(f"[!] Flash encryption key not found: {FLASH_KEY_FILE}")
        print("    Run 'python scripts/generate_key.py' first.")
        return False

    # Check for binaries
    binaries_to_flash = BINARIES if not skip_bootloader else BINARIES[1:]

    for binary in binaries_to_flash:
        if not binary["source"].exists():
            print(f"[!] Binary not found: {binary['source']}")
            print("    Run 'idf.py build' first.")
            return False

    print(f"  Port: {port}")
    print(f"  Key: {FLASH_KEY_FILE}")
    print(f"  Skip bootloader: {skip_bootloader}")
    print()

    # Create temporary directory for encrypted files
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        encrypted_files = []

        print("[*] Encrypting binaries...")
        print()

        for binary in binaries_to_flash:
            encrypted_path = temp_path / f"{binary['name']}-encrypted.bin"

            print(f"  [{binary['address']}] {binary['name']}")
            print(f"    Source: {binary['source']}")
            print(f"    Size: {binary['source'].stat().st_size} bytes")

            if not encrypt_binary(
                binary["source"],
                binary["address"],
                FLASH_KEY_FILE,
                encrypted_path
            ):
                print(f"[!] Failed to encrypt {binary['name']}")
                return False

            print(f"    Encrypted: OK")
            print()

            encrypted_files.append({
                "name": binary["name"],
                "address": binary["address"],
                "encrypted_path": encrypted_path
            })

        # Flash all encrypted binaries
        print("[*] Flashing encrypted binaries...")
        if not flash_binaries(port, encrypted_files):
            print("[!] Flash failed!")
            return False

    print()
    print("=" * 60)
    print("  Flash Complete!")
    print("=" * 60)
    print()
    print("  Your encrypted firmware has been flashed successfully.")
    print("  FLASH_CRYPT_CNT was NOT consumed (unlimited flashes).")
    print()
    print(f"  Monitor: idf.py -p {port} monitor")
    print()

    return True

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python encrypted_flash.py <PORT> [--skip-bootloader]")
        print()
        print("Options:")
        print("  --skip-bootloader  Don't flash the bootloader (faster for app updates)")
        print()
        print("Examples:")
        print("  python encrypted_flash.py COM5")
        print("  python encrypted_flash.py COM5 --skip-bootloader")
        sys.exit(1)

    port = sys.argv[1]
    skip_bootloader = "--skip-bootloader" in sys.argv

    success = encrypted_flash(port, skip_bootloader)
    sys.exit(0 if success else 1)
