import struct
import os

def fnv1a64(s: str) -> int:
    h = 14695981039346656037
    for c in s.encode('utf-8'):
        h = ((h ^ c) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h

def pack_move(m_str: str) -> int:
    if not m_str or len(m_str) < 4:
        return 0
    if '*' in m_str:
        p_char = m_str[0]
        pt_map = {'P': 1, 'L': 2, 'N': 3, 'S': 4, 'G': 5, 'B': 6, 'R': 7}
        pt = pt_map.get(p_char, 1)
        tf = int(m_str[2])
        tr = ord(m_str[3]) - ord('a') + 1
        to_sq = (tf - 1) * 9 + (tr - 1)
        return 0x8000 | (pt << 7) | to_sq
    else:
        ff = int(m_str[0])
        fr = ord(m_str[1]) - ord('a') + 1
        tf = int(m_str[2])
        tr = ord(m_str[3]) - ord('a') + 1
        from_sq = (ff - 1) * 9 + (fr - 1)
        to_sq = (tf - 1) * 9 + (tr - 1)
        promo = 0x4000 if m_str.endswith('+') else 0
        return promo | (from_sq << 7) | to_sq

entries = {}

# 1. static_morse_atlas.txt (444万件)
atlas_txt = r"c:\VS\Workspace\atshogi\atshogi_project\static_morse_atlas.txt"
if os.path.exists(atlas_txt):
    print(f"[*] Reading {atlas_txt}...")
    with open(atlas_txt, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            parts = line.strip().split()
            if len(parts) >= 2:
                try:
                    key = int(parts[0])
                    best_packed = pack_move(parts[1])
                    ponder_packed = pack_move(parts[2]) if len(parts) >= 3 else 0
                    entries[key] = (best_packed, ponder_packed)
                except ValueError:
                    pass

# 2. 初期局面および主要定跡キーの正確な登録
# Startpos hash: 14206493660615813465 -> 7g7f
start_hash = fnv1a64("cx=[]|turn=White|hand=Black:;White:")
entries[start_hash] = (pack_move("7g7f"), 0)

# 初期局面の Zobrist Orbit Hash (13495464856663819704) にも 7g7f を登録
entries[13495464856663819704] = (pack_move("7g7f"), 0)

sorted_keys = sorted(entries.keys())
out_bin = r"app\src\main\assets\engine\static_morse_atlas.bin"

with open(out_bin, "wb") as f:
    for k in sorted_keys:
        best, ponder = entries[k]
        f.write(struct.pack("<QHHBBH", k, best, ponder, 0, 0, 0))

print("Total entries in atlas binary:", len(sorted_keys))
print("Startpos hash:", start_hash, "-> packed:", entries[start_hash])
print("Startpos zobrist hash: 13495464856663819704 -> packed:", entries[13495464856663819704])
