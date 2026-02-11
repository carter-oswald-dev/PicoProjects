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
#include <string.h>
#include "LcdDriver.h"
#include "SoundPresets.h"
#include "UiText.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct ButtonState;
struct Osc;
struct SynthRuntimeCache;

// ---------------- Bluetooth A2DP Source ----------------
A2DPSource bt;
static const uint8_t SPEAKER_ADDR[6] = {0xE8, 0xD0, 0x3C, 0x9B, 0x38, 0x39};

// ---------------- Audio format & block sizing ----------------
constexpr uint32_t SR       = 48000;   // A2DP-friendly
constexpr uint8_t  CHANNELS = 2;       // interleaved stereo (mono duplicated)
constexpr uint8_t  SOUND_VIEW_SLOTS = 4;
constexpr uint32_t BT_RECONNECT_MS = 3000;
constexpr size_t   AUDIO_RING_BYTES = 262144; // 256 KiB
constexpr size_t   AUDIO_RING_SAMPLES = AUDIO_RING_BYTES / sizeof(int16_t);
constexpr size_t   AUDIO_RING_CAP_FRAMES = AUDIO_RING_SAMPLES / CHANNELS;
constexpr size_t   AUDIO_RING_START_FRAMES = 2048;
constexpr size_t   AUDIO_RING_LOW_FRAMES = 1024;
constexpr size_t   AUDIO_RING_HIGH_FRAMES = 12288;
constexpr size_t   AUDIO_SYNTH_BATCH_FRAMES = 512;
constexpr size_t   AUDIO_TX_CHUNK_BYTES = 4096;
constexpr size_t   AUDIO_TX_CHUNK_SAMPLES = AUDIO_TX_CHUNK_BYTES / sizeof(int16_t);

static_assert(AUDIO_RING_SAMPLES % CHANNELS == 0, "Ring samples must align to stereo frames.");
static_assert(AUDIO_RING_START_FRAMES <= AUDIO_RING_CAP_FRAMES,
              "AUDIO_RING_START_FRAMES exceeds ring capacity.");
static_assert(AUDIO_RING_LOW_FRAMES <= AUDIO_RING_CAP_FRAMES,
              "AUDIO_RING_LOW_FRAMES exceeds ring capacity.");
static_assert(AUDIO_RING_HIGH_FRAMES <= AUDIO_RING_CAP_FRAMES,
              "AUDIO_RING_HIGH_FRAMES exceeds ring capacity.");
static_assert(AUDIO_RING_LOW_FRAMES <= AUDIO_RING_HIGH_FRAMES,
              "Low fill watermark must be <= high fill watermark.");
static_assert(AUDIO_RING_START_FRAMES <= AUDIO_RING_HIGH_FRAMES,
              "Start fill watermark must be <= high fill watermark.");
static_assert(AUDIO_TX_CHUNK_SAMPLES % CHANNELS == 0, "Tx chunk must align to stereo frames.");

static uint8_t  g_presetWindowStart = 0;
static uint32_t g_lastReconnectAttemptMs = 0;

static volatile uint32_t g_audioUiState = 0;
static volatile bool g_streamEnabled = true;
static volatile bool g_btConnected = false;
static volatile bool g_btReady = false;
static volatile bool g_btStateChanged = false;

constexpr uint32_t UI_PENDING_SOUND_MS = 250;
static bool g_pendingUiActive = false;
static uint8_t g_pendingUiSoundId = 0;
static uint32_t g_pendingUiMs = 0;
static uint16_t g_lastUiObservedSeq = 0;

// ---------------- Core1 audio ring buffer ----------------
static int16_t g_audioRing[AUDIO_RING_SAMPLES];
static uint32_t g_ringRead = 0;  // sample index
static uint32_t g_ringWrite = 0; // sample index
static uint32_t g_ringCount = 0; // sample count
static bool g_ringStartPrimeActive = false;

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

