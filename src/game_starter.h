#ifndef GAME_STARTER_H
/*
 *
 *
 */

#include"global_includes.h"

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

void game_update_and_render(OffscreenBuffer* offscreen_buffer, SoundBuffer* sound_buffer, GameState* game_state);


#define GAME_STARTER_H
#endif