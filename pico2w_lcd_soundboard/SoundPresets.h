#pragma once

#include <Arduino.h>

constexpr uint8_t PRESET_OSC_COUNT = 3;
constexpr uint8_t PRESET_COUNT = 4;

struct OscPreset {
  float freq_start_hz;
  float freq_end_hz;
  float mix_start;
  float mix_end;
};

struct SoundPreset {
  const char* name;
  float duration_s;
  float attack_s;
  float release_s;
  float loudness_start;
  float loudness_end;
  OscPreset osc[PRESET_OSC_COUNT];
};

static const SoundPreset kSoundPresets[PRESET_COUNT] = {
  {
    "Fart",
    0.95f,
    0.01f,
    0.30f,
    1.0f,
    0.75f,
    {
      {700.0f, 120.0f, 0.55f, 0.40f},
      {180.0f, 70.0f, 0.30f, 0.35f},
      {420.0f, 140.0f, 0.22f, 0.18f},
    }
  },
  {
    "Scream",
    1.15f,
    0.01f,
    0.28f,
    1.0f,
    0.65f,
    {
      {520.0f, 980.0f, 0.50f, 0.45f},
      {780.0f, 1480.0f, 0.28f, 0.30f},
      {1220.0f, 2100.0f, 0.18f, 0.22f},
    }
  },
  {
    "Bird song",
    0.70f,
    0.01f,
    0.20f,
    1.0f,
    0.80f,
    {
      {1400.0f, 3200.0f, 0.55f, 0.40f},
      {900.0f, 2200.0f, 0.28f, 0.30f},
      {2400.0f, 1800.0f, 0.20f, 0.22f},
    }
  },
  {
    "Gunshot",
    0.22f,
    0.001f,
    0.12f,
    1.0f,
    0.08f,
    {
      {120.0f, 70.0f, 0.70f, 0.40f},
      {1800.0f, 300.0f, 0.26f, 0.20f},
      {2600.0f, 420.0f, 0.20f, 0.12f},
    }
  },
};
