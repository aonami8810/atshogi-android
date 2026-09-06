#include <stdio.h>
#include <stdlib.h>
struct LocalTensorContextSoA { float potentials[16][81]; float bond_weights[16][81]; };
int main() { FILE* f = fopen("static_joseki.bin", "rb"); if (!f) return 1; LocalTensorContextSoA ctx; fread(&ctx, sizeof(ctx), 1, f); fclose(f); for(int lane=0; lane<16; ++lane) { printf("Lane %d: pot[0]=%f, pot[80]=%f\n", lane, ctx.potentials[lane][0], ctx.potentials[lane][80]); } return 0; }
