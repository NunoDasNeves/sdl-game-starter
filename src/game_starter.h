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

struct SoundBuffer
{
    void* buffer;
    int buffer_size;
    int samples_per_second;
    int bytes_per_sample;
    int num_channels;
};

struct OffscreenBuffer
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

void game_update_and_render(GameInputBuffer* input_buffer, OffscreenBuffer* offscreen_buffer, SoundBuffer* sound_buffer, GameState* game_state);


#define GAME_STARTER_H
#endif