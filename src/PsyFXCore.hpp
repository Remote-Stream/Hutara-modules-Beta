// =============================================================================
//  Hutara PsyFXCore  --  shared DSP core for PsyOperatorFX
//
//  Pure C++11, no Rack dependency.  This header holds the numeric guards,
//  structural constants, ring accessors, small value types and musical
//  mappings that the PsyOperatorFX module (and its test harness) build on.
//
//  Extracted from PsyOperatorFX.cpp as a behaviour-neutral refactor: every
//  name, value and rounding path below is identical to the in-file definitions
//  it replaces.  New genre blocks (SVF, comb, crusher, ...) land here too as
//  the redesign proceeds.
// =============================================================================

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <pmmintrin.h>
#include <xmmintrin.h>
#define PSYFX_X86_FPU 1
#endif

namespace psyfx {

// -----------------------------------------------------------------------------
//  Numeric helpers
// -----------------------------------------------------------------------------

static inline float clampF(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

static inline int clampI(int x, int lo, int hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

static inline float clamp01(float x) { return clampF(x, 0.f, 1.f); }

static inline float safeF(float x, float fallback = 0.f) {
    return std::isfinite(x) ? x : fallback;
}

static inline float smoothStep01(float t) {
    t = clamp01(t);
    return t * t * (3.f - 2.f * t);
}

static inline float max3(float a, float b, float c) {
    const float m = (a > b) ? a : b;
    return (m > c) ? m : c;
}

static inline float softLimit(float x) {
    if (x > 1.f)
        return 1.f + std::tanh(x - 1.f);
    if (x < -1.f)
        return -(1.f + std::tanh(-x - 1.f));
    return x;
}

static inline void enableFlushToZero() {
#ifdef PSYFX_X86_FPU
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
#ifdef _MM_SET_DENORMALS_ZERO_MODE
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif
#endif
}

// -----------------------------------------------------------------------------
//  Constants
// -----------------------------------------------------------------------------

static const int TAPE_SIZE = 1 << 18;
static const int TAPE_MASK = TAPE_SIZE - 1;
static const float TAPE_F = 262144.f;
static const int SCOPE_BUCKETS = 256;
static const int MAX_GRAINS = 8;

static const float MIN_LAG = 64.f;
static const float MAX_LAG = 0.5f * 262144.f;
static const float NOMINAL_LAG = 512.f;
static const float SLACK_ZONE = 2048.f;

static const int SEAM_XFADE = 96;
static const float SLICE_FADE_SEC = 0.003f;
static const float SLICE_SWAP_SEC = 0.0053f;
static const float DISP_XFADE_SEC = 0.016f;
static const float SCATTER_SLEW_SEC = 0.060f;

static const float MAX_RATE = 6.f;
static const float MAX_RATE_FULL = 7.f;
static const float TAPE_MOD_LO = 0.85f;
static const float TAPE_MOD_HI = 1.15f;

static const float READ_MARGIN = 4096.f;
static const float MAX_BACK = MAX_LAG - READ_MARGIN;
static const float GRAN_LAG_MAX = MAX_LAG;

static const float DISP_EVENT_SEC = 0.025f;
static const float MOD_MIN_SMOOTH = 0.008f;

static const float FREEZE_SECONDS = 1.5f;
static const float SR_MIN = 8000.f;
static const float SR_MAX = 384000.f;
static const float PI_F = 3.14159265358979f;
static const float TWO_PI_F = 6.28318530717959f;

static const float CHAOS_SPEED_DEPTH = 0.18f;
static const float CHAOS_GRAIN_DEPTH = 0.45f;
static const float CHAOS_PITCH_DEPTH = 0.22f;
static const float CHAOS_SCATTER_DEPTH = 0.35f;
static const float CHAOS_DRIVE_DEPTH = 0.20f;
static const float CHAOS_GATE_DEPTH = 0.30f;
static const float CHAOS_DELAY_DEPTH = 0.25f;

static const float EXT_MOD_SCALE = 0.1f;

static const int GATE_STEPS = 16;
static const uint16_t GATE_PATTERNS[8] = {
    0xFFFF, 0xAAAA, 0xCCCC, 0xF0F0, 0x8888, 0xEEEE, 0xF00F, 0x0F0F
};

static const float DELAY_MAX_SEC = 1.4f;
static const float DELAY_FB_MAX = 0.92f;
static const float DELAY_ENABLE_FADE = 0.0008f;

// ---- shared rhythm divisions -----------------------------------------------
// Hi-Tech upgrade: the original eight straight/dotted divisions (indices 0..7)
// are frozen -- their values, order and therefore the meaning of every saved
// STUTTER_RATE / GATE_RATE / DELAY_TIME value is preserved.  Indices 8..15
// append the triplet family plus the 1/128 pair, so widening the rate knob
// never moves an existing selection.
static const int   DIVISION_COUNT = 16;
static const float DIVISIONS[16] = {
    0.25f, 0.125f, 0.1875f, 0.0625f, 0.09375f, 0.03125f, 0.046875f, 0.015625f,
    0.1666667f, 0.0833333f, 0.0416667f, 0.0208333f, 0.0104167f, 0.0234375f,
    0.0078125f, 0.01171875f
};
static const char* DIVISION_NAMES[16] = {
    "1/4", "1/8", "1/8D", "1/16", "1/16D", "1/32", "1/32D", "1/64",
    "1/4T", "1/8T", "1/16T", "1/32T", "1/64T", "1/64D", "1/128", "1/128D"
};

// ---- clock resolution ------------------------------------------------------
static const float CLOCK_PULSES_PER_BEAT = 4.f;

// ---- hi-tech psytrance gate shapes -----------------------------------------
static const float GATE_SHAPE_SHORT = 0.25f;
static const float GATE_SHAPE_LONG  = 0.70f;
static const float GATE_SHAPE_HOLD  = 1.00f;
static const float GATE_ATTACK_FRAC  = 0.06f;
static const float GATE_RELEASE_FRAC = 0.12f;

// -----------------------------------------------------------------------------
//  Ring accessors
// -----------------------------------------------------------------------------

static inline float wrapTape(float pos) {
    if (!std::isfinite(pos))
        return 0.f;
    pos = std::fmod(pos, TAPE_F);
    if (pos < 0.f)
        pos += TAPE_F;
    if (pos >= TAPE_F)
        pos = 0.f;
    return pos;
}

static inline float readLinear(const float* buf, float pos) {
    pos = wrapTape(pos);
    const int i0 = static_cast<int>(pos);
    const int i1 = (i0 + 1) & TAPE_MASK;
    const float frac = pos - static_cast<float>(i0);
    return buf[i0] + (buf[i1] - buf[i0]) * frac;
}

static inline void readLinearStereo(const float* bufL, const float* bufR,
                                    float pos, float& outL, float& outR) {
    pos = wrapTape(pos);
    const int i0 = static_cast<int>(pos);
    const int i1 = (i0 + 1) & TAPE_MASK;
    const float frac = pos - static_cast<float>(i0);
    outL = bufL[i0] + (bufL[i1] - bufL[i0]) * frac;
    outR = bufR[i0] + (bufR[i1] - bufR[i0]) * frac;
}

static inline float readSeam(const float* buf, float pos, float cross, float d) {
    if (d >= static_cast<float>(SEAM_XFADE))
        return readLinear(buf, pos);
    const float t = clamp01(1.f - d / static_cast<float>(SEAM_XFADE));
    return readLinear(buf, pos) * std::sqrt(1.f - t) + readLinear(buf, cross) * std::sqrt(t);
}

static inline float slewRate(float current, float target, float sameCoeff, float flipCoeff,
                             float deadBand) {
    if (!std::isfinite(current))
        return target;
    if (target * current < 0.f) {
        float v = target + flipCoeff * (current - target);
        if (v * target < 0.f && v > -deadBand && v < deadBand)
            v = 0.f;
        return v;
    }
    return target + sameCoeff * (current - target);
}

// -----------------------------------------------------------------------------
//  One pole smoother
// -----------------------------------------------------------------------------

struct OnePole {
    float value;
    float coeff;
    OnePole() : value(0.f), coeff(0.f) {}

    void setTime(float seconds, float sr) {
        coeff = (seconds > 0.f && sr > 1.f) ? std::exp(-1.f / (seconds * sr)) : 0.f;
    }

    void set(float v) { value = safeF(v); }

    float process(float target) {
        target = safeF(target);
        value = target + coeff * (value - target);
        if (!std::isfinite(value))
            value = target;
        return value;
    }
};

// -----------------------------------------------------------------------------
//  Grain
// -----------------------------------------------------------------------------

struct Grain {
    bool active;
    float pos;
    float rate;
    float age;
    float len;
    float gain;
    float panL;
    float panR;

    Grain() { reset(); }

    void reset() {
        active = false;
        pos = 0.f;
        rate = 1.f;
        age = 0.f;
        len = 1.f;
        gain = 0.f;
        panL = 0.7071068f;
        panR = 0.7071068f;
    }
};

// -----------------------------------------------------------------------------
//  Modulation slot
// -----------------------------------------------------------------------------

struct ModSlot {
    float base;
    float cv;
    float mod;
    float ext;
    float value;
    float dsp;

    ModSlot() : base(0.f), cv(0.f), mod(0.f), ext(0.f), value(0.f), dsp(0.f) {}
};

// -----------------------------------------------------------------------------
//  Musical mappings
// -----------------------------------------------------------------------------

static inline float speedToRate(float speed) {
    if (speed > -0.002f && speed < 0.002f)
        return 1.f;
    if (speed >= 0.f)
        return std::pow(2.f, 2.f * speed);
    return -std::pow(2.f, 1.2f * (0.f - speed));
}

static inline float divisionFraction(int index) {
    return DIVISIONS[clampI(index, 0, DIVISION_COUNT - 1)];
}

static inline const char* divisionName(int index) {
    return DIVISION_NAMES[clampI(index, 0, DIVISION_COUNT - 1)];
}

static inline int repeatsFromKnob(float v) {
    v = clamp01(v);
    if (v > 0.985f)
        return -1;
    const int n = 1 + static_cast<int>(std::lround(v * 6.f));
    return clampI(n, 1, 7);
}

// -----------------------------------------------------------------------------
//  Phase 1: modulation matrix, LFO, envelope follower, genre macros
//
//  The module builds one "modulation bus" out of five sources (three bounded
//  random streams, one internal LFO, one audio envelope follower) and routes
//  them onto every CV-able target through a per-target weight matrix.  A genre
//  macro (DARK / HITECH / GLITCH / RISER) re-weights the matrix per target,
//  biases the chaotic rate and adds a small gate-shape offset, without ever
//  overriding the user's own knobs.  Everything is scaled by the modulation
//  amount knob (CHAOS_AMOUNT_PARAM) exactly like the legacy chaos bus.
// -----------------------------------------------------------------------------

static const int N_MOD_SOURCES = 5;
enum ModSource {
    SRC_CHAOS0 = 0,
    SRC_CHAOS1 = 1,
    SRC_CHAOS2 = 2,
    SRC_LFO    = 3,
    SRC_ENV    = 4
};

// Must match the module's ModTarget enum order
// (SPEED, GRAIN, PITCH, SCATTER, RATE, DRIVE, MIX, WOW, FLUTTER, GATE, DELAY);
// enforced by static_assert in the module constructor.
static const int MOD_TARGET_COUNT = 11;

static const int MACRO_COUNT = 4;
static const char* MACRO_NAMES[MACRO_COUNT] = {
    "DARK", "HITECH", "GLITCH", "RISER"
};

static const float MOD_MATRIX[MOD_TARGET_COUNT][N_MOD_SOURCES] = {
    { 1.0f, 0.2f, 0.2f, 0.8f, 0.9f },   // SPEED
    { 0.3f, 0.2f, 1.0f, 0.6f, 0.3f },   // GRAIN
    { 0.2f, 1.0f, 0.2f, 0.7f, 0.4f },   // PITCH
    { 1.0f, 0.3f, 0.2f, 1.0f, 0.5f },   // SCATTER
    { 0.3f, 0.8f, 0.2f, 0.4f, 0.3f },   // RATE    (reserved)
    { 0.3f, 1.0f, 0.2f, 0.6f, 0.6f },   // DRIVE
    { 0.2f, 0.2f, 0.2f, 0.2f, 0.5f },   // MIX
    { 0.2f, 0.4f, 0.2f, 0.9f, 0.3f },   // WOW
    { 0.2f, 0.3f, 0.8f, 0.5f, 0.3f },   // FLUTTER
    { 0.8f, 0.2f, 0.3f, 0.7f, 0.5f },   // GATE
    { 0.3f, 0.8f, 0.2f, 0.5f, 0.4f }    // DELAY
};

// Per-target modulation depth multiplier, one column per genre macro.
// Layout is [target][macro] so that a row is contiguous for macroLerp().
static const float MACRO_BIAS[MOD_TARGET_COUNT][MACRO_COUNT] = {
    { 1.30f, 1.00f, 0.85f, 1.20f },   // SPEED
    { 0.80f, 1.30f, 1.10f, 1.00f },   // GRAIN
    { 1.10f, 0.90f, 0.80f, 1.40f },   // PITCH
    { 1.25f, 1.20f, 1.60f, 0.90f },   // SCATTER
    { 0.90f, 1.50f, 1.30f, 0.80f },   // RATE
    { 1.20f, 0.90f, 1.40f, 1.00f },   // DRIVE
    { 0.70f, 1.10f, 1.20f, 1.30f },   // MIX
    { 1.35f, 1.00f, 0.80f, 1.10f },   // WOW
    { 1.15f, 1.30f, 1.00f, 0.90f },   // FLUTTER
    { 1.40f, 1.20f, 0.90f, 1.10f },   // GATE
    { 0.80f, 1.20f, 1.30f, 1.00f }    // DELAY
};

static const float MACRO_CHAOS_RATE_BIAS[MACRO_COUNT] = { 0.80f, 1.25f, 1.60f, 0.90f };
static const float MACRO_GATE_SHAPE_ADD[MACRO_COUNT]   = { 0.00f, 0.30f, -0.20f, 0.25f };

static inline float macroLerp(const float* table, float mv) {
    float v = safeF(mv);
    if (v < 0.f)
        v = 0.f;
    if (v > static_cast<float>(MACRO_COUNT - 1))
        v = static_cast<float>(MACRO_COUNT - 1);
    int j = static_cast<int>(v);
    if (j > MACRO_COUNT - 2)
        j = MACRO_COUNT - 2;
    const float f = v - static_cast<float>(j);
    return table[j] * (1.f - f) + table[j + 1] * f;
}

// -----------------------------------------------------------------------------
//  Internal modulation LFO (bipolar, triangle -> sine morph)
// -----------------------------------------------------------------------------

struct PsyLFO {
    float phase;
    float value;
    float shape;   // 0 = triangle ... 1 = sine

    PsyLFO() : phase(0.f), value(0.f), shape(0.5f) {}

    float tick(float dt, float hz) {
        float d = safeF(dt);
        float h = safeF(hz);
        if (h > 0.f)
            phase += d * h;
        if (phase >= 1.f)
            phase -= std::floor(phase);
        if (!(phase >= 0.f))
            phase = 0.f;
        const float tri = 4.f * std::fabs(phase - 0.5f) - 1.f;
        const float sine = std::sin(phase * TWO_PI_F);
        value = tri + shape * (sine - tri);
        return value;
    }

    void reset() { phase = 0.f; value = 0.f; }
};

// -----------------------------------------------------------------------------
//  Audio envelope follower (fast attack, musical release)
// -----------------------------------------------------------------------------

struct PsyEnvFollower {
    float level;
    float attCoeff;
    float relCoeff;

    PsyEnvFollower() : level(0.f), attCoeff(0.f), relCoeff(0.f) {}

    void setTime(float attackSec, float releaseSec, float sr) {
        attCoeff = (attackSec > 0.f && sr > 1.f) ? (1.f - std::exp(-1.f / (attackSec * sr))) : 0.f;
        relCoeff = (releaseSec > 0.f && sr > 1.f) ? (1.f - std::exp(-1.f / (releaseSec * sr))) : 0.f;
    }

    float tick(float x) {
        float v = safeF(x);
        const float c = (v > level) ? attCoeff : relCoeff;
        level += c * (v - level);
        if (!std::isfinite(level))
            level = v;
        return level;
    }

    void reset() { level = 0.f; }
};

} // namespace psyfx
