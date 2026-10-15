// =============================================================================
//  Hutara VOIDKICK (model slug "PsyKick" for backward compat)
//  -- dedicated darkpsy / psyforest / hi-tech kick + bass voice.
//
//  VCV Rack 2 module, C++11 only, no external dependencies.
//  Original Hutara design. NOT a kickbaba clone.
//
//  ---------------------------------------------------------------------------
//  Real-time contract
//    * process() performs no allocation, no locks, no logging, no file access.
//    * Every oscillator, envelope, filter and delay line is a fixed size member.
//    * Every state update goes through safeF(), so a NaN can never take over.
//    * One single parameter stage (knob + CV -> clamp -> smooth) feeds the DSP.
//    * All time constants come from args.sampleRate.
//  ---------------------------------------------------------------------------
//  Pattern (v8 — correct Psytrance / Darkpsy / Hi-Tech groove):
//    * Downbeat (slot 0): KICK only. Bass is force-muted.
//    * Offbeats 1,2,3 (slots 1,2,3 at 1/4, 2/4, 3/4 of period): BASS only.
//    * Kick and Bass NEVER sound at the same time.
//    * Pattern: KICK - BASS - BASS - BASS  (one kick + three bass per cycle)
//  ---------------------------------------------------------------------------
//  Pitch routing (v7.1 — split pitch sources):
//    * BASE_PARAM           -> base note of kick & bass
//    * ROOT_PARAM           -> semitone offset applied to BOTH kick & bass
//    * V/OCT_INPUT          -> 1V/oct transpose for BOTH kick & bass
//    * PITCH_CV_INPUT       -> additional fine offset, KICK ONLY
//    * BASS_TRACK_PARAM     -> ratio of bass vs kick (knob)
//    * BASS_TRACK_CV_INPUT  -> modulation of bass ratio, BASS ONLY
//    * Kick pitch  = baseHz * exp((sweep + voct + pitchCv + root) * LN2_12)
//    * Bass pitch  = baseHz * exp((voct + root) * LN2_12) * bassTrackRatio
//  ---------------------------------------------------------------------------
//  Bass architecture (v6 — phase-locked, SAW layer + hard MORPH):
//    * Bass waveform:
//        sub    = sin(2*pi*subPhase)
//        sawRaw = 0.62*polyBLEP(sawPhase1) + 0.38*polyBLEP(sawPhase2)
//        sawSha = fastTanh(sawDrive*sawRaw) / fastTanh(sawDrive)
//        sawMor = MorphShape.processDamped(sawSha, morph)
//        out    = (1 - sawMix)*sub + sawMix*sawMor
//      sawMix comes from BASS_TONE_PARAM (0 = pure sub, 1 = full saw).
//      BASS_CHAR_PARAM gives three preset (SAW/MORPH) bases (SUB/ROLL/SCREAM).
//  ---------------------------------------------------------------------------
//  Display (v9 — Scope-like continuous line over the whole 4-beat cycle):
//    * Linear raw-sample buffer, downsampled with a dynamic factor so that
//      TRACE_N buckets cover the whole measured period (up to 500 ms).
//    * Continuous line drawn with a two-pass glow (thick translucent pass,
//      then thin bright pass) for a clean analog-scope look.
//  ---------------------------------------------------------------------------
//  Layout (v10 — UX-optimized, region-separated):
//    8-column grid, each column at ~11.4 mm pitch.
//    Regions, top to bottom:
//      KICK SOURCE      -> V/OCT, TRIG, ACCENT, BASE, PITCH CV, SWEEP, SWEEP CV, ROOT
//      KICK PITCH       -> CURVE, DECAY, DECAY CV, PUNCH, CLICK, TONE, P-CLICK, TRIG LED
//      KICK BODY        -> BODY, TAIL, TMODE, SHAPE, MORPH, MORPH CV, DRIVE, DRIVE CV
//      KICK CHARACTER   -> FOLD, VOID, FOREST
//      BASS CORE        -> BASS, BASS CV, TRACK, TRACK CV, SAW, DECAY, FILT D, B-SWP
//      BASS CHARACTER   -> BCHAR, ACCENT
//      OUTPUT           -> KICK, BASS, ENV, EOC, LEVEL, CLIP
//    Colored horizontal dividers and section headers keep Kick (cyan) and
//    Bass (pink) visually distinct.
//  ---------------------------------------------------------------------------
// =============================================================================

#include "plugin.hpp"
#include "dsp/digital.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <pmmintrin.h>
#include <xmmintrin.h>
#define PSYKICK_X86_FPU 1
#endif

using namespace rack;

