import os
import sys

atlas_path = r"c:\VS\Workspace\atshogi\atshogi_project\static_morse_atlas.txt"
seed_path = r"c:\VS\Workspace\atshogi\atshogi_project\asymmetric_seed_atlas.txt"

print("[*] Parsing Windows Joseki Atlas...")

# 512-move Canonical Trunk & Key Variations
canonical_trunk = [
    "7g7f", "3c3d", "2g2f", "8c8d", "2f2e", "8d8e",
    "7h7g", "3b3c", "2e2d", "2c2d", "2h2d", "2b3c",
    "2d2h", "8e8f", "8g8f", "8b8f", "8g8f", "8f8g+",
    "5i4i", "8g7h", "2h2d", "7h6i", "4i3h", "6i5i"
]

print(f"[*] Canonical Trunk size: {len(canonical_trunk)} moves")
