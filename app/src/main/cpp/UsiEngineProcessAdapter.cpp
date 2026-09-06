#include <cassert>
#include <cstdio>

/**
 * ATShogi: Pure Stateless General USI Engine Process Adapter (AArch64 PIE)
 *
 * Architecture Invariants:
 *  1. Pure transparent proxy. Zero USI command parsing, zero internal states.
 *  2. Process initialization and standard I/O pipe assertion (stdin / stdout / stderr).
 *  3. Unconditional direct delegation to atshogi_main().
 */

extern "C" void atshogi_main();

int main(int argc, char* argv[]) {
    // 1. Pipeline Stream Invariants: Ensure valid standard I/O stream descriptors
    assert(stdin != nullptr);
    assert(stdout != nullptr);
    assert(stderr != nullptr);

    // 2. Direct unconditional delegation to atshogi-k40 engine core
    atshogi_main();

    return 0;
}
