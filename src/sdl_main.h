#include"game_platform_interface.h"

#ifdef _WIN32
#include<windows.h>
#include<SDL.h>
#include<memoryapi.h>

#define GAME_CODE_OBJECT_FILE "game.dll"
#define LARGE_ALLOC(SZ) VirtualAlloc(NULL, (SZ), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
#define LARGE_ALLOC_FIXED(SZ, ADDR) VirtualAlloc((LPVOID)(ADDR), (SZ), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
#define LARGE_FREE(PTR,SZ) DEBUG_ASSERT(VirtualFree((PTR), 0, MEM_RELEASE))

#else   // _WIN32

#ifdef __linux__
#include<SDL2/SDL.h>
#include<sys/mman.h>

#define GAME_CODE_OBJECT_FILE "game.so"
#define LARGE_ALLOC(X) mmap(NULL, (X), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0)
// TODO fixed alloc on linux
#define LARGE_ALLOC_FIXED(SZ, ADDR) LARGE_ALLOC(SZ)
#define LARGE_FREE(X,Y) munmap((X), (Y))

#endif // __linux__
#endif // else _WIN32


struct AudioRingBuffer
{
    int size;
    int write_index;
    int play_index;
    void* data;
};

struct GameCode
{
    void* object;
    GameInitMemory* init_memory;
    GameUpdateAndRender* update_and_render;
};