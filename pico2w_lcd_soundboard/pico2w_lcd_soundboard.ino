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
#include "SoundPresets.h"

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
  Osc      o[PRESET_OSC_COUNT];
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
  if (id >= PRESET_COUNT) return;
  g_synth.soundId   = id;
  g_synth.playing   = true;
  g_synth.samplePos = 0;
  for (uint8_t i = 0; i < PRESET_OSC_COUNT; ++i) {
    g_synth.o[i].reset();
  }
}

static float synthSample() {
  if (!g_synth.playing) return 0.0f;

  if (g_synth.soundId >= PRESET_COUNT) {
    g_synth.playing = false;
    return 0.0f;
  }

  const SoundPreset& p = kSoundPresets[g_synth.soundId];
  float t = (float)g_synth.samplePos / (float)SR;

  if (p.duration_s <= 0.0f || t >= p.duration_s) {
    g_synth.playing = false;
    return 0.0f;
  }

  float u = clampf(t / p.duration_s, 0.0f, 1.0f);
  float amp = envAR(t, p.duration_s, p.attack_s, p.release_s) *
              lerp(p.loudness_start, p.loudness_end, u);
  float mix = 0.0f;

  for (uint8_t i = 0; i < PRESET_OSC_COUNT; ++i) {
    const OscPreset& o = p.osc[i];
    float f = lerp(o.freq_start_hz, o.freq_end_hz, u);
    float m = lerp(o.mix_start, o.mix_end, u);
    uint32_t inc = phaseIncFromHz(f);
    float s = (float)g_synth.o[i].next(inc) / 32767.0f;
    mix += m * s;
  }

  g_synth.samplePos++;

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
