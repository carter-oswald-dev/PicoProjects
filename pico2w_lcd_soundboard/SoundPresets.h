#pragma once

#include <Arduino.h>

constexpr uint8_t PRESET_OSC_COUNT = 4;
constexpr uint8_t PRESET_COUNT = 5;

// Oscillator source type used by each oscillator slot.
enum SourceType : uint8_t {
  SRC_SINE = 0,
  SRC_NOISE = 1,
  SRC_SQUARE = 2,
  SRC_SAW = 3,
  SRC_PULSE = 4,
};

struct OscPreset {
  uint8_t source;      // SourceType enum value.
  float freq_start_hz; // Base sweep start Hz (tonal sources, > 0).
  float freq_end_hz;   // Base sweep end Hz (tonal sources, > 0).
  float mix_start;     // Start linear gain contribution (typical 0..1.5).
  float mix_end;       // End linear gain contribution (typical 0..1.5).
  float noise_hp_hz;   // Noise high-pass Hz (0 disables HP stage).
  float noise_lp_hz;   // Noise low-pass Hz (0 disables LP stage).
  float duty_start;    // Pulse duty at burst start (0.05..0.95, pulse/square).
  float duty_end;      // Pulse duty at burst end (0.05..0.95, pulse/square).
  float slide_hz_per_s;      // Linear pitch slide rate in Hz/sec.
  float dslide_hz_per_s2;    // Slide acceleration in Hz/sec^2.
};

struct LfoPreset {
  float rate_hz;           // LFO speed in Hz.
  float pitch_cents_depth; // Pitch modulation depth in cents.
  float amp_depth;         // Amplitude modulation depth in 0..1.
};

struct RepeatPreset {
  uint8_t count;               // Number of burst repeats (1 = one-shot).
  float gap_s;                 // Silence gap between repeats in seconds.
  float gain_decay_per_repeat; // Per-repeat gain multiplier.
  float pitch_semitone_step;   // Pitch offset per repeat in semitones.
};

struct Shape2Preset {
  float mid_u;    // Shape midpoint position along 0..1 of sound duration.
  float mid_gain; // Gain at midpoint for the 2-stage contour.
};

struct ArpPreset {
  float jump_time_s;   // Seconds into burst for one-time semitone jump.
  float jump_semitones; // Semitone offset applied once at jump time.
};

struct RetriggerPreset {
  float interval_s;    // Seconds between oscillator hard-resets (0 disables).
};

struct PhaserPreset {
  float mix;            // Wet mix 0..1.
  float delay_start_ms; // Delay at burst start in milliseconds.
  float delay_end_ms;   // Delay at burst end in milliseconds.
};

struct SoundPreset {
  const char* name;              // Display/debug name.
  float duration_s;              // Single burst duration in seconds.
  float attack_s;                // Attack time for AR envelope.
  float release_s;               // Release time for AR envelope.
  float loudness_start;          // Loudness multiplier at burst start.
  float loudness_end;            // Loudness multiplier at burst end.
  Shape2Preset shape;            // Extra 2-stage amplitude contour.
  LfoPreset lfo;                 // Shared LFO modulation settings.
  RepeatPreset repeat;           // Repeat burst settings.
  ArpPreset arp;                 // One-time arpeggio jump settings.
  RetriggerPreset retrigger;     // Hard reset cadence inside a burst.
  PhaserPreset phaser;           // Lightweight swept single-delay phaser.
  OscPreset osc[PRESET_OSC_COUNT]; // Fixed oscillator slots.
};