namespace {

// -----------------------------------------------------------------------------
//  Numeric helpers
// -----------------------------------------------------------------------------

inline float clampF(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

inline float clamp01(float x) { return clampF(x, 0.f, 1.f); }

inline float safeF(float x, float fallback = 0.f) {
    return std::isfinite(x) ? x : fallback;
}

inline float smoothStep01(float t) {
    t = clamp01(t);
    return t * t * (3.f - 2.f * t);
}

inline float lerpF(float a, float b, float t) { return a + (b - a) * t; }

inline float ramp01(float x, float lo, float hi) {
    if (hi - lo < 1e-6f)
        return (x >= hi) ? 1.f : 0.f;
    return clamp01((x - lo) / (hi - lo));
}

inline float softLimit(float x) {
    const float knee = 1.05f;
    if (x > knee) {
        const float d = x - knee;
        return knee + d / (1.f + 2.2f * d);
    }
    if (x < -knee) {
        const float d = -x - knee;
        return -(knee + d / (1.f + 2.2f * d));
    }
    return x;
}

inline float fastTanh(float x) {
    const float x2 = x * x;
    const float num = x * (27.f + x2);
    const float den = 27.f + 9.f * x2;
    float y = num / den;
    if (y >  1.f) y =  1.f;
    if (y < -1.f) y = -1.f;
    return y;
}

inline void enableFlushToZero() {
#ifdef PSYKICK_X86_FPU
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
#ifdef _MM_SET_DENORMALS_ZERO_MODE
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif
#endif
}

// -----------------------------------------------------------------------------
//  Constants
// -----------------------------------------------------------------------------

static const float PI_F = 3.14159265358979f;
static const float TWO_PI_F = 6.28318530717959f;
static const float LN2_12 = 0.0577622650466621f;

static const float SR_MIN = 8000.f;
static const float SR_MAX = 384000.f;

static const float BODY_FREQ_MIN = 15.f;
static const float BODY_FREQ_MAX = 800.f;

static const float BASS_FREQ_MIN = 20.f;
static const float BASS_FREQ_MAX = 400.f;

static const float ENV_IDLE = 1e-5f;
static const float ENV_PARK = 1e-4f;
static const float EOC_THRESHOLD = 0.030f;
static const float EOC_PULSE_SEC = 0.0012f;

static const float ROLL_PERIOD_MIN_SEC = 0.050f;
static const float ROLL_PERIOD_MAX_SEC = 4.000f;

static const float DEFAULT_PERIOD_SEC = 0.500f;

static const int   BASS_SWEEP_STEPS = 6;
static const float BASS_SWEEP_SEMIS[BASS_SWEEP_STEPS] = {
    0.f, 1.f, 2.f, 3.f, 5.f, 7.f
};
static const char* BASS_SWEEP_NAME[BASS_SWEEP_STEPS] = {
    "OFF", "1 st", "2 st", "3 st", "5 st", "7 st"
};

static const char* NOTE_NAMES[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

static const uint32_t NOISE_SEED = 0x12345678u;

static const int OS_TAPS = 10;

// --- Display capture constants (v9) ---
static const int   TRACE_N = 512;
static const float TRACE_MAX_SEC = 0.500f;

// -----------------------------------------------------------------------------
//  Musical mappings
// -----------------------------------------------------------------------------

inline float baseHzFromKnob(float k) {
    return 27.5f * std::pow(2.f, 2.f * clamp01(k));
}

inline float knobFromBaseHz(float hz) {
    return 0.5f * std::log2(clampF(hz, 27.5f, 110.f) / 27.5f);
}

inline float pitchDecayFromKnob(float k) {
    return 0.020f * std::pow(22.5f, clamp01(k));
}

inline float bodyTauFromKnob(float k) {
    return 0.060f * std::pow(8.0f, clamp01(k));
}

inline float clickToneFromKnob(float k) {
    return 1200.f * std::pow(7.5f, clamp01(k));
}

inline float bassDecayFromKnob(float k) {
    return 0.030f * std::pow(2.5f, clamp01(k));
}

inline float bassFiltDecayFromKnob(float k) {
    return 0.010f * std::pow(4.0f, clamp01(k));
}

inline float bassTrackRatioFromKnob(float k) {
    return 0.25f * std::pow(8.f, clamp01(k));
}

inline float knobFromBassTrackRatio(float r) {
    return clamp01(std::log(clampF(r, 0.25f, 2.f) / 0.25f) / std::log(8.f));
}

inline int bassSweepStepFromKnob(float v) {
    int i = static_cast<int>(std::floor(clampF(v, 0.f, static_cast<float>(BASS_SWEEP_STEPS) - 0.001f)));
    if (i < 0) i = 0;
    if (i >= BASS_SWEEP_STEPS) i = BASS_SWEEP_STEPS - 1;
    return i;
}

inline void noteNameFromHz(float hz, int rootPc, char* out, size_t n) {
    if (!out || n < 4) return;
    float ref = clampF(safeF(hz, 55.f), 20.f, 2000.f);
    int rp = ((int)rootPc) % 12;
    if (rp < 0) rp += 12;
    float midiF = 69.f + 12.f * std::log(ref / 440.f) / std::log(2.f);
    int midi = (int)std::floor(midiF + 0.5f);
    int pc = ((midi + rp) % 12 + 12) % 12;
    int oct = (midi / 12) - 1;
    if (oct < -1) oct = -1;
    if (oct > 9) oct = 9;
    const char* nm = NOTE_NAMES[pc];
    std::snprintf(out, n, "%s%d", nm, oct);
}

// -----------------------------------------------------------------------------
//  Parameter displays
// -----------------------------------------------------------------------------

struct BaseQuantity : engine::ParamQuantity {
    float getDisplayValue() override { return baseHzFromKnob(getValue()); }
    void setDisplayValue(float v) override { setValue(knobFromBaseHz(v)); }
    int getDisplayPrecision() override { return 2; }
    std::string getUnit() override { return " Hz"; }
    std::string getDisplayValueString() override {
        float hz = baseHzFromKnob(getValue());
        char nn[16];
        noteNameFromHz(hz, 0, nn, sizeof(nn));
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.2f Hz (%s)", (double)hz, nn);
        return std::string(buf);
    }
};

struct BassTrackQuantity : engine::ParamQuantity {
    float getDisplayValue() override { return bassTrackRatioFromKnob(getValue()); }
    void setDisplayValue(float v) override { setValue(knobFromBassTrackRatio(v)); }
    int getDisplayPrecision() override { return 2; }
    std::string getUnit() override { return " x"; }
    std::string getDisplayValueString() override {
        float r = bassTrackRatioFromKnob(getValue());
        float st = 12.f * std::log(r) / std::log(2.f);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.2fx (%+.0fst)", (double)r, (double)st);
        return std::string(buf);
    }
};

struct RootQuantity : engine::ParamQuantity {
    RootQuantity() { snapEnabled = true; }
    float getDisplayValue() override { return getValue(); }
    void setDisplayValue(float v) override {
        setValue(std::floor(clampF(v, 0.f, 11.f) + 0.5f));
    }
    std::string getDisplayValueString() override {
        int i = (int)std::lround(clampF(getValue(), 0.f, 11.f)) % 12;
        if (i < 0) i += 12;
        return std::string(NOTE_NAMES[i]);
    }
    std::string getUnit() override { return ""; }
};

struct TailModeQuantity : engine::ParamQuantity {
    TailModeQuantity() { snapEnabled = true; }
    float getDisplayValue() override { return std::floor(clampF(getValue(), 0.f, 2.999f)); }
    void setDisplayValue(float v) override {
        setValue(std::floor(clampF(v, 0.f, 2.999f)));
    }
    std::string getDisplayValueString() override {
        const int m = (int)std::floor(clampF(getValue(), 0.f, 2.999f));
        return (m == 0) ? std::string("TIGHT")
             : ((m == 1) ? std::string("FOREST") : std::string("HI-TECH"));
    }
    std::string getUnit() override { return ""; }
};

struct BassCharQuantity : engine::ParamQuantity {
    BassCharQuantity() { snapEnabled = true; }
    float getDisplayValue() override { return std::floor(clampF(getValue(), 0.f, 2.999f)); }
    void setDisplayValue(float v) override {
        setValue(std::floor(clampF(v, 0.f, 2.999f)));
    }
    std::string getDisplayValueString() override {
        const int m = (int)std::floor(clampF(getValue(), 0.f, 2.999f));
        return (m == 0) ? std::string("SUB")
             : ((m == 1) ? std::string("ROLL") : std::string("SCREAM"));
    }
    std::string getUnit() override { return ""; }
};

struct BassSweepQuantity : engine::ParamQuantity {
    BassSweepQuantity() { snapEnabled = true; }
    float getDisplayValue() override {
        return (float)bassSweepStepFromKnob(getValue());
    }
    void setDisplayValue(float v) override {
        setValue(std::floor(clampF(v, 0.f, (float)BASS_SWEEP_STEPS - 0.001f)));
    }
    std::string getDisplayValueString() override {
        return std::string(BASS_SWEEP_NAME[bassSweepStepFromKnob(getValue())]);
    }
    std::string getUnit() override { return ""; }
};

struct PitchDecayQuantity : engine::ParamQuantity {
    float getDisplayValue() override { return pitchDecayFromKnob(getValue()) * 1000.f; }
    void setDisplayValue(float v) override {
        setValue(std::log(clampF(v, 20.f, 450.f) / 20.f) / std::log(22.5f));
    }
    int getDisplayPrecision() override { return 0; }
    std::string getUnit() override { return " ms"; }
};

struct TailQuantity : engine::ParamQuantity {
    float getDisplayValue() override { return bodyTauFromKnob(getValue()) * 1000.f; }
    void setDisplayValue(float v) override {
        setValue(std::log(clampF(v, 60.f, 480.f) / 60.f) / std::log(8.f));
    }
    int getDisplayPrecision() override { return 0; }
    std::string getUnit() override { return " ms tau"; }
};

struct ClickToneQuantity : engine::ParamQuantity {
    float getDisplayValue() override { return clickToneFromKnob(getValue()) * 0.001f; }
    void setDisplayValue(float v) override {
        setValue(std::log(clampF(v, 1.2f, 9.f) / 1.2f) / std::log(7.5f));
    }
    int getDisplayPrecision() override { return 2; }
    std::string getUnit() override { return " kHz"; }
};

struct BassDecayQuantity : engine::ParamQuantity {
    float getDisplayValue() override { return bassDecayFromKnob(getValue()) * 1000.f; }
    void setDisplayValue(float v) override {
        setValue(std::log(clampF(v, 30.f, 75.f) / 30.f) / std::log(2.5f));
    }
    int getDisplayPrecision() override { return 0; }
    std::string getUnit() override { return " ms"; }
};

struct BassFiltDecayQuantity : engine::ParamQuantity {
    float getDisplayValue() override { return bassFiltDecayFromKnob(getValue()) * 1000.f; }
    void setDisplayValue(float v) override {
        setValue(std::log(clampF(v, 10.f, 40.f) / 10.f) / std::log(4.f));
    }
    int getDisplayPrecision() override { return 0; }
    std::string getUnit() override { return " ms"; }
};

struct LevelQuantity : engine::ParamQuantity {
    int getDisplayPrecision() override { return 2; }
    std::string getUnit() override { return " x"; }
};

struct SawQuantity : engine::ParamQuantity {
    int getDisplayPrecision() override { return 0; }
    std::string getUnit() override { return " %"; }
    std::string getDisplayValueString() override {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.0f %%", (double)(getValue() * 100.f));
        return std::string(buf);
    }
};

struct MorphQuantity : engine::ParamQuantity {
    int getDisplayPrecision() override { return 0; }
    std::string getUnit() override { return " %"; }
    std::string getDisplayValueString() override {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.0f %%", (double)(getValue() * 100.f));
        return std::string(buf);
    }
};

// -----------------------------------------------------------------------------
//  Retrigger safe envelope stage
// -----------------------------------------------------------------------------

struct EnvStage {
    float cont;
    float body;
    float rise;
    float riseStep;
    float contCoeff;
    float coeff;
    float value;

    EnvStage() : cont(0.f), body(0.f), rise(1.f), riseStep(1.f),
                 contCoeff(0.f), coeff(0.f), value(0.f) {}

    void reset() {
        cont = 0.f;
        body = 0.f;
        rise = 1.f;
        value = 0.f;
    }

    void trigger() {
        cont = value;
        body = 1.f;
        rise = 0.f;
    }

    bool idle() const { return body < ENV_PARK && cont < ENV_PARK; }

    inline float process() {
        cont = safeF(cont * contCoeff, 0.f);
        body = safeF(body * coeff, 0.f);
        rise += riseStep;
        if (rise > 1.f) rise = 1.f;
        const float fresh = body * smoothStep01(rise);
        value = (fresh > cont) ? fresh : cont;
        if (!std::isfinite(value)) value = 0.f;
        return value;
    }
};

// -----------------------------------------------------------------------------
//  Polyphase half band converter (2x)
// -----------------------------------------------------------------------------

struct HalfBand2x {
    float a[OS_TAPS];
    float b[OS_TAPS];
    float xh[OS_TAPS];
    float vEarly[OS_TAPS];
    float vLate[OS_TAPS];

    HalfBand2x() { design(); reset(); }

    void design() {
        float h[2 * OS_TAPS];
        const float mid = static_cast<float>(OS_TAPS) - 0.5f;
        float sum = 0.f;
        for (int i = 0; i < 2 * OS_TAPS; ++i) {
            const float t = static_cast<float>(i) - mid;
            const float s = (std::fabs(t) < 1e-6f)
                ? 0.5f
                : std::sin(PI_F * 0.5f * t) / (PI_F * t);
            const float w = 0.42f
                - 0.5f * std::cos(TWO_PI_F * static_cast<float>(i) / (2.f * OS_TAPS - 1.f))
                + 0.08f * std::cos(2.f * TWO_PI_F * static_cast<float>(i) / (2.f * OS_TAPS - 1.f));
            h[i] = s * w;
            sum += h[i];
        }
        if (!(std::fabs(sum) > 1e-9f)) sum = 1.f;
        float sa = 0.f;
        float sb = 0.f;
        for (int k = 0; k < OS_TAPS; ++k) {
            a[k] = 2.f * h[2 * k] / sum;
            b[k] = 2.f * h[2 * k + 1] / sum;
            sa += a[k];
            sb += b[k];
        }
        if (!(std::fabs(sa) > 1e-9f)) sa = 1.f;
        if (!(std::fabs(sb) > 1e-9f)) sb = 1.f;
        for (int k = 0; k < OS_TAPS; ++k) {
            a[k] /= sa;
            b[k] /= sb;
        }
    }

    void reset() {
        for (int i = 0; i < OS_TAPS; ++i) {
            xh[i] = 0.f;
            vEarly[i] = 0.f;
            vLate[i] = 0.f;
        }
    }

    void split(float x, float& yEarly, float& yLate) {
        for (int i = OS_TAPS - 1; i > 0; --i)
            xh[i] = xh[i - 1];
        xh[0] = safeF(x);
        float se = 0.f;
        float sl = 0.f;
        for (int k = 0; k < OS_TAPS; ++k) {
            se += b[k] * xh[k];
            sl += a[k] * xh[k];
        }
        yEarly = se;
        yLate = sl;
    }

    float merge(float vEarlyIn, float vLateIn) {
        for (int i = OS_TAPS - 1; i > 0; --i) {
            vEarly[i] = vEarly[i - 1];
            vLate[i] = vLate[i - 1];
        }
        vEarly[0] = safeF(vEarlyIn);
        vLate[0] = safeF(vLateIn);
        float s = 0.f;
        for (int k = 0; k < OS_TAPS; ++k)
            s += 0.5f * (a[k] * vEarly[k] + b[k] * vLate[k]);
        return safeF(s);
    }
};

// -----------------------------------------------------------------------------
//  Zero delay feedback state variable low pass (TPT form)
// -----------------------------------------------------------------------------

struct SvfLp {
    float g, k, a1, a2, a3;
    float s1, s2;

    SvfLp() : g(0.f), k(1.f), a1(0.f), a2(0.f), a3(0.f), s1(0.f), s2(0.f) {}

    void setCutoff(float fc, float sr, float q) {
        const float f = clampF(fc, 5.f, 0.45f * sr);
        g = std::tan(PI_F * f / sr);
        k = 1.f / clampF(q, 0.2f, 4.f);
        a1 = 1.f / (1.f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void reset() { s1 = 0.f; s2 = 0.f; }

    inline float process(float x) {
        const float v3 = safeF(x) - s2;
        const float v1 = a1 * s1 + a2 * v3;
        const float v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2.f * v1 - s1;
        s2 = 2.f * v2 - s2;
        if (!std::isfinite(s1) || !std::isfinite(s2)) {
            s1 = 0.f;
            s2 = 0.f;
            return 0.f;
        }
        return v2;
    }
};

// -----------------------------------------------------------------------------
//  SHAPE / MORPH engine (shared between kick body and bass saw)
// -----------------------------------------------------------------------------

struct MorphShape {
    float w3, w5, w7, w9, w11, w13;
    float norm;
    float normTarget;
    float normCoeff;
    float lastMorph;
    float lastDepth;

    MorphShape() : w3(0.f), w5(0.f), w7(0.f), w9(0.f), w11(0.f), w13(0.f),
                   norm(1.f), normTarget(1.f), normCoeff(0.f),
                   lastMorph(-2.f), lastDepth(-2.f) {}

    void setSmooth(float seconds, float sr) {
        normCoeff = (seconds > 0.f && sr > 1.f) ? std::exp(-1.f / (seconds * sr)) : 0.f;
    }

    void reset() {
        norm = 1.f;
        normTarget = 1.f;
        lastMorph = -2.f;
        lastDepth = -2.f;
    }

    inline float evalPoly(float s) const {
        const float s2 = 2.f * s;
        const float t1 = s;
        const float t2 = s2 * t1 - 1.f;
        const float t3 = s2 * t2 - t1;
        const float t4 = s2 * t3 - t2;
        const float t5 = s2 * t4 - t3;
        const float t6 = s2 * t5 - t4;
        const float t7 = s2 * t6 - t5;
        const float t8 = s2 * t7 - t6;
        const float t9 = s2 * t8 - t7;
        const float t10 = s2 * t9 - t8;
        const float t11 = s2 * t10 - t9;
        const float t12 = s2 * t11 - t10;
        const float t13 = s2 * t12 - t11;
        return s + w3 * t3 + w5 * t5 + w7 * t7 + w9 * t9 + w11 * t11 + w13 * t13;
    }

    float scanPeak() const {
        float p = 1e-6f;
        for (int i = 0; i <= 16; ++i) {
            const float s = -1.f + 2.f * static_cast<float>(i) * (1.f / 16.f);
            const float y = std::fabs(evalPoly(s));
            if (y > p) p = y;
        }
        return p;
    }

    void update(float morph, float depth) {
        const float m = clamp01(safeF(morph));
        const float d = clamp01(safeF(depth));

        w3  = d * ramp01(m, 0.00f, 0.18f) * (1.f / 3.f)  * 1.40f;
        w5  = d * ramp01(m, 0.08f, 0.30f) * (1.f / 5.f)  * 1.50f;
        w7  = d * ramp01(m, 0.20f, 0.44f) * (1.f / 7.f)  * 1.55f;
        w9  = d * ramp01(m, 0.32f, 0.58f) * (1.f / 9.f)  * 1.60f;
        w11 = d * ramp01(m, 0.44f, 0.72f) * (1.f / 11.f) * 1.65f;
        w13 = d * ramp01(m, 0.56f, 0.86f) * (1.f / 13.f) * 1.70f;

        const float mq = std::floor(m * 64.f) * (1.f / 64.f);
        const float dq = std::floor(d * 64.f) * (1.f / 64.f);
        if (mq == lastMorph && dq == lastDepth) return;
        lastMorph = mq;
        lastDepth = dq;
        normTarget = 1.f / scanPeak();
    }

    inline float process(float s) {
        norm = normTarget + normCoeff * (norm - normTarget);
        if (!std::isfinite(norm) || norm <= 0.f)
            norm = normTarget;
        return evalPoly(s) * norm;
    }

    inline float processDamped(float s, float morph) {
        const float raw = evalPoly(s);
        const float m = clamp01(safeF(morph));
        const float comp = 1.f + 0.90f * m;
        float y = raw * comp;
        y = fastTanh(y * 0.85f) * 1.15f;
        if (y >  1.5f) y =  1.5f;
        if (y < -1.5f) y = -1.5f;
        return y;
    }
};

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

    void set(float v) { value = clamp01(safeF(v)); }

    float process(float target) {
        target = clamp01(safeF(target));
        value = target + coeff * (value - target);
        if (!std::isfinite(value)) value = target;
        return value;
    }
};

// -----------------------------------------------------------------------------
//  BASS VOICE — phase-locked, real SAW layer, hard MORPH.
// -----------------------------------------------------------------------------
struct BassVoice {
    float subPhase;
    float sawPhase1;
    float sawPhase2;
    float freqSm;

    float dcPrev;
    float dcOut;

    float ampEnv;
    float filtEnv;
    float ampDecayCoeff;
    float filtDecayCoeff;
    float atkCoeff;

    float holdSamples;
    float holdCounter;

    float bpEnv;
    float bpEnvCoeff;
    float bpSweepSt;

    float filtBase;
    float filtDepth;
    float filtQ;
    float lp1, lp2, lp3, lp4;

    float drive;
    float level;

    float sawMix;
    float morphAmt;
    float sawActualRatio;
    MorphShape morphShape;

    BassVoice() {
        ampDecayCoeff = 0.999f;
        filtDecayCoeff = 0.999f;
        atkCoeff = 0.01f;
        bpEnv = 0.f;
        bpEnvCoeff = 0.999f;
        bpSweepSt = 0.f;
        filtBase = 0.02f;
        filtDepth = 0.05f;
        filtQ = 0.9f;
        drive = 0.30f;
        holdSamples = 1920.f;
        level = 0.7f;
        sawMix = 0.5f;
        morphAmt = 0.3f;
        sawActualRatio = 0.5f;
        reset();
    }

    void reset() {
        subPhase = 0.f;
        sawPhase1 = 0.f;
        sawPhase2 = 0.f;
        freqSm = 55.f;
        dcPrev = 0.f;
        dcOut = 0.f;
        ampEnv = 0.f;
        filtEnv = 0.f;
        holdCounter = 0.f;
        bpEnv = 0.f;
        lp1 = lp2 = lp3 = lp4 = 0.f;
        morphShape.reset();
        sawActualRatio = 0.5f;
    }

    void trigger() {
        subPhase = 0.f;
        sawPhase1 = 0.f;
        sawPhase2 = 0.f;
        ampEnv = 0.f;
        filtEnv = 1.f;
        holdCounter = holdSamples;
        bpEnv = 1.f;
        morphShape.reset();
    }

    void mute() {
        ampEnv = 0.f;
        filtEnv = 0.f;
        holdCounter = 0.f;
        bpEnv = 0.f;
        subPhase = 0.f;
        sawPhase1 = 0.f;
        sawPhase2 = 0.f;
    }

    bool idle() const {
        return ampEnv < ENV_PARK && filtEnv < ENV_PARK && holdCounter <= 0.f;
    }

    void setCoeffs(float sr, float ampDecaySec, float filtDecaySec,
                   float cutBaseHz, float cutDepthHz) {
        ampDecayCoeff = std::exp(-1.f / (ampDecaySec * sr));
        filtDecayCoeff = std::exp(-1.f / (filtDecaySec * sr));
        filtBase = clampF(cutBaseHz / sr, 0.001f, 0.45f);
        filtDepth = clampF(cutDepthHz / sr, 0.0f, 0.45f);
    }

    void setChar(int mode, float sr) {
        if (mode == 0) {         // SUB
            drive = 0.15f;
            filtQ = 0.7f;
            holdSamples = 0.045f * sr;
        }
        else if (mode == 2) {    // SCREAM
            drive = 0.55f;
            filtQ = 1.3f;
            holdSamples = 0.025f * sr;
        }
        else {                    // ROLL
            drive = 0.35f;
            filtQ = 1.0f;
            holdSamples = 0.035f * sr;
        }
    }

    inline float process(float freq, float dt, float morphInput) {
        if (holdCounter > 0.f) {
            holdCounter -= 1.f;
            ampEnv = safeF(ampEnv + (1.f - ampEnv) * atkCoeff, 0.f);
        } else {
            ampEnv *= ampDecayCoeff;
        }
        filtEnv *= filtDecayCoeff;
        bpEnv *= bpEnvCoeff;

        if (!std::isfinite(ampEnv)) ampEnv = 0.f;
        if (!std::isfinite(filtEnv)) filtEnv = 0.f;
        if (!std::isfinite(bpEnv)) bpEnv = 0.f;

        if (bpSweepSt <= 1e-6f) bpEnv = 0.f;
        if (ampEnv < 1e-6f && filtEnv < 1e-6f) return 0.f;

        const float bpGlideSt = safeF(bpSweepSt * bpEnv, 0.f);
        const float freqGlide = (bpGlideSt > 1e-6f)
            ? clampF(freq * std::exp(bpGlideSt * LN2_12), BASS_FREQ_MIN, BASS_FREQ_MAX)
            : freq;

        freqSm = freqGlide + 0.0008f * (freqSm - freqGlide);
        if (!std::isfinite(freqSm)) freqSm = freqGlide;

        const float dtp = freqSm * dt;
        const float dtp2 = freqSm * 1.0035f * dt;

        subPhase += 0.5f * freqSm * dt;
        if (subPhase >= 1.f || subPhase < 0.f) subPhase -= std::floor(subPhase);

        sawPhase1 += dtp;
        if (sawPhase1 >= 1.f || sawPhase1 < 0.f) sawPhase1 -= std::floor(sawPhase1);

        sawPhase2 += dtp2;
        if (sawPhase2 >= 1.f || sawPhase2 < 0.f) sawPhase2 -= std::floor(sawPhase2);

        auto polyBlepSaw = [&](float phase, float dtNorm) -> float {
            float s = 2.f * phase - 1.f;
            if (dtNorm > 1e-8f) {
                if (phase < dtNorm) {
                    const float x = phase / dtNorm;
                    s -= (x + x - x * x - 1.f);
                } else if (phase > 1.f - dtNorm) {
                    const float x = (phase - 1.f) / dtNorm;
                    s -= (x * x + x + x + 1.f);
                }
            }
            return s;
        };

        const float saw1 = polyBlepSaw(sawPhase1, dtp);
        const float saw2 = polyBlepSaw(sawPhase2, dtp2);
        float sawRaw = 0.62f * saw1 + 0.38f * saw2;
        sawRaw = clampF(sawRaw, -1.2f, 1.2f);

        const float sawDrive = 1.f + 3.4f * sawMix;
        const float tanhDrive = fastTanh(sawDrive);
        const float invTanhDrive = (tanhDrive > 1e-6f) ? (1.f / tanhDrive) : 1.f;
        float sawShaped = fastTanh(sawDrive * sawRaw) * invTanhDrive;

        morphShape.setSmooth(0.040f, 1.f / dt);
        morphShape.update(morphInput, 1.f);
        const float morphedSaw = morphShape.processDamped(sawShaped, morphInput);

        const float sub = std::sin(TWO_PI_F * subPhase);
        const float pre = (1.f - sawMix) * sub + sawMix * morphedSaw;

        {
            const float eSub = std::fabs(sub);
            const float eSaw = std::fabs(morphedSaw);
            const float denom = eSub + eSaw + 1e-6f;
            sawActualRatio = clamp01(eSaw / denom);
            if (!std::isfinite(sawActualRatio)) sawActualRatio = 0.5f;
        }

        const float dcCoeff = std::exp(-TWO_PI_F * 8.f * dt);
        dcOut = safeF(pre - dcPrev + dcCoeff * dcOut, 0.f);
        dcPrev = safeF(pre, 0.f);
        const float clean = safeF(dcOut, 0.f);

        const float cutNorm = clampF(filtBase + filtDepth * filtEnv, 0.0008f, 0.40f);
        const float coeff = std::exp(-TWO_PI_F * cutNorm);
        lp1 = safeF(clean + coeff * (lp1 - clean), 0.f);
        lp2 = safeF(lp1   + coeff * (lp2 - lp1),   0.f);
        lp3 = safeF(lp2   + coeff * (lp3 - lp2),   0.f);
        lp4 = safeF(lp3   + coeff * (lp4 - lp3),   0.f);
        const float resAmt = clampF((filtQ - 0.7f) * 0.5f, 0.f, 0.4f);
        const float lpOut = safeF(lp4 + resAmt * (lp2 - lp4), 0.f);

        const float dg = 1.f + drive * 1.5f;
        const float x = safeF(dg * lpOut, 0.f);
        float sat;
        if (x >= 0.f) {
            sat = safeF(x / (1.f + 0.6f * x), 0.f);
        } else {
            sat = safeF(x / (1.f + 0.9f * (-x)), 0.f);
        }
        const float out = safeF(lerpF(lpOut, sat * 1.25f, drive) * ampEnv * level, 0.f);
        return std::isfinite(out) ? out : 0.f;
    }
};

// -----------------------------------------------------------------------------
//  Slot scheduler — three offbeat bass triggers at 1/4, 2/4, 3/4 of period.
// -----------------------------------------------------------------------------
struct SlotScheduler {
    float periodSamples;
    float nextTriggerSample;
    int   nextSlot;

    SlotScheduler() : periodSamples(0.f), nextTriggerSample(0.f), nextSlot(1) {}

    void reset() {
        periodSamples = 0.f;
        nextTriggerSample = 0.f;
        nextSlot = 1;
    }

    void onTrigger(float period) {
        periodSamples = period;
        nextSlot = 1;
        nextTriggerSample = periodSamples * 0.25f;
    }

    bool process(int samplesSinceTrig) {
        if (periodSamples < 10.f) return false;
        if (nextSlot > 3) return false;
        const float t = (float)samplesSinceTrig;
        if (t >= nextTriggerSample) {
            nextSlot++;
            if (nextSlot <= 3) {
                nextTriggerSample = periodSamples * (float)nextSlot * 0.25f;
            }
            return true;
        }
        return false;
    }
};

// -----------------------------------------------------------------------------
//  Module
// -----------------------------------------------------------------------------

struct PsyKick : Module {

enum ParamIds {
    BASE_PARAM,
    SWEEP_PARAM,
    CURVE_PARAM,
    DECAY_PARAM,
    PUNCH_PARAM,
    CLICK_PARAM,
    CLICK_TONE_PARAM,
    BODY_PARAM,
    TAIL_PARAM,
    SHAPE_PARAM,
    MORPH_PARAM,
    DRIVE_PARAM,
    FOLD_PARAM,
    DARK_PARAM,
    FOREST_PARAM,
    PUNCH_CLICK_PARAM,
    BASS_LEVEL_PARAM,
    BASS_TONE_PARAM,
    BASS_DECAY_PARAM,
    BASS_FILT_DECAY_PARAM,
    BASS_TRACK_PARAM,
    BASS_SWEEP_PARAM,
    ROOT_PARAM,
    ACCENT_PARAM,
    LEVEL_PARAM,
    KICK_TAILMODE_PARAM,
    BASS_CHAR_PARAM,
    NUM_PARAMS
};

enum InputIds {
    TRIG_INPUT,
    VOCT_INPUT,
    ACCENT_INPUT,
    PITCH_CV_INPUT,
    SWEEP_CV_INPUT,
    DECAY_CV_INPUT,
    MORPH_CV_INPUT,
    DRIVE_CV_INPUT,
    BASS_CV_INPUT,
    BASS_TRACK_CV_INPUT,
    NUM_INPUTS
};

enum OutputIds {
    KICK_OUTPUT,
    BASS_OUTPUT,
    ENV_OUTPUT,
    EOC_OUTPUT,
    NUM_OUTPUTS
};

enum LightIds {
    TRIG_LIGHT,
    CLIP_LIGHT,
    NUM_LIGHTS
};

enum Stage {
    STAGE_IDLE,
    STAGE_ATTACK,
    STAGE_BODY,
    STAGE_TAIL
};

enum ModTarget {
    MOD_BASE,
    MOD_SWEEP,
    MOD_CURVE,
    MOD_DECAY,
    MOD_PUNCH,
    MOD_CLICK,
    MOD_CLICK_TONE,
    MOD_BODY,
    MOD_TAIL,
    MOD_SHAPE,
    MOD_MORPH,
    MOD_DRIVE,
    MOD_FOLD,
    MOD_DARK,
    MOD_FOREST,
    MOD_PUNCH_CLICK,
    MOD_BASS_LEVEL,
    MOD_BASS_TONE,
    MOD_BASS_DECAY,
    MOD_BASS_FILT_DECAY,
    MOD_BASS_TRACK,
    MOD_BASS_SWEEP,
    MOD_ACCENT,
    MOD_LEVEL,
    MOD_TAILMODE,
    MOD_BASS_CHAR,
    MOD_COUNT
};

struct KickParams {
    float baseHz;
    float sweepSt;
    float curve;
    float pitchDecay;
    float punch;
    float click;
    float clickTone;
    float body;
    float tailTau;
    float shape;
    float morph;
    float drive;
    float fold;
    float dark;
    float forest;
    float punchClick;
    float bassLevel;
    float bassSaw;
    float bassMorph;
    float bassDecay;
    float bassFiltDecay;
    float bassTrackRatio;
    float bassSweepSt;
    int   bassSweepStep;
    int rootPc;
    float accentDepth;
    float level;
    int tailMode;
    int bassChar;

    float voctSemis;
    float pitchCvSemis;
    float rootSemis;
};

// ---------------- trigger and performance ----------------
dsp::SchmittTrigger trigDetector;
bool kickActive;
bool eocArmed;
float kickT;
float accentN;
float trigFlash;

// ---------------- envelopes ----------------
EnvStage envBody;
EnvStage envPunch;
EnvStage envClick;
float pitchEF;
float pitchES;
float punchSweepE;
float clickChirp;

// ---------------- oscillators ----------------
float bodyPhase;
float punchPhase;
float clickPhase;
float freqSm;

// ---------------- bass voice ----------------
BassVoice bassVoice;
SlotScheduler slotSched;

// ---------------- trigger/period ----------------
float periodSamples;
int samplesSinceTrig;
bool havePeriod;
bool periodValid;
int trigCount;
bool triggerPending;

// ---------------- shaping and forest/void ----------------
MorphShape morphShape;
HalfBand2x os;
float darkTiltLp;
float forestFb;
float voidLp80;
float voidLp8k;
float voidMidLp;
float voidSubPhase;
uint32_t noiseState;
float punchClickHp;
float punchClickBp;
float punchClickEnv;
float coeffPunchClickHp;
float coeffPunchClickBp;
float coeffPunchClickEnv;
float gnarlLp;
float voidSub2Phase;
float voidSubSlowEnv;
float coeffVoidSubSlow;

// ---------------- output conditioning ----------------
SvfLp bassLp;
float dcXK;
float dcYK;
float dcXB;
float dcYB;
dsp::PulseGenerator eocPulse;
float sigLevel;
float clipFlash;

// ---------------- parameters and coefficients ----------------
OnePole smooth[MOD_COUNT];
float pitchCvSm;
float pitchKickCvSm;
float coeffPitchCv;
float sampleRate;
float dt;

float attackStepKick;
float attackStepPunch;
float attackStepClick;
float coeffBody;
float coeffTail;
float coeffCont;
float coeffPunch;
float coeffClick;
float coeffPitchF;
float coeffPitchS;
float coeffPunchSweep;
float coeffClickChirp;
float coeffTilt;
float coeffFreqSlew;
float coeffVoid80;
float coeffVoid8k;
float coeffVoidMid;
float dcRk;
float dcRb;
float clickToneMax;
float clickHarm3;
float gainDrive;
float satAsym;
float bassAtkCoeff;

// coefficient caches
float cachePitchDecay;
float cacheBodyTau;
float cacheCurve;
float cacheClickTone;
float cacheDark;
float cacheDrive;
float cacheFold;
float cacheBassDecay;
float cacheBassFiltDecay;
float cacheBassSaw;
float cacheBassMorph;
float cacheBassSweepSt;
int   cacheBassSweepStep;
int cacheTailMode;
int cacheBassChar;

float cacheBaseN;
float cacheBaseHz;

// =============================================================================
//  DISPLAY CAPTURE (v9)
// =============================================================================

float capKickRaw[TRACE_N];
float capBassRaw[TRACE_N];
float capBassEnv[TRACE_N];
float capPitch[TRACE_N];
float capEnv[TRACE_N];

float dispKickRaw[TRACE_N];
float dispBassRaw[TRACE_N];
float dispBassEnv[TRACE_N];
float dispPitch[TRACE_N];
float dispEnv[TRACE_N];

int   capturePos;
int   captureLen;
int   captureTargetLen;
int   captureBucketAcc;
int   captureBucketSamples;
bool  captureActive;
bool  captureValid;
bool  captureKickSeen;

float kAccum;
float bAccum;
int   kAccumCount;

float dispStage;
float dispHz;
float dispSemis;
float dispSweep;
float dispPeriodMs;
float dispBpm;
float dispLevel;
float dispClip;
float dispBaseHz;
float dispBassHz;
float dispBassSemis;
float dispRootPc;
float dispVoctSemis;
float dispBassSaw;
float dispBassMorph;

// -------------------------------------------------------------------------
//  Construction
// -------------------------------------------------------------------------

PsyKick() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    configParam<BaseQuantity>(BASE_PARAM, 0.f, 1.f, 0.42f, "Base frequency");
    configParam(SWEEP_PARAM, 0.f, 36.f, 30.f, "Sweep", " st");
    configParam(CURVE_PARAM, 0.f, 1.f, 0.18f, "Curve", " (0 = instant drop, 1 = long glide)");
    configParam<PitchDecayQuantity>(DECAY_PARAM, 0.f, 1.f, 0.62f, "Decay");

    configParam(PUNCH_PARAM, 0.f, 1.f, 0.55f, "Punch");
    configParam(CLICK_PARAM, 0.f, 1.f, 0.35f, "Click level");
    configParam<ClickToneQuantity>(CLICK_TONE_PARAM, 0.f, 1.f, 0.60f, "Click tone");

    configParam(BODY_PARAM, 0.f, 1.f, 0.85f, "Body level");
    configParam<TailQuantity>(TAIL_PARAM, 0.f, 1.f, 0.48f, "Tail");
    configParam(SHAPE_PARAM, 0.f, 1.f, 0.55f, "Shape depth", " (0 = pure sine)");
    configParam<MorphQuantity>(MORPH_PARAM, 0.f, 1.f, 0.30f, "Morph",
        " (sine -> soft saw -> hard saw)");

    configParam(DRIVE_PARAM, 0.f, 1.f, 0.30f, "Drive", " (0 = clean)");
    configParam(FOLD_PARAM, 0.f, 1.f, 0.15f, "Fold", " (0 = clean)");
    configParam(DARK_PARAM, 0.f, 1.f, 0.55f, "Void");
    configParam(FOREST_PARAM, 0.f, 1.f, 0.35f, "Forest", " (clip -> fold -> phase-distort)");
    configParam(PUNCH_CLICK_PARAM, 0.f, 1.f, 0.40f, "Punch click", " (noise spit)");

    configParam(BASS_LEVEL_PARAM, 0.f, 1.f, 0.70f, "Bass level");
    configParam<SawQuantity>(BASS_TONE_PARAM, 0.f, 1.f, 0.45f, "Bass SAW",
        " (0 = pure sub, 1 = full saw)");
    configParam<BassDecayQuantity>(BASS_DECAY_PARAM, 0.f, 1.f, 0.35f, "Bass decay");
    configParam<BassFiltDecayQuantity>(BASS_FILT_DECAY_PARAM, 0.f, 1.f, 0.30f, "Bass filter decay");
    configParam<BassTrackQuantity>(BASS_TRACK_PARAM, 0.f, 1.f, 0.6666667f, "Bass track",
        " (interval vs kick)");
    configParam<BassSweepQuantity>(BASS_SWEEP_PARAM, 0.f, (float)BASS_SWEEP_STEPS - 1.f, 0.f,
        "Bass sweep step", " (0 = off, 5 = 7 st)");
    configParam<RootQuantity>(ROOT_PARAM, 0.f, 11.f, 0.f, "Root note");

    configParam(ACCENT_PARAM, 0.f, 1.f, 0.50f, "Accent depth", " (0 = ignore the accent input)");

    configParam<LevelQuantity>(LEVEL_PARAM, 0.f, 2.f, 1.f, "Level");
    configParam<TailModeQuantity>(KICK_TAILMODE_PARAM, 0.f, 2.f, 1.f, "Kick tail mode");
    configParam<BassCharQuantity>(BASS_CHAR_PARAM, 0.f, 2.f, 1.f, "Bass character",
        " (SUB / ROLL / SCREAM preset)");

    configInput(TRIG_INPUT, "Trigger (Schmitt, 0.12 V / 0.5 V)");
    configInput(VOCT_INPUT, "V/OCT (1 V = 1 octave, both kick & bass)");
    configInput(ACCENT_INPUT, "Accent (0..10 V, latched at the trigger)");
    configInput(PITCH_CV_INPUT, "Pitch CV (1 V = 1 octave, kick only)");
    configInput(SWEEP_CV_INPUT, "Sweep CV (1 V = 10 st)");
    configInput(DECAY_CV_INPUT, "Decay CV (1 V = 20% of the range)");
    configInput(MORPH_CV_INPUT, "Morph CV (1 V = 20% of the range)");
    configInput(DRIVE_CV_INPUT, "Drive CV (1 V = 20% of the range)");
    configInput(BASS_CV_INPUT, "Bass CV (1 V = 20% of the range)");
    configInput(BASS_TRACK_CV_INPUT, "Bass track CV (1 V = 20% of the range, bass only)");

    configOutput(KICK_OUTPUT, "Kick");
    configOutput(BASS_OUTPUT, "Bass");
    configOutput(ENV_OUTPUT, "Envelope (0..10 V)");
    configOutput(EOC_OUTPUT, "End of cycle (10 V pulse)");

    configLight(TRIG_LIGHT, "Trigger");
    configLight(CLIP_LIGHT, "Overload");

    for (int i = 0; i < NUM_PARAMS; ++i) {
        if (paramQuantities[i])
            params[i].setValue(paramQuantities[i]->getDefaultValue());
    }
    onReset();
}

// -------------------------------------------------------------------------
//  Reset
// -------------------------------------------------------------------------

void onReset() override {
    enableFlushToZero();
    trigDetector.reset();
    trigDetector.process(0.f, 0.12f, 0.5f);
    kickActive = false;
    eocArmed = false;
    kickT = 0.f;
    accentN = 0.f;
    trigFlash = 0.f;

    envBody.reset();
    envPunch.reset();
    envClick.reset();
    pitchEF = 0.f;
    pitchES = 0.f;
    punchSweepE = 0.f;
    clickChirp = 0.f;

    bodyPhase = 0.f;
    punchPhase = 0.f;
    clickPhase = 0.f;
    freqSm = 40.f;

    bassVoice.reset();
    slotSched.reset();

    periodSamples = DEFAULT_PERIOD_SEC * 48000.f;
    samplesSinceTrig = 0;
    havePeriod = false;
    periodValid = false;
    trigCount = 0;
    triggerPending = false;

    os.reset();
    morphShape.reset();
    darkTiltLp = 0.f;
    forestFb = 0.f;
    voidLp80 = 0.f;
    voidLp8k = 0.f;
    voidMidLp = 0.f;
    voidSubPhase = 0.f;
    noiseState = NOISE_SEED;
    punchClickHp = 0.f;
    punchClickBp = 0.f;
    punchClickEnv = 0.f;
    gnarlLp = 0.f;
    voidSub2Phase = 0.f;
    voidSubSlowEnv = 0.f;

    bassLp.reset();
    dcXK = dcYK = dcXB = dcYB = 0.f;
    eocPulse.reset();
    sigLevel = 0.f;
    clipFlash = 0.f;

    for (int i = 0; i < TRACE_N; ++i) {
        capKickRaw[i] = 0.f;
        capBassRaw[i] = 0.f;
        capBassEnv[i] = 0.f;
        capPitch[i] = 0.f;
        capEnv[i] = 0.f;
        dispKickRaw[i] = 0.f;
        dispBassRaw[i] = 0.f;
        dispBassEnv[i] = 0.f;
        dispPitch[i] = 0.f;
        dispEnv[i] = 0.f;
    }
    capturePos = 0;
    captureLen = 0;
    captureTargetLen = 0;
    captureBucketAcc = 0;
    captureBucketSamples = 1;
    captureActive = false;
    captureValid = false;
    captureKickSeen = false;
    kAccum = 0.f;
    bAccum = 0.f;
    kAccumCount = 0;

    dispStage = STAGE_IDLE;
    dispHz = 0.f;
    dispSemis = 0.f;
    dispSweep = 0.f;
    dispPeriodMs = 500.f;
    dispBpm = 120.f;
    dispLevel = 0.f;
    dispClip = 0.f;
    dispBaseHz = 40.f;
    dispBassHz = 40.f;
    dispBassSemis = 0.f;
    dispRootPc = 0.f;
    dispVoctSemis = 0.f;
    dispBassSaw = 0.5f;
    dispBassMorph = 0.3f;

    pitchCvSm = 0.f;
    pitchKickCvSm = 0.f;

    cachePitchDecay = -1.f;
    cacheBodyTau = -1.f;
    cacheCurve = -1.f;
    cacheClickTone = -1.f;
    cacheDark = -1.f;
    cacheDrive = -1.f;
    cacheFold = -1.f;
    cacheBassDecay = -1.f;
    cacheBassFiltDecay = -1.f;
    cacheBassSaw = -1.f;
    cacheBassMorph = -1.f;
    cacheBassSweepSt = -1.f;
    cacheBassSweepStep = -1;
    cacheTailMode = -1;
    cacheBassChar = -1;

    cacheBaseN = -1.f;
    cacheBaseHz = 40.f;

    sampleRate = clampF(safeF((APP && APP->engine) ? APP->engine->getSampleRate() : 48000.f, 48000.f),
        SR_MIN, SR_MAX);
    updateRates();
}

void onSampleRateChange() override { updateRates(); }

// -------------------------------------------------------------------------
//  Rates and coefficients
// -------------------------------------------------------------------------

void updateRates() {
    sampleRate = clampF(safeF(sampleRate, 48000.f), SR_MIN, SR_MAX);
    dt = 1.f / sampleRate;

    smooth[MOD_BASE].setTime(0.004f, sampleRate);
    smooth[MOD_SWEEP].setTime(0.025f, sampleRate);
    smooth[MOD_CURVE].setTime(0.040f, sampleRate);
    smooth[MOD_DECAY].setTime(0.025f, sampleRate);
    smooth[MOD_PUNCH].setTime(0.015f, sampleRate);
    smooth[MOD_CLICK].setTime(0.015f, sampleRate);
    smooth[MOD_CLICK_TONE].setTime(0.020f, sampleRate);
    smooth[MOD_BODY].setTime(0.025f, sampleRate);
    smooth[MOD_TAIL].setTime(0.030f, sampleRate);
    smooth[MOD_SHAPE].setTime(0.030f, sampleRate);
    smooth[MOD_MORPH].setTime(0.030f, sampleRate);
    smooth[MOD_DRIVE].setTime(0.020f, sampleRate);
    smooth[MOD_FOLD].setTime(0.020f, sampleRate);
    smooth[MOD_DARK].setTime(0.040f, sampleRate);
    smooth[MOD_FOREST].setTime(0.030f, sampleRate);
    smooth[MOD_PUNCH_CLICK].setTime(0.015f, sampleRate);
    smooth[MOD_BASS_LEVEL].setTime(0.020f, sampleRate);
    smooth[MOD_BASS_TONE].setTime(0.030f, sampleRate);
    smooth[MOD_BASS_DECAY].setTime(0.030f, sampleRate);
    smooth[MOD_BASS_FILT_DECAY].setTime(0.030f, sampleRate);
    smooth[MOD_BASS_TRACK].setTime(0.020f, sampleRate);
    smooth[MOD_BASS_SWEEP].setTime(0.005f, sampleRate);
    smooth[MOD_ACCENT].setTime(0.015f, sampleRate);
    smooth[MOD_LEVEL].setTime(0.020f, sampleRate);
    smooth[MOD_TAILMODE].setTime(0.030f, sampleRate);
    smooth[MOD_BASS_CHAR].setTime(0.030f, sampleRate);

    morphShape.setSmooth(0.050f, sampleRate);
    bassVoice.morphShape.setSmooth(0.040f, sampleRate);

    coeffPitchCv = std::exp(-1.f / (0.001f * sampleRate));
    if (!std::isfinite(coeffPitchCv)) coeffPitchCv = 0.f;

    coeffTilt = std::exp(-TWO_PI_F * 1100.f * dt);
    coeffFreqSlew = std::exp(-dt / 0.00035f);

    coeffVoid80 = std::exp(-TWO_PI_F * 80.f * dt);
    coeffVoid8k = std::exp(-TWO_PI_F * 8000.f * dt);
    coeffVoidMid = std::exp(-TWO_PI_F * 300.f * dt);

    coeffPunchClickHp = std::exp(-TWO_PI_F * 4000.f * dt);
    coeffPunchClickBp = std::exp(-TWO_PI_F * 400.f * dt);
    coeffPunchClickEnv = std::exp(-dt / 0.012f);

    coeffVoidSubSlow = std::exp(-dt / 0.250f);

    dcRk = clampF(1.f - TWO_PI_F * 12.f * dt, 0.90f, 0.99995f);
    dcRb = clampF(1.f - TWO_PI_F * 16.f * dt, 0.90f, 0.99995f);
    bassLp.setCutoff(260.f, sampleRate, 0.707f);
    if (!havePeriod) {
        periodSamples = safeF(DEFAULT_PERIOD_SEC * sampleRate, 24000.f);
    }

    clickToneMax = clampF(0.1538f * sampleRate, 1200.f, 9000.f);

    bassAtkCoeff = 1.f - std::exp(-dt / 0.0006f);
    if (!std::isfinite(bassAtkCoeff) || bassAtkCoeff < 0.f) bassAtkCoeff = 0.f;

    primeSmoothers();

    updateCoeffs(pitchDecayFromKnob(clamp01(params[DECAY_PARAM].getValue())),
        bodyTauFromKnob(clamp01(params[TAIL_PARAM].getValue())),
        clamp01(params[CURVE_PARAM].getValue()),
        clampF(clickToneFromKnob(clamp01(params[CLICK_TONE_PARAM].getValue())), 1200.f, clickToneMax),
        clamp01(params[DARK_PARAM].getValue()),
        clamp01(params[DRIVE_PARAM].getValue()),
        clamp01(params[FOLD_PARAM].getValue()),
        bassDecayFromKnob(clamp01(params[BASS_DECAY_PARAM].getValue())),
        bassFiltDecayFromKnob(clamp01(params[BASS_FILT_DECAY_PARAM].getValue())),
        clamp01(params[BASS_TONE_PARAM].getValue()),
        clamp01(params[MORPH_PARAM].getValue()),
        0.f, 0,
        (int)std::floor(clampF(params[KICK_TAILMODE_PARAM].getValue(), 0.f, 2.999f)),
        (int)std::floor(clampF(params[BASS_CHAR_PARAM].getValue(), 0.f, 2.999f)),
        true);
}

void primeSmoothers() {
    smooth[MOD_BASE].set(clamp01(params[BASE_PARAM].getValue()));
    smooth[MOD_SWEEP].set(clamp01(params[SWEEP_PARAM].getValue() * (1.f / 36.f)));
    smooth[MOD_CURVE].set(clamp01(params[CURVE_PARAM].getValue()));
    smooth[MOD_DECAY].set(clamp01(params[DECAY_PARAM].getValue()));
    smooth[MOD_PUNCH].set(clamp01(params[PUNCH_PARAM].getValue()));
    smooth[MOD_CLICK].set(clamp01(params[CLICK_PARAM].getValue()));
    smooth[MOD_CLICK_TONE].set(clamp01(params[CLICK_TONE_PARAM].getValue()));
    smooth[MOD_BODY].set(clamp01(params[BODY_PARAM].getValue()));
    smooth[MOD_TAIL].set(clamp01(params[TAIL_PARAM].getValue()));
    smooth[MOD_SHAPE].set(clamp01(params[SHAPE_PARAM].getValue()));
    smooth[MOD_MORPH].set(clamp01(params[MORPH_PARAM].getValue()));
    smooth[MOD_DRIVE].set(clamp01(params[DRIVE_PARAM].getValue()));
    smooth[MOD_FOLD].set(clamp01(params[FOLD_PARAM].getValue()));
    smooth[MOD_DARK].set(clamp01(params[DARK_PARAM].getValue()));
    smooth[MOD_FOREST].set(clamp01(params[FOREST_PARAM].getValue()));
    smooth[MOD_PUNCH_CLICK].set(clamp01(params[PUNCH_CLICK_PARAM].getValue()));
    smooth[MOD_BASS_LEVEL].set(clamp01(params[BASS_LEVEL_PARAM].getValue()));
    smooth[MOD_BASS_TONE].set(clamp01(params[BASS_TONE_PARAM].getValue()));
    smooth[MOD_BASS_DECAY].set(clamp01(params[BASS_DECAY_PARAM].getValue()));
    smooth[MOD_BASS_FILT_DECAY].set(clamp01(params[BASS_FILT_DECAY_PARAM].getValue()));
    smooth[MOD_BASS_TRACK].set(clamp01(params[BASS_TRACK_PARAM].getValue()));
    smooth[MOD_BASS_SWEEP].set(0.f);
    smooth[MOD_ACCENT].set(clamp01(params[ACCENT_PARAM].getValue()));
    smooth[MOD_LEVEL].set(clamp01(params[LEVEL_PARAM].getValue() * 0.5f));
    smooth[MOD_TAILMODE].set(clamp01(params[KICK_TAILMODE_PARAM].getValue() * 0.5f));
    smooth[MOD_BASS_CHAR].set(clamp01(params[BASS_CHAR_PARAM].getValue() * 0.5f));
    freqSm = clampF(baseHzFromKnob(smooth[MOD_BASE].value), BODY_FREQ_MIN, BODY_FREQ_MAX);
}

void updateCoeffs(float pitchDecay, float bodyTau, float curve,
    float clickTone, float dark, float drive, float fold,
    float bassDecay, float bassFiltDecay,
    float bassSaw, float bassMorph,
    float bassSweepSt, int bassSweepStep,
    int tailMode, int bassChar, bool force) {

    pitchDecay = clampF(safeF(pitchDecay, 0.09f), 0.002f, 1.0f);

    if (tailMode == 0) {
        bodyTau = safeF(bodyTau * 0.45f, bodyTau);
    }
    bodyTau = clampF(safeF(bodyTau, 0.02f), 0.010f, 0.600f);
    curve = clamp01(safeF(curve));
    clickTone = clampF(safeF(clickTone, 3500.f), 1200.f, clickToneMax);
    dark = clamp01(safeF(dark));
    drive = clamp01(safeF(drive));
    fold = clamp01(safeF(fold));
    bassDecay = clampF(safeF(bassDecay, 0.045f), 0.005f, 0.2f);
    bassFiltDecay = clampF(safeF(bassFiltDecay, 0.020f), 0.003f, 0.15f);
    bassSaw = clamp01(safeF(bassSaw));
    bassMorph = clamp01(safeF(bassMorph));
    bassSweepSt = clampF(safeF(bassSweepSt, 0.f), 0.f, 7.f);

    if (!force
        && std::fabs(pitchDecay - cachePitchDecay) < 5e-5f
        && std::fabs(bodyTau - cacheBodyTau) < 5e-5f
        && std::fabs(curve - cacheCurve) < 1e-4f
        && std::fabs(clickTone - cacheClickTone) < 2.f
        && std::fabs(dark - cacheDark) < 1e-4f
        && std::fabs(drive - cacheDrive) < 1e-4f
        && std::fabs(fold - cacheFold) < 1e-4f
        && std::fabs(bassDecay - cacheBassDecay) < 5e-5f
        && std::fabs(bassFiltDecay - cacheBassFiltDecay) < 5e-5f
        && std::fabs(bassSaw - cacheBassSaw) < 1e-4f
        && std::fabs(bassMorph - cacheBassMorph) < 1e-4f
        && std::fabs(bassSweepSt - cacheBassSweepSt) < 1e-3f
        && bassSweepStep == cacheBassSweepStep
        && tailMode == cacheTailMode
        && bassChar == cacheBassChar)
        return;

    cachePitchDecay = pitchDecay;
    cacheBodyTau = bodyTau;
    cacheCurve = curve;
    cacheClickTone = clickTone;
    cacheDark = dark;
    cacheDrive = drive;
    cacheFold = fold;
    cacheBassDecay = bassDecay;
    cacheBassFiltDecay = bassFiltDecay;
    cacheBassSweepSt = bassSweepSt;
    cacheBassSweepStep = bassSweepStep;
    cacheTailMode = tailMode;
    cacheBassChar = bassChar;

    const float glideSharp = (tailMode == 2) ? 0.55f : 1.f;
    const float tauF = pitchDecay * (0.10f + 0.06f * (1.f - curve)) * glideSharp;
    coeffPitchF = std::exp(-dt / tauF);
    coeffPitchS = std::exp(-dt / (pitchDecay * 1.15f * glideSharp));
    coeffPunchSweep = std::exp(-dt / (pitchDecay * 0.45f));

    coeffBody = std::exp(-dt / bodyTau);
    coeffTail = std::exp(-dt / (bodyTau * 2.6f * (1.f - 0.45f * dark)));
    coeffCont = std::exp(-dt / (bodyTau * 0.5f));
    coeffPunch = std::exp(-dt / (bodyTau * 0.30f));

    const float atkKick = lerpF(0.0010f, 0.00035f, dark);
    attackStepKick = dt / atkKick;
    attackStepPunch = dt / (atkKick * 0.45f);

    const float toneSoft = clampF(std::sqrt(3500.f / clickTone), 0.5f, 2.2f);
    const float atkClick = clampF(0.00012f * toneSoft * (1.f - 0.40f * dark), 2e-5f, 0.01f);
    attackStepClick = dt / atkClick;
    coeffClickChirp = std::exp(-dt / 0.0005f);
    const float tauClick = clampF(0.00035f * std::pow(3500.f / clickTone, 0.3f)
        * (1.f - 0.35f * dark), 2e-5f, 0.01f);
    coeffClick = std::exp(-dt / tauClick);

    gainDrive = 1.f + 3.f * drive + 0.6f * dark;
    satAsym = 1.02f - 0.12f * dark;
    clickHarm3 = 0.30f * dark * ramp01(clickTone, clickToneMax, clickToneMax * 0.85f);

    float charSawBase = 0.f;
    float charMorphBase = 0.f;
    float bassBaseHz = 250.f;
    float bassDepthHz = 200.f;
    if (bassChar == 0) {
        charSawBase = 0.05f;
        charMorphBase = 0.10f;
        bassBaseHz = 250.f;
        bassDepthHz = 120.f;
    }
    else if (bassChar == 2) {
        charSawBase = 0.65f;
        charMorphBase = 0.75f;
        bassBaseHz = 800.f;
        bassDepthHz = 500.f;
    }
    else {
        charSawBase = 0.35f;
        charMorphBase = 0.45f;
        bassBaseHz = 400.f;
        bassDepthHz = 300.f;
    }

    const float finalSaw = clamp01(0.70f * bassSaw + 0.30f * charSawBase);
    const float finalMorph = clamp01(0.75f * bassMorph + 0.25f * charMorphBase);

    bassVoice.sawMix = finalSaw;
    bassVoice.morphAmt = finalMorph;
    bassVoice.setCoeffs(sampleRate, bassDecay, bassFiltDecay, bassBaseHz, bassDepthHz);
    bassVoice.setChar(bassChar, sampleRate);
    bassVoice.atkCoeff = bassAtkCoeff;
    bassVoice.bpSweepSt = bassSweepSt;
    bassVoice.bpEnvCoeff = std::exp(-dt / clampF(pitchDecay * 0.30f, 0.002f, 0.5f));

    cacheBassSaw = finalSaw;
    cacheBassMorph = finalMorph;
}

// -------------------------------------------------------------------------
//  Parameter stage
// -------------------------------------------------------------------------

float evalParam(int target, int inputId, float cvPerVolt, float rawNorm) {
    const float base = clamp01(safeF(rawNorm));
    float cv = 0.f;
    if (inputId >= 0 && inputs[inputId].isConnected())
        cv = clampF(safeF(inputs[inputId].getVoltage()) * cvPerVolt, -1.f, 1.f);
    return smooth[target].process(clamp01(base + cv));
}

void updateParameters(KickParams& p) {
    const float baseN      = evalParam(MOD_BASE, -1, 0.f, params[BASE_PARAM].getValue());
    const float sweepN     = evalParam(MOD_SWEEP, SWEEP_CV_INPUT, 10.f * (1.f / 36.f),
                                       params[SWEEP_PARAM].getValue() * (1.f / 36.f));
    const float curveN     = evalParam(MOD_CURVE, -1, 0.f, params[CURVE_PARAM].getValue());
    const float decayN     = evalParam(MOD_DECAY, DECAY_CV_INPUT, 0.20f, params[DECAY_PARAM].getValue());
    const float punchN     = evalParam(MOD_PUNCH, -1, 0.f, params[PUNCH_PARAM].getValue());
    const float clickN     = evalParam(MOD_CLICK, -1, 0.f, params[CLICK_PARAM].getValue());
    const float toneN      = evalParam(MOD_CLICK_TONE, -1, 0.f, params[CLICK_TONE_PARAM].getValue());
    const float bodyN      = evalParam(MOD_BODY, -1, 0.f, params[BODY_PARAM].getValue());
    const float tailN      = evalParam(MOD_TAIL, -1, 0.f, params[TAIL_PARAM].getValue());
    const float shapeN     = evalParam(MOD_SHAPE, -1, 0.f, params[SHAPE_PARAM].getValue());
    const float morphN     = evalParam(MOD_MORPH, MORPH_CV_INPUT, 0.20f, params[MORPH_PARAM].getValue());
    const float driveN     = evalParam(MOD_DRIVE, DRIVE_CV_INPUT, 0.20f, params[DRIVE_PARAM].getValue());
    const float foldN      = evalParam(MOD_FOLD, -1, 0.f, params[FOLD_PARAM].getValue());
    const float darkN      = evalParam(MOD_DARK, -1, 0.f, params[DARK_PARAM].getValue());
    const float forestN    = evalParam(MOD_FOREST, -1, 0.f, params[FOREST_PARAM].getValue());
    const float pclickN    = evalParam(MOD_PUNCH_CLICK, -1, 0.f, params[PUNCH_CLICK_PARAM].getValue());
    const float bassN      = evalParam(MOD_BASS_LEVEL, BASS_CV_INPUT, 0.20f, params[BASS_LEVEL_PARAM].getValue());
    const float sawN       = evalParam(MOD_BASS_TONE, -1, 0.f, params[BASS_TONE_PARAM].getValue());
    const float bassDecayN = evalParam(MOD_BASS_DECAY, -1, 0.f, params[BASS_DECAY_PARAM].getValue());
    const float bassFiltDecayN = evalParam(MOD_BASS_FILT_DECAY, -1, 0.f,
                                           params[BASS_FILT_DECAY_PARAM].getValue());
    const float accentN2   = evalParam(MOD_ACCENT, -1, 0.f, params[ACCENT_PARAM].getValue());
    const float levelN     = evalParam(MOD_LEVEL, -1, 0.f, params[LEVEL_PARAM].getValue() * 0.5f);
    const float tailModeN  = evalParam(MOD_TAILMODE, -1, 0.f,
                                       params[KICK_TAILMODE_PARAM].getValue() * 0.5f);
    const float bassCharN  = evalParam(MOD_BASS_CHAR, -1, 0.f,
                                       params[BASS_CHAR_PARAM].getValue() * 0.5f);

    float voctV = 0.f;
    if (inputs[VOCT_INPUT].isConnected())
        voctV = clampF(safeF(inputs[VOCT_INPUT].getVoltage()), -10.f, 10.f);

    float pitchCvV = 0.f;
    if (inputs[PITCH_CV_INPUT].isConnected())
        pitchCvV = clampF(safeF(inputs[PITCH_CV_INPUT].getVoltage()), -10.f, 10.f);

    const float voctTarget    = clampF(voctV * 12.f, -60.f, 60.f);
    const float pitchCvTarget = clampF(pitchCvV * 12.f, -24.f, 24.f);

    pitchCvSm      += coeffPitchCv * (voctTarget    - pitchCvSm);
    pitchKickCvSm  += coeffPitchCv * (pitchCvTarget - pitchKickCvSm);
    if (!std::isfinite(pitchCvSm))     pitchCvSm     = voctTarget;
    if (!std::isfinite(pitchKickCvSm)) pitchKickCvSm = pitchCvTarget;

    const float bassTrackKnobN = clamp01(params[BASS_TRACK_PARAM].getValue());
    float bassTrackCv = 0.f;
    if (inputs[BASS_TRACK_CV_INPUT].isConnected())
        bassTrackCv = clampF(safeF(inputs[BASS_TRACK_CV_INPUT].getVoltage()) * 0.20f, -1.f, 1.f);
    const float bassTrackN = clamp01(bassTrackKnobN + bassTrackCv);

    const int rootPc = ((int)std::lround(clampF(params[ROOT_PARAM].getValue(), 0.f, 11.f))) % 12;
    p.rootPc = (rootPc < 0) ? rootPc + 12 : rootPc;

    if (std::fabs(baseN - cacheBaseN) > 1e-5f) {
        cacheBaseN = baseN;
        cacheBaseHz = clampF(baseHzFromKnob(baseN), BODY_FREQ_MIN, BODY_FREQ_MAX);
    }
    p.baseHz = cacheBaseHz;

    p.sweepSt        = 36.f * sweepN;
    p.curve          = curveN;
    p.pitchDecay     = pitchDecayFromKnob(decayN);
    p.punch          = punchN;
    p.click          = clickN;
    p.clickTone      = clampF(clickToneFromKnob(toneN), 1200.f, clickToneMax);
    p.body           = bodyN;
    p.tailTau        = bodyTauFromKnob(tailN);
    p.shape          = shapeN;
    p.morph          = morphN;
    p.drive          = driveN;
    p.fold           = foldN;
    p.dark           = darkN;
    p.forest         = forestN;
    p.punchClick     = pclickN;
    p.bassLevel      = bassN;
    p.bassSaw        = sawN;
    p.bassMorph      = morphN;
    p.bassDecay      = bassDecayFromKnob(bassDecayN);
    p.bassFiltDecay  = bassFiltDecayFromKnob(bassFiltDecayN);
    p.bassTrackRatio = bassTrackRatioFromKnob(bassTrackN);

    p.bassSweepStep = bassSweepStepFromKnob(params[BASS_SWEEP_PARAM].getValue());
    p.bassSweepSt   = BASS_SWEEP_SEMIS[p.bassSweepStep];

    p.tailMode = (int)std::floor(clampF(tailModeN * 2.f, 0.f, 2.999f));
    if (p.tailMode < 0) p.tailMode = 0;
    if (p.tailMode > 2) p.tailMode = 2;

    p.bassChar = (int)std::floor(clampF(bassCharN * 2.f, 0.f, 2.999f));
    if (p.bassChar < 0) p.bassChar = 0;
    if (p.bassChar > 2) p.bassChar = 2;

    p.accentDepth  = accentN2;
    p.level        = levelN;

    p.voctSemis    = pitchCvSm;
    p.pitchCvSemis = pitchKickCvSm;
    p.rootSemis    = (float)p.rootPc;

    dispBaseHz     = p.baseHz;
    dispRootPc     = (float)p.rootPc;
    dispVoctSemis  = p.voctSemis + p.rootSemis;
    dispBassSaw    = bassVoice.sawActualRatio;
    dispBassMorph  = p.bassMorph;
}

// -------------------------------------------------------------------------
//  Capture helpers
// -------------------------------------------------------------------------

void startCapture() {
    capturePos = 0;
    captureBucketAcc = 0;
    captureKickSeen = false;
    kAccum = 0.f;
    bAccum = 0.f;
    kAccumCount = 0;

    const float periodSec = (havePeriod && periodSamples > 10.f)
        ? safeF(periodSamples / sampleRate, DEFAULT_PERIOD_SEC)
        : DEFAULT_PERIOD_SEC;
    const float targetSec = clampF(periodSec, 0.020f, TRACE_MAX_SEC);

    int factor = (int)std::lround((targetSec * sampleRate) / (float)TRACE_N);
    if (factor < 1) factor = 1;
    captureBucketSamples = factor;

    int tlen = (int)std::lround((targetSec * sampleRate) / (float)factor);
    if (tlen < 32) tlen = 32;
    if (tlen > TRACE_N) tlen = TRACE_N;
    captureTargetLen = tlen;

    for (int i = 0; i < TRACE_N; ++i) {
        capKickRaw[i] = 0.f;
        capBassRaw[i] = 0.f;
        capBassEnv[i] = 0.f;
        capPitch[i] = 0.f;
        capEnv[i] = 0.f;
    }
    captureActive = true;
}

void writeCapture(float kickOut, float bassOut, bool trigHit) {
    if (!captureActive) return;

    kAccum += safeF(kickOut, 0.f);
    bAccum += safeF(bassOut, 0.f);
    kAccumCount++;

    const int i = capturePos;
    if (i >= 0 && i < TRACE_N) {
        capEnv[i] = clampF(safeF(envBody.value, 0.f), 0.f, 1.f);
        capBassEnv[i] = clampF(safeF(bassVoice.ampEnv, 0.f), 0.f, 1.f);
        const float pitchNorm = (dispSweep > 1e-4f)
            ? clamp01(safeF(dispSemis / dispSweep, 0.f))
            : 0.f;
        capPitch[i] = pitchNorm;
        (void)trigHit;
    }

    if (++captureBucketAcc >= captureBucketSamples) {
        captureBucketAcc = 0;
        const float inv = (kAccumCount > 0) ? (1.f / (float)kAccumCount) : 1.f;
        if (i >= 0 && i < TRACE_N) {
            capKickRaw[i] = safeF(kAccum * inv, 0.f);
            capBassRaw[i] = safeF(bAccum * inv, 0.f);
        }
        kAccum = 0.f;
        bAccum = 0.f;
        kAccumCount = 0;

        capturePos++;
        if (capturePos >= captureTargetLen) {
            for (int j = 0; j < TRACE_N; ++j) {
                dispKickRaw[j] = capKickRaw[j];
                dispBassRaw[j] = capBassRaw[j];
                dispBassEnv[j] = capBassEnv[j];
                dispPitch[j] = capPitch[j];
                dispEnv[j] = capEnv[j];
            }
            captureLen = captureTargetLen;
            captureValid = true;
            captureActive = false;
            capturePos = 0;
        }
    }
}

// -------------------------------------------------------------------------
//  Trigger — KICK ONLY on the downbeat; bass is force-muted.
// -------------------------------------------------------------------------

void startKick(const KickParams& p) {
    float accentV = 0.f;
    if (inputs[ACCENT_INPUT].isConnected())
        accentV = clampF(safeF(inputs[ACCENT_INPUT].getVoltage()), 0.f, 10.f) * 0.1f;
    accentN = clamp01(accentV) * clamp01(p.accentDepth);

    bodyPhase = 0.f;
    clickPhase = 0.f;
    punchPhase = 0.f;
    envBody.trigger();
    envPunch.trigger();
    envClick.trigger();

    bassVoice.mute();
    slotSched.onTrigger(periodSamples);

    pitchEF = 1.f;
    pitchES = 1.f;
    punchSweepE = 1.f;
    clickChirp = 1.f;
    kickT = 0.f;
    kickActive = true;
    eocArmed = false;
    trigFlash = 1.f;
    samplesSinceTrig = 0;

    punchClickEnv = 1.f;
    punchClickHp = 0.f;
    punchClickBp = 0.f;
    voidSubPhase = 0.f;
    voidSub2Phase = 0.f;
    voidSubSlowEnv = 1.f;
    gnarlLp = 0.f;

    startCapture();
    captureKickSeen = true;
}

inline float nextNoise() {
    if (noiseState == 0) noiseState = NOISE_SEED;
    uint32_t x = noiseState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    noiseState = x;
    float u = (float)(x >> 16) * (1.f / 65535.f);
    return safeF(u * 2.f - 1.f, 0.f);
}

// -------------------------------------------------------------------------
//  FOREST core + VOID macro
// -------------------------------------------------------------------------

inline float forestClip(float x) const {
    const float y = safeF(x);
    if (y >= 0.f) {
        return safeF(y / (1.f + 0.45f * y), 0.f);
    }
    return safeF(y / (1.f + 0.85f * (-y)), 0.f);
}

inline float forestFold(float x, float foldAmt) const {
    const float g1 = 1.f + 1.6f * foldAmt;
    const float g2 = 1.4f + 1.6f * foldAmt;
    const float g3 = 1.8f + 1.6f * foldAmt;
    float y = safeF(fastTanh(g1 * safeF(x, 0.f)), 0.f);
    y = safeF(fastTanh(g2 * y), 0.f);
    y = safeF(fastTanh(g3 * y) * 0.75f, 0.f);
    return y;
}

inline float forestStage(float x, const KickParams& p, bool updateFb) {
    if (p.drive <= 1e-5f && p.fold <= 1e-5f && p.forest <= 1e-5f) {
        return safeF(x);
    }
    float y = safeF(x);

    if (p.drive > 1e-5f) {
        const float sy = gainDrive * y;
        const float sat = satAsym * sy / (satAsym + std::fabs(sy));
        y = lerpF(y, sat, p.drive);
    }

    const float f = clamp01(safeF(p.forest, 0.f));
    float clipAmt = 1.f - 2.f * f;
    if (clipAmt < 0.f) clipAmt = 0.f;
    float pdAmt = 2.f * f - 1.f;
    if (pdAmt < 0.f) pdAmt = 0.f;
    float d = f - 0.5f;
    if (d < 0.f) d = -d;
    float foldAmt = 1.f - d * 2.f;
    foldAmt = clamp01(foldAmt);

    float sum = clipAmt + foldAmt + pdAmt;
    if (!(sum > 1e-6f)) sum = 1.f;
    clipAmt /= sum;
    foldAmt /= sum;
    pdAmt /= sum;

    float out = 0.f;
    if (clipAmt > 1e-5f) {
        out += clipAmt * forestClip(y);
    }
    if (foldAmt > 1e-5f) {
        const float fg = 0.5f + 0.5f * clamp01(p.fold + 0.5f * f);
        out += foldAmt * forestFold(y, fg);
    }
    if (pdAmt > 1e-5f) {
        const float pdIn = safeF(y + 0.4f * forestFb, 0.f);
        const float pd = safeF(fastTanh(2.2f * pdIn) * 0.9f, 0.f);
        if (updateFb) forestFb = safeF(pd, 0.f);
        out += pdAmt * pd;
    }
    else if (updateFb) {
        forestFb = safeF(forestFb * 0.999f, 0.f);
    }

    if (p.fold > 1e-5f && f < 0.05f) {
        const float fg = 1.f + 3.f * p.fold;
        const float folded = (2.f / PI_F) * std::sin((PI_F * 0.5f) * y * fg);
        out = lerpF(out, folded * (1.f / (1.f + 0.30f * (fg - 1.f))),
            p.fold * (1.f - f * 10.f) * 0.5f);
    }
    return safeF(out, 0.f);
}

inline float voidMacro(float shaped, float kickFreq, float ampEnv, const KickParams& p) {
    const float v = clamp01(safeF(p.dark, 0.f));
    if (v <= 1e-5f) return safeF(shaped);

    const float s = safeF(shaped, 0.f);
    voidLp80 = safeF(voidLp80 + coeffVoid80 * (s - voidLp80), 0.f);
    voidLp8k = safeF(voidLp8k + coeffVoid8k * (s - voidLp8k), 0.f);
    voidMidLp = safeF(voidMidLp + coeffVoidMid * (s - voidMidLp), 0.f);

    const float lowBoost = safeF(voidLp80 * 1.0f, 0.f);
    const float midBand = safeF(voidMidLp - voidLp80, 0.f);
    const float highs = safeF(s - voidLp8k, 0.f);
    float y = safeF(s + v * (lowBoost - 0.37f * midBand - 0.5f * highs), 0.f);

    const float kf = clampF(safeF(kickFreq, 50.f), BODY_FREQ_MIN, BODY_FREQ_MAX);
    voidSubPhase = safeF(voidSubPhase + 0.5f * kf * dt, 0.f);
    if (voidSubPhase >= 1.f || voidSubPhase < 0.f) {
        voidSubPhase -= std::floor(voidSubPhase);
    }
    const float gate = clamp01(safeF(ampEnv, 0.f));
    const float sub = safeF(std::sin(TWO_PI_F * voidSubPhase) * 0.15f * v * gate, 0.f);
    y = safeF(y + sub, 0.f);

    voidSub2Phase = safeF(voidSub2Phase + 0.25f * kf * dt, 0.f);
    if (voidSub2Phase >= 1.f || voidSub2Phase < 0.f) {
        voidSub2Phase -= std::floor(voidSub2Phase);
    }
    voidSubSlowEnv = safeF(voidSubSlowEnv * coeffVoidSubSlow, 0.f);
    const float sub2 = safeF(std::sin(TWO_PI_F * voidSub2Phase) * 0.08f * v * voidSubSlowEnv, 0.f);
    y = safeF(y + sub2, 0.f);

    return y;
}

// -------------------------------------------------------------------------
//  Audio thread entry point
// -------------------------------------------------------------------------

void process(const ProcessArgs& args) override {
    const float sr = clampF(safeF(args.sampleRate, sampleRate), SR_MIN, SR_MAX);
    if (std::fabs(sr - sampleRate) > 0.5f) {
        sampleRate = sr;
        updateRates();
    }
    dt = clampF(safeF(args.sampleTime, dt), 1.f / SR_MAX, 1.f / SR_MIN);

    KickParams p;
    updateParameters(p);
    updateCoeffs(p.pitchDecay, p.tailTau, p.curve, p.clickTone, p.dark,
        p.drive, p.fold, p.bassDecay, p.bassFiltDecay,
        p.bassSaw, p.bassMorph,
        p.bassSweepSt, p.bassSweepStep,
        p.tailMode, p.bassChar, false);

    const float trigV = inputs[TRIG_INPUT].isConnected()
        ? clampF(safeF(inputs[TRIG_INPUT].getVoltage()), -12.f, 12.f)
        : 0.f;
    const bool trig = trigDetector.process(trigV, 0.12f, 0.5f);

    const float timeoutSamp = 2.f * DEFAULT_PERIOD_SEC * sampleRate;
    if (!trig && samplesSinceTrig > timeoutSamp) {
        trigCount = 0;
        havePeriod = false;
        periodValid = false;
        samplesSinceTrig = 0;
    }

    if (trig) {
        if (trigCount == 0) {
            periodSamples = safeF(DEFAULT_PERIOD_SEC * sampleRate, 24000.f);
            havePeriod = false;
            periodValid = false;
            trigCount = 1;
        }
        else {
            const float raw = safeF((float)samplesSinceTrig, 0.f);
            const float m = clampF(raw,
                ROLL_PERIOD_MIN_SEC * sampleRate, ROLL_PERIOD_MAX_SEC * sampleRate);
            if (raw >= ROLL_PERIOD_MIN_SEC * sampleRate) {
                if (havePeriod) {
                    periodSamples = safeF(0.5f * periodSamples + 0.5f * m, m);
                }
                else {
                    periodSamples = safeF(m, periodSamples);
                }
                havePeriod = true;
                periodValid = true;
                if (trigCount < 1000) trigCount++;
            }
            else {
                if (trigCount < 1000) trigCount++;
            }
        }
        startKick(p);
        triggerPending = true;
    }
    else if (samplesSinceTrig < 1000000000) {
        samplesSinceTrig++;
    }

    const bool voiceIdle = envBody.idle() && envPunch.idle() && envClick.idle()
        && bassVoice.idle();

    if (kickActive && voiceIdle) {
        kickActive = false;
        envBody.reset();
        envPunch.reset();
        envClick.reset();
        pitchEF = 0.f;
        pitchES = 0.f;
        punchSweepE = 0.f;
        clickChirp = 0.f;
        os.reset();
        morphShape.reset();
        darkTiltLp = 0.f;
        forestFb = 0.f;
        voidLp80 = 0.f;
        voidLp8k = 0.f;
        voidMidLp = 0.f;
        punchClickHp = 0.f;
        punchClickBp = 0.f;
        punchClickEnv = 0.f;
        gnarlLp = 0.f;
        bassLp.reset();
        bassVoice.reset();
        dcXK = dcYK = dcXB = dcYB = 0.f;
        sigLevel = 0.f;
    }

    if (!kickActive && !trig && voiceIdle) {
        outputs[KICK_OUTPUT].setVoltage(0.f);
        outputs[BASS_OUTPUT].setVoltage(0.f);
        outputs[ENV_OUTPUT].setVoltage(clampF(envBody.value, 0.f, 1.f) * 10.f);
        outputs[EOC_OUTPUT].setVoltage(eocPulse.process(dt) ? 10.f : 0.f);
        dispSemis = 0.f;
        dispSweep = 1.f;
        dispVoctSemis = p.voctSemis + p.rootSemis;
        updateDisplay(p, 0.f, 0.f, trig);
        return;
    }

    const float VS = 0.52f;

    pitchEF = safeF(pitchEF * coeffPitchF, 0.f);
    pitchES = safeF(pitchES * coeffPitchS, 0.f);
    punchSweepE = safeF(punchSweepE * coeffPunchSweep, 0.f);
    clickChirp = safeF(clickChirp * coeffClickChirp, 0.f);

    const float sweepEff = safeF(p.sweepSt * (1.f + 0.35f * accentN), 0.f);
    const float semis = safeF(sweepEff * ((1.f - 0.9f * p.curve) * pitchEF + p.curve * pitchES), 0.f);
    dispSemis = safeF(semis, 0.f);
    dispSweep = (sweepEff > 1.f) ? sweepEff : 1.f;

    const float kickTranspose = p.voctSemis + p.pitchCvSemis + p.rootSemis;
    const float pitchRefHz = safeF(
        p.baseHz * std::exp((semis + kickTranspose) * LN2_12), p.baseHz);
    const float freqTarget = clampF(pitchRefHz, BODY_FREQ_MIN, BODY_FREQ_MAX);
    freqSm = safeF(freqTarget + coeffFreqSlew * (freqSm - freqTarget), freqTarget);
    if (!std::isfinite(freqSm)) freqSm = freqTarget;

    envBody.riseStep = attackStepKick;
    envBody.contCoeff = coeffCont;
    envBody.coeff = (envBody.body > 0.12f) ? coeffBody : coeffTail;
    envPunch.riseStep = attackStepPunch;
    envPunch.contCoeff = coeffCont;
    envPunch.coeff = coeffPunch;
    envClick.riseStep = attackStepClick;
    envClick.contCoeff = coeffCont;
    envClick.coeff = coeffClick;

    const float bodyAmp = safeF(envBody.process(), 0.f);
    const float punchAmp = safeF(envPunch.process(), 0.f);
    const float clickAmp = safeF(envClick.process(), 0.f);
    kickT = safeF(kickT + dt, 0.f);

    if (triggerPending) {
        triggerPending = false;
    }
    else if (trigCount >= 2 && havePeriod && periodValid
        && periodSamples >= ROLL_PERIOD_MIN_SEC * sampleRate) {
        if (slotSched.process(samplesSinceTrig)) {
            bassVoice.trigger();
        }
    }

    bodyPhase = safeF(bodyPhase + freqSm * dt, 0.f);
    if (bodyPhase >= 1.f || bodyPhase < 0.f)
        bodyPhase -= std::floor(bodyPhase);
    bodyPhase = safeF(bodyPhase, 0.f);
    const float bodySine = safeF(std::sin(TWO_PI_F * bodyPhase), 0.f);

    morphShape.update(clamp01(safeF(p.morph + 0.12f * p.dark, 0.f)), p.shape);
    const float shapedBody = safeF(morphShape.process(bodySine), 0.f);

    const float punchSemis = safeF((19.f + 6.f * p.dark) * punchSweepE, 0.f);
    const float punchFreq = clampF(safeF(p.baseHz * std::exp(punchSemis * LN2_12), 55.f),
        BODY_FREQ_MIN, 4000.f);
    punchPhase = safeF(punchPhase + punchFreq * dt, 0.f);
    if (punchPhase >= 1.f || punchPhase < 0.f)
        punchPhase -= std::floor(punchPhase);
    punchPhase = safeF(punchPhase, 0.f);
    const float punchSine = safeF(std::sin(TWO_PI_F * punchPhase), 0.f);

    const float clickFreq = clampF(p.clickTone * lerpF(0.72f, 1.f, clickChirp), 200.f, clickToneMax);
    clickPhase = safeF(clickPhase + clickFreq * dt, 0.f);
    if (clickPhase >= 1.f || clickPhase < 0.f)
        clickPhase -= std::floor(clickPhase);
    const float clickSine = std::sin(TWO_PI_F * clickPhase);
    float clickWave = safeF(clickSine
        + clickHarm3 * (4.f * clickSine * clickSine * clickSine - 3.f * clickSine), 0.f);

    punchClickEnv = safeF(punchClickEnv * coeffPunchClickEnv, 0.f);
    float spit = 0.f;
    if (p.punchClick > 1e-4f && punchClickEnv > 1e-4f) {
        const float w = nextNoise();
        punchClickHp = safeF(punchClickHp + coeffPunchClickHp * (w - punchClickHp), 0.f);
        const float hp = safeF(w - punchClickHp, 0.f);
        punchClickBp = safeF(punchClickBp + coeffPunchClickBp * (w - punchClickBp), 0.f);
        const float bp = safeF(w - punchClickBp, 0.f);
        spit = safeF((hp + 0.4f * bp) * punchClickEnv * p.punchClick, 0.f);
    }
    else {
        punchClickHp = safeF(punchClickHp * 0.999f, 0.f);
        punchClickBp = safeF(punchClickBp * 0.999f, 0.f);
    }
    clickWave = safeF(clickWave + 1.2f * spit, 0.f);

    {
        const float gMode = (p.tailMode == 0) ? 0.f : ((p.tailMode == 2) ? 0.5f : 1.f);
        if (gMode > 1e-4f) {
            const float gFreq = clampF(350.f * std::exp(semis * 0.35f * LN2_12), 120.f, 2400.f);
            const float gCoeff = std::exp(-TWO_PI_F * gFreq * dt);
            gnarlLp = safeF(gnarlLp + gCoeff * (clickWave - gnarlLp), 0.f);
            const float gnarlHp = safeF(clickWave - gnarlLp, 0.f);
            clickWave = safeF(clickWave + gMode * 0.55f * gnarlHp, 0.f);
        }
    }

    const float bassTranspose = p.voctSemis + p.rootSemis;
    const float bassBase = safeF(
        p.baseHz * std::exp(bassTranspose * LN2_12), p.baseHz);
    const float bassFreqRaw = safeF(bassBase * p.bassTrackRatio, 55.f);
    const float bassFreq = clampF(bassFreqRaw, BASS_FREQ_MIN, BASS_FREQ_MAX);
    dispBassHz = safeF(bassFreq, 55.f);
    dispBassSemis = safeF(bassTranspose
        + 12.f * std::log(p.bassTrackRatio) / std::log(2.f), 0.f);

    bassVoice.level = safeF(p.bassLevel * 0.85f, 0.6f);
    const float bassSignal = safeF(bassVoice.process(bassFreq, dt, p.bassMorph), 0.f);

    const float accentBody = safeF(1.f + 0.15f * accentN, 1.f);
    const float accentClick = safeF(1.f + 0.40f * accentN, 1.f);

    const float bodySig = safeF(shapedBody * bodyAmp * p.body * VS * accentBody, 0.f);
    const float punchSig = safeF(punchSine * punchAmp * p.punch * 0.45f * VS, 0.f);
    const float clickSig = safeF(clickWave * clickAmp * p.click * 0.85f * VS * accentClick, 0.f);

    const float kickSrc = safeF(bodySig + punchSig + clickSig, 0.f);

    float yEarly = 0.f;
    float yLate = 0.f;
    os.split(kickSrc, yEarly, yLate);
    const float vEarly = forestStage(yEarly, p, false);
    const float vLate = forestStage(yLate, p, true);
    float shaped = safeF(os.merge(vEarly, vLate), 0.f);

    darkTiltLp = safeF(darkTiltLp + coeffTilt * (shaped - darkTiltLp), 0.f);
    shaped = safeF(shaped + p.dark * 0.12f * (shaped - darkTiltLp), shaped);
    shaped = voidMacro(shaped, freqSm, bodyAmp, p);

    const float level = safeF(1.5f * p.level, 1.5f);
    dcYK = safeF(shaped - dcXK + dcRk * dcYK, 0.f);
    dcXK = safeF(shaped, 0.f);
    float kickOut = safeF(dcYK, 0.f);
    kickOut = clampF(softLimit(safeF(kickOut * level, 0.f)), -1.5f, 1.5f);
    kickOut = safeF(kickOut, 0.f);

    float bass = safeF(bassSignal, 0.f);
    if (p.drive > 1e-5f) {
        const float bg = safeF(1.f + 1.2f * p.drive, 1.f);
        const float by = safeF(bg * bass, 0.f);
        const float bsat = safeF(1.30f * by / (1.30f + std::fabs(by)), 0.f);
        bass = safeF(lerpF(bass, bsat, 0.5f * p.drive), 0.f);
    }

    const float bassPre = safeF(bass, 0.f);
    dcYB = safeF(bassPre - dcXB + dcRb * dcYB, 0.f);
    dcXB = safeF(bassPre, 0.f);
    float bassOut = safeF(dcYB, 0.f);

    const float bassOutGain = safeF(level * 1.2f, 1.2f);
    bassOut = clampF(softLimit(safeF(bassOut * bassOutGain, 0.f)), -1.2f, 1.2f);
    bassOut = safeF(bassOut, 0.f);

    outputs[KICK_OUTPUT].setVoltage(safeF(kickOut * 5.f, 0.f));
    outputs[BASS_OUTPUT].setVoltage(safeF(bassOut * 5.f, 0.f));
    outputs[ENV_OUTPUT].setVoltage(clampF(safeF(envBody.value, 0.f), 0.f, 1.f) * 10.f);

    if (!eocArmed && kickActive && envBody.value <= EOC_THRESHOLD) {
        eocArmed = true;
        eocPulse.trigger(EOC_PULSE_SEC);
    }
    outputs[EOC_OUTPUT].setVoltage(eocPulse.process(dt) ? 10.f : 0.f);

    updateDisplay(p, kickOut, bassOut, trig);
}

void updateDisplay(const KickParams& p, float kickOut, float bassOut, bool trigHit) {
    writeCapture(kickOut, bassOut, trigHit);

    if (!kickActive && envBody.value < ENV_IDLE)
        dispStage = STAGE_IDLE;
    else if (envBody.rise < 0.999f)
        dispStage = STAGE_ATTACK;
    else if (envBody.body > 0.12f)
        dispStage = STAGE_BODY;
    else
        dispStage = STAGE_TAIL;

    dispHz = freqSm;
    dispPeriodMs = havePeriod
        ? clampF(periodSamples / sampleRate, 0.02f, 8.f) * 1000.f
        : 500.f;
    dispBpm = (dispPeriodMs > 1.f) ? (60000.f / dispPeriodMs) : 0.f;

    sigLevel += 0.0015f * (std::fabs(kickOut) - sigLevel);
    if (!std::isfinite(sigLevel) || sigLevel < 0.f) sigLevel = 0.f;
    dispLevel = clamp01(sigLevel);

    if (std::fabs(kickOut) > 1.05f) clipFlash = 1.f;
    clipFlash -= dt * 4.f;
    if (clipFlash < 0.f) clipFlash = 0.f;
    dispClip = clipFlash;

    trigFlash -= dt * 6.f;
    if (trigFlash < 0.f) trigFlash = 0.f;

    lights[TRIG_LIGHT].setBrightness(clamp01(trigFlash));
    lights[CLIP_LIGHT].setBrightness(clamp01(clipFlash));
}
};

// -----------------------------------------------------------------------------
//  Panel (v10 — UX-optimized, region-separated)
// -----------------------------------------------------------------------------

struct PsyKickPanel : Widget {
    std::shared_ptr<Svg> panelSvg;

    PsyKickPanel() {
        box.size = mm2px(Vec(91.44f, 128.5f));
    }

    void onAdd(const AddEvent& e) override {
        Widget::onAdd(e);
        if (!panelSvg && APP && APP->window) {
            panelSvg = APP->window->loadSvg(
                asset::plugin(pluginInstance, "res/PsyKick.svg"));
        }
    }

    void text(NVGcontext* vg, float mmX, float mmY, float px,
        int r, int g, int b, int a, const char* s) {
        const Vec p = mm2px(Vec(mmX, mmY));
        nvgFontSize(vg, px);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, nvgRGBA(r, g, b, a));
        nvgText(vg, p.x, p.y, s, nullptr);
    }

    void draw(const DrawArgs& args) override {
        // SVG background (if present)
        if (panelSvg && panelSvg->handle) {
            nvgSave(args.vg);
            const float sx = box.size.x / panelSvg->handle->width;
            const float sy = box.size.y / panelSvg->handle->height;
            nvgScale(args.vg, sx, sy);
            svgDraw(args.vg, panelSvg->handle);
            nvgRestore(args.vg);
        }

        const int lr = 200, lg = 210, lb = 220;
        const int cr = 0,   cg = 200, cb = 225;
        const int gr = 120, gg = 220, gb = 190;
        const int orr = 235, og = 190, ob = 120;

        // Column x positions (must match widget layout)
        const float X0 = 7.0f;
        const float X1 = 18.4f;
        const float X2 = 29.8f;
        const float X3 = 41.2f;
        const float X4 = 52.6f;
        const float X5 = 64.0f;
        const float X6 = 75.4f;
        const float X7 = 86.8f;

        // =========================================================
        // Section headers
        // =========================================================
        text(args.vg, (X0 + X7) * 0.5f, 34.8f, 6.4f, 172, 186, 198, 230, "KICK SOURCE");
        text(args.vg, (X0 + X7) * 0.5f, 49.3f, 6.4f, 172, 186, 198, 230, "KICK PITCH");
        text(args.vg, (X0 + X7) * 0.5f, 63.8f, 6.4f, 172, 186, 198, 230, "KICK BODY");
        text(args.vg, (X0 + X7) * 0.5f, 78.3f, 6.4f, 172, 186, 198, 230, "KICK CHARACTER");
        text(args.vg, (X0 + X7) * 0.5f, 92.8f, 6.4f, 255, 150, 200, 230, "BASS CORE");
        text(args.vg, (X0 + X7) * 0.5f, 107.3f, 6.4f, 255, 150, 200, 230, "BASS CHARACTER");

        // =========================================================
        // Row 1 (y=38.5) — labels at y=43.5
        // =========================================================
        text(args.vg, X0, 43.5f, 5.2f, lr, lg, lb, 255, "V/OCT");
        text(args.vg, X1, 43.5f, 5.2f, lr, lg, lb, 255, "TRIG");
        text(args.vg, X2, 43.5f, 5.2f, lr, lg, lb, 255, "ACCENT");
        text(args.vg, X3, 43.5f, 5.2f, lr, lg, lb, 255, "BASE");
        text(args.vg, X4, 43.5f, 5.2f, lr, lg, lb, 255, "PITCH CV");
        text(args.vg, X5, 43.5f, 5.2f, lr, lg, lb, 255, "SWEEP");
        text(args.vg, X6, 43.5f, 5.2f, lr, lg, lb, 255, "SWEEP CV");
        text(args.vg, X7, 43.5f, 5.2f, lr, lg, lb, 255, "ROOT");

        // =========================================================
        // Row 2 (y=53.0) — labels at y=58.0
        // =========================================================
        text(args.vg, X0, 58.0f, 5.2f, lr, lg, lb, 255, "CURVE");
        text(args.vg, X1, 58.0f, 5.2f, lr, lg, lb, 255, "DECAY");
        text(args.vg, X2, 58.0f, 5.2f, lr, lg, lb, 255, "DECAY CV");
        text(args.vg, X3, 58.0f, 5.2f, lr, lg, lb, 255, "PUNCH");
        text(args.vg, X4, 58.0f, 5.2f, lr, lg, lb, 255, "CLICK");
        text(args.vg, X5, 58.0f, 5.2f, lr, lg, lb, 255, "TONE");
        text(args.vg, X6, 58.0f, 5.2f, lr, lg, lb, 255, "P-CLICK");
        text(args.vg, X7, 58.0f, 5.2f, lr, lg, lb, 255, "TRIG");

        // =========================================================
        // Row 3 (y=67.5) — labels at y=72.5
        // =========================================================
        text(args.vg, X0, 73.0f, 5.2f, lr, lg, lb, 255, "BODY");
        text(args.vg, X1, 73.0f, 5.2f, lr, lg, lb, 255, "TAIL");
        text(args.vg, X2, 73.0f, 5.2f, lr, lg, lb, 255, "TMODE");
        text(args.vg, X3, 73.0f, 5.2f, lr, lg, lb, 255, "SHAPE");
        text(args.vg, X2, 88.0f, 5.2f, lr, lg, lb, 255, "MORPH"); 
        text(args.vg, X3, 88.0f, 5.2f, lr, lg, lb, 255, "MORPH CV");
        text(args.vg, X6, 73.0f, 5.2f, lr, lg, lb, 255, "DRIVE");
        text(args.vg, X7, 73.0f, 5.2f, lr, lg, lb, 255, "DRIVE CV");

        // =========================================================
        // Row 4 (y=82.0) — labels at y=87.0
        // =========================================================
        text(args.vg, X0, 88.0f, 5.2f, lr, lg, lb, 255, "FOLD");
        text(args.vg, X1, 88.0f, 5.2f, lr, lg, lb, 255, "VOID");
        text(args.vg, X4, 73.0f, 5.2f, lr, lg, lb, 255, "FOREST");

        // =========================================================
        // Row 5 (y=96.5) — labels at y=101.5
        // =========================================================
        text(args.vg, X0, 101.5f, 5.2f, lr, lg, lb, 255, "BASS");
        text(args.vg, X1, 101.5f, 5.2f, lr, lg, lb, 255, "BASS CV");
        text(args.vg, X2, 101.5f, 5.2f, lr, lg, lb, 255, "TRACK");
        text(args.vg, X3, 101.5f, 5.2f, lr, lg, lb, 255, "TRACK CV");
        text(args.vg, X4, 101.5f, 5.2f, lr, lg, lb, 255, "SAW");
        text(args.vg, X5, 101.5f, 5.2f, lr, lg, lb, 255, "DECAY");
        text(args.vg, X6, 101.5f, 5.2f, lr, lg, lb, 255, "FILT D");
        text(args.vg, X7, 101.5f, 5.2f, lr, lg, lb, 255, "B-SWP");

        // =========================================================
        // Row 6 (y=111.0) — labels at y=116.0
        // =========================================================
        text(args.vg, X0, 116.0f, 5.2f, lr, lg, lb, 255, "BCHAR");
        text(args.vg, X1, 116.0f, 5.2f, lr, lg, lb, 255, "ACCENT");

        // =========================================================
        // Row 7 (y=122.0) — labels at y=127.0
        // =========================================================
        text(args.vg, X0, 127.0f, 5.2f, lr, lg, lb, 255, "KICK");
        text(args.vg, X1, 127.0f, 5.2f, lr, lg, lb, 255, "BASS");
        text(args.vg, X2, 127.0f, 5.2f, lr, lg, lb, 255, "ENV");
        text(args.vg, X3, 127.0f, 5.2f, lr, lg, lb, 255, "EOC");
        text(args.vg, X4, 127.0f, 5.2f, lr, lg, lb, 255, "LEVEL");
        text(args.vg, X5, 127.0f, 5.2f, lr, lg, lb, 255, "CLIP");

        // =========================================================
        // Type tags (in / cv / out)
        // =========================================================
        // Row 1
        text(args.vg, X0, 36.6f, 4.4f, gr, gg, gb, 200, "in");
        text(args.vg, X1, 36.6f, 4.4f, gr, gg, gb, 200, "in");
        text(args.vg, X2, 36.6f, 4.4f, gr, gg, gb, 200, "in");
        text(args.vg, X4, 36.6f, 4.4f, cr, cg, cb, 180, "cv");
        text(args.vg, X6, 36.6f, 4.4f, cr, cg, cb, 180, "cv");
        // Row 2
        text(args.vg, X2, 51.1f, 4.4f, cr, cg, cb, 180, "cv");
        // Row 3
        text(args.vg, X5, 65.6f, 4.4f, cr, cg, cb, 180, "cv");
        text(args.vg, X7, 65.6f, 4.4f, cr, cg, cb, 180, "cv");
        // Row 5
        text(args.vg, X1, 94.6f, 4.4f, cr, cg, cb, 180, "cv");
        text(args.vg, X3, 94.6f, 4.4f, cr, cg, cb, 180, "cv");
        // Row 7
        text(args.vg, X0, 120.1f, 4.4f, orr, og, ob, 200, "out");
        text(args.vg, X1, 120.1f, 4.4f, orr, og, ob, 200, "out");
        text(args.vg, X2, 120.1f, 4.4f, orr, og, ob, 200, "out");
        text(args.vg, X3, 120.1f, 4.4f, orr, og, ob, 200, "out");

        // =========================================================
        // Region dividers (subtle colored horizontal lines)
        // =========================================================
        auto hline = [&](float y, int r, int g, int b, int a) {
            const Vec p0 = mm2px(Vec(2.0f, y));
            const Vec p1 = mm2px(Vec(89.44f, y));
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, p0.x, p0.y);
            nvgLineTo(args.vg, p1.x, p1.y);
            nvgStrokeColor(args.vg, nvgRGBA(r, g, b, a));
            nvgStrokeWidth(args.vg, 1.f);
            nvgStroke(args.vg);
        };
        hline(36.0f,  0, 180, 220, 55);
        hline(50.5f,  0, 180, 220, 55);
        hline(65.0f,  0, 180, 220, 55);
        hline(79.5f,  0, 180, 220, 55);
        hline(94.0f,  255, 130, 200, 55);
        hline(108.5f, 255, 130, 200, 55);
        hline(119.5f, 235, 190, 120, 55);

        Widget::draw(args);
    }
};

// -----------------------------------------------------------------------------
//  Display (v9 — Scope-like continuous line)
// -----------------------------------------------------------------------------

struct PsyKickDisplay : Widget {
    PsyKick* module;

