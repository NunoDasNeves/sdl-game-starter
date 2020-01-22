#include"game_starter.h"

// TODO define this in our own math library
// TODO redefined on linux, not on windows...hmm
#ifndef M_PI
#define M_PI 3.14159265359
#endif

#define MIDDLE_C_FREQ 256
#define MIDDLE_VOLUME_AMPLITUDE 500

const int MAX_SCROLL_SPEED = 5;
const int MIN_HZ = 20;
const int MAX_HZ = 256*2;
const int MAX_VOLUME_OFFSET = 300;


// TODO move to util functions
int clamp(int val, int lo, int hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

// return the modified new wave offset
static void output_sine_wave(GameSoundBuffer* sound_buffer, int wave_hz, int wave_amplitude)
{
    static double t_sine = 0.0;

    DEBUG_ASSERT(sound_buffer->buffer_size % sound_buffer->bytes_per_sample == 0);
    // how many samples per full wave
    int wave_period = sound_buffer->samples_per_second / wave_hz;
    int num_samples = sound_buffer->buffer_size / sound_buffer->bytes_per_sample;
    // one sample's worth for this period
    // I DONT HAVE GREAT INTUITION ON THIS
    double t_sine_inc = 2.0 * M_PI * 1.0 / (double)wave_period;

    int16_t* curr_sample = (int16_t*)sound_buffer->buffer;

    for (int i = 0; i < num_samples; ++i) {

        for (int ch = 0; ch < sound_buffer->num_channels; ++ch)
        {
            *curr_sample = (int16_t)((double)wave_amplitude * sin(t_sine));
            curr_sample++;
        }

        t_sine += t_sine_inc;
    }
}

static void render_gradient_to_buffer(GameRenderBuffer* buffer, int x_offset, int y_offset)
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

void game_init_memory(GameMemory game_memory)
{
    DEBUG_ASSERT(game_memory.memory_size >= sizeof(GameState));

    // TODO partition memory etc
    GameState* game_state = (GameState*)game_memory.memory;

    // initialize game state etc
    game_state->wave_amplitude = 0;
    game_state->wave_hz = 0;
    game_state->x_offset = 0;
    game_state->y_offset = 0;
}

void game_update_and_render(GameMemory game_memory, GameInputBuffer* input_buffer, GameRenderBuffer* render_buffer, GameSoundBuffer* sound_buffer)
{

    GameState* game_state = (GameState*)game_memory.memory;

    int x_vel = 0;
    int y_vel = 0;

    GameInput* game_input = &(input_buffer->buffer[input_buffer->last]);
    KeyboardInput* keyboard = &(game_input->keyboard);
    // find a plugged in controller
    ControllerInput* controller = NULL;
    for (int i = 0; i < MAX_CONTROLLERS; ++i)
    {
        if (game_input->controllers[i].plugged_in)
        {
            controller = &(game_input->controllers[i]);
            break;
        }
    }

    // testing buffered input
    if (input_buffer->buffer[input_buffer->last].keyboard.up.pressed && \
        !input_buffer->buffer[(input_buffer->last + INPUT_BUFFER_SIZE - 1) % INPUT_BUFFER_SIZE].keyboard.up.pressed)
    {
        //DEBUG_PRINTF("up key pressed\n");
    }
    if (!input_buffer->buffer[input_buffer->last].keyboard.up.pressed && \
        input_buffer->buffer[(input_buffer->last + INPUT_BUFFER_SIZE - 1) % INPUT_BUFFER_SIZE].keyboard.up.pressed)
    {
        //DEBUG_PRINTF("up key released\n");
    }

    if (keyboard->up.pressed && !keyboard->down.pressed) {
        y_vel = -MAX_SCROLL_SPEED;
    } else if (keyboard->down.pressed && !keyboard->up.pressed) {
        y_vel = MAX_SCROLL_SPEED;
    }
    if (keyboard->left.pressed && !keyboard->right.pressed) {
        x_vel = -MAX_SCROLL_SPEED;
    } else if (keyboard->right.pressed && !keyboard->left.pressed) {
        x_vel = MAX_SCROLL_SPEED;
    }
    if (controller)
    {
        // normalize values to approx [-1.0, 1.0]
        double left_stick_y = (double)controller->left_stick_y.value / 32767.0;
        double left_stick_x = (double)controller->left_stick_x.value / 32767.0;

        // TODO replace with Vector2; this doesn't work properly because the vector length must be clamped, not x and y individually
        x_vel = clamp((int)(left_stick_x * (double)MAX_SCROLL_SPEED) + x_vel, -MAX_SCROLL_SPEED, MAX_SCROLL_SPEED);
        y_vel = clamp((int)(left_stick_y * (double)MAX_SCROLL_SPEED) + y_vel, -MAX_SCROLL_SPEED, MAX_SCROLL_SPEED);
        // change freq & volume of wave
        if (left_stick_y >= 0.0)
        {
            game_state->wave_hz = MIDDLE_C_FREQ + (int16_t)((double)MAX_HZ * left_stick_y);
        }
        else if (controller->left_stick_y.value < 0)
        {
            game_state->wave_hz = MIN_HZ + (int16_t)((double)(MIDDLE_C_FREQ - MIN_HZ) * ((left_stick_y + 1.0)/2.0));
        }
        game_state->wave_amplitude = MIDDLE_VOLUME_AMPLITUDE + (int16_t)((double)MAX_VOLUME_OFFSET * left_stick_x);

        // test debug IO
        if (controller->a.pressed)
        {
            DEBUG_platform_write_entire_file("test_file", (void*)"testIO\0", 7);
            DEBUG_PRINTF("wrote a file called \"test_file\"\n");
        }
        if (controller->b.pressed)
        {
            int64_t size;
            void* buf = DEBUG_platform_read_entire_file("test_file", &size);
            DEBUG_ASSERT(size == 7);
            DEBUG_PRINTF("read a file containing \"%s\"\n", (char*)buf);
            DEBUG_platform_free_file_memory(buf);
        }
    }
    else
    {
        game_state->wave_hz = MIDDLE_C_FREQ;
        game_state->wave_amplitude = MIDDLE_VOLUME_AMPLITUDE;
    }

    game_state->x_offset += x_vel;
    game_state->y_offset += y_vel;

    output_sine_wave(sound_buffer, game_state->wave_hz, game_state->wave_amplitude);
    render_gradient_to_buffer(render_buffer, game_state->x_offset, game_state->y_offset);
}