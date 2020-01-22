#ifdef _WIN32
#include<windows.h>
#include<SDL.h>
#include<memoryapi.h>
#endif
#ifdef __linux__
#include<SDL2/SDL.h>
#include<sys/mman.h>
#endif

#include"game_starter.h"

#ifdef _WIN32
#define GAME_CODE_OBJECT_FILE "game_starter.dll"
#define LARGE_ALLOC(SZ) VirtualAlloc(NULL, SZ, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
#define LARGE_ALLOC_FIXED(SZ, ADDR) VirtualAlloc((LPVOID)ADDR, SZ, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
#define LARGE_FREE(PTR,SZ) DEBUG_ASSERT(VirtualFree(PTR, 0, MEM_RELEASE))
#endif
#ifdef __linux__
#define GAME_CODE_OBJECT_FILE "game_starter.so"
#define LARGE_ALLOC(X) mmap(NULL, X, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0)
// TODO fixed alloc on linux
#define LARGE_ALLOC_FIXED(SZ, ADDR) LARGE_ALLOC(SZ)
#define LARGE_FREE(X,Y) munmap(X, Y)
#endif

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
} game_code;

static bool running = true;

// Rendering stuff
static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* texture = NULL;
static const int BYTES_PER_PIXEL = 4;
// TODO dynamic or adjustable
static int target_framerate = 60;
// TODO recompute based on framerate
static double target_frame_ms = 1000.0/(double)target_framerate;

// Audio stuff
// TODO BUG/weird issues - audio skips during some OS interactions; holding on window X, typing in search box...arg
// From cursory research, it seems to be okay to lag anywhere from 3-9 frames before audio latency is noticeable
// However we still want to minimize it without causing skips etc

// current latency is only a frame or two! pretty good

static const int AUDIO_SAMPLES_PER_SECOND = 48000;

// TODO this number is pretty crucial
// if it's too low, the audio callback has to run very frequently, blocking the main loop
// if it's too high, the callback runs less frequently and the play index is less accurate
// it should possibly be tuned at startup, or even at runtime? (have to close and open the audio device)
// e.g. 512 means that the play cursor updates every 10ms
// To mitigate this we take averages of stuff
static const int SDL_AUDIO_BUFFER_SAMPLES = 256;    // must be power of 2
static const int AUDIO_RING_BUFFER_SIZE_SAMPLES = AUDIO_SAMPLES_PER_SECOND;
static const int APPROX_AUDIO_SAMPLES_PER_FRAME = AUDIO_SAMPLES_PER_SECOND / target_framerate;
static const int NUM_AUDIO_CHANNELS = 2;
static const int AUDIO_SAMPLE_SIZE = (AUDIO_S16SYS & SDL_AUDIO_MASK_BITSIZE) / BITS_PER_BYTE; //in bytes
static const int BYTES_PER_AUDIO_SAMPLE = AUDIO_SAMPLE_SIZE * NUM_AUDIO_CHANNELS;

static SDL_AudioDeviceID audio_device_id = 0;
static SDL_AudioSpec audio_settings;
static AudioRingBuffer audio_ring_buffer{};

// Input stuff
static int num_controllers = 0;
static SDL_GameController* controller_handles[MAX_CONTROLLERS];

// Stuff passed to game
static GameInputBuffer game_input_buffer{};
static GameRenderBuffer game_render_buffer{};
static GameSoundBuffer game_sound_buffer{};
static GameMemory game_memory{};


static FUNC_DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUG_platform_read_entire_file)
{
    SDL_RWops* file = SDL_RWFromFile(filename, "rb");
    DEBUG_ASSERT_MSG(file, "SDL Error: %s\n", SDL_GetError());

    int64_t size = SDL_RWsize(file);
    DEBUG_ASSERT_MSG(size >= 0, "SDL Error: %s\n", SDL_GetError());

    void* buffer = malloc(size);
    DEBUG_ASSERT_MSG(buffer, "malloc of read buffer failed\n");

    int64_t len = SDL_RWread(file, buffer, size, 1);
    DEBUG_ASSERT_MSG(len >= 0, "%s\n", SDL_GetError());
    if (len != 1)
    {
        FATAL_PRINTF("Read less than expected: read %ld, expected 1\n", len);
    }

    if (SDL_RWclose(file))
    {
        FATAL_PRINTF("SDL Error: %s\n", SDL_GetError());
    }

    *returned_size = size;

    return buffer;
}

