#ifndef GAME_STARTER_H
/*
 *
 *
 */

#include"global_includes.h"

#define INPUT_BUFFER_SIZE 2
#define MAX_CONTROLLERS 4

struct GameState
{
    int wave_hz;
    int wave_amplitude;
    int x_offset;
    int y_offset;
    bool running;
};

struct GameSoundBuffer
{
    void* buffer;
    int buffer_size;
    int samples_per_second;
    int bytes_per_sample;
    int num_channels;
};

struct GameRenderBuffer
{
    void* pixels;
    int width;
    int height;
    int pitch;
};

struct ButtonInput
{
    bool pressed;
};

struct AnalogInput
{
    int16_t value;
};

struct ControllerInput
{
    bool plugged_in;

    AnalogInput left_stick_x;
    AnalogInput left_stick_y;

    ButtonInput a;
    ButtonInput b;
    ButtonInput x;
    ButtonInput y;

    ButtonInput up;
    ButtonInput down;
    ButtonInput left;
    ButtonInput right;
};

struct KeyboardInput
{
    ButtonInput up;
    ButtonInput down;
    ButtonInput left;
    ButtonInput right;

    // etc; add keys as needed (or do this differently...)
};

struct GameInput
{
    // TODO pass time elapsed since last frame
    KeyboardInput keyboard;
    int num_controllers;
    ControllerInput controllers[MAX_CONTROLLERS];
};

struct GameInputBuffer
{
    // circular game input buffer;
    // input_buffer[last] is input we saw at the start of this frame,
    // input_buffer[(last-1) % INPUT_BUFFER_SIZE] etc are previous inputs
    int last;
    GameInput buffer[INPUT_BUFFER_SIZE];
};

struct GameMemory
{
    unsigned memory_size;
    void* memory;
};

// debug/prototyping functions only
void* DEBUG_platform_read_entire_file(const char* filename, int64_t* returned_size);
void DEBUG_platform_free_file_memory(void* memory);
void DEBUG_platform_write_entire_file(const char* filename, void* buffer, uint32_t len);

void game_init_memory(GameMemory game_memory);

void game_update_and_render(GameMemory game_memory, GameInputBuffer* input_buffer, GameRenderBuffer* render_buffer, GameSoundBuffer* sound_buffer);

#define GAME_STARTER_H
#endif