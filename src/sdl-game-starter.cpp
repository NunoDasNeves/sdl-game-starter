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

struct sdl_offscreen_buffer
{
    SDL_Texture* texture;
    void* pixels;
    int width;
    int height;
    int pitch;
};

static bool running;

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static const int BYTES_PER_PIXEL = 4;

static sdl_offscreen_buffer screen_buffer;

#define MAX_CONTROLLERS 4
static int num_controllers = 0;
static SDL_GameController* controller_handles[MAX_CONTROLLERS];


static void render_gradient_to_buffer(sdl_offscreen_buffer* buffer, int offset)
{
    DEBUG_ASSERT(buffer->pixels);

    int width = buffer->width;
    int height = buffer->height;
    int pitch = buffer->pitch;
    uint8_t* row = (uint8_t*)buffer->pixels;

    for (int r = 0; r < height; ++r)
    {
        uint8_t * byte = row;
        for (int c = 0; c < width; ++c)
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

static void render_offscreen_buffer(sdl_offscreen_buffer* buffer)
{
    DEBUG_ASSERT(buffer->texture);

    SDL_UpdateTexture(buffer->texture, NULL, buffer->pixels, buffer->pitch);
    // this will stretch the texture to the render target if necessary, using bilinear interpolation
    SDL_RenderCopy(renderer, buffer->texture, NULL, NULL);
}

static sdl_offscreen_buffer create_offscreen_buffer(int width, int height)
{
    sdl_offscreen_buffer buffer{};

    buffer.pitch = width * BYTES_PER_PIXEL;
    buffer.width = width;
    buffer.height = height;

    buffer.texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        width,
        height);

    if(buffer.texture == NULL)
    {
        FATAL_PRINTF("Window could not be created - SDL_Error: %s\n", SDL_GetError());
    }

    buffer.pixels = LARGE_ALLOC(height * buffer.pitch);
    if(buffer.pixels == NULL)
    {
        FATAL_PRINTF("Couldn't allocate pixels buffer");
    }

    return buffer;
}

static void free_offscreen_buffer(sdl_offscreen_buffer* buffer)
{
    if (buffer->texture)
    {
        SDL_DestroyTexture(buffer->texture);
    }
    if (buffer->pixels)
    {
        LARGE_FREE(buffer->pixels, buffer->height * buffer->pitch);
    }
}

static void add_controller(int joystick_index) {
    if (!SDL_IsGameController(joystick_index))
    {
        return;
    }
    if (num_controllers >= MAX_CONTROLLERS)
    {
        return;
    }
    controller_handles[num_controllers] = SDL_GameControllerOpen(joystick_index);
    num_controllers++;
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
                    // comment out to stretch to window size
                    //free_offscreen_buffer(&screen_buffer);
                    //screen_buffer = create_offscreen_buffer(e->window.data1, e->window.data2);
                    break;
                }
            }
        } break;
        case SDL_CONTROLLERDEVICEADDED:
        {
            add_controller(e->cdevice.which);
        }
        // TODO remove controller
    }
}

int main(int argc, char* args[])
{
    int width = 800;
    int height = 600;

    // Init SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0)
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

    // get initially plugged in controllers
    int num_joysticks = SDL_NumJoysticks();
    for (int joy_index = 0; joy_index < num_joysticks; ++joy_index)
    {
        add_controller(joy_index);
    }
    DEBUG_PRINTF("Found %d game controllers\n", num_controllers);

    screen_buffer = create_offscreen_buffer(width, height);

    // Loop
    running = true;
    SDL_Event e;
    int offset = 0;

    while(running)
    {

        SDL_SetRenderDrawColor(renderer, 0x50, 0x00, 0x50, 0xFF);
        SDL_RenderClear(renderer);

        render_gradient_to_buffer(&screen_buffer, offset++);
        render_offscreen_buffer(&screen_buffer);

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