static const SoundPreset kSoundPresets[PRESET_COUNT] = {
  {
    "Fart",
    0.88f,
    0.005f,
    0.24f,
    1.0f,
    0.55f,
    {0.20f, 0.72f},
    {11.0f, 75.0f, 0.28f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f},
    {0.045f},
    {0.06f, 0.8f, 1.6f},
    {
      {SRC_PULSE, 115.0f, 52.0f, 0.62f, 0.50f, 0.0f, 0.0f, 0.62f, 0.43f, -46.0f, -90.0f},
      {SRC_SINE, 72.0f, 32.0f, 0.36f, 0.26f, 0.0f, 0.0f, 0.50f, 0.50f, -20.0f, -44.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.12f, 0.03f, 45.0f, 680.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_SAW, 210.0f, 80.0f, 0.18f, 0.08f, 0.0f, 0.0f, 0.50f, 0.50f, -80.0f, -130.0f},
    }
  },
  {
    "Scream",
    1.18f,
    0.006f,
    0.34f,
    1.0f,
    0.78f,
    {0.52f, 0.94f},
    {8.5f, 165.0f, 0.12f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.22f, 3.0f},
    {0.0f},
    {0.18f, 1.2f, 3.8f},
    {
      {SRC_PULSE, 2100.0f, 3200.0f, 0.42f, 0.34f, 0.0f, 0.0f, 0.22f, 0.12f, 280.0f, 150.0f},
      {SRC_SAW, 2900.0f, 5100.0f, 0.24f, 0.22f, 0.0f, 0.0f, 0.50f, 0.50f, 220.0f, 100.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.70f, 0.56f, 1500.0f, 9800.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_SINE, 3800.0f, 6400.0f, 0.14f, 0.19f, 0.0f, 0.0f, 0.50f, 0.50f, 320.0f, 130.0f},
    }
  },
  {
    "Bird song",
    0.40f,
    0.002f,
    0.09f,
    1.0f,
    0.86f,
    {0.26f, 0.91f},
    {17.0f, 52.0f, 0.18f},
    {3, 0.045f, 0.92f, 1.4f},
    {0.11f, 2.5f},
    {0.030f},
    {0.10f, 0.7f, 1.8f},
    {
      {SRC_PULSE, 2100.0f, 4300.0f, 0.54f, 0.48f, 0.0f, 0.0f, 0.22f, 0.11f, 240.0f, 80.0f},
      {SRC_SINE, 3200.0f, 5600.0f, 0.26f, 0.24f, 0.0f, 0.0f, 0.50f, 0.50f, 180.0f, 70.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.07f, 0.02f, 2400.0f, 10500.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_SAW, 1000.0f, 2200.0f, 0.20f, 0.15f, 0.0f, 0.0f, 0.50f, 0.50f, 120.0f, 40.0f},
    }
  },
  {
    "Gunshot",
    0.19f,
    0.0004f,
    0.13f,
    1.0f,
    0.04f,
    {0.04f, 0.82f},
    {0.0f, 0.0f, 0.0f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.014f, -5.0f},
    {0.0f},
    {0.04f, 0.3f, 0.9f},
    {
      {SRC_NOISE, 0.0f, 0.0f, 1.35f, 0.42f, 1400.0f, 12000.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_SINE, 130.0f, 58.0f, 0.52f, 0.24f, 0.0f, 0.0f, 0.50f, 0.50f, -240.0f, -360.0f},
      {SRC_SAW, 3600.0f, 560.0f, 0.48f, 0.12f, 0.0f, 0.0f, 0.50f, 0.50f, -1200.0f, -1900.0f},
      {SRC_PULSE, 220.0f, 92.0f, 0.32f, 0.18f, 0.0f, 0.0f, 0.47f, 0.40f, -300.0f, -500.0f},
    }
  },
  {
    "Wolf whistle",
    0.82f,
    0.009f,
    0.18f,
    1.0f,
    0.72f,
    {0.46f, 0.97f},
    {5.8f, 42.0f, 0.06f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.34f, 4.0f},
    {0.0f},
    {0.12f, 1.4f, 3.0f},
    {
      {SRC_PULSE, 880.0f, 2900.0f, 0.56f, 0.44f, 0.0f, 0.0f, 0.30f, 0.18f, 220.0f, 120.0f},
      {SRC_SINE, 1280.0f, 3600.0f, 0.24f, 0.20f, 0.0f, 0.0f, 0.50f, 0.50f, 180.0f, 90.0f},
      {SRC_SAW, 420.0f, 1300.0f, 0.16f, 0.11f, 0.0f, 0.0f, 0.50f, 0.50f, 100.0f, 50.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.04f, 0.00f, 2200.0f, 10000.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
};
