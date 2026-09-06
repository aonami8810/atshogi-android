#include "app/src/main/cpp/ShogiBitboardCore.h"
#include "app/src/main/cpp/MeraTensorNetworkNEON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    atshogi_init_tables();
    printf("=================================================================\n");
    printf("  Testing Startpos Move Selection with Pure Tensor Contraction   \n");
    printf("=================================================================\n\n");

    ShogiBoard b;
    atshogi_from_sfen("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1", &b);

    char best[64];
    atshogi_select_best_move_k40(&b, best, sizeof(best));
    printf("Engine Selected Startpos Move: bestmove %s\n\n", best);

    return 0;
}
