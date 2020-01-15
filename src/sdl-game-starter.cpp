#ifdef _WIN32
#include<SDL.h>
#endif
#ifdef __linux__
#include<SDL2/SDL.h>
#endif

#include<stdio.h>
#include<stdlib.h>

#ifdef CONSOLE_DEBUG
#define DEBUG_PRINTF(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_PRINTF(...)
#endif

#define FATAL_PRINTF(...)               \
    do                                  \
    {                                   \
        fprintf(stderr, __VA_ARGS__);   \
        exit(1);                        \
    } while(false)                      \

/*
#define SDL_TRY_CALL(f, ...)            \
    do                                  \
    {                                   \
        if (f(__VA_ARGS__))             \
        {                               \
            FATAL_PRINTF("SDL Error: %s\n", SDL_GetError());
        }                       \
    } while(false)              \
*/

static bool running;

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* texture = NULL;
static void* pixels = NULL;
static int tex_width;
static const int BYTES_PER_PIXEL = 4;

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

    pixels = malloc(width * height * BYTES_PER_PIXEL);
    if(pixels == NULL)
    {
        FATAL_PRINTF("Couldn't allocate pixels buffer");
    }
    tex_width = width;
}

static void free_texture()
{
    if (texture)
    {
        SDL_DestroyTexture(texture);
    }
    if (pixels)
    {
        free(pixels);
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

    while(running)
    {

        SDL_SetRenderDrawColor(renderer, 0x50, 0x00, 0x50, 0xFF);
        SDL_RenderClear(renderer);

        if (SDL_UpdateTexture(texture, NULL, pixels, tex_width * BYTES_PER_PIXEL))
        {
            FATAL_PRINTF("Could not update texture - SDL_Error: %s\n", SDL_GetError());
        }
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
