#!/usr/bin/env python3
"""
ATShogi EGTB Tensor Decomposition & Compression (TNRG / MPS)
============================================================
Endgame Tablebase (EGTB) and Opening Atlas tensor network decomposition
using Tensor Train Singular Value Decomposition (TT-SVD).

Author: ATShogi Project
"""

import struct
import numpy as np
from typing import List, Tuple, Optional


def compress_to_mps(
    tensor: np.ndarray,
    max_bond_dim: int = 4,
    eps: float = 1e-5
) -> List[np.ndarray]:
    """
    Decomposes an N-dimensional tensor into Matrix Product State (MPS / Tensor Train) cores.

    Parameters:
        tensor (np.ndarray): Input tensor of shape (d_1, d_2, ..., d_N).
        max_bond_dim (int): Maximum virtual bond dimension chi.
        eps (float): Singular value truncation threshold.

    Returns:
        cores (list of np.ndarray): List of N cores.
            cores[0] ~ cores[N-2]: shape [r_k-1, d_k, r_k]
            cores[N-1]: shape [r_N-1, d_N]
    """
    shape = tensor.shape
    N = len(shape)
    C = tensor.astype(np.float32).copy()
    cores = []
    r_prev = 1

    for k in range(N - 1):
        d_k = shape[k]
        # Reshape to 2D matrix: (r_prev * d_k) x (d_{k+1} * ... * d_N)
        rows = r_prev * d_k
        cols = C.size // rows
        C_matrix = C.reshape(rows, cols)

        # Singular Value Decomposition (SVD)
        U, S, Vt = np.linalg.svd(C_matrix, full_matrices=False)

        # Determine rank truncation
        r_k = int(np.sum(S > eps))
        r_k = min(r_k, max_bond_dim)
        r_k = max(1, r_k)

        # Truncate
        U_tr = U[:, :r_k]
        S_tr = S[:r_k]
        Vt_tr = Vt[:r_k, :]

        # Extract core: shape [r_prev, d_k, r_k]
        core = U_tr.reshape(r_prev, d_k, r_k)
        cores.append(core)

        # Prepare remaining tensor
        C = np.diag(S_tr) @ Vt_tr
        r_prev = r_k

    # Final core: [r_{N-1}, d_N]
    cores.append(C)
    return cores


def reconstruct_from_mps(cores: List[np.ndarray]) -> np.ndarray:
    """
    Reconstructs the full N-dimensional tensor from MPS cores.
    """
    C = cores[0]
    for k in range(1, len(cores)):
        next_core = cores[k]
        C = np.tensordot(C, next_core, axes=(-1, 0))
    return C


def evaluate_mps_point(cores: List[np.ndarray], indices: List[int]) -> float:
    """
    Evaluates the tensor at a specific multi-index (i_1, i_2, ..., i_N) in O(N * chi^2) time
    without reconstructing the full tensor.
    """
    N = len(cores)
    assert len(indices) == N, f"Indices count {len(indices)} != core count {N}"

    # Initial vector: v = [1.0] (dimension 1)
    v = np.array([1.0], dtype=np.float32)

    for k in range(N - 1):
        idx = indices[k]
        # core shape: [r_in, d_k, r_out]
        # slice M: [r_in, r_out]
        M = cores[k][:, idx, :]
        v = v @ M

    # Final core: [r_last, d_N]
    final_idx = indices[-1]
    final_col = cores[-1][:, final_idx]
    result = float(v @ final_col)
    return result


def export_mps_binary(cores: List[np.ndarray], filename: str) -> None:
    """
    Exports MPS cores to ATShogi Native Binary Format (.atmp).

    Binary Layout:
      - Header:
          - Magic: b'ATMP' (4 bytes)
          - Version: uint32 (1)
          - NumCores: uint32 (N)
      - For each core k in 0..N-1:
          - r_in: uint32
          - d: uint32
          - r_out: uint32
          - Payload: float32 array formatted as [d, r_in, r_out] for contiguous slice access.
            For final core [r_in, d], stored as [d, r_in, 1] with r_out=1.
    """
    with open(filename, "wb") as f:
        # Magic & Header
        f.write(b"ATMP")
        f.write(struct.pack("<II", 1, len(cores)))

        for k, core in enumerate(cores):
            if k < len(cores) - 1:
                # Shape: [r_in, d, r_out] -> Transpose to [d, r_in, r_out]
                r_in, d, r_out = core.shape
                reordered = np.transpose(core, (1, 0, 2)).astype(np.float32)
            else:
                # Final core: [r_in, d] -> Transpose to [d, r_in, 1]
                r_in, d = core.shape
                r_out = 1
                reordered = np.transpose(core, (1, 0)).reshape(d, r_in, 1).astype(np.float32)

            f.write(struct.pack("<III", r_in, d, r_out))
            f.write(reordered.tobytes())


