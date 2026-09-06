# -*- coding: utf-8 -*-
"""
ATShogi-k40 Kifu Formatter
"""

import sys
import io

if sys.platform == 'win32':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

moves = [
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

kanji_num = {"1":"一", "2":"二", "3":"三", "4":"四", "5":"五", "6":"六", "7":"七", "8":"八", "9":"九"}
kanji_zenkaku = {"1":"１", "2":"２", "3":"３", "4":"４", "5":"５", "6":"６", "7":"７", "8":"８", "9":"９"}
piece_name = {"P":"歩", "L":"香", "N":"桂", "S":"銀", "G":"金", "B":"角", "R":"飛", "K":"玉"}

def to_japanese_move(ply, m, prev_dest):
    turn_mark = "▲" if ply % 2 == 1 else "△"
    if '*' in m:
        p = piece_name[m[0]]
        col = kanji_zenkaku[m[2]]
        row = kanji_num[str(ord(m[3]) - ord('a') + 1)]
        return f"{turn_mark}{col}{row}{p}打", (m[2], str(ord(m[3]) - ord('a') + 1))
    else:
        from_c = m[0]
        from_r = str(ord(m[1]) - ord('a') + 1)
        to_c = m[2]
        to_r = str(ord(m[3]) - ord('a') + 1)
        is_prom = m.endswith('+')

        to_str = f"{kanji_zenkaku[to_c]}{kanji_num[to_r]}"
        if prev_dest == (to_c, to_r):
            to_str = "同　"
        
        prom_str = "成" if is_prom else ""
        return f"{turn_mark}{to_str}{prom_str}({from_c}{from_r})", (to_c, to_r)

print("=== 棋譜一覧 (全99手) ===")
prev_dest = None
formatted_moves = []
for i, m in enumerate(moves):
    ply = i + 1
    jp_str, prev_dest = to_japanese_move(ply, m, prev_dest)
    formatted_moves.append(f"{ply:2d}. {jp_str:<12} ({m})")

for i in range(0, len(formatted_moves), 3):
    print("  ".join(formatted_moves[i:i+3]))
