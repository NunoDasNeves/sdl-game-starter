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

struct GameKeyboardState
{
    bool up;
    bool down;
    bool left;
    bool right;
};

static bool running;

// Rendering stuff
static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static const int BYTES_PER_PIXEL = 4;

static sdl_offscreen_buffer screen_buffer;

// Input stuff
#define MAX_CONTROLLERS 4
static int num_controllers = 0;
static SDL_GameController* controller_handles[MAX_CONTROLLERS];

static GameKeyboardState game_keyboard_state{};


int clamp(int val, int lo, int hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

static void render_gradient_to_buffer(sdl_offscreen_buffer* buffer, int x_offset, int y_offset)
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
            *byte = (uint8_t)(c + x_offset);
            byte++;

            // G
            *byte = (uint8_t)(r + y_offset);
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
    bool key_down = false;

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
        // TODO support text input as per: https://wiki.libsdl.org/Tutorials/TextInput
        case SDL_KEYDOWN:
            key_down = true;
        case SDL_KEYUP:
        {
            SDL_Keycode keycode = e->key.keysym.sym;
            switch(keycode)
            {
                case SDLK_LEFT:
                case SDLK_a:
                    game_keyboard_state.left = key_down;
                    break;
                case SDLK_UP:
                case SDLK_w:
                    game_keyboard_state.up = key_down;
                    break;
                case SDLK_RIGHT:
                case SDLK_d:
                    game_keyboard_state.right = key_down;
                    break;
                case SDLK_DOWN:
                case SDLK_s:
                    game_keyboard_state.down = key_down;
                    break;
            }
            /*
            DEBUG_PRINTF("up: %s\n", game_keyboard_state.up ? "DOWN" : "UP");
            DEBUG_PRINTF("down: %s\n", game_keyboard_state.down ? "DOWN" : "UP");
            DEBUG_PRINTF("left: %s\n", game_keyboard_state.left ? "DOWN" : "UP");
            DEBUG_PRINTF("right: %s\n", game_keyboard_state.right ? "DOWN" : "UP");
            */
            break;
        }
    }
}

static const int deadzone_left = 5000;
static void poll_controllers(int16_t* stick_x, int16_t* stick_y)
{
    // TODO: store this or something
    for (int i = 0; i < MAX_CONTROLLERS; ++i)
    {
        if(controller_handles[i] != NULL && SDL_GameControllerGetAttached(controller_handles[i]))
        {
            bool up = SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_DPAD_UP);
            bool down = SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_DPAD_DOWN);
            bool left = SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_DPAD_LEFT);
            bool right = SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
            bool start = SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_START);
            bool back = SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_BACK);
            bool leftShoulder = SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
            bool rightShoulder = SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
            bool a_button = SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_A);
            bool b_button = SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_B);
            bool x_button = SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_X);
            bool y_button = SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_Y);

            *stick_x = SDL_GameControllerGetAxis(controller_handles[i], SDL_CONTROLLER_AXIS_LEFTX);
            *stick_y = SDL_GameControllerGetAxis(controller_handles[i], SDL_CONTROLLER_AXIS_LEFTY);
            *stick_x = abs(*stick_x) < deadzone_left ? 0 : *stick_x;
            *stick_y = abs(*stick_y) < deadzone_left ? 0 : *stick_y;
        }
        else
        {
            // TODO: controller is not plugged in
        }
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
    int16_t stick_x = 0;
    int16_t stick_y = 0;
    int x_offset = 0;
    int y_offset = 0;
    int x_vel = 0;
    int y_vel = 0;
    static const int MAX_SCROLL_SPEED = 5;

    while(running)
    {
        
        while (SDL_PollEvent(&e))
        {
            handle_event(&e);
        }

        x_vel = 0;
        y_vel = 0;
        if (game_keyboard_state.up && !game_keyboard_state.down) {
            y_vel = -MAX_SCROLL_SPEED;
        } else if (game_keyboard_state.down && !game_keyboard_state.up) {
            y_vel = MAX_SCROLL_SPEED;
        }
        if (game_keyboard_state.left && !game_keyboard_state.right) {
            x_vel = -MAX_SCROLL_SPEED;
        } else if (game_keyboard_state.right && !game_keyboard_state.left) {
            x_vel = +MAX_SCROLL_SPEED;
        }
        poll_controllers(&stick_x, &stick_y);
        // TODO replace with Vector2; this doesn't work properly because the vector length must be clamped, not x and y individually
        x_vel = clamp((int)(((double)stick_x / 32767.0) * (double)MAX_SCROLL_SPEED) + x_vel, -MAX_SCROLL_SPEED, MAX_SCROLL_SPEED);
        y_vel = clamp((int)(((double)stick_y / 32767.0) * (double)MAX_SCROLL_SPEED) + y_vel, -MAX_SCROLL_SPEED, MAX_SCROLL_SPEED);
        x_offset += x_vel;
        y_offset += y_vel;

        SDL_SetRenderDrawColor(renderer, 0x50, 0x00, 0x50, 0xFF);
        SDL_RenderClear(renderer);

        render_gradient_to_buffer(&screen_buffer, x_offset, y_offset);
        render_offscreen_buffer(&screen_buffer);

        SDL_RenderPresent(renderer);        
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
