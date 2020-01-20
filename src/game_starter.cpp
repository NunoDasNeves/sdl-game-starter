#include"game_starter.h"

#define M_PI 3.14159265359

// return the modified new wave offset
static void output_sine_wave(SoundBuffer* sound_buffer, int wave_hz, int wave_amplitude)
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

static void render_gradient_to_buffer(OffscreenBuffer* buffer, int x_offset, int y_offset)
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

void game_update_and_render(OffscreenBuffer* offscreen_buffer, SoundBuffer* sound_buffer, GameState* game_state)
{
    output_sine_wave(sound_buffer, game_state->wave_hz, game_state->wave_amplitude);
    render_gradient_to_buffer(offscreen_buffer, game_state->x_offset, game_state->y_offset);
}