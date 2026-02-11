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

static const SoundPreset kSoundPresets[PRESET_COUNT] = {
  {
    "Bird song",
    0.42f,
    0.002f,
    0.10f,
    1.0f,
    0.88f,
    {0.24f, 0.93f},
    {18.0f, 56.0f, 0.20f},
    {3, 0.042f, 0.92f, 1.5f},
    {0.11f, 2.5f},
    {0.028f},
    {0.10f, 0.7f, 1.8f},
    {
      {SRC_PULSE, 2150.0f, 4500.0f, 0.54f, 0.48f, 0.0f, 0.0f, 0.21f, 0.10f, 250.0f, 90.0f},
      {SRC_SINE, 3150.0f, 5750.0f, 0.28f, 0.25f, 0.0f, 0.0f, 0.50f, 0.50f, 190.0f, 72.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.08f, 0.02f, 2600.0f, 10600.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_SAW, 980.0f, 2300.0f, 0.19f, 0.14f, 0.0f, 0.0f, 0.50f, 0.50f, 130.0f, 45.0f},
    }
  },
  {
    "Gunshot",
    0.28f,
    0.0003f,
    0.20f,
    1.0f,
    0.02f,
    {0.03f, 0.95f},
    {0.0f, 0.0f, 0.0f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.010f, -7.0f},
    {0.0f},
    {0.0f, 0.0f, 0.0f},
    {
      {SRC_NOISE, 0.0f, 0.0f, 1.45f, 0.35f, 650.0f, 9500.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_SINE, 110.0f, 52.0f, 0.62f, 0.22f, 0.0f, 0.0f, 0.50f, 0.50f, -200.0f, -280.0f},
      {SRC_SAW, 2200.0f, 280.0f, 0.30f, 0.04f, 0.0f, 0.0f, 0.50f, 0.50f, -900.0f, -1400.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.22f, 0.05f, 1800.0f, 7000.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
  {
    "Coin chime",
    0.17f,
    0.001f,
    0.055f,
    1.0f,
    0.48f,
    {0.16f, 0.98f},
    {0.0f, 0.0f, 0.0f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.028f, 9.0f},
    {0.0f},
    {0.06f, 0.4f, 1.1f},
    {
      {SRC_PULSE, 1180.0f, 2460.0f, 0.62f, 0.40f, 0.0f, 0.0f, 0.18f, 0.10f, 300.0f, 120.0f},
      {SRC_SINE, 860.0f, 1720.0f, 0.28f, 0.18f, 0.0f, 0.0f, 0.50f, 0.50f, 220.0f, 80.0f},
      {SRC_SAW, 1500.0f, 3100.0f, 0.18f, 0.10f, 0.0f, 0.0f, 0.50f, 0.50f, 380.0f, 140.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.02f, 0.00f, 3500.0f, 11000.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
  {
    "Prank horn",
    0.36f,
    0.001f,
    0.11f,
    1.0f,
    0.58f,
    {0.35f, 0.90f},
    {6.0f, 8.0f, 0.07f},
    {2, 0.045f, 0.86f, 1.5f},
    {0.060f, 4.0f},
    {0.0f},
    {0.04f, 0.5f, 1.3f},
    {
      {SRC_SAW, 420.0f, 760.0f, 0.52f, 0.40f, 0.0f, 0.0f, 0.50f, 0.50f, 80.0f, 20.0f},
      {SRC_PULSE, 310.0f, 520.0f, 0.34f, 0.28f, 0.0f, 0.0f, 0.38f, 0.28f, 60.0f, 15.0f},
      {SRC_SINE, 840.0f, 1180.0f, 0.18f, 0.12f, 0.0f, 0.0f, 0.50f, 0.50f, 30.0f, 8.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.04f, 0.00f, 1200.0f, 7000.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
  {
    "Boing spring",
    0.42f,
    0.001f,
    0.16f,
    1.0f,
    0.50f,
    {0.30f, 0.86f},
    {7.8f, 22.0f, 0.08f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.090f, -5.0f},
    {0.060f},
    {0.08f, 0.7f, 2.0f},
    {
      {SRC_PULSE, 280.0f, 980.0f, 0.58f, 0.30f, 0.0f, 0.0f, 0.24f, 0.44f, 520.0f, -1800.0f},
      {SRC_SINE, 180.0f, 620.0f, 0.34f, 0.18f, 0.0f, 0.0f, 0.50f, 0.50f, 260.0f, -900.0f},
      {SRC_SAW, 700.0f, 260.0f, 0.20f, 0.10f, 0.0f, 0.0f, 0.50f, 0.50f, -300.0f, -600.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.03f, 0.00f, 1600.0f, 9000.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
  {
    "Jump arc",
    0.25f,
    0.001f,
    0.075f,
    1.0f,
    0.58f,
    {0.35f, 0.90f},
    {7.0f, 24.0f, 0.05f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.056f, 4.2f},
    {0.0f},
    {0.08f, 0.9f, 2.0f},
    {
      {SRC_PULSE, 260.0f, 980.0f, 0.62f, 0.44f, 0.0f, 0.0f, 0.22f, 0.13f, 260.0f, 120.0f},
      {SRC_SINE, 190.0f, 720.0f, 0.30f, 0.20f, 0.0f, 0.0f, 0.50f, 0.50f, 170.0f, 80.0f},
      {SRC_SAW, 420.0f, 1450.0f, 0.12f, 0.10f, 0.0f, 0.0f, 0.50f, 0.50f, 320.0f, 130.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.02f, 0.00f, 2300.0f, 8500.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
  {
    "Hit hurt",
    0.17f,
    0.0007f,
    0.085f,
    1.0f,
    0.28f,
    {0.13f, 0.82f},
    {0.0f, 0.0f, 0.0f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.012f, -4.0f},
    {0.0f},
    {0.06f, 0.6f, 1.4f},
    {
      {SRC_NOISE, 0.0f, 0.0f, 0.54f, 0.20f, 1200.0f, 8500.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_SAW, 520.0f, 160.0f, 0.38f, 0.14f, 0.0f, 0.0f, 0.50f, 0.50f, -420.0f, -520.0f},
      {SRC_PULSE, 340.0f, 120.0f, 0.24f, 0.09f, 0.0f, 0.0f, 0.42f, 0.26f, -240.0f, -320.0f},
      {SRC_SINE, 180.0f, 70.0f, 0.20f, 0.08f, 0.0f, 0.0f, 0.50f, 0.50f, -150.0f, -180.0f},
    }
  },
  {
    "Power up",
    0.48f,
    0.002f,
    0.120f,
    1.0f,
    0.74f,
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
  {
    "Power down",
    0.50f,
    0.002f,
    0.130f,
    1.0f,
    0.66f,
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
  {
    "Laser zap",
    0.13f,
    0.0005f,
    0.055f,
    1.0f,
    0.22f,
    {0.15f, 0.85f},
    {1.2f, 8.0f, 0.05f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.015f, -6.0f},
    {0.026f},
    {0.07f, 0.4f, 1.5f},
    {
      {SRC_SAW, 3200.0f, 420.0f, 0.62f, 0.18f, 0.0f, 0.0f, 0.50f, 0.50f, -1400.0f, -1800.0f},
      {SRC_PULSE, 1800.0f, 260.0f, 0.34f, 0.10f, 0.0f, 0.0f, 0.20f, 0.10f, -900.0f, -1200.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.10f, 0.02f, 2500.0f, 10500.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_SINE, 820.0f, 120.0f, 0.20f, 0.06f, 0.0f, 0.0f, 0.50f, 0.50f, -420.0f, -500.0f},
    }
  },
  {
    "Laser cannon burst",
    0.22f,
    0.0008f,
    0.090f,
    1.0f,
    0.28f,
    {0.18f, 0.92f},
    {4.0f, 22.0f, 0.05f},
    {3, 0.022f, 0.86f, 0.8f},
    {0.020f, -4.0f},
    {0.0f},
    {0.10f, 0.7f, 2.0f},
    {
      {SRC_SAW, 2200.0f, 520.0f, 0.68f, 0.18f, 0.0f, 0.0f, 0.50f, 0.50f, -760.0f, -1100.0f},
      {SRC_PULSE, 820.0f, 180.0f, 0.34f, 0.11f, 0.0f, 0.0f, 0.28f, 0.14f, -360.0f, -520.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.16f, 0.05f, 2200.0f, 11000.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_SINE, 480.0f, 120.0f, 0.22f, 0.08f, 0.0f, 0.0f, 0.50f, 0.50f, -150.0f, -200.0f},
    }
  },
  {
    "Explosion",
    0.70f,
    0.0008f,
    0.44f,
    1.0f,
    0.18f,
    {0.08f, 0.88f},
    {0.0f, 0.0f, 0.0f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.020f, -7.0f},
    {0.0f},
    {0.06f, 0.4f, 1.4f},
    {
      {SRC_NOISE, 0.0f, 0.0f, 1.35f, 0.34f, 80.0f, 7000.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_SINE, 120.0f, 38.0f, 0.62f, 0.24f, 0.0f, 0.0f, 0.50f, 0.50f, -160.0f, -220.0f},
      {SRC_SAW, 300.0f, 55.0f, 0.34f, 0.12f, 0.0f, 0.0f, 0.50f, 0.50f, -240.0f, -340.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.30f, 0.08f, 1800.0f, 11000.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
  {
    "Magic sparkle",
    0.46f,
    0.001f,
    0.18f,
    1.0f,
    0.48f,
    {0.22f, 0.96f},
    {6.8f, 28.0f, 0.08f},
    {3, 0.035f, 0.84f, 3.5f},
    {0.040f, 7.0f},
    {0.0f},
    {0.16f, 0.6f, 2.6f},
    {
      {SRC_PULSE, 900.0f, 2400.0f, 0.46f, 0.30f, 0.0f, 0.0f, 0.18f, 0.08f, 300.0f, 120.0f},
      {SRC_SINE, 1400.0f, 3200.0f, 0.28f, 0.20f, 0.0f, 0.0f, 0.50f, 0.50f, 220.0f, 90.0f},
      {SRC_SAW, 2200.0f, 4800.0f, 0.18f, 0.14f, 0.0f, 0.0f, 0.50f, 0.50f, 280.0f, 110.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.05f, 0.00f, 5200.0f, 13000.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
  {
    "Emergency siren",
    1.60f,
    0.002f,
    0.25f,
    1.0f,
    0.88f,
    {0.50f, 1.00f},
    {1.35f, 420.0f, 0.10f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f},
    {0.0f},
    {0.08f, 1.0f, 2.6f},
    {
      {SRC_PULSE, 680.0f, 1180.0f, 0.50f, 0.46f, 0.0f, 0.0f, 0.42f, 0.38f, 0.0f, 0.0f},
      {SRC_SAW, 540.0f, 940.0f, 0.32f, 0.30f, 0.0f, 0.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_SQUARE, 880.0f, 1520.0f, 0.22f, 0.20f, 0.0f, 0.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.06f, 0.02f, 1500.0f, 8000.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
  {
    "Wolf whistle",
    0.95f,
    0.006f,
    0.22f,
    1.0f,
    0.74f,
    {0.42f, 0.98f},
    {5.5f, 38.0f, 0.05f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.26f, 5.5f},
    {0.0f},
    {0.04f, 0.8f, 2.0f},
    {
      {SRC_SINE, 820.0f, 2550.0f, 0.56f, 0.44f, 0.0f, 0.0f, 0.50f, 0.50f, 160.0f, 60.0f},
      {SRC_PULSE, 1150.0f, 3400.0f, 0.26f, 0.18f, 0.0f, 0.0f, 0.30f, 0.16f, 210.0f, 80.0f},
      {SRC_SINE, 420.0f, 1280.0f, 0.20f, 0.12f, 0.0f, 0.0f, 0.50f, 0.50f, 90.0f, 40.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.02f, 0.00f, 3200.0f, 11000.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
  {
    "Rubber duck quack",
    0.48f,
    0.001f,
    0.17f,
    1.0f,
    0.50f,
    {0.24f, 0.86f},
    {10.5f, 26.0f, 0.12f},
    {2, 0.055f, 0.82f, -0.7f},
    {0.050f, -3.0f},
    {0.042f},
    {0.05f, 0.5f, 1.5f},
    {
      {SRC_PULSE, 520.0f, 260.0f, 0.62f, 0.36f, 0.0f, 0.0f, 0.38f, 0.25f, -280.0f, -620.0f},
      {SRC_SAW, 760.0f, 320.0f, 0.26f, 0.14f, 0.0f, 0.0f, 0.50f, 0.50f, -240.0f, -520.0f},
      {SRC_SINE, 330.0f, 180.0f, 0.22f, 0.12f, 0.0f, 0.0f, 0.50f, 0.50f, -130.0f, -240.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.04f, 0.01f, 1000.0f, 6500.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
};
