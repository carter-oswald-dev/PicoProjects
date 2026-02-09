/**
 * Pico 2W + Waveshare Pico-LCD-1.3: 4-button Bluetooth soundboard
 *
 * Target: Raspberry Pi Pico 2W (Arduino-Pico core by Earle Philhower)
 * Audio:  A2DP Source via BluetoothAudio.h (streams synthesized audio)
 * Display: Initialized through LcdDriver.c/.h (from PicoLcdGenerativeArt)
 *
 * Notes:
 * - Update SPEAKER_ADDR to your Bluetooth speaker's MAC address.
 * - Install library: "BluetoothAudio".
 * - LCD/button pin mapping comes from LcdDriver.h.
 */

#include <Arduino.h>
#include <BluetoothAudio.h>
#include <math.h>
#include "LcdDriver.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct ButtonState;

// ---------------- Bluetooth A2DP Source ----------------
A2DPSource bt;
// REPLACE with your speaker's MAC address:
static const uint8_t SPEAKER_ADDR[6] = {0xE8, 0xD0, 0x3C, 0x9B, 0x38, 0x39};

// ---------------- Audio format & block sizing ----------------
constexpr uint32_t SR       = 48000;   // A2DP-friendly
constexpr uint8_t  CHANNELS = 2;       // interleaved stereo (mono duplicated)
constexpr size_t   FRAMES   = 512;     // ~10.7 ms @ 48k

// ---------------- Small sine LUT + linear interpolation ----------------
constexpr int      LUT_BITS = 10;             // 1024 entries
constexpr uint32_t LUT_LEN  = (1u << LUT_BITS);
static int16_t     SINE_LUT[LUT_LEN];

static inline uint32_t phaseIncFromHz(float f) {
  return (uint32_t)((double)f * (4294967296.0 / (double)SR));  // f * 2^32 / SR
}

static inline int16_t lutSineLerp(uint32_t phase) {
  const uint32_t idx  = phase >> (32 - LUT_BITS);
  const uint32_t frac = (phase >> (32 - LUT_BITS - 8)) & 0xFF;
  const int16_t  a = SINE_LUT[idx & (LUT_LEN - 1)];
  const int16_t  b = SINE_LUT[(idx + 1) & (LUT_LEN - 1)];
  const int32_t  d = (int32_t)b - (int32_t)a;
  return (int16_t)(a + ((d * (int32_t)frac) >> 8));
}

struct Osc {
  uint32_t phase = 0;
  inline int16_t next(uint32_t inc) { phase += inc; return lutSineLerp(phase); }
  inline void reset() { phase = 0; }
};

// ---------------- Synth state ----------------
struct SynthState {
  uint8_t  soundId    = 0;
  bool     playing    = false;
  uint32_t samplePos  = 0;
  float    duration_s = 0.0f;
  Osc      o1, o2, o3;
};

static SynthState g_synth;

// ---------------- Helpers ----------------
static inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static inline float lerp(float a, float b, float t)   { return a + (b - a) * t; }

static float envAR(float t, float dur, float attack, float release) {
  if (t < 0.0f) return 0.0f;
  if (t < attack) return (attack > 0.0f) ? (t / attack) : 1.0f;
  float rem = dur - t;
  if (rem < release) return (release > 0.0f) ? (rem / release) : 0.0f;
  return 1.0f;
}

static void triggerSound(uint8_t id) {
  g_synth.soundId   = id;
  g_synth.playing   = true;
  g_synth.samplePos = 0;
  g_synth.o1.reset();
  g_synth.o2.reset();
  g_synth.o3.reset();
}

