#include <stdio.h>

#include "LcdDriver.h"
#include "pico/rand.h"
#include "pico/multicore.h"
#include "hardware/timer.h"
#include <math.h>
#include <stdint.h>

#define W 240
#define H 240
#define SIN_LUT_SIZE 1024
#define SIN_LUT_MASK (SIN_LUT_SIZE - 1)

static float sin_lut[SIN_LUT_SIZE];

static void init_sin_lut(void) {
    for (int i = 0; i < SIN_LUT_SIZE; i++) {
        sin_lut[i] = sinf((2.0f * M_PI * i) / SIN_LUT_SIZE);
    }
}

static inline float fast_sin(float x) {
    // Map radians → [0, 1024)
    int idx = (int)(x * (SIN_LUT_SIZE / (2.0f * M_PI)));
    return sin_lut[idx & SIN_LUT_MASK];
}


static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}


typedef struct {
    // Spatial frequency (shape scale)
    float kx;
    float ky;
    float kr;

    // Temporal frequency (motion)
    float tx;
    float ty;
    float tr;

    // Component weights
    float wx;
    float wy;
    float wr;

    // Palette phase offsets
    float pr;
    float pg;
    float pb;

    // Color intensity
    float color_gain;
} PlasmaParams;

static const PlasmaParams plasma_default = {
    .kx = 0.04f,
    .ky = 0.04f,
    .kr = 0.05f,

    .tx = 1.0f,
    .ty = 1.3f,
    .tr = 1.5f,

    .wx = 1.0f,
    .wy = 1.0f,
    .wr = 1.0f,

    .pr = 0.0f,
    .pg = 2.1f,
    .pb = 4.2f,

    .color_gain = 1.0f
};

static const PlasmaParams plasma_calm = {
    .kx = 0.02f,   // wider horizontal waves
    .ky = 0.02f,   // wider vertical waves
    .kr = 0.025f,  // gentle radial pattern

    .tx = 0.4f,    // slow horizontal drift
    .ty = 0.6f,    // slow vertical drift
    .tr = 0.7f,    // slow radial drift

    .wx = 1.0f,
    .wy = 1.0f,
    .wr = 0.7f,    // slightly weaker radial

    .pr = 0.0f,
    .pg = 1.5f,
    .pb = 3.0f,    // pastel palette offsets

    .color_gain = 0.8f // softer colors
};

static const PlasmaParams plasma_chaos = {
    .kx = 0.08f,   // tight horizontal waves
    .ky = 0.08f,   // tight vertical waves
    .kr = 0.08f,   // strong radial spikes

    .tx = 1.5f,    // fast horizontal drift
    .ty = 2.0f,    // fast vertical drift
    .tr = 2.5f,    // rapid radial motion

    .wx = 1.2f,
    .wy = 0.9f,
    .wr = 1.5f,    // radial dominates

    .pr = 0.0f,
    .pg = 3.0f,
    .pb = 5.0f,    // high-contrast, vibrant palette

    .color_gain = 1.2f // strong neon colors
};

static const PlasmaParams plasma_spiral = {
    .kx = 0.03f,
    .ky = 0.03f,
    .kr = 0.07f,    // radial dominates → spiral-like interference

    .tx = 1.0f,
    .ty = 1.0f,
    .tr = 1.8f,     // radial motion faster than axes

    .wx = 0.8f,
    .wy = 0.8f,
    .wr = 1.5f,     // strong radial weighting

    .pr = 0.0f,
    .pg = 2.5f,
    .pb = 4.5f,     // warm + cool contrast

    .color_gain = 1.0f
};


