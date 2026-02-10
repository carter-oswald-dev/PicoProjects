/**
 * Pico 2W + Waveshare Pico-LCD-1.3: 4-button Bluetooth soundboard
 *
 * Target: Raspberry Pi Pico 2W (Arduino-Pico core by Earle Philhower)
 * Audio:  A2DP Source via BluetoothAudio.h (streams synthesized audio)
 * Display: Initialized through LcdDriver.c/.h (from PicoLcdGenerativeArt)
 */

#include <Arduino.h>
#include <BluetoothAudio.h>
#include <math.h>
#include "LcdDriver.h"
#include "SoundPresets.h"
#include "UiText.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct ButtonState;
struct Osc;

// ---------------- Bluetooth A2DP Source ----------------
A2DPSource bt;
static const uint8_t SPEAKER_ADDR[6] = {0xE8, 0xD0, 0x3C, 0x9B, 0x38, 0x39};

// ---------------- Audio format & block sizing ----------------
constexpr uint32_t SR       = 48000;   // A2DP-friendly
constexpr uint8_t  CHANNELS = 2;       // interleaved stereo (mono duplicated)
constexpr size_t   FRAMES   = 512;     // ~10.7 ms @ 48k
constexpr uint8_t  SOUND_VIEW_SLOTS = 4;
constexpr uint32_t BT_RECONNECT_MS = 3000;

static uint8_t  g_presetWindowStart = 0;
static uint32_t g_lastReconnectAttemptMs = 0;

static volatile bool g_audioPlaying = false;
static volatile uint8_t g_audioSoundId = 0;
static volatile bool g_streamEnabled = true;
static volatile bool g_btConnected = false;
static volatile bool g_btReady = false;
static volatile bool g_btStateChanged = false;

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
  inline uint32_t advance(uint32_t inc) { phase += inc; return phase; }
  inline void reset() { phase = 0; }
};

constexpr uint16_t PHASER_BUF_SIZE = 256;
static_assert((PHASER_BUF_SIZE & (PHASER_BUF_SIZE - 1)) == 0,
              "PHASER_BUF_SIZE must be power-of-two");

// ---------------- Synth state (core1 only) ----------------
struct SynthState {
  uint8_t  soundId    = 0;
  bool     playing    = false;
  uint8_t  repeatIndex = 0;
  uint32_t burstSamplePos = 0;
  uint32_t gapSamplesRemaining = 0;
  uint32_t lfoPhase = 0;
  uint32_t rng = 0xA5A5A5A5u;
  uint32_t retriggerSamples = 0;
  uint32_t retriggerCounter = 0;
  bool     arpApplied = false;
  float    noiseHpLpState[PRESET_OSC_COUNT] = {0.0f};
  float    noiseLpState[PRESET_OSC_COUNT] = {0.0f};
  float    curFreqHz[PRESET_OSC_COUNT] = {0.0f};
  float    curSlideHzPerS[PRESET_OSC_COUNT] = {0.0f};
  float    curDuty[PRESET_OSC_COUNT] = {0.5f};
  float    phaserBuf[PHASER_BUF_SIZE] = {0.0f};
  uint16_t phaserWrite = 0;
  Osc      o[PRESET_OSC_COUNT];
};

static SynthState g_synth;

// ---------------- Helpers ----------------
static inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static inline float safeClampf(float x, float a, float b, float fallback) {
  if (!isfinite(x)) return fallback;
  return clampf(x, a, b);
}
static inline float lerp(float a, float b, float t)   { return a + (b - a) * t; }
static inline uint8_t slotToPreset(uint8_t slot) {
  return (uint8_t)((g_presetWindowStart + slot) % PRESET_COUNT);
}
static inline float onePoleAlpha(float fc) {
  return clampf((2.0f * (float)M_PI * fc) / (float)SR, 0.0f, 1.0f);
}
static inline float semitoneRatio(float semitones) {
  return exp2f(semitones / 12.0f);
}

static inline uint32_t xorshift32(uint32_t &s) {
  if (s == 0u) s = 0x6D2B79F5u;
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return s;
}

static inline float whiteNoise() {
  const uint32_t r = xorshift32(g_synth.rng);
  return ((float)r * (1.0f / 4294967295.0f)) * 2.0f - 1.0f;
}

