#ifdef _WIN32
#include<SDL.h>
#endif
#ifdef __linux__
#include<SDL2/SDL.h>
#endif

#include<stdio.h>

#ifdef CONSOLE_DEBUG
#define DEBUG_PRINTF(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_PRINTF(...)
#endif

SDL_Window * window = NULL;
SDL_Renderer * renderer = NULL;

void init_sdl(int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        DEBUG_PRINTF("SDL couldn't be initialized - SDL_Error: %s\n", SDL_GetError());
        exit(1);
    }

    // Create Window
    window = SDL_CreateWindow(
        "Game",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        width, height, SDL_WINDOW_RESIZABLE);

    if(window == NULL) {
        DEBUG_PRINTF("Window could not be created - SDL_Error: %s\n", SDL_GetError());
        exit(1);
    }

    // Create Renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if(renderer == NULL) {
        DEBUG_PRINTF("Renderer could not be created - SDL_Error: %s\n", SDL_GetError());
        exit(1);
    }
}

bool handle_event(SDL_Event* e)
{

    switch(e->type)
    {
        case SDL_QUIT:
            return true;

        case SDL_WINDOWEVENT:
        {
            switch(e->window.event)
            {
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                {
                    DEBUG_PRINTF("SDL_WINDOWEVENT_RESIZED (%d, %d)\n", e->window.data1, e->window.data2);
                    break;
                }
            }
        } break;
    }
    return false;
}


int main(int argc, char* args[])
{

    init_sdl(800, 600);
    
    SDL_Event e;
    bool quit = false;

    while(!quit) {

        SDL_SetRenderDrawColor(renderer, 0x30, 0x00, 0x30, 0xFF);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);

        while (SDL_PollEvent(&e) != 0) {
            quit = handle_event(&e);
        }
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