static void fill_plasma(uint16_t *fb, float t, const PlasmaParams *p, float hue_shift) {
    const float cx = W * 0.5f;
    const float cy = H * 0.5f;

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {

            float dx = x - cx;
            float dy = y - cy;

            float r2 = dx * dx + dy * dy;

            float v =
                p->wx * fast_sin(dx * p->kx + t * p->tx) +
                p->wy * fast_sin(dy * p->ky + t * p->ty) +
                p->wr * fast_sin(sqrtf(r2) * p->kr - t * p->tr);

            // Normalize from roughly [-3, +3] → [0, 1]
            float n = (v + 3.0f) * (1.0f / 6.0f);

            float phase = n * M_PI;

            uint8_t r = (uint8_t)(255.0f * p->color_gain *
                                  fast_sin(phase + p->pr + hue_shift));
            uint8_t g = (uint8_t)(255.0f * p->color_gain *
                                  fast_sin(phase + p->pg + hue_shift));
            uint8_t b = (uint8_t)(255.0f * p->color_gain *
                                  fast_sin(phase + p->pb + hue_shift));
            fb[y * W + x] = rgb565(r, g, b);
        }
    }
}


// No debouncing for simplicity (this is called once per frame)
static void handle_input(PlasmaParams *params, float *dt, float *hue_shift) {
    if (LcdGetKey(LCD_KEY_A) == 0) {
        *params = plasma_default;
        printf("Switched to default plasma mode.\n");
    }
    if (LcdGetKey(LCD_KEY_B) == 0) {
        *params = plasma_calm;
        printf("Switched to calm plasma mode.\n");
    }
    if (LcdGetKey(LCD_KEY_X) == 0) {
        *params = plasma_chaos;
        printf("Switched to chaos plasma mode.\n");
    }
    if (LcdGetKey(LCD_KEY_Y) == 0) {
        *params = plasma_spiral;
        printf("Switched to spiral plasma mode.\n");
    }

    if (LcdGetKey(LCD_KEY_UP) == 0) {
        *dt = fmin(*dt + 0.01f, 0.5f);
        printf("Increased time step to %.2f.\n", *dt);
    }
    if (LcdGetKey(LCD_KEY_DOWN) == 0) {
        *dt = fmax(*dt - 0.01f, 0.01f);
        printf("Decreased time step to %.2f.\n", *dt);
    }
    if (LcdGetKey(LCD_KEY_LEFT) == 0) {
        *hue_shift -= 0.1f;
        printf("Decreased hue shift to %.2f.\n", *hue_shift);
    }
    if (LcdGetKey(LCD_KEY_RIGHT) == 0) {
        *hue_shift += 0.1f;
        printf("Increased hue shift to %.2f.\n", *hue_shift);
    }
    if (LcdGetKey(LCD_KEY_CTRL) == 0) {
        *dt = 0.05f;
        *hue_shift = 0.0f;
        printf("Reset time and hue shift.\n");
    }
}



void core1_entry() {
    while (true) {
        // Wait for frame ready signal
        multicore_fifo_pop_blocking();

        uint64_t start = time_us_64();

        // Push framebuffer to LCD (blocking SPI)
        LcdWriteToScreen();

        uint64_t end = time_us_64();
        uint64_t frame_us = end - start;

        float fps = 1000000.0f / (float)frame_us;

        // Even though pushing is in lockstep with main core, frame rate is calculated as potential
        printf("LCD write time: %llu us | FPS: %.2f\n",
               frame_us, fps);
    }
}


int main()
{
    init_sin_lut();

    LcdModuleInit();
    LcdBacklightPercent(80);
    const uint16_t *frame_buffer = LcdDisplayInit(SCAN_DIR_HORIZONTAL);

    float t = 0.0f;
    float hue_shift = 0.0f;
    float dt = 0.05f;

    multicore_launch_core1(core1_entry);

    uint64_t last_time = time_us_64();

    PlasmaParams params = plasma_default;

    while (true) {
        uint64_t start = time_us_64();

        fill_plasma((uint16_t *)frame_buffer, t, &params, hue_shift);
        // Signal core1 to push frame
        multicore_fifo_push_blocking(1);

        uint64_t end = time_us_64();
        uint64_t frame_us = end - start;

        float fps = 1000000.0f / (float)frame_us;

        printf("Frame time: %llu us | FPS: %.2f\n",
               frame_us, fps);
        
        handle_input(&params, &dt, &hue_shift);

        t += dt;
    }

    return 0;
}