static FUNC_DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUG_platform_free_file_memory)
{
    free(memory);
}

static FUNC_DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUG_platform_write_entire_file)
{
    SDL_RWops* file = SDL_RWFromFile(filename, "wb");
    DEBUG_ASSERT_MSG(file, "SDL Error: %s\n", SDL_GetError());

    int64_t written = SDL_RWwrite(file, buffer, len, 1);
    DEBUG_ASSERT_MSG(written == 1, "Wrote %ld, SDL Error: %s\n", written, SDL_GetError());

    if (SDL_RWclose(file))
    {
        FATAL_PRINTF("SDL Error: %s\n", SDL_GetError());
    }

}

static void load_game_code()
{
    if (game_code.object)
    {
        SDL_UnloadObject(game_code.object);
        game_code = GameCode{};
    }

    game_code.object = SDL_LoadObject(GAME_CODE_OBJECT_FILE);
    if (!game_code.object)
    {
        FATAL_PRINTF("%s\n", SDL_GetError());
    }

    game_code.init_memory = (GameInitMemory*) SDL_LoadFunction(game_code.object, "game_init_memory");
    if (!game_code.init_memory)
    {
        FATAL_PRINTF("%s\n", SDL_GetError());
    }
    game_code.update_and_render = (GameUpdateAndRender*) SDL_LoadFunction(game_code.object, "game_update_and_render");
    if (!game_code.update_and_render)
    {
        FATAL_PRINTF("%s\n", SDL_GetError());
    }

}

static void render_offscreen_buffer(GameRenderBuffer* b)
{
    DEBUG_ASSERT(texture);

    SDL_UpdateTexture(texture, NULL, b->pixels, b->pitch);
    // this will stretch the texture to the render target if necessary, using bilinear interpolation
    SDL_RenderCopy(renderer, texture, NULL, NULL);
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
    bool success = false;
    const char * error = "maximum number of controllers reached";
    // find empty slot
    for (int i = 0; i < MAX_CONTROLLERS; ++i)
    {
        if (!controller_handles[i])
        {
            controller_handles[i] = SDL_GameControllerOpen(joystick_index);
            if (controller_handles[i])
            {
                num_controllers++;
                success = true;
            }
            else
            {
                error = SDL_GetError();
            }

            break;
        }
    }
    if (!success)
    {
        DEBUG_PRINTF("Tried to add controller, but failed: %s\n", error);
    }
}

static void remove_controller(SDL_JoystickID joystick_id) {
    if (num_controllers == 0)
    {
        return;
    }
    for (int i = 0; i < MAX_CONTROLLERS; ++i)
    {
        if (controller_handles[i])
        {
            SDL_JoystickID this_id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller_handles[i]));
            if (this_id == joystick_id)
            {
                SDL_GameControllerClose(controller_handles[i]);
                controller_handles[i] = NULL;
                num_controllers--;
            }
            break;
        }
    }
}

