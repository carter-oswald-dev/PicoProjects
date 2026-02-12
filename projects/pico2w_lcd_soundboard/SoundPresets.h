#pragma once

#include <Arduino.h>

constexpr uint8_t PRESET_OSC_COUNT = 4;
constexpr uint8_t PRESET_COUNT = 16;

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

// Preset order is part of user-facing behavior:
// - firmware paging/UI maps indices directly to button slots
// - preset lab sync preserves this ordering
// Keep names/order stable unless intentionally retuning the soundboard UX.
static const SoundPreset kSoundPresets[PRESET_COUNT] = {
  // 1) Natural short chirp cluster. Kept bright but not piercing so it stays
  // recognizable on small Bluetooth speakers.
  {
    "Bird song",
    0.28f,
    0.0015f,
    0.075f,
    1.0f,
    0.84f,
    {0.22f, 0.92f},
    {16.0f, 48.0f, 0.18f},
    {3, 0.030f, 0.90f, 1.4f},
    {0.080f, 2.0f},
    {0.024f},
    {0.08f, 0.6f, 1.6f},
    {
      {SRC_PULSE, 1850.0f, 3900.0f, 0.56f, 0.48f, 0.0f, 0.0f, 0.20f, 0.11f, 220.0f, 80.0f},
      {SRC_SINE, 2750.0f, 5100.0f, 0.30f, 0.26f, 0.0f, 0.0f, 0.50f, 0.50f, 170.0f, 60.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.07f, 0.02f, 2400.0f, 9800.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_SAW, 880.0f, 1950.0f, 0.17f, 0.12f, 0.0f, 0.0f, 0.50f, 0.50f, 120.0f, 40.0f},
    }
  },
  // 2) Broadband transient + short low body tail.
  {
    "Gunshot",
    0.31f,
    0.0002f,
    0.22f,
    1.0f,
    0.01f,
    {0.03f, 0.98f},
    {0.0f, 0.0f, 0.0f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.009f, -8.0f},
    {0.0f},
    {0.0f, 0.0f, 0.0f},
    {
      {SRC_NOISE, 0.0f, 0.0f, 1.70f, 0.40f, 520.0f, 9800.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_SINE, 105.0f, 42.0f, 0.78f, 0.26f, 0.0f, 0.0f, 0.50f, 0.50f, -260.0f, -360.0f},
      {SRC_SAW, 2900.0f, 240.0f, 0.42f, 0.06f, 0.0f, 0.0f, 0.50f, 0.50f, -1200.0f, -1800.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.34f, 0.08f, 1600.0f, 7600.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
  // 3) Low mechanical idle bed intended as "always moving" texture.
  {
    "Engine idle",
    1.85f,
    0.003f,
    0.45f,
    1.0f,
    0.70f,
    {0.55f, 0.94f},
    {2.2f, 8.0f, 0.06f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f},
    {0.052f},
    {0.00f, 0.0f, 0.0f},
    {
      {SRC_NOISE, 0.0f, 0.0f, 0.72f, 0.56f, 30.0f, 1500.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_PULSE, 58.0f, 52.0f, 0.44f, 0.38f, 0.0f, 0.0f, 0.46f, 0.50f, -10.0f, -3.0f},
      {SRC_SAW, 120.0f, 90.0f, 0.26f, 0.22f, 0.0f, 0.0f, 0.50f, 0.50f, -14.0f, -4.0f},
      {SRC_SINE, 32.0f, 28.0f, 0.30f, 0.24f, 0.0f, 0.0f, 0.50f, 0.50f, -4.0f, -1.0f},
    }
  },
  // 4) Long horn with stacked harmonics and restrained turbulence.
  {
    "Air horn",
    1.30f,
    0.001f,
    0.20f,
    1.0f,
    0.86f,
    {0.30f, 0.86f},
    {1.2f, 2.0f, 0.01f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f},
    {0.300f},
    {0.03f, 0.8f, 1.5f},
    {
      {SRC_SAW, 380.0f, 505.0f, 0.78f, 0.62f, 0.0f, 0.0f, 0.50f, 0.50f, 14.0f, 2.0f},
      {SRC_PULSE, 290.0f, 410.0f, 0.52f, 0.40f, 0.0f, 0.0f, 0.40f, 0.36f, 12.0f, 2.0f},
      {SRC_SINE, 640.0f, 790.0f, 0.26f, 0.22f, 0.0f, 0.0f, 0.50f, 0.50f, 10.0f, 2.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.05f, 0.01f, 1000.0f, 6200.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
  // 5) Cartoon spring with strong pitch bend and resonant body.
  {
    "Boing spring",
    0.88f,
    0.001f,
    0.34f,
    1.0f,
    0.52f,
    {0.30f, 0.92f},
    {9.4f, 34.0f, 0.13f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.11f, -6.0f},
    {0.068f},
    {0.10f, 0.8f, 2.2f},
    {
      {SRC_PULSE, 230.0f, 980.0f, 0.66f, 0.36f, 0.0f, 0.0f, 0.24f, 0.48f, 580.0f, -2000.0f},
      {SRC_SINE, 150.0f, 640.0f, 0.40f, 0.22f, 0.0f, 0.0f, 0.50f, 0.50f, 320.0f, -1050.0f},
      {SRC_SAW, 720.0f, 210.0f, 0.26f, 0.13f, 0.0f, 0.0f, 0.50f, 0.50f, -300.0f, -720.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.06f, 0.01f, 1400.0f, 8200.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
  // 6) UI/game jump accent with upward contour and quick release.
  {
    "Jump arc",
    0.26f,
    0.001f,
    0.08f,
    1.0f,
    0.60f,
    {0.34f, 0.91f},
    {7.2f, 26.0f, 0.06f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.060f, 4.4f},
    {0.0f},
    {0.09f, 0.9f, 1.9f},
    {
      {SRC_PULSE, 260.0f, 980.0f, 0.64f, 0.46f, 0.0f, 0.0f, 0.22f, 0.13f, 280.0f, 130.0f},
      {SRC_SINE, 190.0f, 720.0f, 0.32f, 0.21f, 0.0f, 0.0f, 0.50f, 0.50f, 180.0f, 90.0f},
      {SRC_SAW, 420.0f, 1450.0f, 0.14f, 0.11f, 0.0f, 0.0f, 0.50f, 0.50f, 330.0f, 140.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.03f, 0.00f, 2300.0f, 8500.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
  // 7) Positive rise cue.
  {
    "Power up",
    0.48f,
    0.002f,
    0.120f,
    1.0f,
    0.72f,
    {0.42f, 0.98f},
    {5.8f, 30.0f, 0.05f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.140f, 7.0f},
    {0.0f},
    {0.12f, 1.4f, 3.2f},
    {
      {SRC_PULSE, 360.0f, 1380.0f, 0.52f, 0.40f, 0.0f, 0.0f, 0.28f, 0.14f, 220.0f, 110.0f},
      {SRC_SINE, 540.0f, 1960.0f, 0.28f, 0.24f, 0.0f, 0.0f, 0.50f, 0.50f, 260.0f, 100.0f},
      {SRC_SQUARE, 820.0f, 2600.0f, 0.16f, 0.12f, 0.0f, 0.0f, 0.50f, 0.50f, 280.0f, 90.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.02f, 0.00f, 4000.0f, 11000.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
  // 8) Inverse/downward variant of the power cue.
  {
    "Power down",
    0.50f,
    0.002f,
    0.130f,
    1.0f,
    0.68f,
    {0.36f, 0.90f},
    {4.6f, 16.0f, 0.03f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.145f, -7.0f},
    {0.0f},
    {0.11f, 1.3f, 2.8f},
    {
      {SRC_PULSE, 1480.0f, 320.0f, 0.50f, 0.34f, 0.0f, 0.0f, 0.16f, 0.32f, -260.0f, -130.0f},
      {SRC_SINE, 1800.0f, 420.0f, 0.30f, 0.22f, 0.0f, 0.0f, 0.50f, 0.50f, -300.0f, -120.0f},
      {SRC_SAW, 2200.0f, 560.0f, 0.18f, 0.10f, 0.0f, 0.0f, 0.50f, 0.50f, -360.0f, -150.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.03f, 0.00f, 3600.0f, 9000.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
  // 9) Long descending whistle, intentionally no terminal impact layer.
  {
    "Falling missile",
    2.45f,
    0.003f,
    0.42f,
    1.0f,
    0.56f,
    {0.62f, 0.94f},
    {3.2f, 18.0f, 0.05f},
    {1, 0.0f, 1.0f, 0.0f},
    {1.55f, -2.5f},
    {0.0f},
    {0.06f, 0.8f, 2.8f},
    {
      {SRC_PULSE, 1380.0f, 620.0f, 0.54f, 0.42f, 0.0f, 0.0f, 0.16f, 0.24f, -160.0f, -70.0f},
      {SRC_SINE, 1120.0f, 520.0f, 0.36f, 0.28f, 0.0f, 0.0f, 0.50f, 0.50f, -140.0f, -60.0f},
      {SRC_SAW, 760.0f, 360.0f, 0.18f, 0.12f, 0.0f, 0.0f, 0.50f, 0.50f, -120.0f, -50.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.03f, 0.01f, 3200.0f, 9600.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
  // 10) Short high-energy zap with aggressive downward sweep.
  {
    "Laser zap",
    0.15f,
    0.0004f,
    0.070f,
    1.0f,
    0.16f,
    {0.14f, 0.92f},
    {1.8f, 12.0f, 0.03f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.013f, -6.0f},
    {0.022f},
    {0.10f, 0.4f, 1.3f},
    {
      {SRC_SAW, 3600.0f, 360.0f, 0.86f, 0.24f, 0.0f, 0.0f, 0.50f, 0.50f, -1700.0f, -2200.0f},
      {SRC_PULSE, 2100.0f, 220.0f, 0.48f, 0.14f, 0.0f, 0.0f, 0.18f, 0.08f, -1100.0f, -1400.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.18f, 0.05f, 2400.0f, 11200.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_SINE, 920.0f, 100.0f, 0.28f, 0.09f, 0.0f, 0.0f, 0.50f, 0.50f, -500.0f, -640.0f},
    }
  },
  // 11) Multi-pulse weapon burst; repeat block does the cadence.
  {
    "Laser burst",
    0.18f,
    0.0006f,
    0.085f,
    1.0f,
    0.20f,
    {0.17f, 0.93f},
    {5.5f, 18.0f, 0.04f},
    {3, 0.014f, 0.90f, 0.6f},
    {0.015f, -5.0f},
    {0.0f},
    {0.12f, 0.7f, 2.0f},
    {
      {SRC_SAW, 2600.0f, 480.0f, 0.86f, 0.22f, 0.0f, 0.0f, 0.50f, 0.50f, -900.0f, -1200.0f},
      {SRC_PULSE, 980.0f, 180.0f, 0.46f, 0.14f, 0.0f, 0.0f, 0.24f, 0.12f, -420.0f, -560.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.22f, 0.08f, 2400.0f, 12000.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_SINE, 520.0f, 110.0f, 0.30f, 0.10f, 0.0f, 0.0f, 0.50f, 0.50f, -190.0f, -260.0f},
    }
  },
  // 12) Extended decay blast for pairing with missile-style presets.
  {
    "Explosion",
    1.05f,
    0.0008f,
    0.62f,
    1.0f,
    0.12f,
    {0.09f, 0.90f},
    {0.0f, 0.0f, 0.0f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.030f, -8.0f},
    {0.0f},
    {0.08f, 0.5f, 1.6f},
    {
      {SRC_NOISE, 0.0f, 0.0f, 1.55f, 0.42f, 70.0f, 6800.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_SINE, 118.0f, 30.0f, 0.74f, 0.30f, 0.0f, 0.0f, 0.50f, 0.50f, -180.0f, -260.0f},
      {SRC_SAW, 340.0f, 50.0f, 0.40f, 0.16f, 0.0f, 0.0f, 0.50f, 0.50f, -250.0f, -360.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.34f, 0.10f, 1700.0f, 10800.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
  // 13) Heavier low-frequency machine rumble.
  {
    "Tank rumble",
    3.10f,
    0.003f,
    0.95f,
    1.0f,
    0.46f,
    {0.52f, 0.90f},
    {4.2f, 7.5f, 0.08f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f},
    {0.048f},
    {0.00f, 0.0f, 0.0f},
    {
      {SRC_NOISE, 0.0f, 0.0f, 0.86f, 0.52f, 32.0f, 1600.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_PULSE, 68.0f, 58.0f, 0.46f, 0.34f, 0.0f, 0.0f, 0.48f, 0.42f, -9.0f, -3.0f},
      {SRC_SAW, 132.0f, 88.0f, 0.30f, 0.20f, 0.0f, 0.0f, 0.50f, 0.50f, -15.0f, -5.0f},
      {SRC_SINE, 33.0f, 27.0f, 0.26f, 0.18f, 0.0f, 0.0f, 0.50f, 0.50f, -3.0f, -1.0f},
    }
  },
  // 14) Rotor-like modulation with mid-low noisy bed.
  {
    "Helicopter",
    2.45f,
    0.005f,
    0.54f,
    1.0f,
    0.70f,
    {0.56f, 0.95f},
    {5.8f, 9.0f, 0.16f},
    {1, 0.0f, 1.0f, 0.0f},
    {1.60f, -0.8f},
    {0.072f},
    {0.06f, 1.0f, 2.4f},
    {
      {SRC_PULSE, 82.0f, 79.0f, 0.58f, 0.52f, 0.0f, 0.0f, 0.44f, 0.48f, 0.0f, 0.0f},
      {SRC_SAW, 164.0f, 154.0f, 0.44f, 0.38f, 0.0f, 0.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_SINE, 390.0f, 350.0f, 0.24f, 0.22f, 0.0f, 0.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.22f, 0.14f, 130.0f, 2300.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
  // 15) Lower pass-by profile with gradual build and release.
  {
    "Race car",
    2.05f,
    0.002f,
    0.62f,
    1.0f,
    0.56f,
    {0.64f, 0.98f},
    {6.8f, 18.0f, 0.08f},
    {1, 0.0f, 1.0f, 0.0f},
    {1.12f, 2.0f},
    {0.032f},
    {0.04f, 0.4f, 1.4f},
    {
      {SRC_PULSE, 88.0f, 74.0f, 0.56f, 0.46f, 0.0f, 0.0f, 0.44f, 0.52f, -12.0f, -4.0f},
      {SRC_SAW, 152.0f, 108.0f, 0.44f, 0.34f, 0.0f, 0.0f, 0.50f, 0.50f, -20.0f, -7.0f},
      {SRC_SINE, 62.0f, 50.0f, 0.32f, 0.26f, 0.0f, 0.0f, 0.50f, 0.50f, -7.0f, -2.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.14f, 0.05f, 60.0f, 2200.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
  // 16) High-frequency tone cluster with subtle movement.
  {
    "Mosquito",
    1.80f,
    0.002f,
    0.28f,
    1.0f,
    0.72f,
    {0.78f, 0.98f},
    {6.2f, 24.0f, 0.08f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f},
    {0.0f},
    {0.00f, 0.0f, 0.0f},
    {
      {SRC_PULSE, 14800.0f, 15150.0f, 0.52f, 0.46f, 0.0f, 0.0f, 0.08f, 0.14f, 90.0f, -50.0f},
      {SRC_SINE, 14200.0f, 14900.0f, 0.34f, 0.30f, 0.0f, 0.0f, 0.50f, 0.50f, 120.0f, -80.0f},
      {SRC_SAW, 9600.0f, 12200.0f, 0.10f, 0.06f, 0.0f, 0.0f, 0.50f, 0.50f, 180.0f, -120.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.02f, 0.00f, 6000.0f, 13000.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
};
