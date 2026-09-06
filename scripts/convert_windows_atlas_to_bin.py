import struct
import os
import sys

# cshogi または USI 座標パッカー
def pack_move(m_str):
    if not m_str or len(m_str) < 4:
        return 0
    if '*' in m_str:
        # 打つ: P*5b -> flag | (pt << 7) | sq
        p_char = m_str[0]
        pt_map = {'P': 1, 'L': 2, 'N': 3, 'S': 4, 'G': 5, 'B': 6, 'R': 7}
        pt = pt_map.get(p_char, 1)
        tf = int(m_str[2])
        tr = ord(m_str[3]) - ord('a') + 1
        to_sq = (9 - tf) * 9 + (tr - 1)
        return 0x8000 | (pt << 7) | to_sq
    else:
        ff = int(m_str[0])
        fr = ord(m_str[1]) - ord('a') + 1
        tf = int(m_str[2])
        tr = ord(m_str[3]) - ord('a') + 1
        from_sq = (9 - ff) * 9 + (fr - 1)
        to_sq = (9 - tf) * 9 + (tr - 1)
        promo = 0x4000 if m_str.endswith('+') else 0
        return promo | (from_sq << 7) | to_sq

# Windows 版 static_morse_atlas.txt から抽出
atlas_txt = r"c:\VS\Workspace\atshogi\atshogi_project\static_morse_atlas.txt"
out_bin = r"app\src\main\assets\engine\static_morse_atlas.bin"

entries = []

if os.path.exists(atlas_txt):
    print(f"[*] Reading Windows static_morse_atlas.txt...")
    with open(atlas_txt, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            parts = line.strip().split()
            if len(parts) >= 2:
                try:
                    key = int(parts[0])
                    best_mv = parts[1]
                    ponder_mv = parts[2] if len(parts) >= 3 else ""
                    best_packed = pack_move(best_mv)
                    ponder_packed = pack_move(ponder_mv)
                    entries.append((key, best_packed, ponder_packed))
                except ValueError:
                    continue

# キー順でソート（二分探索用）
entries.sort(key=lambda x: x[0])
print(f"[*] Total valid sorted entries: {len(entries)}")

# BinAtlasEntry16: uint64 key (8B), uint16 best (2B), uint16 ponder (2B), uint8 mate (1B), uint8 morse (1B), uint16 flags (2B) = 16B
with open(out_bin, "wb") as f:
    for key, best, ponder in entries:
        f.write(struct.pack("<QHHBBH", key, best, ponder, 0, 0, 0))

print(f"[OK] Successfully exported {out_bin} ({os.path.getsize(out_bin):,} bytes)")