    PsyKickDisplay() : module(nullptr) {}

    void label(NVGcontext* vg, float x, float y, float px,
        int r, int g, int b, int a, int align, const char* s) {
        nvgFontSize(vg, px);
        nvgTextAlign(vg, align | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, nvgRGBA(r, g, b, a));
        nvgText(vg, x, y, s, nullptr);
    }

    void draw(const DrawArgs& args) override {
        const float w = box.size.x;
        const float h = box.size.y;

        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0.f, 0.f, w, h);
        nvgFillColor(args.vg, nvgRGB(8, 11, 13));
        nvgFill(args.vg);
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0.5f, 0.5f, w - 1.f, h - 1.f);
        nvgStrokeColor(args.vg, nvgRGBA(0, 180, 220, 90));
        nvgStrokeWidth(args.vg, 1.f);
        nvgStroke(args.vg);

        if (!module) return;

        const float waveTop = 3.f;
        const float waveBot = 28.f;
        const float waveMid = (waveTop + waveBot) * 0.5f;
        const float waveAmp = (waveBot - waveTop) * 0.5f - 1.f;
        const float envTop = 33.f;
        const float envBot = 51.f;

        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, 1.f, waveMid);
        nvgLineTo(args.vg, w - 1.f, waveMid);
        nvgStrokeColor(args.vg, nvgRGBA(56, 76, 88, 110));
        nvgStrokeWidth(args.vg, 1.f);
        nvgStroke(args.vg);

        const bool haveData = module->captureValid && module->captureLen > 0;

        if (!haveData) {
            const int previewBuckets = 120;
            const float previewColW = w / (float)previewBuckets;

            nvgBeginPath(args.vg);
            for (int i = 0; i < previewBuckets; ++i) {
                const float t = (float)i / (float)previewBuckets;
                const float y = waveMid - std::sin(TWO_PI_F * t) * waveAmp * 0.55f;
                const float x = ((float)i + 0.5f) * previewColW;
                if (i == 0) nvgMoveTo(args.vg, x, y);
                else nvgLineTo(args.vg, x, y);
            }
            nvgStrokeColor(args.vg, nvgRGBA(92, 236, 236, 150));
            nvgStrokeWidth(args.vg, 1.2f);
            nvgStroke(args.vg);

            const float sawAmt = clamp01(module->dispBassSaw);
            nvgBeginPath(args.vg);
            for (int i = 0; i < previewBuckets; ++i) {
                const float t = (float)i / (float)previewBuckets;
                const float sub = std::sin(TWO_PI_F * t);
                const float saw = 2.f * t - 1.f;
                const float v = (1.f - sawAmt) * sub + sawAmt * saw;
                const float y = waveMid - v * waveAmp * 0.55f;
                const float x = ((float)i + 0.5f) * previewColW;
                if (i == 0) nvgMoveTo(args.vg, x, y);
                else nvgLineTo(args.vg, x, y);
            }
            nvgStrokeColor(args.vg, nvgRGBA(255, 130, 200, 150));
            nvgStrokeWidth(args.vg, 1.2f);
            nvgStroke(args.vg);

            char line[192];
            std::snprintf(line, sizeof(line),
                "IDLE  ready  SAW:%d%%  MOR:%d%%",
                (int)(module->dispBassSaw * 100.f),
                (int)(module->dispBassMorph * 100.f));
            label(args.vg, 5.f, h - 5.5f, 7.0f, 170, 190, 200, 235, NVG_ALIGN_LEFT, line);
            label(args.vg, w - 46.f, 7.f, 7.5f, 92, 236, 236, 235, NVG_ALIGN_LEFT, "KICK");
            label(args.vg, w - 6.f, 7.f, 7.5f, 255, 130, 200, 235, NVG_ALIGN_RIGHT, "BASS");
            return;
        }

        const int len = module->captureLen;
        const float colW = w / (float)len;

        {
            const float x = 0.5f * colW;
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, x, 2.f);
            nvgLineTo(args.vg, x, h - 11.f);
            nvgStrokeColor(args.vg, nvgRGBA(228, 240, 250, 60));
            nvgStrokeWidth(args.vg, 1.f);
            nvgStroke(args.vg);
        }

        // KICK — two-pass glow
        for (int pass = 0; pass < 2; ++pass) {
            nvgBeginPath(args.vg);
            for (int i = 0; i < len; ++i) {
                const float v = clampF(module->dispKickRaw[i], -1.4f, 1.4f);
                const float x = ((float)i + 0.5f) * colW;
                const float y = waveMid - v * waveAmp;
                if (i == 0) nvgMoveTo(args.vg, x, y);
                else nvgLineTo(args.vg, x, y);
            }
            if (pass == 0) {
                nvgStrokeColor(args.vg, nvgRGBA(92, 236, 236, 55));
                nvgStrokeWidth(args.vg, 3.2f);
            } else {
                nvgStrokeColor(args.vg, nvgRGB(150, 250, 250));
                nvgStrokeWidth(args.vg, 1.4f);
            }
            nvgLineJoin(args.vg, NVG_ROUND);
            nvgLineCap(args.vg, NVG_ROUND);
            nvgStroke(args.vg);
        }

        // BASS — two-pass glow
        for (int pass = 0; pass < 2; ++pass) {
            nvgBeginPath(args.vg);
            for (int i = 0; i < len; ++i) {
                const float v = clampF(module->dispBassRaw[i], -1.4f, 1.4f);
                const float x = ((float)i + 0.5f) * colW;
                const float y = waveMid - v * waveAmp;
                if (i == 0) nvgMoveTo(args.vg, x, y);
                else nvgLineTo(args.vg, x, y);
            }
            if (pass == 0) {
                nvgStrokeColor(args.vg, nvgRGBA(255, 130, 200, 55));
                nvgStrokeWidth(args.vg, 3.2f);
            } else {
                nvgStrokeColor(args.vg, nvgRGB(255, 160, 220));
                nvgStrokeWidth(args.vg, 1.4f);
            }
            nvgLineJoin(args.vg, NVG_ROUND);
            nvgLineCap(args.vg, NVG_ROUND);
            nvgStroke(args.vg);
        }

        // Kick env fill
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, 0.f, envBot);
        for (int i = 0; i < len; ++i) {
            const float x = ((float)i + 0.5f) * colW;
            nvgLineTo(args.vg, x, envBot - clamp01(module->dispEnv[i]) * (envBot - envTop));
        }
        nvgLineTo(args.vg, w, envBot);
        nvgClosePath(args.vg);
        nvgFillColor(args.vg, nvgRGBA(0, 190, 225, 50));
        nvgFill(args.vg);

        // Bass env
        nvgBeginPath(args.vg);
        for (int i = 0; i < len; ++i) {
            const float x = ((float)i + 0.5f) * colW;
            const float y = envBot - clamp01(module->dispBassEnv[i]) * (envBot - envTop);
            if (i == 0) nvgMoveTo(args.vg, x, y);
            else nvgLineTo(args.vg, x, y);
        }
        nvgStrokeColor(args.vg, nvgRGBA(90, 235, 150, 110));
        nvgStrokeWidth(args.vg, 1.2f);
        nvgStroke(args.vg);

        // Pitch env
        nvgBeginPath(args.vg);
        for (int i = 0; i < len; ++i) {
            const float x = ((float)i + 0.5f) * colW;
            const float y = envBot - clamp01(module->dispPitch[i]) * (envBot - envTop);
            if (i == 0) nvgMoveTo(args.vg, x, y);
            else nvgLineTo(args.vg, x, y);
        }
        nvgStrokeColor(args.vg, nvgRGB(255, 170, 70));
        nvgStrokeWidth(args.vg, 1.4f);
        nvgStroke(args.vg);

        // Status line
        const char* stage = "IDLE";
        if (module->dispStage == PsyKick::STAGE_ATTACK) stage = "ATTACK";
        else if (module->dispStage == PsyKick::STAGE_BODY) stage = "BODY";
        else if (module->dispStage == PsyKick::STAGE_TAIL) stage = "TAIL";

        char kickN[16];
        char bassN[16];
        {
            int rpc = (int)module->dispRootPc % 12;
            if (rpc < 0) rpc += 12;
            noteNameFromHz(module->dispHz > 1.f ? module->dispHz : 55.f, rpc, kickN, sizeof(kickN));
            noteNameFromHz(module->dispBassHz > 1.f ? module->dispBassHz : 55.f, rpc, bassN, sizeof(bassN));
        }
        char line[192];
        std::snprintf(line, sizeof(line),
            "%s K:%s %.0f B:%s %.0f %.0fBPM SAW:%d%% MOR:%d%%",
            stage,
            kickN, (double)module->dispHz,
            bassN, (double)module->dispBassHz,
            (double)module->dispBpm,
            (int)(module->dispBassSaw * 100.f),
            (int)(module->dispBassMorph * 100.f));
        label(args.vg, 5.f, h - 5.5f, 7.0f, 170, 190, 200, 235, NVG_ALIGN_LEFT, line);

        label(args.vg, w - 46.f, 7.f, 7.5f, 92, 236, 236, 235, NVG_ALIGN_LEFT, "KICK");
        label(args.vg, w - 6.f, 7.f, 7.5f, 255, 130, 200, 235, NVG_ALIGN_RIGHT, "BASS");
        if (module->dispClip > 0.05f)
            label(args.vg, 6.f, 7.f, 8.5f, 255, 120, 90, 240, NVG_ALIGN_LEFT, "CLIP");
        else {
            char nn[48];
            std::snprintf(nn, sizeof(nn), "%s %+.0fst", kickN, (double)module->dispVoctSemis);
            label(args.vg, 6.f, 7.f, 7.5f, 130, 165, 180, 200, NVG_ALIGN_LEFT, nn);
        }
    }
};

