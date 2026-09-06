# -*- coding: utf-8 -*-
"""
ATShogi-k40 Accurate Board Renderer with cshogi
"""

import sys
import io
import cshogi

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

board = cshogi.Board()
for idx, m in enumerate(moves):
    move_val = board.move_from_usi(m)
    board.push(move_val)

kanji_rows = ["一", "二", "三", "四", "五", "六", "七", "八", "九"]

piece_display = {
    0: " ・ ",
    # 先手
    cshogi.BPAWN: " 歩 ", cshogi.BLANCE: " 香 ", cshogi.BKNIGHT: " 桂 ", cshogi.BSILVER: " 銀 ",
    cshogi.BGOLD: " 金 ", cshogi.BBISHOP: " 角 ", cshogi.BROOK: " 飛 ", cshogi.BKING: " 玉 ",
    cshogi.BPROM_PAWN: " と ", cshogi.BPROM_LANCE: " 杏 ", cshogi.BPROM_KNIGHT: " 圭 ", cshogi.BPROM_SILVER: " 全 ",
    cshogi.BPROM_BISHOP: " 馬 ", cshogi.BPROM_ROOK: " 竜 ",
    # 後手
    cshogi.WPAWN: "v歩 ", cshogi.WLANCE: "v香 ", cshogi.WKNIGHT: "v桂 ", cshogi.WSILVER: "v銀 ",
    cshogi.WGOLD: "v金 ", cshogi.WBISHOP: "v角 ", cshogi.WROOK: "v飛 ", cshogi.WKING: "v玉 ",
    cshogi.WPROM_PAWN: "vと ", cshogi.WPROM_LANCE: "v杏 ", cshogi.WPROM_KNIGHT: "v圭 ", cshogi.WPROM_SILVER: "v全 ",
    cshogi.WPROM_BISHOP: "v馬 ", cshogi.WPROM_ROOK: "v竜 "
}

hand_names = ["歩", "香", "桂", "銀", "金", "角", "飛"]
# hand indices: 0:歩, 1:香, 2:桂, 3:銀, 4:金, 5:角, 6:飛
hand_order = [6, 5, 4, 3, 2, 1, 0] # 飛 角 金 銀 桂 香 歩

def format_hand(pieces_in_hand, color):
    items = []
    for h in hand_order:
        cnt = pieces_in_hand[color][h]
        name = hand_names[h]
        if cnt == 1:
            items.append(name)
        elif cnt > 1:
            items.append(f"{name}{cnt}")
    return " ".join(items) if items else "なし"

def render_board(b):
    lines = []
    lines.append(f"後手の持駒: {format_hand(b.pieces_in_hand, cshogi.WHITE)}")
    lines.append("  ９  ８  ７  ６  ５  ４  ３  ２  １")
    lines.append("+---------------------------+")

    for row in range(9):
        row_str = "|"
        for col in range(9):
            # cshogi square index: col 0 is 9筋 (square 0..8 for 9筋, 9..17 for 8筋, etc.)
            # col*9 + row
            sq = col * 9 + row
            p = b.pieces[sq]
            row_str += piece_display.get(p, " ？ ")
        row_str += f"| {kanji_rows[row]}"
        lines.append(row_str)
    
    lines.append("+---------------------------+")
    lines.append(f"先手の持駒: {format_hand(b.pieces_in_hand, cshogi.BLACK)}")

    return "\n".join(lines)

print("=== 正確な終局図 (99手目 ▲5二飛成 まで) ===")
print(render_board(board))
print()
print("SFEN:", board.sfen())
