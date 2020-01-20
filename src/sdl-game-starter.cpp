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
#include<limits.h>
#include<assert.h>
#include<math.h>

// TODO put these kind of defines in a header unless platform-specific
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

static_assert(CHAR_BIT == 8, "char not 8 bits");

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

struct AudioRingBuffer
{
    int size;
    int write_index;
    int play_index;
    void* data;
};

static bool running;

// Rendering stuff
static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static const int BYTES_PER_PIXEL = 4;

static sdl_offscreen_buffer screen_buffer;

// Audio stuff
static const int AUDIO_SAMPLES_PER_SECOND = 48000;
static const int SDL_AUDIO_BUFFER_SAMPLES = 512;
static const int AUDIO_RING_BUFFER_NUM_SAMPLES = AUDIO_SAMPLES_PER_SECOND;
static const int AUDIO_LATENCY_SAMPLE_COUNT = AUDIO_SAMPLES_PER_SECOND / 15;
static const int NUM_AUDIO_CHANNELS = 2;
static const int AUDIO_SAMPLE_SIZE = (AUDIO_S16SYS & SDL_AUDIO_MASK_BITSIZE)/8; //in bytes
static const int BYTES_PER_AUDIO_SAMPLE = AUDIO_SAMPLE_SIZE * NUM_AUDIO_CHANNELS;
static const int MIDDLE_C_FREQ = 256;
static const int MIDDLE_VOLUME_AMPLITUDE = 500;

static SDL_AudioDeviceID audio_device_id = 0;
static SDL_AudioSpec audio_settings;
static AudioRingBuffer audio_ring_buffer{};
double t_sine = 0.0;
int16_t wave_amplitude = MIDDLE_VOLUME_AMPLITUDE;
int wave_hz = MIDDLE_C_FREQ;

// Input stuff
#define MAX_CONTROLLERS 4
static int num_controllers = 0;
static SDL_GameController* controller_handles[MAX_CONTROLLERS];

static GameKeyboardState game_keyboard_state{};

// Timer stuff
static double target_frame_ms = 16.66666;


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

static void audio_callback(void* user_data, uint8_t* audio_data, int length)
{
    AudioRingBuffer* ring_buffer = (AudioRingBuffer*)user_data;

    DEBUG_ASSERT(length <= ring_buffer->size);

    // Copy length bytes from ring buffer to audio_data buffer
    int space_left = ring_buffer->size - ring_buffer->play_index;

    if (length > space_left)
    {
        memcpy(audio_data, &((uint8_t*)ring_buffer->data)[ring_buffer->play_index], space_left);
        memcpy(&audio_data[space_left], ring_buffer->data, length - space_left);
        ring_buffer->play_index = length - space_left;
    }
    else
    {
        memcpy(audio_data, &((uint8_t*)ring_buffer->data)[ring_buffer->play_index], length);
        ring_buffer->play_index += length;
    }
}

// return the modified new wave offset
static void audio_sine_wave(void * buffer, int buffer_size)
{
    DEBUG_ASSERT(buffer_size % BYTES_PER_AUDIO_SAMPLE == 0);
    // how many samples per full wave
    int wave_period = AUDIO_SAMPLES_PER_SECOND / wave_hz;
    int num_samples = buffer_size / BYTES_PER_AUDIO_SAMPLE;
    // one sample's worth for this period
    // I DONT HAVE GREAT INTUITION ON THIS
    double t_sine_inc = 2.0 * M_PI * 1.0 / (double)wave_period;

    int16_t* curr_sample = (int16_t*)buffer;

    for (int i = 0; i < num_samples; ++i) {

        for (int ch = 0; ch < NUM_AUDIO_CHANNELS; ++ch)
        {
            *curr_sample = (int16_t)((double)wave_amplitude * sin(t_sine));
            curr_sample++;
        }

        t_sine += t_sine_inc;
    }
}

