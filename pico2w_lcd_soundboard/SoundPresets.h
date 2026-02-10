#pragma once

#include <Arduino.h>

constexpr uint8_t PRESET_OSC_COUNT = 4;
constexpr uint8_t PRESET_COUNT = 14;

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
    "Coin pickup",
    0.16f,
    0.001f,
    0.05f,
    1.0f,
    0.45f,
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
    "Menu blip",
    0.085f,
    0.0006f,
    0.030f,
    1.0f,
    0.50f,
    {0.20f, 0.95f},
    {0.0f, 0.0f, 0.0f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f},
    {0.0f},
    {0.03f, 0.3f, 0.8f},
    {
      {SRC_PULSE, 1350.0f, 1850.0f, 0.58f, 0.42f, 0.0f, 0.0f, 0.22f, 0.18f, 80.0f, 0.0f},
      {SRC_SINE, 980.0f, 1320.0f, 0.24f, 0.20f, 0.0f, 0.0f, 0.50f, 0.50f, 40.0f, 0.0f},
      {SRC_SQUARE, 2200.0f, 2400.0f, 0.12f, 0.08f, 0.0f, 0.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.01f, 0.00f, 4500.0f, 12000.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
  {
    "UI click",
    0.045f,
    0.0003f,
    0.020f,
    1.0f,
    0.20f,
    {0.08f, 0.75f},
    {0.0f, 0.0f, 0.0f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f},
    {0.0f},
    {0.0f, 0.0f, 0.0f},
    {
      {SRC_NOISE, 0.0f, 0.0f, 0.80f, 0.15f, 2800.0f, 13000.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_PULSE, 2500.0f, 1800.0f, 0.22f, 0.08f, 0.0f, 0.0f, 0.12f, 0.08f, -700.0f, -1200.0f},
      {SRC_SQUARE, 3400.0f, 2600.0f, 0.16f, 0.04f, 0.0f, 0.0f, 0.50f, 0.50f, -500.0f, -800.0f},
      {SRC_SINE, 520.0f, 260.0f, 0.10f, 0.02f, 0.0f, 0.0f, 0.50f, 0.50f, -160.0f, -200.0f},
    }
  },
  {
    "Jump arc",
    0.24f,
    0.001f,
    0.070f,
    1.0f,
    0.55f,
    {0.35f, 0.90f},
    {6.5f, 22.0f, 0.05f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.060f, 4.0f},
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
    0.16f,
    0.0007f,
    0.080f,
    1.0f,
    0.25f,
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
    0.46f,
    0.002f,
    0.110f,
    1.0f,
    0.72f,
    {0.44f, 0.98f},
    {5.5f, 28.0f, 0.04f},
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
    0.48f,
    0.002f,
    0.120f,
    1.0f,
    0.64f,
    {0.38f, 0.90f},
    {4.8f, 18.0f, 0.03f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.150f, -7.0f},
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
    0.12f,
    0.0005f,
    0.050f,
    1.0f,
    0.20f,
    {0.15f, 0.85f},
    {0.0f, 0.0f, 0.0f},
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
    "Laser cannon",
    0.28f,
    0.0008f,
    0.110f,
    1.0f,
    0.22f,
    {0.22f, 0.92f},
    {3.2f, 18.0f, 0.05f},
    {2, 0.030f, 0.78f, -2.0f},
    {0.020f, -4.0f},
    {0.0f},
    {0.10f, 0.8f, 2.4f},
    {
      {SRC_SAW, 2600.0f, 260.0f, 0.70f, 0.20f, 0.0f, 0.0f, 0.50f, 0.50f, -900.0f, -1300.0f},
      {SRC_PULSE, 950.0f, 120.0f, 0.36f, 0.12f, 0.0f, 0.0f, 0.30f, 0.15f, -420.0f, -600.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.14f, 0.05f, 1800.0f, 10000.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_SINE, 420.0f, 70.0f, 0.22f, 0.08f, 0.0f, 0.0f, 0.50f, 0.50f, -180.0f, -220.0f},
    }
  },
  {
    "Explosion",
    0.58f,
    0.0008f,
    0.340f,
    1.0f,
    0.14f,
    {0.07f, 0.78f},
    {0.0f, 0.0f, 0.0f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.020f, -7.0f},
    {0.0f},
    {0.08f, 0.5f, 1.6f},
    {
      {SRC_NOISE, 0.0f, 0.0f, 1.10f, 0.26f, 120.0f, 6200.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_SINE, 150.0f, 45.0f, 0.46f, 0.16f, 0.0f, 0.0f, 0.50f, 0.50f, -180.0f, -240.0f},
      {SRC_SAW, 360.0f, 70.0f, 0.26f, 0.08f, 0.0f, 0.0f, 0.50f, 0.50f, -260.0f, -380.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.22f, 0.06f, 2200.0f, 10000.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
  {
    "Synth ping",
    0.30f,
    0.001f,
    0.120f,
    1.0f,
    0.52f,
    {0.30f, 0.96f},
    {5.2f, 16.0f, 0.06f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.090f, 3.0f},
    {0.0f},
    {0.14f, 1.1f, 3.0f},
    {
      {SRC_SINE, 780.0f, 760.0f, 0.50f, 0.34f, 0.0f, 0.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_PULSE, 1560.0f, 1500.0f, 0.28f, 0.22f, 0.0f, 0.0f, 0.26f, 0.20f, 0.0f, 0.0f},
      {SRC_SAW, 2340.0f, 2250.0f, 0.12f, 0.10f, 0.0f, 0.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.01f, 0.00f, 4800.0f, 12000.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
  {
    "Emergency siren",
    0.82f,
    0.002f,
    0.160f,
    1.0f,
    0.74f,
    {0.52f, 0.97f},
    {1.65f, 320.0f, 0.09f},
    {1, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f},
    {0.110f},
    {0.10f, 1.2f, 2.8f},
    {
      {SRC_PULSE, 760.0f, 1580.0f, 0.46f, 0.42f, 0.0f, 0.0f, 0.42f, 0.38f, 0.0f, 0.0f},
      {SRC_SAW, 640.0f, 1320.0f, 0.24f, 0.22f, 0.0f, 0.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_SQUARE, 920.0f, 1780.0f, 0.18f, 0.16f, 0.0f, 0.0f, 0.50f, 0.50f, 0.0f, 0.0f},
      {SRC_NOISE, 0.0f, 0.0f, 0.03f, 0.00f, 2500.0f, 9000.0f, 0.50f, 0.50f, 0.0f, 0.0f},
    }
  },
};
