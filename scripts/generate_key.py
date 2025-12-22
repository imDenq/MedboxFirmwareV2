#!/usr/bin/env python3
"""
Medbox - Flash Encryption Key Generator
Generates a flash encryption key and saves it securely.
"""

import os
import sys
import secrets
from pathlib import Path

# Key file location
KEYS_DIR = Path(__file__).parent.parent / "keys"
KEY_FILE = KEYS_DIR / "flash_encryption_key.bin"

def generate_flash_encryption_key():
    """Generate a 256-bit (32 bytes) flash encryption key."""

    # Create keys directory if it doesn't exist
    KEYS_DIR.mkdir(exist_ok=True)

    # Check if key already exists
    if KEY_FILE.exists():
        print(f"[!] Key already exists: {KEY_FILE}")
        response = input("    Overwrite? (yes/no): ").strip().lower()
        if response != "yes":
            print("[*] Keeping existing key.")
            return False

    # Generate 32 bytes of cryptographically secure random data
    key = secrets.token_bytes(32)

    # Write the key
    KEY_FILE.write_bytes(key)

    print(f"[+] Flash encryption key generated: {KEY_FILE}")
    print(f"[+] Key size: {len(key)} bytes (256 bits)")
    print()
    print("[!] IMPORTANT: Back up this key securely!")
    print("    Without this key, you cannot flash encrypted firmware.")
    print()

    # Show key hash for verification
    import hashlib
    key_hash = hashlib.sha256(key).hexdigest()[:16]
    print(f"[*] Key fingerprint (SHA256): {key_hash}...")

    return True

if __name__ == "__main__":
    print("=" * 50)
    print("  Medbox Flash Encryption Key Generator")
    print("=" * 50)
    print()

    generate_flash_encryption_key()
