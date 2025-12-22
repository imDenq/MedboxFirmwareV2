#!/usr/bin/env python3
"""
Medbox - ESP32 Security Setup
Burns the flash encryption key and configures eFuses on a NEW ESP32.

WARNING: This operation is IRREVERSIBLE!
Only run this on a fresh/virgin ESP32.
"""

import os
import sys
import subprocess
from pathlib import Path

# Use the same Python that's running this script
PYTHON = sys.executable

# Paths
KEYS_DIR = Path(__file__).parent.parent / "keys"
FLASH_KEY_FILE = KEYS_DIR / "flash_encryption_key.bin"
SECURE_BOOT_KEY_FILE = KEYS_DIR / "secure_boot_signing_key.pem"

def run_command(cmd, check=True):
    """Run a command and return the result."""
    print(f"[>] {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.stdout:
        print(result.stdout)
    if result.stderr:
        print(result.stderr)
    if check and result.returncode != 0:
        print(f"[!] Command failed with code {result.returncode}")
        return False
    return True

def check_efuse_status(port):
    """Check if the ESP32 is virgin (no security eFuses burned)."""
    print("\n[*] Checking eFuse status...")

    result = subprocess.run(
        [PYTHON, "-m", "espefuse", "--port", port, "summary"],
        capture_output=True, text=True
    )

    output = result.stdout + result.stderr

    # Check for already burned security eFuses
    if "FLASH_CRYPT_CNT" in output:
        for line in output.split('\n'):
            if "FLASH_CRYPT_CNT" in line and "= 0" not in line and "0b0000000" not in line:
                print("[!] WARNING: Flash encryption already configured on this ESP32!")
                return False

    if "ABS_DONE_0" in output:
        for line in output.split('\n'):
            if "ABS_DONE_0" in line and "True" in line:
                print("[!] WARNING: Secure Boot already enabled on this ESP32!")
                return False

    print("[+] ESP32 appears to be virgin - safe to proceed.")
    return True

def setup_esp32(port):
    """Setup a new ESP32 with flash encryption and secure boot."""

    print("=" * 60)
    print("  Medbox ESP32 Security Setup")
    print("=" * 60)
    print()
    print(f"  Port: {port}")
    print(f"  Flash Key: {FLASH_KEY_FILE}")
    print(f"  Secure Boot Key: {SECURE_BOOT_KEY_FILE}")
    print()

    # Check if keys exist
    if not FLASH_KEY_FILE.exists():
        print(f"[!] Flash encryption key not found: {FLASH_KEY_FILE}")
        print("    Run 'python scripts/generate_key.py' first.")
        return False

    if not SECURE_BOOT_KEY_FILE.exists():
        print(f"[!] Secure boot key not found: {SECURE_BOOT_KEY_FILE}")
        return False

    # Check ESP32 status
    if not check_efuse_status(port):
        print("\n[!] This ESP32 already has security features configured.")
        print("    Use a fresh ESP32 or continue with existing configuration.")
        response = input("    Continue anyway? (yes/no): ").strip().lower()
        if response != "yes":
            return False

    print("\n" + "=" * 60)
    print("  WARNING: IRREVERSIBLE OPERATION")
    print("=" * 60)
    print()
    print("  This will permanently burn the following eFuses:")
    print("    - Flash encryption key (BLOCK1)")
    print("    - FLASH_CRYPT_CNT = 1 (enable encryption)")
    print("    - FLASH_CRYPT_CONFIG = 0xF")
    print()
    print("  This operation CANNOT be undone!")
    print()

    response = input("  Type 'BURN' to proceed: ").strip()
    if response != "BURN":
        print("[*] Aborted.")
        return False

    print()

    # Step 1: Burn the flash encryption key
    print("[1/3] Burning flash encryption key...")
    if not run_command([
        PYTHON, "-m", "espefuse", "--port", port,
        "--do-not-confirm",
        "burn_key", "flash_encryption", str(FLASH_KEY_FILE)
    ]):
        print("[!] Failed to burn flash encryption key.")
        return False

    # Step 2: Burn FLASH_CRYPT_CONFIG
    print("\n[2/3] Burning FLASH_CRYPT_CONFIG...")
    if not run_command([
        PYTHON, "-m", "espefuse", "--port", port,
        "--do-not-confirm",
        "burn_efuse", "FLASH_CRYPT_CONFIG", "0xF"
    ]):
        print("[!] Failed to burn FLASH_CRYPT_CONFIG.")
        return False

    # Step 3: Burn FLASH_CRYPT_CNT to enable encryption
    print("\n[3/3] Enabling flash encryption (FLASH_CRYPT_CNT)...")
    if not run_command([
        PYTHON, "-m", "espefuse", "--port", port,
        "--do-not-confirm",
        "burn_efuse", "FLASH_CRYPT_CNT", "1"
    ]):
        print("[!] Failed to enable flash encryption.")
        return False

    print()
    print("=" * 60)
    print("  ESP32 Security Setup Complete!")
    print("=" * 60)
    print()
    print("  Flash encryption is now ENABLED on this ESP32.")
    print()
    print("  Next steps:")
    print("    1. Build your firmware: idf.py build")
    print("    2. Flash with encryption: python scripts/encrypted_flash.py COM5")
    print()

    return True

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python setup_esp32.py <PORT>")
        print("Example: python setup_esp32.py COM5")
        sys.exit(1)

    port = sys.argv[1]
    success = setup_esp32(port)
    sys.exit(0 if success else 1)
