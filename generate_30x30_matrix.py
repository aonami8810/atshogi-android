import subprocess
import time

sente_moves = [
    "1g1f", "2g2f", "3g3f", "4g4f", "5g5f", "6g6f", "7g7f", "8g8f", "9g9f",
    "1i1h", "9i9h", "2i3h", "2i1h", "8i7h", "8i9h", "3i4h", "3i3h", "3i2h", 
    "7i6h", "7i7h", "7i8h", "4i5h", "4i4h", "4i3h", "6i5h", "6i6h", "6i7h",
    "5i6h", "5i5h", "5i4h"
]
gote_moves = [
    "1c1d", "2c2d", "3c3d", "4c4d", "5c5d", "6c6d", "7c7d", "8c8d", "9c9d",
    "1a1b", "9a9b", "2a3b", "2a1b", "8a7b", "8a9b", "3a4b", "3a3b", "3a2b",
    "7a6b", "7a7b", "7a8b", "4a5b", "4a4b", "4a3b", "6a5b", "6a6b", "6a7b",
    "5a6b", "5a5b", "5a4b"
]

proc = subprocess.Popen(["build/atshogi_engine_host.exe"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

proc.stdin.write("isready\n")
proc.stdin.flush()

for line in proc.stdout:
    if "readyok" in line:
        break

matrix = {}

for sm in sente_moves:
    matrix[sm] = {}
    for gm in gote_moves:
        proc.stdin.write(f"position startpos moves {sm} {gm}\ngo\n")
        proc.stdin.flush()
        
        best_val = -9999.0
        while True:
            line = proc.stdout.readline().strip()
            if line.startswith("Move ") and "val=" in line:
                val_str = line.split("val=")[1]
                val = float(val_str)
                if val > best_val:
                    best_val = val
            if line.startswith("bestmove"):
                break
        matrix[sm][gm] = best_val

proc.terminate()

with open("matrix_output.md", "w") as f:
    f.write("| Sente \\ Gote | " + " | ".join(gote_moves[:15]) + " | ... |\n")
    f.write("|---|" + "|".join(["---"] * 16) + "\n")

    for sm in sente_moves:
        row = [sm]
        for gm in gote_moves[:15]:
            val = matrix[sm][gm]
            row.append(f"{val:+.1f}")
        row.append("...")
        f.write("| " + " | ".join(row) + " |\n")

import csv
with open("30x30_matrix.csv", "w", newline="") as f:
    writer = csv.writer(f)
    writer.writerow(["Sente \\ Gote"] + gote_moves)
    for sm in sente_moves:
        writer.writerow([sm] + [matrix[sm][gm] for gm in gote_moves])