static void handle_event(SDL_Event* e)
{
    bool key_state = false;

    switch(e->type)
    {
        case SDL_QUIT:
            running = false;
            break;

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
            break;
        }
        case SDL_CONTROLLERDEVICEREMOVED:
        {
            remove_controller(e->cdevice.which);
            break;
        }
        // TODO remove controller
        // TODO support text input as per: https://wiki.libsdl.org/Tutorials/TextInput
        case SDL_KEYDOWN:
            key_state = true;
        case SDL_KEYUP:
        {
            SDL_Keycode keycode = e->key.keysym.sym;
            KeyboardInput* keyboard = &(game_input_buffer.buffer[game_input_buffer.last].keyboard);
            switch(keycode)
            {
                case SDLK_LEFT:
                case SDLK_a:
                    keyboard->left.pressed = key_state;
                    break;
                case SDLK_UP:
                case SDLK_w:
                    keyboard->up.pressed = key_state;
                    break;
                case SDLK_RIGHT:
                case SDLK_d:
                    keyboard->right.pressed = key_state;
                    break;
                case SDLK_DOWN:
                case SDLK_s:
                    keyboard->down.pressed = key_state;
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

static void poll_controllers()
{
    // TODO better deadzone handling, maybe adjustable
    static const int deadzone_left = 5000;
    GameInput* game_input = &(game_input_buffer.buffer[game_input_buffer.last]);
    game_input->num_controllers = 0;

    for (int i = 0; i < MAX_CONTROLLERS; ++i)
    {

        ControllerInput* controller = &(game_input->controllers[i]);

        if(controller_handles[i] != NULL && SDL_GameControllerGetAttached(controller_handles[i]))
        {
            game_input->num_controllers++;
            controller->plugged_in = true;

            controller->up.pressed = SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_DPAD_UP);
            controller->down.pressed = SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_DPAD_DOWN);
            controller->left.pressed = SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_DPAD_LEFT);
            controller->right.pressed = SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
            //= SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_START);
            //= SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_BACK);
            //= SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
            //= SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
            controller->a.pressed = SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_A);
            controller->b.pressed = SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_B);
            controller->x.pressed = SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_X);
            controller->y.pressed = SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_Y);

            int16_t stick_x = SDL_GameControllerGetAxis(controller_handles[i], SDL_CONTROLLER_AXIS_LEFTX);
            int16_t stick_y = SDL_GameControllerGetAxis(controller_handles[i], SDL_CONTROLLER_AXIS_LEFTY);
            stick_x = abs(stick_x) < deadzone_left ? 0 : stick_x;
            stick_y = abs(stick_y) < deadzone_left ? 0 : stick_y;
            controller->left_stick_x.value = stick_x;
            controller->left_stick_y.value = stick_y;
        }
        else
        {
            controller->plugged_in = false;
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

int main(int argc, char* args[])
{

    int width = 800;
    int height = 600;

    // Init SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) < 0)
    {
        FATAL_PRINTF("SDL couldn't be initialized - SDL_Error: %s\n", SDL_GetError());
    }

    // Create Window and Renderer

    window = SDL_CreateWindow(
        "Game",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        width, height,
        SDL_WINDOW_RESIZABLE);

    if(window == NULL)
    {
        FATAL_PRINTF("Window could not be created - SDL_Error: %s\n", SDL_GetError());
    }

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

    game_render_buffer.pitch = width * BYTES_PER_PIXEL;
    game_render_buffer.width = width;
    game_render_buffer.height = height;
    game_render_buffer.pixels = LARGE_ALLOC(height * game_render_buffer.pitch);
    if(game_render_buffer.pixels == NULL)
    {
        FATAL_PRINTF("Couldn't allocate pixels buffer");
    }

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
        audio_ring_buffer.size = BYTES_PER_AUDIO_SAMPLE * AUDIO_RING_BUFFER_SIZE_SAMPLES;
        DEBUG_ASSERT((int)audio_settings.size <= audio_ring_buffer.size);

        audio_ring_buffer.write_index = 0;
        audio_ring_buffer.play_index = 0;
        DEBUG_PRINTF("Audio ring buffer size: %d\n", audio_ring_buffer.size);

        audio_ring_buffer.data = LARGE_ALLOC(audio_ring_buffer.size);
        if (!audio_ring_buffer.data)
        {
            FATAL_PRINTF("Couldn't allocate audio ring buffer\n");
        }

        // fill ring buffer with silence
        memset(audio_ring_buffer.data, audio_settings.silence, audio_ring_buffer.size);
    }
    // Unlock the callback
    SDL_UnlockAudioDevice(audio_device_id);

    // Initialize game code, memory, sound, input
    load_game_code();

    game_memory.memory_size = GIBIBYTES(1);
#ifdef FIXED_GAME_MEMORY
    game_memory.memory = LARGE_ALLOC_FIXED(game_memory.memory_size, TEBIBYTES(2));
#else
    game_memory.memory = LARGE_ALLOC(game_memory.memory_size);
#endif

    if (!game_memory.memory)
    {
        FATAL_PRINTF("Couldn't allocate game memory\n");
    }
    game_memory.DEBUG_platform_read_entire_file = DEBUG_platform_read_entire_file;
    game_memory.DEBUG_platform_free_file_memory = DEBUG_platform_free_file_memory;
    game_memory.DEBUG_platform_write_entire_file = DEBUG_platform_write_entire_file;

    game_code.init_memory(game_memory);

    game_sound_buffer.buffer_size = AUDIO_SAMPLES_PER_SECOND * BYTES_PER_AUDIO_SAMPLE;
    game_sound_buffer.buffer = LARGE_ALLOC(game_sound_buffer.buffer_size);
    if (!game_sound_buffer.buffer)
    {
        FATAL_PRINTF("Couldn't allocate game sound buffer\n");
    }
    game_sound_buffer.samples_per_second = audio_settings.freq;
    game_sound_buffer.bytes_per_sample = BYTES_PER_AUDIO_SAMPLE;
    game_sound_buffer.num_channels = NUM_AUDIO_CHANNELS;

    memset(&game_input_buffer, 0, sizeof(GameInputBuffer));

    // Now do the game loop
    SDL_Event e;

    // timer
    uint64_t frame_start_time = SDL_GetPerformanceCounter();


    // Use this to compute how far ahead we should write audio (also determines our audio latency)
    const int SAMPLES_PER_FRAME_COUNT = 30;
    int avg_samples_per_frame = APPROX_AUDIO_SAMPLES_PER_FRAME;                   // bootstrap; estimate/ideal
    int avg_samples_since_start_of_frame = 0;
    int play_sample_set_target = 0;
    int play_sample_write_data = 0;

    while(running)
    {
        // Get initial play cursor
        SDL_LockAudioDevice(audio_device_id);
        int play_sample_init = audio_ring_buffer.play_index / BYTES_PER_AUDIO_SAMPLE;
        SDL_UnlockAudioDevice(audio_device_id);

        // Input
        // advance game input buffer, and clear next entry
        game_input_buffer.last = (game_input_buffer.last + 1) % INPUT_BUFFER_SIZE;
        memset(&game_input_buffer.buffer[game_input_buffer.last], 0, sizeof(GameInput));
        // copy previous keyboard state (otherwise keys only fire on each keyboard event bounded by OS repeat rate)
        game_input_buffer.buffer[game_input_buffer.last].keyboard = \
            game_input_buffer.buffer[(game_input_buffer.last + INPUT_BUFFER_SIZE - 1) % INPUT_BUFFER_SIZE].keyboard;
        
        while (SDL_PollEvent(&e))
        {
            handle_event(&e);
        }
        poll_controllers();

        // Rendering and audio
        SDL_SetRenderDrawColor(renderer, 0x50, 0x00, 0x50, 0xFF);
        SDL_RenderClear(renderer);

        /*
         *  How audio works (lots of buffers)
         *  - In this loop:
         *      - We figure out how far ahead we need to write so the audio doesn't skip
         *      - Game code fills a sound buffer
         *      - We copy it to the ring buffer
         *  - In SDL's callback function (run in another thread), we copy from the ring buffer to SDL's internal buffer
         *  - SDL copies to the platform's sound buffer
         * 
         *  We need to tell the game code to give us more than 1 frame of sound, or there could be skips!
         * 
         *  Frame timeline 
         * 
         * |----------------------------------------------|----------------------------------------------|
         *     ^ |---(get sound from game)---| ^                ^                          ^
         *  set target                    write data        set target                  write data
         *                                                                  (need to get here before play catches up) 
         * 
         *  We need to ask the game for enough sound data to get us to the end of the next frame
         *  So, starting at set target, we need to set our target at the end of the next frame
         *  This is just 2 frames minus the first bit of the frame before set target
         *  We use rolling exponentially weighted averages of the number of samples between each frame to hopefully make it more accurate
         */

        // Set audio target
        SDL_LockAudioDevice(audio_device_id);
        int new_play_sample_set_target = audio_ring_buffer.play_index / BYTES_PER_AUDIO_SAMPLE;
        SDL_UnlockAudioDevice(audio_device_id);

        int samples_since_last_frame_set_target = DIST_IN_RING_BUFFER(play_sample_set_target, new_play_sample_set_target, AUDIO_RING_BUFFER_SIZE_SAMPLES);
        int samples_since_start_of_frame = DIST_IN_RING_BUFFER(play_sample_init, new_play_sample_set_target, AUDIO_RING_BUFFER_SIZE_SAMPLES);
        avg_samples_since_start_of_frame = (int)EXP_WEIGHTED_AVG(avg_samples_since_start_of_frame, SAMPLES_PER_FRAME_COUNT, samples_since_start_of_frame);

        play_sample_set_target = new_play_sample_set_target;

        // 2 frames minus any samples from the start of this frame, plus one sdl buffer size for safety
        // TODO extra SDL buffer here is probably not needed
        int target_samples_ahead = (avg_samples_per_frame * 2 - avg_samples_since_start_of_frame) + SDL_AUDIO_BUFFER_SAMPLES;

        //DEBUG_PRINTF("avg samples in frame %d\n", avg_samples_per_frame);
        //DEBUG_PRINTF("avg samples since start of frame: %d\n", avg_samples_since_start_of_frame);
        //DEBUG_PRINTF("target samples ahead: %d\n", target_samples_ahead);
        int rem = target_samples_ahead % SDL_AUDIO_BUFFER_SAMPLES;
        target_samples_ahead += (SDL_AUDIO_BUFFER_SAMPLES - rem);

        int target_index = ((play_sample_set_target + target_samples_ahead) * BYTES_PER_AUDIO_SAMPLE) % audio_ring_buffer.size;
        // round to multiple of sdl buffer size
        int region_size_1 = 0, region_size_2 = 0;

        // get region/s to fill (2 cases because of circular buffer)
        if (audio_ring_buffer.write_index <= target_index)
        {
            region_size_1 = target_index - audio_ring_buffer.write_index;
            region_size_2 = 0;
        }
        else
        {
            region_size_1 = audio_ring_buffer.size - audio_ring_buffer.write_index;
            region_size_2 = target_index;
        }

        game_sound_buffer.buffer_size = region_size_1 + region_size_2;
        //DEBUG_PRINTF("game sound buffer samples size %d\n", game_sound_buffer.buffer_size / BYTES_PER_AUDIO_SAMPLE);

        // Call the game code
        game_code.update_and_render(game_memory, &game_input_buffer, &game_render_buffer, &game_sound_buffer);

        // Write audio data to the ring buffer
        SDL_LockAudioDevice(audio_device_id);
        {
            int new_play_sample_write_data = audio_ring_buffer.play_index / BYTES_PER_AUDIO_SAMPLE;
            int samples_since_last_frame_write_data = DIST_IN_RING_BUFFER(play_sample_write_data, new_play_sample_write_data, AUDIO_RING_BUFFER_SIZE_SAMPLES);

            // avg of this frame
            double avg_this_frame = ((double)samples_since_last_frame_set_target + (double)samples_since_last_frame_write_data) / 2.0;
            // exponentially weighted rolling average
            avg_samples_per_frame = (int)EXP_WEIGHTED_AVG(avg_samples_per_frame, SAMPLES_PER_FRAME_COUNT, avg_this_frame);

            play_sample_write_data = new_play_sample_write_data;

            if (game_sound_buffer.buffer_size)
            {

                void* region = (void*)&((int8_t*)audio_ring_buffer.data)[audio_ring_buffer.write_index];

                memcpy(region, game_sound_buffer.buffer, region_size_1);
                if (region_size_2)
                {
                    memcpy(audio_ring_buffer.data, (void*)&((int8_t*)game_sound_buffer.buffer)[region_size_1], region_size_2);
                }

                audio_ring_buffer.write_index = (audio_ring_buffer.write_index + region_size_1 + region_size_2) % audio_ring_buffer.size;

            }
        }
        SDL_UnlockAudioDevice(audio_device_id);

        // Actually render to the screen
        render_offscreen_buffer(&game_render_buffer);
        SDL_RenderPresent(renderer);
        
        // Timing
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
        //DEBUG_PRINTF("frame_time_ms: %lf\n", frame_time_ms);
        frame_start_time = frame_end_time;

    }

    SDL_CloseAudioDevice(audio_device_id);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