int main(int argc, char* args[])
{
    int width = 800;
    int height = 600;

    // Init SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) < 0)
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

    // Get initially plugged in controllers
    int num_joysticks = SDL_NumJoysticks();
    for (int joy_index = 0; joy_index < num_joysticks; ++joy_index)
    {
        add_controller(joy_index);
    }
    DEBUG_PRINTF("Found %d game controllers\n", num_controllers);

    // Initialize rendering buffer
    screen_buffer = create_offscreen_buffer(width, height);

    // Initialize audio

    // TODO store the name of the device for debugging and/or settings menu stuff (must be copied)
    // WARNING I don't think this index always equals the audio device id, because devices can change at runtime
    int num_audio_devices = SDL_GetNumAudioDevices(0); // iscapture = 0
    DEBUG_PRINTF("Audio devices:\n");
    for (int i = 0; i < num_audio_devices; ++i)
    {
        DEBUG_PRINTF("  %d: %s\n", i, SDL_GetAudioDeviceName(i, 0));
    }

    SDL_AudioSpec actual_settings{};

    audio_settings.freq = AUDIO_SAMPLES_PER_SECOND;
    audio_settings.format = AUDIO_S16SYS;
    audio_settings.channels = NUM_AUDIO_CHANNELS;
    audio_settings.samples = SDL_AUDIO_BUFFER_SAMPLES;
    audio_settings.callback = audio_callback;
    audio_settings.userdata = &audio_ring_buffer;

    // find a suitable device, no changes in format allowed
    // TODO make selectable and automatically change at runtime by listening for SDL_AudioDeviceEvent
    audio_device_id = SDL_OpenAudioDevice(NULL, 0, &audio_settings, &actual_settings, 0);
    if (audio_device_id == 0) {
        FATAL_PRINTF("Failed to open audio - SDL_Error: %s\n", SDL_GetError());
    }
    if (actual_settings.format != AUDIO_S16LSB || actual_settings.freq != AUDIO_SAMPLES_PER_SECOND || actual_settings.samples != SDL_AUDIO_BUFFER_SAMPLES) {
        FATAL_PRINTF("Audio settings don't match requested\n");
    }
    DEBUG_PRINTF("Audio device selected: %d\n", audio_device_id);
    audio_settings = actual_settings;

    // Lock the callback
    SDL_LockAudioDevice(audio_device_id);
    {
        SDL_PauseAudioDevice(audio_device_id, 0); /* start audio playing. */

        DEBUG_PRINTF("Audio silence: %d\n", audio_settings.silence);
        DEBUG_PRINTF("SDL audio buffer size: %d\n", audio_settings.size);

        // initialize ring buffer to 1 second
        audio_ring_buffer.size = BYTES_PER_AUDIO_SAMPLE * AUDIO_RING_BUFFER_NUM_SAMPLES;
        DEBUG_ASSERT((int)audio_settings.size <= audio_ring_buffer.size);

        audio_ring_buffer.write_index = 0;
        audio_ring_buffer.play_index = 0;
        DEBUG_PRINTF("Audio ring buffer size: %d\n", audio_ring_buffer.size);

        audio_ring_buffer.data = LARGE_ALLOC(audio_ring_buffer.size);
        if (!audio_ring_buffer.data)
        {
            FATAL_PRINTF("Couldn't allocate audio ring buffer\n");
        }

        // fill ring buffer with sine wave
        audio_sine_wave(audio_ring_buffer.data, AUDIO_LATENCY_SAMPLE_COUNT * AUDIO_SAMPLE_SIZE);
        audio_ring_buffer.write_index += AUDIO_LATENCY_SAMPLE_COUNT * AUDIO_SAMPLE_SIZE;

    }
    // Unlock the callback
    SDL_UnlockAudioDevice(audio_device_id);


    // Now do the game loop
    running = true;

    // input and gradient scroll stuff
    SDL_Event e;
    int16_t stick_x = 0;
    int16_t stick_y = 0;
    int x_offset = 0;
    int y_offset = 0;
    int x_vel = 0;
    int y_vel = 0;
    const int MAX_SCROLL_SPEED = 5;

    // audio stuff
    const int MIN_HZ = 20;
    const int MAX_HZ = 256*2;
    const int MAX_VOLUME_OFFSET = 300;

    // timer
    uint64_t frame_start_time = SDL_GetPerformanceCounter();

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
        // change freq & volume of wave
        if (stick_y >= 0)
        {
            wave_hz = MIDDLE_C_FREQ + (int16_t)((double)MAX_HZ * (double)stick_y / 32767.0);
        }
        else if (stick_y < 0)
        {
            wave_hz = MIN_HZ + (int16_t)((double)(MIDDLE_C_FREQ - MIN_HZ) * (((double)stick_y / 32767.0) + 1.0));
        }
        wave_amplitude = MIDDLE_VOLUME_AMPLITUDE + (int16_t)((double)MAX_VOLUME_OFFSET * (double)stick_x / 32767.0);

        SDL_SetRenderDrawColor(renderer, 0x50, 0x00, 0x50, 0xFF);
        SDL_RenderClear(renderer);

        render_gradient_to_buffer(&screen_buffer, x_offset, y_offset);
        render_offscreen_buffer(&screen_buffer);

        SDL_RenderPresent(renderer);

        // Fill next part of audio buffer with sound, up to the play cursor + a bit ahead
        // need to make 1 assumption: we can write 'a bit ahead' faster than it can be played
        // hence, write index is always considered ahead of play index
        // i.e. play index shouldn't completely loop write index; writing should always be able to catch up even if it gets behind

        SDL_LockAudioDevice(audio_device_id);
        int target_index = (audio_ring_buffer.play_index + AUDIO_LATENCY_SAMPLE_COUNT * AUDIO_SAMPLE_SIZE) % audio_ring_buffer.size;
        DEBUG_ASSERT(audio_ring_buffer.play_index % BYTES_PER_AUDIO_SAMPLE == 0);
        DEBUG_ASSERT(audio_ring_buffer.write_index % BYTES_PER_AUDIO_SAMPLE == 0);
        DEBUG_ASSERT(target_index % BYTES_PER_AUDIO_SAMPLE == 0);
        // if write_index has caught up, don't write anything;
        if (audio_ring_buffer.write_index != target_index)
        {
            // we want to write AUDIO_LATENCY_SAMPLE_COUNT samples ahead of the play cursor
            // get region/s to fill (2 cases because of circular buffer)
            int region_size_1, region_size_2;
            if (audio_ring_buffer.write_index < target_index)
            {
                region_size_1 = target_index - audio_ring_buffer.write_index;
                region_size_2 = 0;
            }
            else
            {
                region_size_1 = audio_ring_buffer.size - audio_ring_buffer.write_index;
                region_size_2 = target_index;
            }

            void* region = (void*)&((int8_t*)audio_ring_buffer.data)[audio_ring_buffer.write_index];

            audio_sine_wave(region, region_size_1);
            if (region_size_2)
            {
                audio_sine_wave(audio_ring_buffer.data, region_size_2);
            }

            audio_ring_buffer.write_index = (audio_ring_buffer.write_index + region_size_1 + region_size_2) % audio_ring_buffer.size;

        }
        SDL_UnlockAudioDevice(audio_device_id);

        uint64_t frame_end_time = SDL_GetPerformanceCounter();
        double frame_time_ms = 1000.0 * (double)(frame_end_time - frame_start_time)/(double)SDL_GetPerformanceFrequency();
        double diff_ms = target_frame_ms - frame_time_ms;
        int loops = 0;
        while (diff_ms > 0.0)
        {
            if (diff_ms > 1.1)
            {
                SDL_Delay((uint32_t)diff_ms);
            }
            frame_end_time = SDL_GetPerformanceCounter();
            frame_time_ms = 1000.0 * (double)(frame_end_time - frame_start_time)/(double)SDL_GetPerformanceFrequency();
            diff_ms = target_frame_ms - frame_time_ms;
            loops++;
        }
        //DEBUG_PRINTF("loops: %d\n", loops);
        DEBUG_PRINTF("frame_time_ms: %lf\n", frame_time_ms);
        frame_start_time = frame_end_time;

    }

    SDL_CloseAudioDevice(audio_device_id);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