static inline float twoStageShape(float u, float mid_u, float mid_gain) {
  const float mu = safeClampf(mid_u, 0.01f, 0.99f, 0.5f);
  const float mg = safeClampf(mid_gain, 0.0f, 1.0f, 1.0f);
  const float x = clampf(u, 0.0f, 1.0f);
  if (x < mu) return lerp(1.0f, mg, x / mu);
  return lerp(mg, 0.0f, (x - mu) / (1.0f - mu));
}

static float envAR(float t, float dur, float attack, float release) {
  if (t < 0.0f) return 0.0f;
  if (t < attack) return (attack > 0.0f) ? (t / attack) : 1.0f;
  float rem = dur - t;
  if (rem < release) return (release > 0.0f) ? (rem / release) : 0.0f;
  return 1.0f;
}

static inline float oscWaveSample(Osc& osc, uint32_t inc, uint8_t source, float duty) {
  const uint32_t phase = osc.advance(inc);
  const float phaseU = (float)phase * (1.0f / 4294967296.0f);
  switch (source) {
    case SRC_SINE:
      return (float)lutSineLerp(phase) / 32767.0f;
    case SRC_SQUARE:
      return (phaseU < 0.5f) ? 1.0f : -1.0f;
    case SRC_SAW:
      return phaseU * 2.0f - 1.0f;
    case SRC_PULSE:
      return (phaseU < duty) ? 1.0f : -1.0f;
    default:
      return (float)lutSineLerp(phase) / 32767.0f;
  }
}

static void resetBurstRuntime(const SoundPreset& p, bool resetPhaser, bool resetArp) {
  const float retrigInterval = safeClampf(p.retrigger.interval_s, 0.0f, 2.0f, 0.0f);
  uint32_t retrigSamples = (uint32_t)lrintf(retrigInterval * (float)SR);
  if (retrigInterval > 0.0f && retrigSamples == 0u) retrigSamples = 1u;
  g_synth.retriggerSamples = retrigSamples;
  g_synth.retriggerCounter = 0;
  if (resetArp) {
    g_synth.arpApplied = false;
  }

  for (uint8_t i = 0; i < PRESET_OSC_COUNT; ++i) {
    g_synth.o[i].reset();
    g_synth.noiseHpLpState[i] = 0.0f;
    g_synth.noiseLpState[i] = 0.0f;
    const OscPreset& osc = p.osc[i];
    g_synth.curFreqHz[i] =
        safeClampf(osc.freq_start_hz, 10.0f, 18000.0f, 440.0f);
    g_synth.curSlideHzPerS[i] =
        safeClampf(osc.slide_hz_per_s, -12000.0f, 12000.0f, 0.0f);
    g_synth.curDuty[i] =
        safeClampf(osc.duty_start, 0.05f, 0.95f, 0.5f);
  }

  if (resetPhaser) {
    for (uint16_t i = 0; i < PHASER_BUF_SIZE; ++i) {
      g_synth.phaserBuf[i] = 0.0f;
    }
    g_synth.phaserWrite = 0;
  }
}

static inline float applyPhaser(float dry, float u, const PhaserPreset& p) {
  const float mix = safeClampf(p.mix, 0.0f, 1.0f, 0.0f);
  if (mix <= 0.0f) return dry;

  const float maxDelayMs =
      ((float)(PHASER_BUF_SIZE - 2) * 1000.0f) / (float)SR;
  const float dStart = safeClampf(p.delay_start_ms, 0.0f, maxDelayMs, 0.0f);
  const float dEnd = safeClampf(p.delay_end_ms, 0.0f, maxDelayMs, dStart);
  const float dMs = lerp(dStart, dEnd, clampf(u, 0.0f, 1.0f));
  const float dSamplesF = dMs * (float)SR * 0.001f;
  const uint16_t delaySamples =
      (uint16_t)clampf(dSamplesF, 0.0f, (float)(PHASER_BUF_SIZE - 2));
  const uint16_t readIdx =
      (uint16_t)((g_synth.phaserWrite - delaySamples) & (PHASER_BUF_SIZE - 1));

  const float wet = g_synth.phaserBuf[readIdx];
  g_synth.phaserBuf[g_synth.phaserWrite] = dry;
  g_synth.phaserWrite =
      (uint16_t)((g_synth.phaserWrite + 1u) & (PHASER_BUF_SIZE - 1u));

  return lerp(dry, wet, mix);
}

