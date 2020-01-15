#ifdef _WIN32
#include<SDL.h>
#endif
#ifdef __linux__
#include<SDL2/SDL.h>
#endif

#include<stdio.h>

SDL_Window * window = NULL;
SDL_Renderer * renderer = NULL;

void init_sdl(int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL couldn't be initialized - SDL_Error: %s\n", SDL_GetError());
        exit(1);
    }

    // Create Window
    window = SDL_CreateWindow(
        "Click somewhere",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        width, height, SDL_WINDOW_SHOWN);

    if(window == NULL) {
        printf("Window could not be created - SDL_Error: %s\n", SDL_GetError());
        exit(1);
    }

    // Create Renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if(renderer == NULL) {
        printf("Renderer could not be created - SDL_Error: %s\n", SDL_GetError());
        exit(1);
    }
}

bool handle_input() {

    SDL_Event e;
    bool quit = false;

    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
            quit = true;
        }
    }
    return quit;
}


int main( int argc, char* args[] ) {

    init_sdl(800, 600);
    
    bool quit = false;
    while(!quit) {

        SDL_SetRenderDrawColor(renderer, 0x30, 0x00, 0x30, 0xFF);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);

        quit = handle_input();
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
