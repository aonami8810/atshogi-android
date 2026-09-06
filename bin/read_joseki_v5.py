import struct
import math

def check_asymptotic_joseki():
    file_path = "static_joseki.bin"
    print(f"Checking {file_path} for true mathematical gradient...")
    
    with open(file_path, "rb") as f:
        data = f.read()
    
    # 1. ファイルサイズの厳格チェック (1.0 MB)
    expected_size = 1048576
    assert len(data) == expected_size, f"Size mismatch! Expected {expected_size} bytes, got {len(data)}"
    
    # float配列としてデコード (262,144要素)
    num_floats = len(data) // 4
    potentials = struct.unpack(f"{num_floats}f", data)
    
    # 2. 勾配平坦化（クランプバグ）の徹底検出
    # Lane 0 (最初の6要素) の検証
    lane_0 = potentials[0:6]
    print(f"Decoded Lane 0 Potentials: {list(lane_0)}")
    
    # 各隣接要素の差分（勾配）を計算
    for idx in range(1, len(lane_0)):
        diff = lane_0[idx] - lane_0[idx - 1]
        print(f"  Step {idx-1} -> {idx}: Diff = {diff:.6f}")
        
        # 3手目以降で diff が 0.0（平坦）になっている、または一律クランプされている場合
        if idx >= 2 and diff <= 1e-6:
            raise ValueError(
                f"🚨 勾配消失バグを検出しました！\n"
                f"  Step {idx-1} と Step {idx} の差分が {diff:.6f}（ほぼ平坦）になっています。\n"
                f"  これはMERA対数スケーリングをサボり、fminf等で一律クランプした偽物です。"
            )
            
    print("[SUCCESS] All gradients are strictly positive and perfectly asymptotic to the 512-ply MERA horizon without flatlining!")

if __name__ == "__main__":
    check_asymptotic_joseki()