static void triggerSound(uint8_t id) {
  if (id >= PRESET_COUNT) return;
  const SoundPreset& p = kSoundPresets[id];
  g_synth.soundId   = id;
  g_synth.playing   = true;
  g_synth.repeatIndex = 0;
  g_synth.burstSamplePos = 0;
  g_synth.gapSamplesRemaining = 0;
  g_synth.lfoPhase = 0;
  g_synth.rng = 0xA5A5A5A5u ^ ((uint32_t)id * 0x9E3779B9u);
  resetBurstRuntime(p, true, true);
}

static float synthSample() {
  if (!g_synth.playing) return 0.0f;

  if (g_synth.soundId >= PRESET_COUNT) {
    g_synth.playing = false;
    return 0.0f;
  }

  const SoundPreset& p = kSoundPresets[g_synth.soundId];
  const float dur = safeClampf(p.duration_s, 0.005f, 8.0f, 0.2f);
  if (dur <= 0.0f) {
    g_synth.playing = false;
    return 0.0f;
  }

  const uint8_t repeatCount =
      (uint8_t)safeClampf((float)p.repeat.count, 1.0f, 8.0f, 1.0f);
  const float gap_s = safeClampf(p.repeat.gap_s, 0.0f, 1.0f, 0.0f);
  const float repeatDecay =
      safeClampf(p.repeat.gain_decay_per_repeat, 0.0f, 1.0f, 1.0f);
  const float repeatSemiStep =
      safeClampf(p.repeat.pitch_semitone_step, -24.0f, 24.0f, 0.0f);

  const float lfoRateHz = safeClampf(p.lfo.rate_hz, 0.0f, 30.0f, 0.0f);
  const float lfoPitchCentsDepth =
      safeClampf(p.lfo.pitch_cents_depth, 0.0f, 400.0f, 0.0f);
  const float lfoAmpDepth = safeClampf(p.lfo.amp_depth, 0.0f, 1.0f, 0.0f);

  float lfo = 0.0f;
  if (lfoRateHz > 0.0f && (lfoPitchCentsDepth > 0.0f || lfoAmpDepth > 0.0f)) {
    g_synth.lfoPhase += phaseIncFromHz(lfoRateHz);
    lfo = (float)lutSineLerp(g_synth.lfoPhase) / 32767.0f;
  }

  if (g_synth.gapSamplesRemaining > 0) {
    g_synth.gapSamplesRemaining--;
    return 0.0f;
  }

  const float t = (float)g_synth.burstSamplePos / (float)SR;
  if (t >= dur) {
    if ((g_synth.repeatIndex + 1u) < repeatCount) {
      g_synth.repeatIndex++;
      g_synth.burstSamplePos = 0;
      g_synth.gapSamplesRemaining = (uint32_t)lrintf(gap_s * (float)SR);
      resetBurstRuntime(p, true, true);
      return 0.0f;
    }
    g_synth.playing = false;
    return 0.0f;
  }

  if (g_synth.retriggerSamples > 0u) {
    if (g_synth.retriggerCounter >= g_synth.retriggerSamples) {
      resetBurstRuntime(p, false, false);
      g_synth.retriggerCounter = 0u;
    }
    g_synth.retriggerCounter++;
  }

  const float u = clampf(t / dur, 0.0f, 1.0f);
  const float attack = safeClampf(p.attack_s, 0.0f, dur, 0.0f);
  const float release = safeClampf(p.release_s, 0.0f, dur, 0.0f);
  const float loudStart = safeClampf(p.loudness_start, 0.0f, 1.2f, 1.0f);
  const float loudEnd = safeClampf(p.loudness_end, 0.0f, 1.2f, loudStart);

  const float arpJumpTime = safeClampf(p.arp.jump_time_s, 0.0f, dur, 0.0f);
  const float arpJumpSemi = safeClampf(p.arp.jump_semitones, -24.0f, 24.0f, 0.0f);
  if (!g_synth.arpApplied && arpJumpTime > 0.0f && t >= arpJumpTime) {
    g_synth.arpApplied = true;
  }

  const float ampShape = twoStageShape(u, p.shape.mid_u, p.shape.mid_gain);
  const float ampBase = envAR(t, dur, attack, release) *
                        lerp(loudStart, loudEnd, u) * ampShape;
  const float ampLfo = 1.0f - lfoAmpDepth + lfoAmpDepth * ((lfo + 1.0f) * 0.5f);
  const float repeatGain = powf(repeatDecay, (float)g_synth.repeatIndex);
  const float repeatSemi = repeatSemiStep * (float)g_synth.repeatIndex;
  const float pitchSemi =
      repeatSemi + (lfo * lfoPitchCentsDepth * 0.01f) + (g_synth.arpApplied ? arpJumpSemi : 0.0f);
  const float pitchRatio = semitoneRatio(pitchSemi);
  float mix = 0.0f;
  const float dt = 1.0f / (float)SR;

  for (uint8_t i = 0; i < PRESET_OSC_COUNT; ++i) {
    const OscPreset& o = p.osc[i];
    const float m =
        safeClampf(lerp(o.mix_start, o.mix_end, u), -2.0f, 2.0f, 0.0f);
    float s = 0.0f;

    if (o.source == SRC_NOISE) {
      const float hpHz = safeClampf(o.noise_hp_hz, 0.0f, 12000.0f, 0.0f);
      const float lpHz = safeClampf(o.noise_lp_hz, 0.0f, 20000.0f, 0.0f);
      float n = whiteNoise();

      float hpOut = n;
      if (hpHz > 0.0f) {
        const float aHp = onePoleAlpha(hpHz);
        g_synth.noiseHpLpState[i] += aHp * (n - g_synth.noiseHpLpState[i]);
        hpOut = n - g_synth.noiseHpLpState[i];
      }

      if (lpHz > 0.0f) {
        const float aLp = onePoleAlpha(lpHz);
        g_synth.noiseLpState[i] += aLp * (hpOut - g_synth.noiseLpState[i]);
        s = g_synth.noiseLpState[i];
      } else {
        s = hpOut;
      }
    } else {
      const float fStart = safeClampf(o.freq_start_hz, 10.0f, 18000.0f, 440.0f);
      const float fEnd = safeClampf(o.freq_end_hz, 10.0f, 18000.0f, fStart);
      const float dSlide = safeClampf(o.dslide_hz_per_s2, -18000.0f, 18000.0f, 0.0f);
      g_synth.curSlideHzPerS[i] += dSlide * dt;
      g_synth.curSlideHzPerS[i] =
          safeClampf(g_synth.curSlideHzPerS[i], -18000.0f, 18000.0f, 0.0f);
      g_synth.curFreqHz[i] += g_synth.curSlideHzPerS[i] * dt;
      g_synth.curDuty[i] =
          safeClampf(lerp(o.duty_start, o.duty_end, u), 0.05f, 0.95f, 0.5f);

      const float baseFreq = lerp(fStart, fEnd, u);
      const float slideOffset = g_synth.curFreqHz[i] - fStart;
      float f = (baseFreq + slideOffset) * pitchRatio;
      f = safeClampf(f, 10.0f, 18000.0f, fStart);
      const uint32_t inc = phaseIncFromHz(f);
      s = oscWaveSample(g_synth.o[i], inc, o.source, g_synth.curDuty[i]);
    }

    mix += m * s;
  }

  g_synth.burstSamplePos++;
  const float dry = ampBase * ampLfo * repeatGain * mix;
  return applyPhaser(dry, u, p.phaser);
}

