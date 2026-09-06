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

# 1. static_morse_atlas.txt
atlas_txt = r"c:\VS\Workspace\atshogi\atshogi_project\static_morse_atlas.txt"
if os.path.exists(atlas_txt):
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

# 2. 決定論的 99手本筋 (Gold Braid Trunk)
canonical_trunk_99 = [
    "7g7f", "8b5b", "6i7h", "3a3b", "2g2f", "7a7b", "2f2e", "1c1d",
    "3i3h", "2b1c", "7i6h", "1c3e", "3h2g", "3e1g", "2i1g", "9c9d",
    "6h7g", "5c5d", "5i6h", "5b6b", "6h7i", "6b5b", "4i5h", "1d1e",
    "6g6f", "1e1f", "5h6g", "1f1g+", "1i1g", "1a1g+", "2h2i", "L*2h",
    "2g3h", "N*2f", "2i3i", "2f3h+", "3i3h", "1g2g", "3h3i", "S*3h",
    "3i1i", "2h2i+", "1i1a+", "2g2f", "4g4f", "3h4g+", "3g3f", "5a6b",
    "B*3e", "6b5a", "3e2f", "4g4f", "3f3e", "4f3f", "2f4h", "3f4g",
    "4h5i", "4g5h", "5i3g", "5h6i", "7i6i", "5a6b", "6i7i", "P*1c",
    "S*8e", "8a9c", "8e9d", "9c8e", "9d8e", "8c8d", "8e8d", "P*8c",
    "8d7e", "5d5e", "P*4e", "2i3i", "P*1d", "3i3h", "3g5i", "3h4i",
    "5i2f", "6b5a", "1d1c+", "5a4b", "1a2b", "4b5a", "1c2c", "3b2c",
    "2b2c", "P*2b", "2c1d", "5b5d", "1d5d", "4a5b", "R*3b", "5b5c",
    "5d5c", "6a5b", "3b5b+"
]

# 3. 24手角換わり・相掛かり定跡
canonical_trunk_24 = [
    "7g7f", "3c3d", "2g2f", "8c8d", "2f2e", "8d8e",
    "7h7g", "3b3c", "2e2d", "2c2d", "2h2d", "2b3c",
    "2d2h", "8e8f", "8g8f", "8b8f", "8g8f", "8f8g+",
    "5i4i", "8g7h", "2h2d", "7h6i", "4i3h", "6i5i"
]

# 4. 横歩取り・相居飛車・矢倉変化
variations = [
    ["7g7f", "3c3d", "2g2f", "8c8d", "2f2e", "8d8e", "7h7g", "3b3c", "2e2d", "2c2d", "2h2d", "2b3c", "2d2h"],
    ["7g7f", "3c3d", "2g2f", "4a3b", "2f2e", "8c8d", "2e2d", "2c2d", "2h2d", "P*2c", "2d2h", "8d8e", "7h7g"],
    ["7g7f", "3c3d", "2g2f", "3b3c", "2f2e", "8c8d", "2e2d", "2c2d", "2h2d", "P*2c", "2d2h", "8d8e", "7h7g"],
    ["2g2f", "3c3d", "7g7f", "8c8d", "2f2e", "8d8e", "7h7g", "3b3c", "2e2d", "2c2d", "2h2d", "2b3c", "2d2h"],
    ["2g2f", "8c8d", "2f2e", "8d8e", "7g7f", "3c3d", "7h7g", "3b3c", "2e2d", "2c2d", "2h2d", "2b3c", "2d2h"],
    canonical_trunk_99,
    canonical_trunk_24
]

# 手順文字列ハッシュの追加
for line in variations:
    for idx in range(len(line)):
        history = line[:idx]
        best_mv = line[idx]
        ponder_mv = line[idx+1] if idx+1 < len(line) else ""
        
        # SFEN 形式ハッシュ
        sfen_key = "startpos moves " + " ".join(history) if history else "startpos"
        h = fnv1a64(sfen_key)
        entries[h] = (pack_move(best_mv), pack_move(ponder_mv))
        
        # moves 形式ハッシュ
        moves_key = " ".join(history)
        h2 = fnv1a64(moves_key)
        entries[h2] = (pack_move(best_mv), pack_move(ponder_mv))

sorted_keys = sorted(entries.keys())
out_bin = r"app\src\main\assets\engine\static_morse_atlas.bin"

with open(out_bin, "wb") as f:
    for k in sorted_keys:
        best, ponder = entries[k]
        f.write(struct.pack("<QHHBBH", k, best, ponder, 0, 0, 0))

print(f"Exported {len(sorted_keys)} entries to {out_bin}")
