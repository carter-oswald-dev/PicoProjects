#pragma once

#include <Arduino.h>

constexpr uint8_t PRESET_OSC_COUNT = 4;
constexpr uint8_t PRESET_COUNT = 4;

// Oscillator source type used by each oscillator slot.
enum SourceType : uint8_t { SRC_SINE = 0, SRC_NOISE = 1 };

struct OscPreset {
  uint8_t source;      // SourceType: SRC_SINE or SRC_NOISE.
  float freq_start_hz; // Start frequency in Hz for sweep (sine sources).
  float freq_end_hz;   // End frequency in Hz for sweep (sine sources).
  float mix_start;     // Start linear gain for this oscillator.
  float mix_end;       // End linear gain for this oscillator.
  float noise_hp_hz;   // Noise high-pass cutoff in Hz (noise sources).
  float noise_lp_hz;   // Noise low-pass cutoff in Hz (noise sources).
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
  OscPreset osc[PRESET_OSC_COUNT]; // Fixed oscillator slots.
};

static const SoundPreset kSoundPresets[PRESET_COUNT] = {
  {
    "Fart",
    0.88f,
    0.005f,
    0.26f,
    1.0f,
    0.58f,
    {0.25f, 0.70f},
    {9.2f, 62.0f, 0.24f},
    {1, 0.0f, 1.0f, 0.0f},
    {
      {SRC_SINE, 120.0f, 45.0f, 0.58f, 0.44f, 0.0f, 0.0f},
      {SRC_SINE, 75.0f, 35.0f, 0.38f, 0.30f, 0.0f, 0.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.08f, 0.02f, 40.0f, 700.0f},
      {SRC_SINE, 260.0f, 90.0f, 0.22f, 0.10f, 0.0f, 0.0f},
    }
  },
  {
    "Scream",
    1.25f,
    0.004f,
    0.33f,
    1.0f,
    0.75f,
    {0.40f, 0.95f},
    {9.5f, 140.0f, 0.10f},
    {1, 0.0f, 1.0f, 0.0f},
    {
      {SRC_SINE, 1800.0f, 3200.0f, 0.36f, 0.30f, 0.0f, 0.0f},
      {SRC_SINE, 2600.0f, 4200.0f, 0.24f, 0.22f, 0.0f, 0.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.75f, 0.58f, 1200.0f, 8500.0f},
      {SRC_SINE, 4200.0f, 6200.0f, 0.12f, 0.16f, 0.0f, 0.0f},
    }
  },
  {
    "Bird song",
    0.46f,
    0.003f,
    0.12f,
    1.0f,
    0.88f,
    {0.30f, 0.90f},
    {14.0f, 58.0f, 0.16f},
    {5, 0.050f, 0.93f, 1.2f},
    {
      {SRC_SINE, 1800.0f, 3600.0f, 0.52f, 0.48f, 0.0f, 0.0f},
      {SRC_SINE, 2500.0f, 4600.0f, 0.28f, 0.25f, 0.0f, 0.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.04f, 0.01f, 2200.0f, 9000.0f},
      {SRC_SINE, 900.0f, 1700.0f, 0.22f, 0.18f, 0.0f, 0.0f},
    }
  },
  {
    "Gunshot",
    0.20f,
    0.0005f,
    0.14f,
    1.0f,
    0.06f,
    {0.03f, 0.75f},
    {0.0f, 0.0f, 0.0f},
    {1, 0.0f, 1.0f, 0.0f},
    {
      {SRC_NOISE, 0.0f, 0.0f, 1.20f, 0.35f, 1200.0f, 11000.0f},
      {SRC_SINE, 110.0f, 55.0f, 0.40f, 0.20f, 0.0f, 0.0f},
      {SRC_SINE, 3200.0f, 650.0f, 0.42f, 0.10f, 0.0f, 0.0f},
      {SRC_SINE, 190.0f, 85.0f, 0.26f, 0.16f, 0.0f, 0.0f},
    }
  },
};