// ---------------- Audio command channel (core0 -> core1) ----------------
enum AudioCmdType : uint8_t {
  CMD_NONE = 0,
  CMD_TRIGGER_SOUND = 1,
  CMD_STOP_SOUND = 2,
  CMD_STREAM_ENABLE = 3,
};

static inline uint32_t packAudioCmd(uint8_t cmd, uint8_t arg) {
  return ((uint32_t)cmd << 24) | (uint32_t)arg;
}

static inline uint8_t unpackAudioCmdType(uint32_t raw) {
  return (uint8_t)((raw >> 24) & 0xFFu);
}

static inline uint8_t unpackAudioCmdArg(uint32_t raw) {
  return (uint8_t)(raw & 0xFFu);
}

static bool sendAudioCmd(uint8_t cmd, uint8_t arg = 0) {
  const uint32_t packed = packAudioCmd(cmd, arg);
  for (int i = 0; i < 200; ++i) {
    if (rp2040.fifo.push_nb(packed)) {
      return true;
    }
    delayMicroseconds(50);
  }
  return false;
}

static void handleAudioCmd(uint32_t raw) {
  const uint8_t cmd = unpackAudioCmdType(raw);
  const uint8_t arg = unpackAudioCmdArg(raw);

  switch (cmd) {
    case CMD_TRIGGER_SOUND:
      triggerSound(arg);
      break;
    case CMD_STOP_SOUND:
      g_synth.playing = false;
      break;
    case CMD_STREAM_ENABLE:
      g_streamEnabled = (arg != 0);
      if (!g_streamEnabled) {
        g_synth.playing = false;
      }
      break;
    default:
      break;
  }
}