def load_mps_binary(filename: str) -> List[np.ndarray]:
    """
    Loads MPS cores from ATShogi Native Binary Format (.atmp).
    """
    cores = []
    with open(filename, "rb") as f:
        magic = f.read(4)
        if magic != b"ATMP":
            raise ValueError(f"Invalid ATMP magic: {magic}")
        version, num_cores = struct.unpack("<II", f.read(8))

        for k in range(num_cores):
            r_in, d, r_out = struct.unpack("<III", f.read(12))
            count = d * r_in * r_out
            raw_data = f.read(count * 4)
            arr = np.frombuffer(raw_data, dtype=np.float32).reshape(d, r_in, r_out)

            if k < num_cores - 1:
                # Transpose back: [d, r_in, r_out] -> [r_in, d, r_out]
                core = np.transpose(arr, (1, 0, 2))
            else:
                # Transpose back: [d, r_in, 1] -> [r_in, d]
                core = np.transpose(arr.reshape(d, r_in), (1, 0))
            cores.append(core)
    return cores


def benchmark_egtb_compression():
    """
    Simulates Endgame Tablebase (EGTB) compression and verifies reconstruction accuracy.
    """
    print("=================================================================")
    print("  ATShogi EGTB / Atlas Tensor Network (MPS) Benchmark & Verification")
    print("=================================================================")

    # Scenario: 4-Site Shogi Endgame Space (e.g. King1 (16) x King2 (16) x Gold (16) x Turn (2))
    # Total states = 16 x 16 x 16 x 2 = 8,192 states
    d1, d2, d3, d4 = 16, 16, 16, 2
    raw_shape = (d1, d2, d3, d4)
    raw_size = int(np.prod(raw_shape))

    # Generate synthetic smooth topological evaluation field
    x = np.linspace(-1.0, 1.0, d1)
    y = np.linspace(-1.0, 1.0, d2)
    z = np.linspace(-1.0, 1.0, d3)
    t = np.array([1.0, -1.0])

    X, Y, Z, T = np.meshgrid(x, y, z, t, indexing="ij")
    # Quadratic potential field with low-frequency spatial entanglement
    egtb_raw = -(X**2 + 0.5 * Y**2 + 0.2 * Z**2) * T + 0.05 * np.sin(X * Y)

    print(f"\n[1] Original EGTB Tensor Shape: {raw_shape}")
    print(f"    Total State Count: {raw_size:,} float32 elements ({raw_size * 4 / 1024:.2f} KB)")

    # Compress with bond dimension chi = 4
    max_bond = 4
    cores = compress_to_mps(egtb_raw, max_bond_dim=max_bond, eps=1e-6)

    total_compressed_params = sum(c.size for c in cores)
    compression_ratio = raw_size / total_compressed_params

    print(f"\n[2] Compressed MPS Structure (Max Bond Dimension = {max_bond}):")
    for i, c in enumerate(cores):
        print(f"    Site {i + 1} Core Shape: {c.shape} (Params: {c.size})")
    print(f"    Total Compressed Params: {total_compressed_params:,} ({total_compressed_params * 4 / 1024:.2f} KB)")
    print(f"    Compression Ratio: {compression_ratio:.2f}x ({(1.0 / compression_ratio) * 100:.2f}% of original)")

    # Accuracy check
    reconstructed = reconstruct_from_mps(cores)
    rel_l2_err = np.linalg.norm(egtb_raw - reconstructed) / np.linalg.norm(egtb_raw)
    max_abs_err = np.max(np.abs(egtb_raw - reconstructed))

    print(f"\n[3] Reconstruction Quality:")
    print(f"    Relative L2 Error: {rel_l2_err:.2e}")
    print(f"    Max Absolute Error: {max_abs_err:.2e}")

    # Point evaluation test
    test_indices = [7, 8, 5, 0]
    expected_val = float(egtb_raw[tuple(test_indices)])
    fast_eval_val = evaluate_mps_point(cores, test_indices)
    print(f"\n[4] O(N * chi^2) On-Demand Evaluation Test:")
    print(f"    Query Index: {test_indices}")
    print(f"    Exact Value:         {expected_val:.6f}")
    print(f"    MPS Eval Value:      {fast_eval_val:.6f}")
    print(f"    Point Absolute Diff: {abs(expected_val - fast_eval_val):.2e}")

    # Binary export and reload verification
    binary_path = "egtb_test_model.atmp"
    export_mps_binary(cores, binary_path)
    reloaded_cores = load_mps_binary(binary_path)

    reloaded_val = evaluate_mps_point(reloaded_cores, test_indices)
    assert np.isclose(fast_eval_val, reloaded_val), "Binary serialization mismatch!"
    print(f"\n[5] Binary Serialization (.atmp):")
    print(f"    Exported to: {binary_path}")
    print(f"    Reloaded & Verified: Success (Value={reloaded_val:.6f})")
    print("=================================================================\n")


