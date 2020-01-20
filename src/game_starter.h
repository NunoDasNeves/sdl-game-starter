#ifndef GAME_STARTER_H
/*
 *
 *
 */

#include"global_includes.h"

struct GameState
{
    int x_offset;
    int y_offset;
    bool running;
};

struct OffscreenBuffer
{
    void* pixels;
    int width;
    int height;
    int pitch;
};

void game_update_and_render(OffscreenBuffer* offscreen_buffer, GameState* game_state);


#define GAME_STARTER_H
#endif