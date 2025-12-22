#!/usr/bin/env python3
"""
Medbox - ESP32 Status Checker
Checks the security status of an ESP32 and recommends the appropriate flash method.
"""

import sys
import subprocess
import re

# Use the same Python that's running this script
PYTHON = sys.executable

def check_esp32(port):
    """Check the security status of an ESP32."""

    print("=" * 60)
    print("  Medbox ESP32 Security Status")
    print("=" * 60)
    print()
    print(f"  Port: {port}")
    print()

    # Get eFuse summary
    result = subprocess.run(
        [PYTHON, "-m", "espefuse", "--port", port, "summary"],
        capture_output=True, text=True
    )

    if result.returncode != 0:
        print(f"[!] Failed to read eFuses: {result.stderr}")
        return

    output = result.stdout

    # Parse important values
    flash_crypt_cnt = None
    flash_crypt_config = None
    abs_done_0 = None
    abs_done_1 = None
    rd_dis = None

    for line in output.split('\n'):
        if "FLASH_CRYPT_CNT" in line and "=" in line:
            match = re.search(r'= (\d+)', line)
            if match:
                flash_crypt_cnt = int(match.group(1))
        elif "FLASH_CRYPT_CONFIG" in line and "=" in line:
            match = re.search(r'= (\d+)', line)
            if match:
                flash_crypt_config = int(match.group(1))
        elif "ABS_DONE_0" in line:
            abs_done_0 = "True" in line
        elif "ABS_DONE_1" in line:
            abs_done_1 = "True" in line
        elif "RD_DIS" in line and "=" in line:
            match = re.search(r'= (\d+)', line)
            if match:
                rd_dis = int(match.group(1))

    # Analyze results
    print("  Security Status:")
    print("  " + "-" * 40)

    # Flash Encryption
    if flash_crypt_cnt is not None:
        is_encrypted = (flash_crypt_cnt % 2) == 1
        bits_used = bin(flash_crypt_cnt).count('1')
        flashes_left = (4 - (bits_used + 1) // 2) if bits_used < 7 else 0

        print(f"  Flash Encryption: {'ENABLED' if is_encrypted else 'DISABLED'}")
        print(f"  FLASH_CRYPT_CNT:  {flash_crypt_cnt} (0b{flash_crypt_cnt:07b})")

        if is_encrypted:
            print(f"  Plaintext flashes remaining: {flashes_left}")
    else:
        print("  Flash Encryption: NOT CONFIGURED")
        is_encrypted = False
        flashes_left = 4

    # Secure Boot
    print()
    if abs_done_0:
        print("  Secure Boot V1:   ENABLED")
    else:
        print("  Secure Boot V1:   NOT ENABLED")

    if abs_done_1:
        print("  Secure Boot V2:   ENABLED")
    else:
        print("  Secure Boot V2:   NOT ENABLED")

    # Key protection
    print()
    if rd_dis is not None:
        key_protected = (rd_dis & 0x3) != 0
        print(f"  Key Read Protect: {'YES (keys cannot be extracted)' if key_protected else 'NO (keys readable)'}")
    else:
        print("  Key Read Protect: Unknown")

    # Recommendations
    print()
    print("  " + "-" * 40)
    print("  Recommendation:")
    print()

    if flash_crypt_cnt == 0 or flash_crypt_cnt is None:
        print("  [VIRGIN ESP32]")
        print("  This ESP32 has no security configured.")
        print()
        print("  To set up with YOUR key (unlimited flashes):")
        print("    1. python scripts/generate_key.py")
        print("    2. python scripts/setup_esp32.py " + port)
        print("    3. idf.py build")
        print("    4. python scripts/encrypted_flash.py " + port)

    elif is_encrypted and key_protected:
        print("  [CONFIGURED - KEY PROTECTED]")
        print(f"  Flash encryption is enabled with a protected key.")
        print(f"  You have {flashes_left} plaintext flash(es) remaining.")
        print()
        if flashes_left > 0:
            print("  To flash (uses 1 plaintext flash):")
            print(f"    idf.py -p {port} flash")
        else:
            print("  [!] No plaintext flashes remaining!")
            print("  You need a new ESP32 configured with your own key.")
        print()
        print("  For unlimited flashes, use a NEW ESP32 with:")
        print("    python scripts/setup_esp32.py <NEW_PORT>")

    elif is_encrypted and not key_protected:
        print("  [CONFIGURED - KEY READABLE]")
        print("  Flash encryption is enabled but key is readable.")
        print()
        print("  You can extract the key and use encrypted flashing:")
        print("    python -m espefuse --port " + port + " dump")
        print()
        print("  Or use plaintext flashing (limited):")
        print(f"    idf.py -p {port} flash")

    print()

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python check_esp32.py <PORT>")
        print("Example: python check_esp32.py COM5")
        sys.exit(1)

    port = sys.argv[1]
    check_esp32(port)
