#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define open _open
#define close _close
#define fstat _fstat
#define stat _stat
#define O_RDONLY _O_RDONLY
#else
#include <unistd.h>
#include <sys/mman.h>
#endif

#pragma pack(push, 1)
typedef struct {
    uint64_t key81;           // 8 bytes: FNV-1a 64-bit canonical orbit hash
    uint16_t bestMovePacked;  // 2 bytes: Packed USI move
    uint16_t ponderMovePacked;// 2 bytes: Packed ponder move
    uint8_t  mateDistance;    // 1 byte : Quasi-metric distance to mate
    uint8_t  morseIndex;      // 1 byte : Critical Morse index gamma
    uint16_t flags;           // 2 bytes: Metadata flags
} BinAtlasEntry16; // 16 Bytes aligned
#pragma pack(pop)

typedef struct {
    int fd;
    void* pData;
    size_t fileSize;
    uint32_t numEntries;
} PosixAtlasHandle;

static PosixAtlasHandle* g_global_atlas = NULL;

extern "C" {

PosixAtlasHandle* atshogi_mmap_open_atlas(const char* filepath) {
    if (!filepath) return NULL;

    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        return NULL;
    }

    struct stat st;
    if (fstat(fd, &st) < 0 || st.st_size == 0) {
        close(fd);
        return NULL;
    }

    void* pData = NULL;
#ifdef _WIN32
    pData = malloc(st.st_size);
    if (pData) {
        if (_read(fd, pData, st.st_size) != st.st_size) {
            free(pData);
            pData = NULL;
        }
    }
#else
    pData = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (pData == MAP_FAILED) {
        pData = NULL;
    }
#endif

    if (!pData) {
        close(fd);
        return NULL;
    }

    PosixAtlasHandle* handle = (PosixAtlasHandle*)malloc(sizeof(PosixAtlasHandle));
    if (!handle) {
#ifdef _WIN32
        free(pData);
#else
        munmap(pData, st.st_size);
#endif
        close(fd);
        return NULL;
    }

    handle->fd = fd;
    handle->pData = pData;
    handle->fileSize = (size_t)st.st_size;
    handle->numEntries = (uint32_t)(handle->fileSize / sizeof(BinAtlasEntry16));

    return handle;
}

void atshogi_mmap_init_global(const char* filepath) {
    if (g_global_atlas) return;
    g_global_atlas = atshogi_mmap_open_atlas(filepath);
}

const BinAtlasEntry16* atshogi_mmap_lookup_key(uint64_t targetKey) {
    if (!g_global_atlas || !g_global_atlas->pData || g_global_atlas->numEntries == 0) return NULL;

    const BinAtlasEntry16* entries = (const BinAtlasEntry16*)g_global_atlas->pData;
    uint32_t low = 0;
    uint32_t high = g_global_atlas->numEntries - 1;

    while (low <= high) {
        uint32_t mid = low + (high - low) / 2;
        if (entries[mid].key81 == targetKey) {
            return &entries[mid];
        }
        if (entries[mid].key81 < targetKey) {
            low = mid + 1;
        } else {
            if (mid == 0) break;
            high = mid - 1;
        }
    }

    return NULL;
}

void atshogi_mmap_close_atlas(PosixAtlasHandle* handle) {
    if (!handle) return;
    if (handle->pData) {
#ifdef _WIN32
        free(handle->pData);
#else
        if (handle->pData != MAP_FAILED) {
            munmap(handle->pData, handle->fileSize);
        }
#endif
    }
    if (handle->fd >= 0) {
        close(handle->fd);
    }
    free(handle);
}

static size_t g_static_joseki_size = 0;

void* load_topological_tensor_mmap() {
    size_t mapping_size = 10368; // 16 * 81 * 2 * 4 bytes // sizeof(LocalTensorContextSoA)
#ifdef _WIN32
    // Open for read/write so we can backpropagate!
    FILE* f = fopen("static_joseki.bin", "r+b");
    if (!f) {
        // Create if missing
        f = fopen("static_joseki.bin", "wb");
        if (f) {
            void* zeros = calloc(1, mapping_size);
            // initialize bonds to 1.0f
            float* fptr = (float*)zeros;
            // potentials are first 16*81 = 1296 floats. bonds are next 1296 floats.
            for (int i = 1296; i < 2592; ++i) fptr[i] = 1.0f;
            fwrite(zeros, 1, mapping_size, f);
            free(zeros);
            fclose(f);
        }
        f = fopen("static_joseki.bin", "r+b");
        if (!f) return NULL;
    }
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size == 0) {
        fclose(f);
        return NULL;
    }
    
    HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(f));
    HANDLE hMapping = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, 0, 0, NULL);
    if (!hMapping) {
        fclose(f);
        return NULL;
    }
    
    void* pData = MapViewOfFile(hMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (pData) {
        g_static_joseki_size = size;
    }
    fclose(f);
    return pData;
#else
    int fd = open("static_joseki.bin", O_RDONLY);
    if (fd < 0) return NULL;
    struct stat st;
    if (fstat(fd, &st) < 0 || st.st_size == 0) {
        close(fd);
        return NULL;
    }
    void* pData = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (pData == MAP_FAILED) {
        close(fd);
        return NULL;
    }
    g_static_joseki_size = st.st_size;
    return pData;
#endif
}

size_t get_topological_tensor_mmap_size() {
    return g_static_joseki_size;
}

void encode_sfen_to_tensor_state(void* mmap_ptr, const char* sfen) {
    (void)mmap_ptr;
    (void)sfen;
}

} // extern "C"