struct OscRuntimeCache {
  uint8_t source = SRC_SINE;
  float mixStart = 0.0f;
  float mixEnd = 0.0f;
  float freqStart = 440.0f;
  float freqEnd = 440.0f;
  float slideStart = 0.0f;
  float dutyStart = 0.5f;
  float dutyEnd = 0.5f;
  float dSlide = 0.0f;
  float hpAlpha = 0.0f;
  float lpAlpha = 0.0f;
  bool hasHp = false;
  bool hasLp = false;
};

struct SynthRuntimeCache {
  float dur = 0.2f;
  float attack = 0.0f;
  float release = 0.0f;
  float loudStart = 1.0f;
  float loudEnd = 1.0f;
  float shapeMidU = 0.5f;
  float shapeMidGain = 1.0f;
  uint8_t repeatCount = 1;
  uint32_t gapSamples = 0;
  float repeatDecay = 1.0f;
  float repeatSemiStep = 0.0f;
  float repeatGain = 1.0f;
  float repeatSemi = 0.0f;
  float lfoPitchCentsDepth = 0.0f;
  float lfoAmpDepth = 0.0f;
  uint32_t lfoPhaseInc = 0;
  bool lfoEnabled = false;
  float arpJumpTime = 0.0f;
  float arpJumpSemi = 0.0f;
  uint32_t retriggerSamples = 0;
  float phaserMix = 0.0f;
  float phaserDelayStartMs = 0.0f;
  float phaserDelayEndMs = 0.0f;
  OscRuntimeCache osc[PRESET_OSC_COUNT];
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
  SynthRuntimeCache c;
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
static inline uint8_t presetPageCount() {
  return (uint8_t)((PRESET_COUNT + SOUND_VIEW_SLOTS - 1u) / SOUND_VIEW_SLOTS);
}
static inline int16_t slotToPreset(uint8_t slot) {
  const uint16_t idx = (uint16_t)g_presetWindowStart + (uint16_t)slot;
  return (idx < PRESET_COUNT) ? (int16_t)idx : -1;
}
static inline void stepPresetWindow(bool forward) {
  const uint8_t pageCount = presetPageCount();
  if (pageCount == 0u) {
    g_presetWindowStart = 0u;
    return;
  }
  uint8_t page = (uint8_t)(g_presetWindowStart / SOUND_VIEW_SLOTS);
  if (forward) {
    page = (uint8_t)((page + 1u) % pageCount);
  } else {
    page = (uint8_t)((page + pageCount - 1u) % pageCount);
  }
  g_presetWindowStart = (uint8_t)(page * SOUND_VIEW_SLOTS);
}
static inline float onePoleAlpha(float fc) {
  return clampf((2.0f * (float)M_PI * fc) / (float)SR, 0.0f, 1.0f);
}
static inline float semitoneRatio(float semitones) {
  return exp2f(semitones / 12.0f);
}

static inline uint32_t ringFramesAvailable() {
  return g_ringCount / CHANNELS;
}

static inline uint32_t ringFramesFree() {
  return (uint32_t)AUDIO_RING_CAP_FRAMES - ringFramesAvailable();
}

static inline void ringClear() {
  g_ringRead = 0;
  g_ringWrite = 0;
  g_ringCount = 0;
}

static inline bool ringPushStereoFrame(int16_t left, int16_t right) {
  if ((g_ringCount + CHANNELS) > AUDIO_RING_SAMPLES) return false;
  g_audioRing[g_ringWrite] = left;
  g_ringWrite = (g_ringWrite + 1u) % AUDIO_RING_SAMPLES;
  g_audioRing[g_ringWrite] = right;
  g_ringWrite = (g_ringWrite + 1u) % AUDIO_RING_SAMPLES;
  g_ringCount += CHANNELS;
  return true;
}

static size_t ringPopSamples(int16_t* out, size_t maxSamples) {
  if (maxSamples == 0 || g_ringCount == 0) return 0;
  size_t samples = maxSamples;
  if (samples > g_ringCount) samples = g_ringCount;
  size_t first = samples;
  const size_t toEnd = AUDIO_RING_SAMPLES - g_ringRead;
  if (first > toEnd) first = toEnd;

  memcpy(out, &g_audioRing[g_ringRead], first * sizeof(int16_t));
  if (samples > first) {
    memcpy(out + first, &g_audioRing[0], (samples - first) * sizeof(int16_t));
  }

  g_ringRead = (g_ringRead + (uint32_t)samples) % AUDIO_RING_SAMPLES;
  g_ringCount -= (uint32_t)samples;
  return samples;
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

static inline float twoStageShapeClamped(float u, float mid_u, float mid_gain) {
  const float x = clampf(u, 0.0f, 1.0f);
  if (x < mid_u) return lerp(1.0f, mid_gain, x / mid_u);
  return lerp(mid_gain, 0.0f, (x - mid_u) / (1.0f - mid_u));
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

static void buildRuntimeCache(const SoundPreset& p) {
  SynthRuntimeCache& c = g_synth.c;
  c.dur = safeClampf(p.duration_s, 0.005f, 8.0f, 0.2f);
  c.attack = safeClampf(p.attack_s, 0.0f, c.dur, 0.0f);
  c.release = safeClampf(p.release_s, 0.0f, c.dur, 0.0f);
  c.loudStart = safeClampf(p.loudness_start, 0.0f, 1.2f, 1.0f);
  c.loudEnd = safeClampf(p.loudness_end, 0.0f, 1.2f, c.loudStart);
  c.shapeMidU = safeClampf(p.shape.mid_u, 0.01f, 0.99f, 0.5f);
  c.shapeMidGain = safeClampf(p.shape.mid_gain, 0.0f, 1.0f, 1.0f);
  c.repeatCount = (uint8_t)safeClampf((float)p.repeat.count, 1.0f, 8.0f, 1.0f);
  const float gapS = safeClampf(p.repeat.gap_s, 0.0f, 1.0f, 0.0f);
  c.gapSamples = (uint32_t)lrintf(gapS * (float)SR);
  c.repeatDecay = safeClampf(p.repeat.gain_decay_per_repeat, 0.0f, 1.0f, 1.0f);
  c.repeatSemiStep = safeClampf(p.repeat.pitch_semitone_step, -24.0f, 24.0f, 0.0f);
  c.repeatGain = 1.0f;
  c.repeatSemi = 0.0f;
  c.lfoPitchCentsDepth = safeClampf(p.lfo.pitch_cents_depth, 0.0f, 400.0f, 0.0f);
  c.lfoAmpDepth = safeClampf(p.lfo.amp_depth, 0.0f, 1.0f, 0.0f);
  const float lfoRateHz = safeClampf(p.lfo.rate_hz, 0.0f, 30.0f, 0.0f);
  c.lfoEnabled = (lfoRateHz > 0.0f) &&
                 (c.lfoPitchCentsDepth > 0.0f || c.lfoAmpDepth > 0.0f);
  c.lfoPhaseInc = c.lfoEnabled ? phaseIncFromHz(lfoRateHz) : 0u;
  c.arpJumpTime = safeClampf(p.arp.jump_time_s, 0.0f, c.dur, 0.0f);
  c.arpJumpSemi = safeClampf(p.arp.jump_semitones, -24.0f, 24.0f, 0.0f);

  const float retrigInterval = safeClampf(p.retrigger.interval_s, 0.0f, 2.0f, 0.0f);
  c.retriggerSamples = (uint32_t)lrintf(retrigInterval * (float)SR);
  if (retrigInterval > 0.0f && c.retriggerSamples == 0u) c.retriggerSamples = 1u;

  const float maxDelayMs =
      ((float)(PHASER_BUF_SIZE - 2) * 1000.0f) / (float)SR;
  c.phaserMix = safeClampf(p.phaser.mix, 0.0f, 1.0f, 0.0f);
  c.phaserDelayStartMs = safeClampf(p.phaser.delay_start_ms, 0.0f, maxDelayMs, 0.0f);
  c.phaserDelayEndMs = safeClampf(p.phaser.delay_end_ms, 0.0f, maxDelayMs, c.phaserDelayStartMs);

  for (uint8_t i = 0; i < PRESET_OSC_COUNT; ++i) {
    const OscPreset& o = p.osc[i];
    OscRuntimeCache& oc = c.osc[i];
    oc.source = o.source;
    oc.mixStart = safeClampf(o.mix_start, -2.0f, 2.0f, 0.0f);
    oc.mixEnd = safeClampf(o.mix_end, -2.0f, 2.0f, 0.0f);
    oc.freqStart = safeClampf(o.freq_start_hz, 10.0f, 18000.0f, 440.0f);
    oc.freqEnd = safeClampf(o.freq_end_hz, 10.0f, 18000.0f, oc.freqStart);
    oc.slideStart = safeClampf(o.slide_hz_per_s, -12000.0f, 12000.0f, 0.0f);
    oc.dSlide = safeClampf(o.dslide_hz_per_s2, -18000.0f, 18000.0f, 0.0f);
    oc.dutyStart = safeClampf(o.duty_start, 0.05f, 0.95f, 0.5f);
    oc.dutyEnd = safeClampf(o.duty_end, 0.05f, 0.95f, 0.5f);
    const float hpHz = safeClampf(o.noise_hp_hz, 0.0f, 12000.0f, 0.0f);
    const float lpHz = safeClampf(o.noise_lp_hz, 0.0f, 20000.0f, 0.0f);
    oc.hasHp = hpHz > 0.0f;
    oc.hasLp = lpHz > 0.0f;
    oc.hpAlpha = oc.hasHp ? onePoleAlpha(hpHz) : 0.0f;
    oc.lpAlpha = oc.hasLp ? onePoleAlpha(lpHz) : 0.0f;
  }
}

static inline void updateRepeatDerivedCache() {
  g_synth.c.repeatGain = powf(g_synth.c.repeatDecay, (float)g_synth.repeatIndex);
  g_synth.c.repeatSemi = g_synth.c.repeatSemiStep * (float)g_synth.repeatIndex;
}

static void resetBurstRuntime(bool resetPhaser, bool resetArp) {
  const SynthRuntimeCache& c = g_synth.c;
  g_synth.retriggerSamples = c.retriggerSamples;
  g_synth.retriggerCounter = 0;
  if (resetArp) {
    g_synth.arpApplied = false;
  }

  for (uint8_t i = 0; i < PRESET_OSC_COUNT; ++i) {
    g_synth.o[i].reset();
    g_synth.noiseHpLpState[i] = 0.0f;
    g_synth.noiseLpState[i] = 0.0f;
    g_synth.curFreqHz[i] = c.osc[i].freqStart;
    g_synth.curSlideHzPerS[i] = c.osc[i].slideStart;
    g_synth.curDuty[i] = c.osc[i].dutyStart;
  }

  if (resetPhaser) {
    for (uint16_t i = 0; i < PHASER_BUF_SIZE; ++i) {
      g_synth.phaserBuf[i] = 0.0f;
    }
    g_synth.phaserWrite = 0;
  }
}

static inline float applyPhaser(float dry, float u, const SynthRuntimeCache& c) {
  if (c.phaserMix <= 0.0f) return dry;

  const float dMs = lerp(c.phaserDelayStartMs, c.phaserDelayEndMs, clampf(u, 0.0f, 1.0f));
  const float dSamplesF = dMs * (float)SR * 0.001f;
  const uint16_t delaySamples =
      (uint16_t)clampf(dSamplesF, 0.0f, (float)(PHASER_BUF_SIZE - 2));
  const uint16_t readIdx =
      (uint16_t)((g_synth.phaserWrite - delaySamples) & (PHASER_BUF_SIZE - 1));

  const float wet = g_synth.phaserBuf[readIdx];
  g_synth.phaserBuf[g_synth.phaserWrite] = dry;
  g_synth.phaserWrite =
      (uint16_t)((g_synth.phaserWrite + 1u) & (PHASER_BUF_SIZE - 1u));

  return lerp(dry, wet, c.phaserMix);
}

static void triggerSound(uint8_t id) {
  if (id >= PRESET_COUNT) return;
  const SoundPreset& p = kSoundPresets[id];
  buildRuntimeCache(p);
  g_synth.soundId = id;
  g_synth.playing = true;
  g_synth.repeatIndex = 0;
  g_synth.burstSamplePos = 0;
  g_synth.gapSamplesRemaining = 0;
  g_synth.lfoPhase = 0;
  g_synth.rng = 0xA5A5A5A5u ^ ((uint32_t)id * 0x9E3779B9u);
  updateRepeatDerivedCache();
  resetBurstRuntime(true, true);
}

static float synthSample() {
  if (!g_synth.playing) return 0.0f;

  if (g_synth.soundId >= PRESET_COUNT) {
    g_synth.playing = false;
    return 0.0f;
  }

  const SynthRuntimeCache& c = g_synth.c;
  if (c.dur <= 0.0f) {
    g_synth.playing = false;
    return 0.0f;
  }

  float lfo = 0.0f;
  if (c.lfoEnabled) {
    g_synth.lfoPhase += c.lfoPhaseInc;
    lfo = (float)lutSineLerp(g_synth.lfoPhase) / 32767.0f;
  }

  if (g_synth.gapSamplesRemaining > 0u) {
    g_synth.gapSamplesRemaining--;
    return 0.0f;
  }

  const float t = (float)g_synth.burstSamplePos / (float)SR;
  if (t >= c.dur) {
    if ((g_synth.repeatIndex + 1u) < c.repeatCount) {
      g_synth.repeatIndex++;
      g_synth.burstSamplePos = 0;
      g_synth.gapSamplesRemaining = c.gapSamples;
      updateRepeatDerivedCache();
      resetBurstRuntime(true, true);
      return 0.0f;
    }
    g_synth.playing = false;
    return 0.0f;
  }

  if (g_synth.retriggerSamples > 0u) {
    if (g_synth.retriggerCounter >= g_synth.retriggerSamples) {
      resetBurstRuntime(false, false);
      g_synth.retriggerCounter = 0u;
    }
    g_synth.retriggerCounter++;
  }

  const float u = clampf(t / c.dur, 0.0f, 1.0f);
  if (!g_synth.arpApplied && c.arpJumpTime > 0.0f && t >= c.arpJumpTime) {
    g_synth.arpApplied = true;
  }

  const float ampShape = twoStageShapeClamped(u, c.shapeMidU, c.shapeMidGain);
  const float ampBase = envAR(t, c.dur, c.attack, c.release) *
                        lerp(c.loudStart, c.loudEnd, u) * ampShape;
  const float ampLfo = 1.0f - c.lfoAmpDepth + c.lfoAmpDepth * ((lfo + 1.0f) * 0.5f);
  const float pitchSemi =
      c.repeatSemi + (lfo * c.lfoPitchCentsDepth * 0.01f) + (g_synth.arpApplied ? c.arpJumpSemi : 0.0f);
  const float pitchRatio = semitoneRatio(pitchSemi);
  float mix = 0.0f;
  constexpr float dt = 1.0f / (float)SR;

  for (uint8_t i = 0; i < PRESET_OSC_COUNT; ++i) {
    const OscRuntimeCache& o = c.osc[i];
    const float m = lerp(o.mixStart, o.mixEnd, u);
    float s = 0.0f;

    if (o.source == SRC_NOISE) {
      const float n = whiteNoise();
      float hpOut = n;
      if (o.hasHp) {
        g_synth.noiseHpLpState[i] += o.hpAlpha * (n - g_synth.noiseHpLpState[i]);
        hpOut = n - g_synth.noiseHpLpState[i];
      }

      if (o.hasLp) {
        g_synth.noiseLpState[i] += o.lpAlpha * (hpOut - g_synth.noiseLpState[i]);
        s = g_synth.noiseLpState[i];
      } else {
        s = hpOut;
      }
    } else {
      g_synth.curSlideHzPerS[i] += o.dSlide * dt;
      g_synth.curSlideHzPerS[i] =
          safeClampf(g_synth.curSlideHzPerS[i], -18000.0f, 18000.0f, 0.0f);
      g_synth.curFreqHz[i] += g_synth.curSlideHzPerS[i] * dt;
      g_synth.curDuty[i] = lerp(o.dutyStart, o.dutyEnd, u);

      const float baseFreq = lerp(o.freqStart, o.freqEnd, u);
      const float slideOffset = g_synth.curFreqHz[i] - o.freqStart;
      float f = (baseFreq + slideOffset) * pitchRatio;
      f = clampf(f, 10.0f, 18000.0f);
      const uint32_t inc = phaseIncFromHz(f);
      s = oscWaveSample(g_synth.o[i], inc, o.source, g_synth.curDuty[i]);
    }

    mix += m * s;
  }

  g_synth.burstSamplePos++;
  const float dry = ampBase * ampLfo * c.repeatGain * mix;
  return applyPhaser(dry, u, c);
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

static inline uint32_t packAudioUiState(bool playing, uint8_t soundId, uint16_t seq) {
  return ((uint32_t)seq << 16) |
         ((uint32_t)soundId << 8) |
         (playing ? 1u : 0u);
}

static inline void unpackAudioUiState(uint32_t raw, bool& playing, uint8_t& soundId, uint16_t& seq) {
  playing = (raw & 0x1u) != 0u;
  soundId = (uint8_t)((raw >> 8) & 0xFFu);
  seq = (uint16_t)((raw >> 16) & 0xFFFFu);
}

static uint16_t g_audioUiSeq = 0;
static bool g_audioUiLastPlaying = false;
static uint8_t g_audioUiLastSoundId = 0;

static inline void publishAudioUiState(bool playing, uint8_t soundId, bool force = false) {
  if (!force && playing == g_audioUiLastPlaying && soundId == g_audioUiLastSoundId) {
    return;
  }
  g_audioUiLastPlaying = playing;
  g_audioUiLastSoundId = soundId;
  g_audioUiSeq++;
  g_audioUiState = packAudioUiState(playing, soundId, g_audioUiSeq);
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
      ringClear();
      g_ringStartPrimeActive = true;
      publishAudioUiState(true, g_synth.soundId);
      break;
    case CMD_STOP_SOUND:
      g_synth.playing = false;
      ringClear();
      g_ringStartPrimeActive = false;
      publishAudioUiState(false, g_synth.soundId);
      break;
    case CMD_STREAM_ENABLE:
      g_streamEnabled = (arg != 0);
      if (!g_streamEnabled) {
        g_synth.playing = false;
        ringClear();
        g_ringStartPrimeActive = false;
        publishAudioUiState(false, g_synth.soundId);
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

static inline void clearPendingUiSound() {
  g_pendingUiActive = false;
}

static bool resolveUiActiveSound(uint8_t& soundIdOut) {
  bool audioPlaying = false;
  uint8_t audioSoundId = 0;
  uint16_t seq = 0;
  unpackAudioUiState(g_audioUiState, audioPlaying, audioSoundId, seq);

  const uint32_t now = millis();
  if (seq != g_lastUiObservedSeq) {
    g_lastUiObservedSeq = seq;
    if (audioPlaying && g_pendingUiActive && audioSoundId == g_pendingUiSoundId) {
      clearPendingUiSound();
    }
  }

  if (audioPlaying && audioSoundId < PRESET_COUNT) {
    soundIdOut = audioSoundId;
    return true;
  }

  if (g_pendingUiActive) {
    const uint32_t elapsed = now - g_pendingUiMs;
    if (elapsed <= UI_PENDING_SOUND_MS && g_pendingUiSoundId < PRESET_COUNT) {
      soundIdOut = g_pendingUiSoundId;
      return true;
    }
    clearPendingUiSound();
  }

  return false;
}

static int8_t currentActiveSlot(uint8_t* activeSoundId = nullptr) {
  uint8_t soundId = 0;
  if (!resolveUiActiveSound(soundId)) {
    if (activeSoundId) *activeSoundId = 0xFF;
    return -1;
  }
  if (activeSoundId) *activeSoundId = soundId;
  for (uint8_t slot = 0; slot < SOUND_VIEW_SLOTS; ++slot) {
    const int16_t presetIdx = slotToPreset(slot);
    if (presetIdx >= 0 && (uint8_t)presetIdx == soundId) return (int8_t)slot;
  }
  return -1;
}

static void renderSoundboardUi(bool force) {
  if (g_lcdFrameBuffer == nullptr) return;

  uint8_t activeSoundId = 0xFF;
  const int8_t activeSlot = currentActiveSlot(&activeSoundId);
  const bool hasActiveSound = (activeSoundId < PRESET_COUNT);
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
  const uint8_t rangeEnd = (uint8_t)min((uint16_t)PRESET_COUNT,
                                        (uint16_t)g_presetWindowStart + (uint16_t)SOUND_VIEW_SLOTS);
  snprintf(rangeText, sizeof(rangeText), "%u-%u / %u",
           (unsigned)rangeStart, (unsigned)rangeEnd, (unsigned)PRESET_COUNT);
  UiDrawText(6, 22, rangeText, 0xFFE0, 0x0000);
  UiDrawText(6, 40, "UP/DN SCROLL", 0xC618, 0x0000);
  UiDrawText(6, 54, "A/B/X/Y PLAY", 0xC618, 0x0000);
  UiDrawText(6, 68, btConn ? "BT: CONNECTED" : "BT: DISCONNECTED",
             btConn ? 0x07E0 : 0xF800, 0x0000);

  if (hasActiveSound) {
    char nowText[32];
    snprintf(nowText, sizeof(nowText), "NOW: %s", kSoundPresets[activeSoundId].name);
    UiDrawTextClipped(6, 86, 13, nowText, 0xFFFF, 0x0000);
  }

  for (uint8_t row = 0; row < SOUND_VIEW_SLOTS; ++row) {
    const int y = (int)row * rowH;
    const int16_t presetIdx = slotToPreset(row);
    const bool isActive = ((int8_t)row == activeSlot);

    const uint16_t rowBg = isActive ? 0x07E0 : 0x2104;
    const uint16_t rowFg = isActive ? 0x0000 : 0xFFFF;

    UiFillRect(rightX + 1, y + 1, rightW - 2, rowH - 2, rowBg);
    UiDrawRect(rightX, y, rightW, rowH, 0x4A69);

    char keyText[2] = {kSlotLabel[row], '\0'};
    char idxText[8];
    if (presetIdx >= 0) {
      snprintf(idxText, sizeof(idxText), "#%u", (unsigned)((uint8_t)presetIdx + 1u));
    } else {
      snprintf(idxText, sizeof(idxText), "--");
    }

    UiDrawText(rightX + 6, y + 8, keyText, rowFg, rowBg);
    UiDrawText(rightX + 6, y + 22, idxText, rowFg, rowBg);
    if (presetIdx >= 0) {
      UiDrawTextClipped(rightX + 36, y + 18, 18, kSoundPresets[(uint8_t)presetIdx].name, rowFg, rowBg);
    }
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

static inline int16_t synthToPcm16(float y) {
  // Keep output scaling identical to prior firmware behavior.
  constexpr float MASTER_GAIN = 0.80f;
  y *= MASTER_GAIN;
  if (y > 0.999f) y = 0.999f;
  if (y < -0.999f) y = -0.999f;
  return (int16_t)lrintf(y * 32767.0f);
}

static void produceAudioBatch(uint32_t maxFrames) {
  uint32_t frameCount = maxFrames;
  if (frameCount > AUDIO_SYNTH_BATCH_FRAMES) frameCount = AUDIO_SYNTH_BATCH_FRAMES;
  const uint32_t freeFrames = ringFramesFree();
  if (frameCount > freeFrames) frameCount = freeFrames;
  for (uint32_t i = 0; i < frameCount; ++i) {
    if (ringFramesFree() == 0u) break;
    const int16_t s = synthToPcm16(synthSample());
    if (!ringPushStereoFrame(s, s)) break;
  }
}

static void writeRingChunkToBt() {
  static int16_t txChunk[AUDIO_TX_CHUNK_SAMPLES];
  size_t availableSamples = g_ringCount;
  if (availableSamples < CHANNELS) return;

  size_t chunkSamples = availableSamples;
  if (chunkSamples > AUDIO_TX_CHUNK_SAMPLES) chunkSamples = AUDIO_TX_CHUNK_SAMPLES;
  chunkSamples -= (chunkSamples % CHANNELS);
  if (chunkSamples == 0) return;

  const size_t gotSamples = ringPopSamples(txChunk, chunkSamples);
  if (gotSamples == 0) return;

  size_t nbytes = gotSamples * sizeof(int16_t);
  const uint8_t* p = reinterpret_cast<const uint8_t*>(txChunk);
  while (nbytes > 0 && g_streamEnabled && g_btConnected) {
    const int written = bt.write(p, nbytes);
    if (written > 0) {
      p += (size_t)written;
      nbytes -= (size_t)written;
    } else {
      processAudioCmdQueue();
      delay(1);
    }
  }
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
    ringClear();
    g_ringStartPrimeActive = false;
    publishAudioUiState(false, g_synth.soundId);
    delay(1);
    return;
  }

  writeRingChunkToBt();
  processAudioCmdQueue();

  const uint32_t framesAvail = ringFramesAvailable();
  if (g_ringStartPrimeActive) {
    if (framesAvail < AUDIO_RING_START_FRAMES) {
      produceAudioBatch(AUDIO_RING_START_FRAMES - framesAvail);
    } else {
      g_ringStartPrimeActive = false;
    }
  } else if (framesAvail < AUDIO_RING_LOW_FRAMES) {
    produceAudioBatch(AUDIO_RING_LOW_FRAMES - framesAvail);
  } else if (framesAvail < AUDIO_RING_HIGH_FRAMES) {
    produceAudioBatch(AUDIO_RING_HIGH_FRAMES - framesAvail);
  }

  publishAudioUiState(g_synth.playing, g_synth.soundId);
}

// ---------------- Arduino Loop (core0) ----------------
void loop() {
  bool uiDirty = false;

  if (g_btStateChanged) {
    g_btStateChanged = false;
    if (!g_btConnected) {
      clearPendingUiSound();
    }
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
    stepPresetWindow(false);
    uiDirty = true;
  }
  if (buttonPressed(btnDown)) {
    stepPresetWindow(true);
    uiDirty = true;
  }

  if (buttonPressed(btnA)) {
    const int16_t idx = slotToPreset(0);
    if (idx >= 0 && sendAudioCmd(CMD_TRIGGER_SOUND, (uint8_t)idx)) {
      g_pendingUiActive = true;
      g_pendingUiSoundId = (uint8_t)idx;
      g_pendingUiMs = millis();
      uiDirty = true;
    }
  }
  if (buttonPressed(btnB)) {
    const int16_t idx = slotToPreset(1);
    if (idx >= 0 && sendAudioCmd(CMD_TRIGGER_SOUND, (uint8_t)idx)) {
      g_pendingUiActive = true;
      g_pendingUiSoundId = (uint8_t)idx;
      g_pendingUiMs = millis();
      uiDirty = true;
    }
  }
  if (buttonPressed(btnX)) {
    const int16_t idx = slotToPreset(2);
    if (idx >= 0 && sendAudioCmd(CMD_TRIGGER_SOUND, (uint8_t)idx)) {
      g_pendingUiActive = true;
      g_pendingUiSoundId = (uint8_t)idx;
      g_pendingUiMs = millis();
      uiDirty = true;
    }
  }
  if (buttonPressed(btnY)) {
    const int16_t idx = slotToPreset(3);
    if (idx >= 0 && sendAudioCmd(CMD_TRIGGER_SOUND, (uint8_t)idx)) {
      g_pendingUiActive = true;
      g_pendingUiSoundId = (uint8_t)idx;
      g_pendingUiMs = millis();
      uiDirty = true;
    }
  }

  renderSoundboardUi(uiDirty);
  delay(1);
}