static float synthSample() {
  if (!g_synth.playing) return 0.0f;

  float t = (float)g_synth.samplePos / (float)SR;
  float u = 0.0f;
  float f1 = 0.0f, f2 = 0.0f, f3 = 0.0f;
  float m1 = 0.0f, m2 = 0.0f, m3 = 0.0f;
  float amp = 0.0f;

  switch (g_synth.soundId) {
    case 0: { // "Bird Fart"
      g_synth.duration_s = 1.0f;
      u = clampf(t / g_synth.duration_s, 0.0f, 1.0f);
      float up = (u < 0.20f) ? (u / 0.20f) : 1.0f;
      float dn = (u < 0.20f) ? 0.0f : ((u - 0.20f) / 0.80f);
      f1 = lerp(900.0f, 2400.0f, up);
      f1 = lerp(f1, 600.0f, dn);
      f2 = lerp(220.0f, 90.0f, u);
      f3 = lerp(520.0f, 260.0f, u);
      m1 = 0.55f; m2 = 0.30f; m3 = 0.20f;
      amp = envAR(t, g_synth.duration_s, 0.02f, 0.35f) * (1.0f - 0.15f * u);
    } break;

    case 1: { // "Alien Goose"
      g_synth.duration_s = 1.3f;
      u = clampf(t / g_synth.duration_s, 0.0f, 1.0f);
      f1 = 280.0f;
      f2 = lerp(520.0f, 430.0f, u);
      f3 = 820.0f;
      m1 = 0.55f; m2 = 0.30f; m3 = 0.15f;
      amp = envAR(t, g_synth.duration_s, 0.03f, 0.40f) * (0.85f - 0.25f * u);
    } break;

    case 2: { // "Laser Pew"
      g_synth.duration_s = 0.6f;
      u = clampf(t / g_synth.duration_s, 0.0f, 1.0f);
      f1 = lerp(2000.0f, 220.0f, u);
      f2 = lerp(1200.0f, 160.0f, u);
      f3 = lerp(700.0f, 120.0f, u);
      m1 = 0.60f; m2 = 0.30f; m3 = 0.10f;
      amp = envAR(t, g_synth.duration_s, 0.01f, 0.18f);
    } break;

    default: { // case 3: "Bubble Pop"
      g_synth.duration_s = 0.9f;
      u = clampf(t / g_synth.duration_s, 0.0f, 1.0f);
      f1 = lerp(300.0f, 900.0f, u);
      f2 = lerp(900.0f, 360.0f, u);
      f3 = lerp(1400.0f, 650.0f, u);
      m1 = 0.50f; m2 = 0.30f; m3 = 0.20f;
      amp = envAR(t, g_synth.duration_s, 0.02f, 0.30f) * (0.90f - 0.20f * u);
    } break;
  }

  if (t >= g_synth.duration_s) {
    g_synth.playing = false;
    return 0.0f;
  }

  uint32_t inc1 = phaseIncFromHz(f1);
  uint32_t inc2 = phaseIncFromHz(f2);
  uint32_t inc3 = phaseIncFromHz(f3);

  float s1 = (float)g_synth.o1.next(inc1) / 32767.0f;
  float s2 = (float)g_synth.o2.next(inc2) / 32767.0f;
  float s3 = (float)g_synth.o3.next(inc3) / 32767.0f;

  g_synth.samplePos++;

  float mix = (m1 * s1) + (m2 * s2) + (m3 * s3);
  return amp * mix;
}

// ---------------- Button handling ----------------
struct ButtonState {
  uint8_t pin = 0;
  bool    last = true;
  uint32_t lastMs = 0;
};

constexpr uint32_t DEBOUNCE_MS = 25;
static ButtonState btnA{LCD_KEY_A}, btnB{LCD_KEY_B}, btnX{LCD_KEY_X}, btnY{LCD_KEY_Y};

static void initButtons() {
  // Pins and pull-ups are configured by LcdModuleInit().
  btnA.last = LcdGetKey(btnA.pin);
  btnB.last = LcdGetKey(btnB.pin);
  btnX.last = LcdGetKey(btnX.pin);
  btnY.last = LcdGetKey(btnY.pin);
}

static bool buttonPressed(ButtonState &b) {
  bool cur = LcdGetKey(b.pin);
  uint32_t now = millis();
  if (cur != b.last && (now - b.lastMs) > DEBOUNCE_MS) {
    b.lastMs = now;
    b.last = cur;
    if (!cur) return true; // active-low
  }
  return false;
}

// ---------------- LCD driver init ----------------
static uint16_t* g_lcdFrameBuffer = nullptr;

static void lcdInit() {
  LcdModuleInit();
  LcdBacklightPercent(80);
  g_lcdFrameBuffer = LcdDisplayInit(SCAN_DIR_HORIZONTAL);
  if (g_lcdFrameBuffer != nullptr) {
    LcdClearScreen(0x0000);
  }
}

// ---------------- Arduino Setup ----------------
void setup() {
  Serial.begin(115200);

  // Build sine LUT
  for (uint32_t i = 0; i < LUT_LEN; ++i) {
    double th = (2.0 * M_PI * i) / (double)LUT_LEN;
    SINE_LUT[i] = (int16_t)lrint(32767.0 * sin(th));
  }

  lcdInit();
  initButtons();

  bt.begin();
  bt.connect(SPEAKER_ADDR);
}

// ---------------- Arduino Loop ----------------
void loop() {
  if (buttonPressed(btnA)) triggerSound(0);
  if (buttonPressed(btnB)) triggerSound(1);
  if (buttonPressed(btnX)) triggerSound(2);
  if (buttonPressed(btnY)) triggerSound(3);

  static int16_t buf[FRAMES * CHANNELS];

  for (size_t i = 0; i < FRAMES; ++i) {
    float y = synthSample();

    // master gain + soft clip guard
    const float MASTER_GAIN = 0.80f;
    y *= MASTER_GAIN;
    if (y > 0.999f) y = 0.999f;
    if (y < -0.999f) y = -0.999f;

    int16_t s = (int16_t)lrintf(y * 32767.0f);
    buf[2 * i + 0] = s;
    buf[2 * i + 1] = s;
  }

  // Stream to A2DP
  size_t nbytes = sizeof(buf);
  const uint8_t* p = reinterpret_cast<const uint8_t*>(buf);
  while (nbytes) {
    int w = bt.write(p, nbytes);
    if (w > 0) { p += w; nbytes -= w; }
    else       { delay(1); }
  }
}
