#ifndef GAME_STARTER_H
/*
 * This file contains interfaces used by the platform to call into game code,
 * and interfaces for the game to call back to platform code
 * This file is used by the platform executable and game shared object library.
 */

#include"global_includes.h"

#define INPUT_BUFFER_SIZE 2
#define MAX_CONTROLLERS 4

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


// debug/prototyping functions only
#define FUNC_DEBUG_PLATFORM_READ_ENTIRE_FILE(name) void* name(const char* filename, int32_t* returned_size)
typedef FUNC_DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile);

#define FUNC_DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(void* memory)
typedef FUNC_DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory);

#define FUNC_DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) void name(const char* filename, void* buffer, int32_t len)
typedef FUNC_DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile);
//


struct GameMemory
{
    unsigned memory_size;
    void* memory;

    DEBUGPlatformReadEntireFile* DEBUG_platform_read_entire_file;
    DEBUGPlatformFreeFileMemory* DEBUG_platform_free_file_memory;
    DEBUGPlatformWriteEntireFile* DEBUG_platform_write_entire_file;
};

#define FUNC_GAME_INIT_MEMORY(name) void name(GameMemory game_memory)
typedef FUNC_GAME_INIT_MEMORY(GameInitMemory);
FUNC_GAME_INIT_MEMORY(game_init_memory_stub)
{
    FATAL_PRINTF("game_init_memory not loaded\n");
}

#define FUNC_GAME_UPDATE_AND_RENDER(name) void name(GameMemory game_memory, GameInputBuffer* input_buffer, GameRenderBuffer* render_buffer, GameSoundBuffer* sound_buffer)
typedef FUNC_GAME_UPDATE_AND_RENDER(GameUpdateAndRender);
FUNC_GAME_UPDATE_AND_RENDER(game_update_and_render_stub)
{
    FATAL_PRINTF("game_update_and_render not loaded\n");
}


#define GAME_STARTER_H
#endif