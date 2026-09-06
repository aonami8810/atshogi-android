cmake_minimum_required(VERSION 3.22.1)

project("atshogi_native")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ATShogi Pure Mathematical Engine (Android 15 W^X Compatible PIE Standalone USI Engine)
add_executable(
    atshogi_engine
    UsiEngineProcessAdapter.cpp
    cshogi_core.c
    android_bionic_compat.c
    OrbifoldMorseNEON.cpp
    TaylorKernelNEON.cpp
    GaifullinP2NEON.cpp
    EgtbMpsNEON.cpp
    DecayKernel.cpp
    BlunderDetector.cpp
    StratifiedCobordism40.cpp
    MeraTensorNetworkNEON.cpp
    PosixAtlasLoader.cpp
)












set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,max-page-size=65536 -Wl,-z,common-page-size=65536")

set_target_properties(atshogi_engine PROPERTIES
    OUTPUT_NAME "atshogi_engine"
    PREFIX "lib"
    SUFFIX ".so"
    POSITION_INDEPENDENT_CODE TRUE
)

target_compile_options(
    atshogi_engine
    PRIVATE
    -O3
    -flto
    -ffast-math
    -fomit-frame-pointer
    -D__ARM_NEON
)

target_link_options(
    atshogi_engine
    PRIVATE
    -pie
    -flto
    -static-libstdc++
    -Wl,-z,max-page-size=65536
    -Wl,-z,common-page-size=65536
)

target_link_libraries(
    atshogi_engine
    PRIVATE
    log
)
