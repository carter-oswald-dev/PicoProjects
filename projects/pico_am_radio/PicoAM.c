#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "squarewave.pio.h"

// One GPIO outputs the carrier wave via PIO.
#define TONE_GPIO   2

// Carrier frequency (Hz)
#define CARRIER_FREQ_HZ  1000000u

// Tempo control (ms per "beat")
#define TEMPO_MS    240

#define ARTICULATION_NUM 9u
#define ARTICULATION_DEN 10u
#define PHRASE_GAP_US    1500000ull

typedef struct {
    int8_t midi_note;   // -1 = rest
    uint16_t beats;     // duration in beats
} note_t;

// A simple "Shave and a Haircut" approximation.
// Adjust notes/durations as desired.
static const note_t song[] = {
    {72, 2},   // C5 quarter (line 3-4)
    {67, 1},   // G4 eighth (line 2)
    {67, 1},   // G4 eighth (line 2)
    {68, 2},   // Ab4 quarter (flat 2-3)
    {67, 2},   // G4 quarter (line 2)
    {-1, 2},   // quarter rest
    {71, 2},   // B4 quarter (line 3)
    {72, 2},   // C5 quarter (line 3-4)
    {-1, 2},   // quarter rest
};

static PIO pio = pio0;
static uint sm;

// Initialize and start the PIO state machine producing a continuous carrier.
// The PIO program toggles the pin every instruction pair, so compute clkdiv accordingly.
static void carrier_pio_init(void) {
    sm = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &squarewave_program);

    // Ensure the pin is usable by PIO
    pio_gpio_init(pio, TONE_GPIO);

    // Compute clock divider: sys_freq / (2 * carrier_freq)
    float sys_hz = (float)clock_get_hz(clk_sys);
    float div = sys_hz / (2.0f * (float)CARRIER_FREQ_HZ);

    // Build a valid SM config: bind SET to our pin and set clkdiv.
    pio_sm_config c = squarewave_program_get_default_config(offset);
    sm_config_set_set_pins(&c, TONE_GPIO, 1);
    sm_config_set_clkdiv(&c, div);

    pio_sm_set_consecutive_pindirs(pio, sm, TONE_GPIO, 1, true); // SM will drive the pin
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);

    // Ensure pin function is handed to PIO so carrier actually outputs immediately.
    gpio_set_function(TONE_GPIO, GPIO_FUNC_PIO0);
}

// Gate carrier off by preloading low then switching control to SIO (GPIO)
static inline void gate_carrier_off(uint gpio) {
    gpio_put(gpio, 0);                  // preload low
    gpio_set_function(gpio, GPIO_FUNC_SIO);
    gpio_set_dir(gpio, GPIO_OUT);       // ensure SIO is driving
    gpio_put(gpio, 0);                  // force low after mux switch
}

// Gate carrier on by switching pin function back to PIO0 (PIO SM keeps running)
static inline void gate_carrier_on(uint gpio) {
    gpio_set_function(gpio, GPIO_FUNC_PIO0);
}

static inline bool deadline_passed_us(uint64_t now_us, uint64_t deadline_us) {
    return (int64_t)(now_us - deadline_us) >= 0;
}

// Convert MIDI note to frequency in Hz at runtime, anchored to A4 (MIDI 69) = 440 Hz.
static float midi_to_freq_hz(int8_t midi_note) {
    if (midi_note < 0 || midi_note > 127) {
        return 0.0f;
    }

    const float semitone_ratio = 1.0594630943592953f; // 2^(1/12)
    int steps = (int)midi_note - 69;
    float freq = 440.0f;

    while (steps > 0) {
        freq *= semitone_ratio;
        steps--;
    }
    while (steps < 0) {
        freq /= semitone_ratio;
        steps++;
    }

    return freq;
}

// Return half-period in microseconds for note-rate OOK toggling.
static uint32_t midi_to_half_period_us(int8_t midi_note) {
    float freq_hz = midi_to_freq_hz(midi_note);
    if (freq_hz <= 0.0f) {
        return 0;
    }

    uint32_t half_period_us = (uint32_t)((500000.0f / freq_hz) + 0.5f);
    if (half_period_us == 0) {
        half_period_us = 1;
    }
    return half_period_us;
}

// Microsecond deadline wait: coarse sleep for most of the interval, then spin.
static void wait_until_us(uint64_t deadline_us) {
    while (true) {
        uint64_t now_us = time_us_64();
        if (deadline_passed_us(now_us, deadline_us)) {
            return;
        }

        int64_t remaining_us = (int64_t)(deadline_us - now_us);
        if (remaining_us > 250) {
            sleep_us((uint32_t)(remaining_us - 150));
        } else {
            tight_loop_contents();
        }
    }
}

static void play_song_forever(void) {
    while (true) {
        for (size_t i = 0; i < sizeof(song)/sizeof(song[0]); i++) {
            const note_t n = song[i];
            uint64_t note_start_us = time_us_64();
            uint64_t dur_us = (uint64_t)n.beats * (uint64_t)TEMPO_MS * 1000ull;
            uint64_t active_us = (dur_us * ARTICULATION_NUM) / ARTICULATION_DEN;
            uint64_t active_end_us = note_start_us + active_us;
            uint64_t note_end_us = note_start_us + dur_us;

            if (n.midi_note < 0) {
                gate_carrier_off(TONE_GPIO);
                wait_until_us(note_end_us);
                continue;
            }

            uint32_t half_period_us = midi_to_half_period_us(n.midi_note);
            if (half_period_us == 0) {
                gate_carrier_off(TONE_GPIO);
                wait_until_us(note_end_us);
                continue;
            }

            bool carrier_on = false;
            uint64_t next_toggle_us = note_start_us;

            // Start each note with carrier hidden, then toggle at note half-period.
            gate_carrier_off(TONE_GPIO);
            while (true) {
                uint64_t now_us = time_us_64();
                if (deadline_passed_us(now_us, active_end_us)) {
                    break;
                }

                if (deadline_passed_us(now_us, next_toggle_us)) {
                    do {
                        carrier_on = !carrier_on;
                        if (carrier_on) {
                            gate_carrier_on(TONE_GPIO);
                        } else {
                            gate_carrier_off(TONE_GPIO);
                        }
                        next_toggle_us += half_period_us;
                    } while (deadline_passed_us(now_us, next_toggle_us));
                } else {
                    int64_t until_toggle_us = (int64_t)(next_toggle_us - now_us);
                    if (until_toggle_us > 200) {
                        sleep_us((uint32_t)(until_toggle_us - 100));
                    } else {
                        tight_loop_contents();
                    }
                }
            }

            // Articulation tail: force OFF for the final fraction of the note.
            if (carrier_on) {
                gate_carrier_off(TONE_GPIO);
            }
            wait_until_us(note_end_us);
        }

        gate_carrier_off(TONE_GPIO);
        wait_until_us(time_us_64() + PHRASE_GAP_US);
    }
}

int main(void) {
    stdio_init_all();
    sleep_ms(200);

    carrier_pio_init();
    play_song_forever();
    return 0;
}