static void processAudioCmdQueue() {
  uint32_t raw = 0;
  while (rp2040.fifo.pop_nb(&raw)) {
    handleAudioCmd(raw);
  }
}

// ---------------- Button handling ----------------
struct ButtonState {
  uint8_t pin = 0;
  bool    last = true;
  uint32_t lastMs = 0;
};

constexpr uint32_t DEBOUNCE_MS = 25;
static ButtonState btnA{LCD_KEY_A}, btnB{LCD_KEY_B}, btnX{LCD_KEY_X}, btnY{LCD_KEY_Y};
static ButtonState btnUp{LCD_KEY_UP}, btnDown{LCD_KEY_DOWN};

static void initButtons() {
  // Pins and pull-ups are configured by LcdModuleInit().
  btnA.last = LcdGetKey(btnA.pin);
  btnB.last = LcdGetKey(btnB.pin);
  btnX.last = LcdGetKey(btnX.pin);
  btnY.last = LcdGetKey(btnY.pin);
  btnUp.last = LcdGetKey(btnUp.pin);
  btnDown.last = LcdGetKey(btnDown.pin);
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

// ---------------- UI rendering ----------------
static uint8_t g_lastUiWindowStart = 0xFF;
static int8_t g_lastUiActiveRow = -2;
static uint8_t g_lastUiBtConnected = 0xFF;

static int8_t currentActiveSlot() {
  if (!g_audioPlaying || g_audioSoundId >= PRESET_COUNT) return -1;
  for (uint8_t slot = 0; slot < SOUND_VIEW_SLOTS; ++slot) {
    if (slotToPreset(slot) == g_audioSoundId) return (int8_t)slot;
  }
  return -1;
}

static void renderSoundboardUi(bool force) {
  if (g_lcdFrameBuffer == nullptr) return;

  const int8_t activeSlot = currentActiveSlot();
  const uint8_t btConn = g_btConnected ? 1u : 0u;
  if (!force &&
      g_lastUiWindowStart == g_presetWindowStart &&
      g_lastUiActiveRow == activeSlot &&
      g_lastUiBtConnected == btConn) {
    return;
  }

  const int screenW = SCREEN_WIDTH_HEIGHT;
  const int screenH = SCREEN_WIDTH_HEIGHT;
  const int rightX = 84;
  const int rightW = screenW - rightX;
  const int rowH = screenH / SOUND_VIEW_SLOTS;
  static const char kSlotLabel[SOUND_VIEW_SLOTS] = {'A', 'B', 'X', 'Y'};

  UiClear(0x0000);
  UiFillRect(rightX, 0, rightW, screenH, 0x18C3);
  UiDrawText(6, 8, "SOUNDBOARD", 0xFFFF, 0x0000);

  char rangeText[24];
  const uint8_t rangeStart = (uint8_t)(g_presetWindowStart + 1);
  const uint8_t rangeEnd =
      (uint8_t)(((g_presetWindowStart + SOUND_VIEW_SLOTS - 1) % PRESET_COUNT) + 1);
  snprintf(rangeText, sizeof(rangeText), "%u-%u / %u",
           (unsigned)rangeStart, (unsigned)rangeEnd, (unsigned)PRESET_COUNT);
  UiDrawText(6, 22, rangeText, 0xFFE0, 0x0000);
  UiDrawText(6, 40, "UP/DN SCROLL", 0xC618, 0x0000);
  UiDrawText(6, 54, "A/B/X/Y PLAY", 0xC618, 0x0000);
  UiDrawText(6, 68, btConn ? "BT: CONNECTED" : "BT: DISCONNECTED",
             btConn ? 0x07E0 : 0xF800, 0x0000);

  if (g_audioPlaying && g_audioSoundId < PRESET_COUNT) {
    char nowText[32];
    snprintf(nowText, sizeof(nowText), "NOW: %s", kSoundPresets[g_audioSoundId].name);
    UiDrawTextClipped(6, 86, 13, nowText, 0xFFFF, 0x0000);
  }

  for (uint8_t row = 0; row < SOUND_VIEW_SLOTS; ++row) {
    const int y = (int)row * rowH;
    const uint8_t presetIdx = slotToPreset(row);
    const bool isActive = ((int8_t)row == activeSlot);

    const uint16_t rowBg = isActive ? 0x07E0 : 0x2104;
    const uint16_t rowFg = isActive ? 0x0000 : 0xFFFF;

    UiFillRect(rightX + 1, y + 1, rightW - 2, rowH - 2, rowBg);
    UiDrawRect(rightX, y, rightW, rowH, 0x4A69);

    char keyText[2] = {kSlotLabel[row], '\0'};
    char idxText[8];
    snprintf(idxText, sizeof(idxText), "#%u", (unsigned)(presetIdx + 1));

    UiDrawText(rightX + 6, y + 8, keyText, rowFg, rowBg);
    UiDrawText(rightX + 6, y + 22, idxText, rowFg, rowBg);
    UiDrawTextClipped(rightX + 36, y + 18, 18, kSoundPresets[presetIdx].name, rowFg, rowBg);
  }

  LcdWriteToScreen();
  g_lastUiWindowStart = g_presetWindowStart;
  g_lastUiActiveRow = activeSlot;
  g_lastUiBtConnected = btConn;
}

static void onBtConnect(void*, bool connected) {
  g_btConnected = connected;
  g_btStateChanged = true;
}

// ---------------- Arduino Setup (core0) ----------------
void setup() {
  Serial.begin(115200);

  // Build sine LUT
  for (uint32_t i = 0; i < LUT_LEN; ++i) {
    double th = (2.0 * M_PI * i) / (double)LUT_LEN;
    SINE_LUT[i] = (int16_t)lrint(32767.0 * sin(th));
  }

  lcdInit();
  initButtons();
  if (g_lcdFrameBuffer != nullptr) {
    UiInit(g_lcdFrameBuffer, SCREEN_WIDTH_HEIGHT, SCREEN_WIDTH_HEIGHT);
  }

  bt.onConnect(onBtConnect, nullptr);
  bt.begin();
  g_btReady = true;
  bt.connect(SPEAKER_ADDR);
  g_lastReconnectAttemptMs = millis();

  sendAudioCmd(CMD_STREAM_ENABLE, 1);
  renderSoundboardUi(true);
}

// ---------------- Core1 audio worker ----------------
void setup1() {
  // Audio worker core; initialization is handled in setup().
}

void loop1() {
  processAudioCmdQueue();

  if (!g_btReady) {
    delay(1);
    return;
  }

  if (!g_streamEnabled || !g_btConnected) {
    if (!g_btConnected) {
      g_synth.playing = false;
    }
    g_audioPlaying = false;
    g_audioSoundId = g_synth.soundId;
    delay(1);
    return;
  }

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

  size_t nbytes = sizeof(buf);
  const uint8_t* p = reinterpret_cast<const uint8_t*>(buf);
  while (nbytes && g_streamEnabled && g_btConnected) {
    int w = bt.write(p, nbytes);
    if (w > 0) {
      p += w;
      nbytes -= (size_t)w;
    } else {
      processAudioCmdQueue();
      delay(1);
    }
  }

  g_audioPlaying = g_synth.playing;
  g_audioSoundId = g_synth.soundId;
}

// ---------------- Arduino Loop (core0) ----------------
void loop() {
  bool uiDirty = false;

  if (g_btStateChanged) {
    g_btStateChanged = false;
    uiDirty = true;
  }

  if (g_btReady && !g_btConnected) {
    const uint32_t now = millis();
    if ((now - g_lastReconnectAttemptMs) >= BT_RECONNECT_MS) {
      g_lastReconnectAttemptMs = now;
      bt.connect(SPEAKER_ADDR);
      uiDirty = true;
    }
  }

  if (buttonPressed(btnUp)) {
    g_presetWindowStart = (uint8_t)((g_presetWindowStart + PRESET_COUNT - 1) % PRESET_COUNT);
    uiDirty = true;
  }
  if (buttonPressed(btnDown)) {
    g_presetWindowStart = (uint8_t)((g_presetWindowStart + 1) % PRESET_COUNT);
    uiDirty = true;
  }

  if (buttonPressed(btnA)) { sendAudioCmd(CMD_TRIGGER_SOUND, slotToPreset(0)); uiDirty = true; }
  if (buttonPressed(btnB)) { sendAudioCmd(CMD_TRIGGER_SOUND, slotToPreset(1)); uiDirty = true; }
  if (buttonPressed(btnX)) { sendAudioCmd(CMD_TRIGGER_SOUND, slotToPreset(2)); uiDirty = true; }
  if (buttonPressed(btnY)) { sendAudioCmd(CMD_TRIGGER_SOUND, slotToPreset(3)); uiDirty = true; }

  renderSoundboardUi(uiDirty);
  delay(1);
}