def benchmark_egtbl7_compression():
    """
    Simulates 7-Piece Endgame Tablebase (EGTBL 7.0 / k=7) compression using TT-SVD (MPS).
    Sites: [SenteKing(8), GoteKing(8), Attacker1(8), Attacker2(8), Defender1(8), Defender2(8), HandTurn(2)]
    Total State Count = 8^6 * 2 = 524,288 states
    """
    print("=================================================================")
    print("  ATShogi EGTBL 7.0 (k=7 Sites) Tensor Network Benchmark")
    print("=================================================================")

    # 7-Site tensor dimensions
    shape_7 = (8, 8, 8, 8, 8, 8, 2)
    raw_size = int(np.prod(shape_7))
    print(f"\n[1] EGTBL 7.0 Raw Tensor Shape: {shape_7}")
    print(f"    Total State Count: {raw_size:,} float32 elements ({raw_size * 4 / 1024:.2f} KB / {raw_size * 4 / (1024*1024):.2f} MB)")

    # Generate synthetic smooth 7-dimensional Morse potential field
    axes = [np.linspace(-1.0, 1.0, d) for d in shape_7[:6]]
    axes.append(np.array([1.0, -1.0])) # Hand/Turn

    grids = np.meshgrid(*axes, indexing="ij")
    X1, X2, X3, X4, X5, X6, T = grids

    # Topological multi-body potential field
    egtb7_raw = -(X1**2 + 0.8*X2**2 + 0.5*X3**2 + 0.4*X4**2 + 0.3*X5**2 + 0.2*X6**2) * T + 0.02 * np.sin(X1 * X2 + X3 * X4)

    # Compress with bond dimension chi = 6
    max_bond = 6
    cores7 = compress_to_mps(egtb7_raw, max_bond_dim=max_bond, eps=1e-5)

    total_params = sum(c.size for c in cores7)
    compression_ratio = raw_size / total_params

    print(f"\n[2] Compressed EGTBL 7.0 Structure (Max Bond Dim chi = {max_bond}):")
    for i, c in enumerate(cores7):
        print(f"    Site {i + 1} Core Shape: {c.shape} (Params: {c.size})")
    print(f"    Total Compressed Params: {total_params:,} ({total_params * 4 / 1024:.2f} KB)")
    print(f"    Compression Ratio: {compression_ratio:.2f}x ({(1.0 / compression_ratio) * 100:.2f}% of original)")

    # Accuracy check
    reconstructed = reconstruct_from_mps(cores7)
    rel_l2_err = np.linalg.norm(egtb7_raw - reconstructed) / np.linalg.norm(egtb7_raw)
    max_abs_err = np.max(np.abs(egtb7_raw - reconstructed))

    print(f"\n[3] Reconstruction Quality:")
    print(f"    Relative L2 Error: {rel_l2_err:.2e}")
    print(f"    Max Absolute Error: {max_abs_err:.2e}")

    # Point evaluation test
    test_idx = [3, 4, 2, 5, 1, 6, 0]
    expected_val = float(egtb7_raw[tuple(test_idx)])
    fast_eval_val = evaluate_mps_point(cores7, test_idx)
    print(f"\n[4] O(k=7) On-Demand Evaluation Test:")
    print(f"    Query Index (7-Site): {test_idx}")
    print(f"    Exact Value:         {expected_val:.6f}")
    print(f"    MPS 7-Site Value:    {fast_eval_val:.6f}")
    print(f"    Point Absolute Diff: {abs(expected_val - fast_eval_val):.2e}")

    # Export binary
    binary_path = "app/src/main/assets/engine/egtbl7_sample.atmp"
    export_mps_binary(cores7, binary_path)
    print(f"\n[5] Exported EGTBL 7.0 Sample to {binary_path} ({total_params * 4 / 1024:.2f} KB)")
    print("=================================================================\n")


if __name__ == "__main__":
    benchmark_egtb_compression()
    benchmark_egtbl7_compression()
