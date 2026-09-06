#!/usr/bin/env python3
"""
ATShogi Multi-Temperature Self-Play Win-Rate Distribution Benchmark
===================================================================
Simulates self-play across varying tactical entropy levels (temperatures),
measuring strict mathematical convergence vs stochastic exploration dynamics.

Author: ATShogi Project
"""

import os
import sys
import numpy as np
from typing import Dict, List, Tuple
from collections import Counter

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
from scripts.egtb_mps_compressor import load_mps_binary, evaluate_mps_point


class ComprehensiveSelfPlayBenchmark:
    def __init__(self, atlas_path: str = "app/src/main/assets/engine/static_morse_atlas_512.atmp"):
        self.atlas_path = atlas_path
        self.cores = load_mps_binary(atlas_path)

    def run_temperature_experiment(self, noise_std: float, num_matches: int = 200, max_moves: int = 512) -> Dict:
        results = []
        depths = []

        for match_id in range(num_matches):
            s1 = np.random.randint(2, 6)
            s2 = np.random.randint(2, 6)
            a1 = np.random.randint(1, 7)
            a2 = np.random.randint(1, 7)
            d1 = np.random.randint(1, 7)
            d2 = 0
            turn = match_id % 2 # Alternate initial initiative

            game_over = False
            outcome = "Draw"
            final_move = max_moves

            for move in range(1, max_moves + 1):
                indices = [s1, s2, a1, a2, d1, d2, turn]
                val = evaluate_mps_point(self.cores, indices)

                # Checkmate / Sink thresholds
                if move >= 30 and val >= 75.0:
                    outcome = "Sente Win"
                    final_move = move
                    game_over = True
                    break
                elif move >= 30 and val <= -75.0:
                    outcome = "Gote Win"
                    final_move = move
                    game_over = True
                    break

                # Minimax decision
                if turn == 0:
                    best_c = (s1, a1)
                    best_v = -1e9
                    for ds in [-1, 0, 1]:
                        for da in [-1, 0, 1]:
                            ns1 = max(0, min(7, s1 + ds))
                            na1 = max(0, min(7, a1 + da))
                            v = evaluate_mps_point(self.cores, [ns1, s2, na1, a2, d1, d2, 1])
                            v_noisy = v + np.random.normal(0.0, noise_std)
                            if v_noisy > best_v:
                                best_v = v_noisy
                                best_c = (ns1, na1)
                    s1, a1 = best_c
                    turn = 1
                else:
                    best_c = (s2, d1)
                    best_v = 1e9
                    for ds in [-1, 0, 1]:
                        for dd in [-1, 0, 1]:
                            ns2 = max(0, min(7, s2 + ds))
                            nd1 = max(0, min(7, d1 + dd))
                            v = evaluate_mps_point(self.cores, [s1, ns2, a1, a2, nd1, d2, 0])
                            v_noisy = v + np.random.normal(0.0, noise_std)
                            if v_noisy < best_v:
                                best_v = v_noisy
                                best_c = (ns2, nd1)
                    s2, d1 = best_c
                    turn = 0

            if not game_over:
                outcome = "Draw (512-Move Limit)"
                final_move = max_moves

            results.append(outcome)
            depths.append(final_move)

        counts = Counter(results)
        s_wins = counts.get("Sente Win", 0)
        g_wins = counts.get("Gote Win", 0)
        draws = counts.get("Draw (512-Move Limit)", 0) + counts.get("Draw", 0)

        return {
            "noise_std": noise_std,
            "num_matches": num_matches,
            "sente_wins": s_wins,
            "gote_wins": g_wins,
            "draws": draws,
            "sente_rate": (s_wins / num_matches) * 100.0,
            "gote_rate": (g_wins / num_matches) * 100.0,
            "draw_rate": (draws / num_matches) * 100.0,
            "avg_depth": float(np.mean(depths))
        }


if __name__ == "__main__":
    bench = ComprehensiveSelfPlayBenchmark("app/src/main/assets/engine/static_morse_atlas_512.atmp")
    print("=================================================================")
    print("  ATShogi Multi-Temperature Self-Play Benchmark (512-Move Space)")
    print("=================================================================")

    # Test under 3 entropy regimes:
    # 1. Deterministic Equilibrium (Pure Mathematical Gradient Flow, noise=0.5)
    # 2. Competitive Tactical Noise (Equivalent to High-Level Engine Match, noise=3.0)
    # 3. High Entropy Chaos (Simulating Human Tactical Fluctuations, noise=8.0)
    for noise in [0.5, 3.0, 8.0]:
        res = bench.run_temperature_experiment(noise_std=noise, num_matches=200)
        print(f"\n[Tactical Entropy Noise sigma = {noise:.1f}]")
        print(f"  Matches: {res['num_matches']}")
        print(f"  Sente Wins : {res['sente_wins']:3d} ({res['sente_rate']:5.1f} %)")
        print(f"  Gote Wins  : {res['gote_wins']:3d} ({res['gote_rate']:5.1f} %)")
        print(f"  Draws      : {res['draws']:3d} ({res['draw_rate']:5.1f} %)")
        print(f"  Avg Moves  : {res['avg_depth']:.1f} moves")
    print("\n=================================================================\n")
