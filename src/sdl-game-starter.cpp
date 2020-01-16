#ifdef _WIN32
#include<windows.h>
#include<SDL.h>
#include<memoryapi.h>
#endif
#ifdef __linux__
#include<SDL2/SDL.h>
#include<sys/mman.h>
#endif

#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<assert.h>

#ifdef CONSOLE_DEBUG
#define DEBUG_PRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DEBUG_ASSERT(E) assert(E)
#else
#define DEBUG_PRINTF(...)
#define DEBUG_ASSERT(E)
#endif

#define FATAL_PRINTF(...)               \
    do                                  \
    {                                   \
        fprintf(stderr, __VA_ARGS__);   \
        exit(1);                        \
    } while(false)

/*
#define SDL_TRY_CALL(f, ...)            \
    do                                  \
    {                                   \
        if (f(__VA_ARGS__))             \
        {                               \
            FATAL_PRINTF("SDL Error: %s\n", SDL_GetError());
        }                       \
    } while(false)
*/

// large alloc and free
#ifdef _WIN32
#define LARGE_ALLOC(X) VirtualAlloc(NULL, X, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
#define LARGE_FREE(X,Y) DEBUG_ASSERT(VirtualFree(X, 0, MEM_RELEASE))
#endif
#ifdef __linux__
// NOT TESTED
#define LARGE_ALLOC(X) mmap(NULL, X, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0)
#define LARGE_FREE(X,Y) munmap(X, Y)
#endif


static bool running;

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* texture = NULL;
static void* pixels = NULL;
static int tex_width;
static int tex_height;
static const int BYTES_PER_PIXEL = 4;

static void render_gradient(int offset)
{
    DEBUG_ASSERT(pixels);

    int pitch = tex_width*BYTES_PER_PIXEL;
    uint8_t* row = (uint8_t*)pixels;
    for (int r = 0; r < tex_height; ++r)
    {
        uint8_t * byte = row;
        for (int c = 0; c < tex_width; ++c)
        {
            // little endian
            // B
            *byte = (uint8_t)(c + offset);
            byte++;

            // G
            *byte = (uint8_t)(r + offset);
            byte++;

            // R
            *byte = 0x00;
            byte++;

            // padding byte
            //*byte = 0x00;
            byte++;
        }
        row += pitch;
    }
}

static void update_texture()
{
    DEBUG_ASSERT(texture);    

    SDL_UpdateTexture(texture, NULL, pixels, tex_width * BYTES_PER_PIXEL);
}

static void create_texture(int width, int height)
{
    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        width,
        height);

    if(texture == NULL)
    {
        FATAL_PRINTF("Window could not be created - SDL_Error: %s\n", SDL_GetError());
    }

    pixels = LARGE_ALLOC(width * height * BYTES_PER_PIXEL);
    if(pixels == NULL)
    {
        FATAL_PRINTF("Couldn't allocate pixels buffer");
    }
    tex_width = width;
    tex_height = height;
}

static void free_texture()
{
    if (texture)
    {
        SDL_DestroyTexture(texture);
    }
    if (pixels)
    {
        LARGE_FREE(pixels, tex_width * tex_height * BYTES_PER_PIXEL);
    }
}

static void handle_event(SDL_Event* e)
{
    switch(e->type)
    {
        case SDL_QUIT:
            running = false;

        case SDL_WINDOWEVENT:
        {
            switch(e->window.event)
            {
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                {
                    DEBUG_PRINTF("Resizing window (%d, %d)\n", e->window.data1, e->window.data2);
                    free_texture();
                    create_texture(e->window.data1, e->window.data2);
                    break;
                }
            }
        } break;
    }
}


int main(int argc, char* args[])
{
    int width = 800;
    int height = 600;

    // Init SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        FATAL_PRINTF("SDL couldn't be initialized - SDL_Error: %s\n", SDL_GetError());
    }

    // Create Window
    window = SDL_CreateWindow(
        "Game",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        width, height,
        SDL_WINDOW_RESIZABLE);

    if(window == NULL)
    {
        FATAL_PRINTF("Window could not be created - SDL_Error: %s\n", SDL_GetError());
    }

    // Create Renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if(renderer == NULL)
    {
        FATAL_PRINTF("Renderer could not be created - SDL_Error: %s\n", SDL_GetError());
    }

    create_texture(width, height);

    // Loop
    running = true;
    SDL_Event e;
    int offset = 0;

    while(running)
    {

        SDL_SetRenderDrawColor(renderer, 0x50, 0x00, 0x50, 0xFF);
        SDL_RenderClear(renderer);

        render_gradient(offset++);
        update_texture();
        SDL_RenderCopy(renderer, texture, NULL, NULL);

        SDL_RenderPresent(renderer);

        while (SDL_PollEvent(&e))
        {
            handle_event(&e);
        }
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
