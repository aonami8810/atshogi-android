#include "app/src/main/cpp/ShogiBitboardCore.h"
#include "app/src/main/cpp/MeraTensorNetworkNEON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    atshogi_init_tables();
    printf("=================================================================\n");
    printf("  Decomposing Startpos Legal Moves and Evaluation Components     \n");
    printf("=================================================================\n\n");

    ShogiBoard b;
    atshogi_from_sfen("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1", &b);

    Move16 moves[600];
    int count = atshogi_generate_legal_moves(&b, moves);

    for (int i = 0; i < count; ++i) {
        char mv_str[16];
        atshogi_move_to_usi(moves[i], mv_str);

        ShogiBoard next_b;
        atshogi_apply_move(&b, moves[i], &next_b);

        Simplex243 s;
        atshogi_get_simplex243(&next_b, &s);
        int black_control = bb_popcount(s.layer_black);
        int white_control = bb_popcount(s.layer_white);
        float control_diff = (float)(black_control - white_control) * 8.0f;

        printf("Move %2d: %-6s -> Black Control: %2d, White Control: %2d, Diff: %+6.1f\n",
               i + 1, mv_str, black_control, white_control, control_diff);
    }

    return 0;
}