// -----------------------------------------------------------------------------
//  Widgets
// -----------------------------------------------------------------------------

struct PsyKickKnobBig : componentlibrary::Davies1900hWhiteKnob {
    PsyKickKnobBig() { box.size = box.size.mult(0.9f); }

    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        const float cx = box.size.x * 0.5f;
        const float cy = box.size.y * 0.5f;
        nvgTranslate(args.vg, cx, cy);
        nvgScale(args.vg, 0.70f, 0.70f);
        nvgTranslate(args.vg, -cx, -cy);
        componentlibrary::Davies1900hWhiteKnob::draw(args);
        nvgRestore(args.vg);
    }
};

struct PsyKickKnobSmall : componentlibrary::RoundSmallBlackKnob {
    PsyKickKnobSmall() { box.size = box.size.mult(0.95f); }

    void draw(const DrawArgs& args) override {
        componentlibrary::RoundSmallBlackKnob::draw(args);
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, box.size.x * 0.5f, box.size.y * 0.5f, box.size.x * 0.13f);
        nvgFillColor(args.vg, nvgRGBA(0, 195, 230, 190));
        nvgFill(args.vg);
    }
};

struct PsyKickKnobSnap : PsyKickKnobSmall {
    PsyKickKnobSnap() { snap = true; }
};

struct PsyKickTrimKnob : componentlibrary::Rogan1PRed {
    PsyKickTrimKnob() { box.size = box.size.mult(0.95f); }
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        const float cx = box.size.x * 0.5f;
        const float cy = box.size.y * 0.5f;
        nvgTranslate(args.vg, cx, cy);
        nvgScale(args.vg, 0.78f, 0.78f);
        nvgTranslate(args.vg, -cx, -cy);
        componentlibrary::Rogan1PRed::draw(args);
        nvgRestore(args.vg);
    }
};

// -----------------------------------------------------------------------------
//  Module widget (v10 — UX-optimized layout)
// -----------------------------------------------------------------------------

struct PsyKickWidget : ModuleWidget {
    PsyKickWidget(PsyKick* module) {
        setModule(module);
        setPanel(createWidget<PsyKickPanel>(Vec(0, 0)));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0.f)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2.f * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2.f * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Display
        PsyKickDisplay* disp = createWidget<PsyKickDisplay>(mm2px(Vec(3.2f, 13.5f)));
        disp->box.size = mm2px(Vec(85.04f, 20.0f));
        disp->module = module;
        addChild(disp);

        // Helpers
        auto big = [&](float x, float y, int id) {
            addParam(createParamCentered<PsyKickKnobBig>(mm2px(Vec(x, y)), module, id));
        };
        auto small = [&](float x, float y, int id) {
            addParam(createParamCentered<PsyKickKnobSmall>(mm2px(Vec(x, y)), module, id));
        };
        auto snapk = [&](float x, float y, int id) {
            addParam(createParamCentered<PsyKickKnobSnap>(mm2px(Vec(x, y)), module, id));
        };
        auto trim = [&](float x, float y, int id) {
            addParam(createParamCentered<PsyKickTrimKnob>(mm2px(Vec(x, y)), module, id));
        };
        auto jin = [&](float x, float y, int id) {
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x, y)), module, id));
        };
        auto jout = [&](float x, float y, int id) {
            addOutput(createOutputCentered<PJ3410Port>(mm2px(Vec(x, y)), module, id));
        };
        auto lamp = [&](float x, float y, int id) {
            addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(x, y)), module, id));
        };

        // 8-column grid
        const float X0 = 7.0f;
        const float X1 = 18.4f;
        const float X2 = 29.8f;
        const float X3 = 41.2f;
        const float X4 = 52.6f;
        const float X5 = 64.0f;
        const float X6 = 75.4f;
        const float X7 = 86.8f;

        // =========================================================
        // ROW 1 (y=38.5) — KICK SOURCE
        // =========================================================
        jin(X0, 38.5f, PsyKick::VOCT_INPUT);
        jin(X1, 38.5f, PsyKick::TRIG_INPUT);
        jin(X2, 38.5f, PsyKick::ACCENT_INPUT);
        big(X3, 38.5f, PsyKick::BASE_PARAM);
        jin(X4, 38.5f, PsyKick::PITCH_CV_INPUT);
        big(X5, 38.5f, PsyKick::SWEEP_PARAM);
        jin(X6, 38.5f, PsyKick::SWEEP_CV_INPUT);
        snapk(X7, 38.5f, PsyKick::ROOT_PARAM);

        // =========================================================
        // ROW 2 (y=53.0) — KICK PITCH
        // =========================================================
        big(X0, 53.0f, PsyKick::CURVE_PARAM);
        big(X1, 53.0f, PsyKick::DECAY_PARAM);
        jin(X2, 53.0f, PsyKick::DECAY_CV_INPUT);
        small(X3, 53.0f, PsyKick::PUNCH_PARAM);
        small(X4, 53.0f, PsyKick::CLICK_PARAM);
        small(X5, 53.0f, PsyKick::CLICK_TONE_PARAM);
        small(X6, 53.0f, PsyKick::PUNCH_CLICK_PARAM);
        lamp(X7, 53.0f, PsyKick::TRIG_LIGHT);

        // =========================================================
        // ROW 3 (y=67.5) — KICK BODY
        // =========================================================
        big(X0, 67.5f, PsyKick::BODY_PARAM);
        small(X1, 67.5f, PsyKick::TAIL_PARAM);
        snapk(X2, 67.5f, PsyKick::KICK_TAILMODE_PARAM);
        small(X3, 67.5f, PsyKick::SHAPE_PARAM);
        big(X4, 67.0f, PsyKick::FOREST_PARAM);
        big(X6, 67.5f, PsyKick::DRIVE_PARAM);
        jin(X7, 67.5f, PsyKick::DRIVE_CV_INPUT);

        // =========================================================
        // ROW 4 (y=82.0) — KICK CHARACTER
        // =========================================================
        small(X0, 82.0f, PsyKick::FOLD_PARAM);
        small(X1, 82.0f, PsyKick::DARK_PARAM);
        big(X2, 82.0f  , PsyKick::MORPH_PARAM);     
        jin(X3, 82.0f, PsyKick::MORPH_CV_INPUT);          
        // (X3..X7 open — visual separator before bass)

        // =========================================================
        // ROW 5 (y=96.5) — BASS CORE
        // =========================================================
        big(X0, 96.5f, PsyKick::BASS_LEVEL_PARAM);
        jin(X1, 96.5f, PsyKick::BASS_CV_INPUT);
        small(X2, 96.5f, PsyKick::BASS_TRACK_PARAM);
        jin(X3, 96.5f, PsyKick::BASS_TRACK_CV_INPUT);
        small(X4, 96.5f, PsyKick::BASS_TONE_PARAM);       // SAW
        small(X5, 96.5f, PsyKick::BASS_DECAY_PARAM);
        small(X6, 96.5f, PsyKick::BASS_FILT_DECAY_PARAM);
        snapk(X7, 96.5f, PsyKick::BASS_SWEEP_PARAM);      // B-SWP

        // =========================================================
        // ROW 6 (y=111.0) — BASS CHARACTER
        // =========================================================
        snapk(X6, 111.0f, PsyKick::BASS_CHAR_PARAM);      // BCHAR
        trim(X7, 111.0f, PsyKick::ACCENT_PARAM);          // ACCENT depth

        // =========================================================
        // ROW 7 (y=122.0) — OUTPUTS
        // =========================================================
        jout(X0, 122.0f, PsyKick::KICK_OUTPUT);
        jout(X1, 122.0f, PsyKick::BASS_OUTPUT);
        jout(X2, 122.0f, PsyKick::ENV_OUTPUT);
        jout(X3, 122.0f, PsyKick::EOC_OUTPUT);
        trim(X4, 122.0f, PsyKick::LEVEL_PARAM);
        lamp(X5, 122.0f, PsyKick::CLIP_LIGHT);
    }
};

} // namespace

Model* modelPsyKick = createModel<PsyKick, PsyKickWidget>("PsyKick");