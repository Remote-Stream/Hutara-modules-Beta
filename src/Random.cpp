#include "plugin.hpp"
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <string>
#include <cstdio>
#include <atomic>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace rack;

struct Hutara_Random_CV;

// =========================================================================
// ParamQuantity classes
// =========================================================================
struct HutaraTimeQuantity : ParamQuantity {
    Hutara_Random_CV* module = nullptr;
    std::string getDisplayValueString() override;
    void setValue(float v) override;
};

struct HutaraBpmMultQuantity : ParamQuantity {
    std::string getDisplayValueString() override;
    void setValue(float v) override;
};

struct HutaraPpqnQuantity : ParamQuantity {
    std::string getDisplayValueString() override;
    void setValue(float v) override;
};

struct HutaraShapeQuantity : ParamQuantity {
    std::string getDisplayValueString() override;
    void setValue(float v) override;
};

struct HutaraGateSpeedQuantity : ParamQuantity {
    std::string getDisplayValueString() override;
    void setValue(float v) override;
};

struct HutaraInputPpqnQuantity : ParamQuantity {
    std::string getDisplayValueString() override;
    void setValue(float v) override;
};

struct HutaraGateModeQuantity : ParamQuantity {
    std::string getDisplayValueString() override;
    void setValue(float v) override;
};

// =========================================================================
// Custom visual components
// =========================================================================
struct MyHutaraSmallKnob : componentlibrary::RoundSmallBlackKnob {
    MyHutaraSmallKnob() { box.size = box.size.mult(0.8f); }
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        nvgGlobalTint(args.vg, nvgRGBA(0, 180, 220, 255));
        componentlibrary::RoundSmallBlackKnob::draw(args);
        nvgRestore(args.vg);
    }
};

struct BefacoSwitchSmallKnob : componentlibrary::BefacoSwitch {
    BefacoSwitchSmallKnob() { box.size = box.size.mult(0.8f); }
};

// =========================================================================
// Module
// =========================================================================
struct Hutara_Random_CV : Module {
    static constexpr float DEFAULT_GATE_LENGTH = 0.7f;
    static constexpr float CLOCK_LOW_THRESHOLD  = 0.3f;
    static constexpr float CLOCK_HIGH_THRESHOLD = 1.0f;
    static constexpr float GATE_HIGH_VOLTAGE = 10.0f;
    static constexpr float MIN_CLOCK_INTERVAL = 0.0001f;
    static constexpr float MAX_CLOCK_INTERVAL = 2.0f;
    static constexpr float MIN_SMOOTH_TIME = 0.001f;
    static constexpr float SMOOTH_DEADZONE = 0.001f;
    static constexpr int   NUM_SHAPES = 24;
    static constexpr int   TABLE_N = 256;
    static constexpr int   NUM_DIVISIONS = 22;
    static constexpr int   NUM_BPM_MULTS = 6;
    static constexpr int   NUM_PPQN = 4;
    static constexpr float DEFAULT_BPM = 120.f;
    static constexpr float BPM_TIMEOUT_SECONDS = 2.0f;
    static constexpr float BPM_SMOOTHING = 0.1f;
    static constexpr float SH_DISPLAY_MAX = 10.f;
    static constexpr int NUM_GATE_PATTERNS = 4;
    static constexpr int NUM_GATE_SPEEDS = 6;
    static constexpr float MIN_GATE_FRAC = 0.03f;
    static constexpr float MAX_GATE_FRAC = 0.98f;
    static constexpr float MIN_GATE_WIDTH_SEC = 0.003f;
    static constexpr int NUM_INPUT_PPQN = 9;

    static constexpr int   AUTO_DETECT_MIN_PULSES = 16;
    static constexpr float PPQN_HYSTERESIS = 1.5f;
    static constexpr int   DEFAULT_DETECTED_PPQN = 4;

    static constexpr float GATE_WIDTH_FACTOR = 3.5f;
    static constexpr float RANDOM_MAX_GATE_FRAC = 0.60f;

    // === FIX: current module version for patch migration ===
    static constexpr int CURRENT_MODULE_VERSION = 2;

    static constexpr int NUM_PPQN_CANDIDATES = 3;
    static const int* ppqnCandidates() {
        static const int v[3] = { 1, 4, 24 };
        return v;
    }
    static bool isValidPpqn(int p) {
        const int* c = ppqnCandidates();
        for (int i = 0; i < NUM_PPQN_CANDIDATES; i++)
            if (c[i] == p) return true;
        return false;
    }

    enum GateMode { GATE_PATTERN_MODE = 0, GATE_RANDOM_MODE, GATE_HYBRID_MODE };
    static constexpr int NUM_GATE_MODES = 3;

    enum VarType { V_ORIG = 0, V_GHOST, V_FILL32, V_DROP, V_ROLL };
    struct StepDNA { float trigger, width; };

    // === FIX: ParamIds kept identical to v1.0 at indices 0-6 for patch compatibility ===
    enum ParamIds {
        OUTPUT_VOLTAGE_PARAM,       // 0
        BIPOLAR_PARAM,              // 1
        STRENGTH_PARAM,             // 2
        RANDOM_GATE_SPEED_PARAM,    // 3
        GATE_LENGTH_PARAM,          // 4
        TRIGGER_DIVISION_DIVIDER_PARAM, // 5
        S_H_OFFSET_PARAM,           // 6
        MODULATION_INPUT_PARAM,     // 7 (new)
        SMOOTH_RANDOM_PARAM,        // 8 (new)
        ENHANCED_MODE_PARAM,        // 9 (new)
        TIME_PARAM,                 // 10 (new)
        SHAPE_PARAM,                // 11 (new)
        WARP_PARAM,                 // 12 (new)
        FLUX_PARAM,                 // 13 (new)
        LOOP_PARAM,                 // 14 (new)
        RANDOMNESS_PARAM,           // 15 (new)
        LEVEL_PARAM,                // 16 (new)
        TRIG_BUTTON_PARAM,          // 17 (new)
        TIME_SYNC_MODE_PARAM,       // 18 (new)
        BPM_MULTIPLIER_PARAM,       // 19 (new)
        CLOCK_PPQN_PARAM,           // 20 (new)
        GATE_PATTERN_PARAM,         // 21 (new)
        ENVELOPE_OFFSET_PARAM,      // 22 (new)
        ENVELOPE_RANGE_PARAM,       // 23 (new)
        GATE_FILL_PARAM,            // 24 (new)
        GATE_PROBABILITY_PARAM,     // 25 (new)
        INPUT_CLOCK_PPQN_PARAM,     // 26 (new)
        SWING_PARAM,                // 27 (new)
        GATE_MODE_PARAM,            // 28 (new)
        NUM_PARAMS
    };
    enum InputIds {
        ON_INPUT, MODULATION_INPUT, TIME_INPUT, SHAPE_INPUT, WARP_INPUT,
        FLUX_INPUT, RANDOMNESS_INPUT, TRIG_INPUT, ENVELOPE_CLOCK_INPUT, NUM_INPUTS
    };
    enum OutputIds {
        GATE_OUTPUT, OUTPUT, GATE_OUTPUT_INVERTED, EOC_OUTPUT,
        ENVELOPE_OUTPUT, NUM_OUTPUTS
    };
    enum LightIds { NUM_LIGHTS };

    // ---------- Legacy S&H state ----------
    float outputVoltage = 0.0f;
    bool  sampleNext = false;
    bool  bipolarOutput = true;
    float clockTimer = 0.0f;
    float lastClockInterval = 0.25f;
    bool  haveClockInterval = false;
    int   triggerCounter = 0;
    int   lastDivider = 1;
    float currentOutput = 0.0f;
    float targetRandomValue = 0.0f;
    bool  smoothActive = false;

    // ---------- Gate engine state ----------
    float gateStepPhase = 0.f;
    int   gateStep = 0;
    int   gateBar = 0;
    VarType gateVar = V_ORIG;
    float gateWidthFrac = 0.f;
    bool  gatePulseState = false;
    int   lastGateMode = -1;

    // ---------- Auto-detect PPQN (input clock) ----------
    float onInputLastInterval = 0.f;
    bool  onInputHaveInterval = false;
    int   onInputDetectedPpqn = DEFAULT_DETECTED_PPQN;
    int   onInputPulseCount = 0;
    int   onInputLockedPpqn = 0;
    float onInputIntervalHistory[16] = {};
    int   onInputIntervalHistIdx = 0;
    int   onInputIntervalHistSize = 0;

    // ---------- Anti-jump ----------
    float gateLastStepDur = 0.f;
    int   lastUsedPpqn = -1;

    // ---------- RNG ----------
    uint32_t rngState = 0x7A31C9D5u;

    // ---------- Enhanced envelope state ----------
    double envelopePhase = 1.0;
    std::atomic<uint32_t> envelopeCycleSeed{0x9E3779B9u};
    bool  running = false;

    // ---------- Draw-table (P0 seqlock) ----------
    float drawTable[TABLE_N];
    std::atomic<bool> drawModeAtomic{false};
    static constexpr int DRAW_UNDO_LEVELS = 15;
    float drawUndoTables[DRAW_UNDO_LEVELS][TABLE_N];
    int   drawUndoCount = 0;
    float drawResetTable[TABLE_N];
    bool  drawResetAvailable = false;
    bool  drawGestureActive = false;

    std::atomic<uint32_t> drawTableSeq{0};
    std::atomic<float> drawTableShared[TABLE_N];

    float audioDrawTable[TABLE_N];
    std::atomic<uint32_t> audioDrawTableVersion{0};
    static constexpr uint32_t INVALID_SEQ = 0u;

    dsp::SchmittTrigger trigTrigger;
    dsp::BooleanTrigger buttonTrigger;
    dsp::PulseGenerator eocPulse;

    // ---------- Display state ----------
    float dispTime = 0.5f;
    float dispShape = 0.f;
    float dispWarp = 0.f;
    float dispFlux = 0.f;
    float dispRandomness = 0.f;
    double dispPhase = 1.0;
    float dispDuration = 1.f;
    bool  dispLoop = false;

    bool  syncMode = false;
    float measuredBpm = DEFAULT_BPM;
    float bpmTimeout = 0.f;
    bool  envelopeClockHigh = false;
    bool  haveBpmMeasurement = false;
    float envelopeClockTimer = 0.f;
    int   currentDivisionIndex = 2;
    float dispBpm = DEFAULT_BPM;
    int   dispDivisionIndex = 2;
    bool  dispSyncMode = false;
    int   dispMultiplierIdx = 2;
    int   dispPpqnIdx = 0;
    int   autoDetectedPpqn = DEFAULT_DETECTED_PPQN;

    // ---------- Gate display ----------
    float dispGateStepDur = 0.f;
    float dispGateQuarterDur = 0.f;
    int   dispGateInputPpqn = DEFAULT_DETECTED_PPQN;

    static constexpr int SH_HISTORY_SIZE = 256;
    static constexpr int SH_DISPLAY_COUNT = 32;
    float shHistory[SH_HISTORY_SIZE] = {};
    int   shHistoryWritePos = 0;
    int   shHistoryCount = 0;
    float dispShValue = 0.f;
    float shSampleTimer = 0.f;

    // =====================================================================
    // Construction
    // =====================================================================
    Hutara_Random_CV() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

        configParam(OUTPUT_VOLTAGE_PARAM, 0.25f, 10.f, 5.f, "S&H Voltage");
        configSwitch(BIPOLAR_PARAM, 0.f, 1.f, 1.f, "Polarity", {"Unipolar", "Bipolar"});
        configParam(STRENGTH_PARAM, 1.f, 10.f, 1.f, "Modulation Strength");

        configParam<HutaraGateSpeedQuantity>(
            RANDOM_GATE_SPEED_PARAM, 0.f,
            static_cast<float>(NUM_GATE_SPEEDS - 1), 1.f, "Gate Speed");
        if (auto* pq = paramQuantities[RANDOM_GATE_SPEED_PARAM]) {
            pq->snapEnabled = true;
            pq->smoothEnabled = false;
        }

        configParam(GATE_LENGTH_PARAM, 0.01f, 2.0f, DEFAULT_GATE_LENGTH, "Gate Width");
        configParam(TRIGGER_DIVISION_DIVIDER_PARAM, 1.f, 3.f, 1.f, "Trigger Division");
        configParam(S_H_OFFSET_PARAM, -5.f, 5.f, 0.f, "S&H Offset");
        configParam(MODULATION_INPUT_PARAM, -1.f, 1.f, 0.f, "Modulation Amount");
        configParam(SMOOTH_RANDOM_PARAM, 0.f, 1.f, 0.f, "Smooth Random");
        configSwitch(ENHANCED_MODE_PARAM, 0.f, 1.f, 1.f, "Mode",
            {"Legacy Random", "Enhanced"});

        configParam<HutaraTimeQuantity>(TIME_PARAM, 0.f, 1.f, 0.5f, "Envelope Time");
        configParam<HutaraShapeQuantity>(SHAPE_PARAM, 0.f, 1.f, 0.0f, "Envelope Shape");
        configParam(WARP_PARAM, -1.f, 1.f, 0.f, "Envelope Warp");
        configParam(FLUX_PARAM, 0.f, 1.f, 0.f, "Envelope Flux", "%", 0.f, 100.f);
        configSwitch(LOOP_PARAM, 0.f, 1.f, 1.f, "Envelope Loop", {"Off", "On"});
        configParam(RANDOMNESS_PARAM, 0.f, 1.f, 0.f, "Random Curve Amount", "%", 0.f, 100.f);
        configParam(LEVEL_PARAM, 0.f, 1.f, 1.f, "Envelope Level", "%", 0.f, 100.f);
        configButton(TRIG_BUTTON_PARAM, "Envelope Trigger");
        configSwitch(TIME_SYNC_MODE_PARAM, 0.f, 1.f, 0.f, "Time Mode",
            {"Free", "BPM Sync"});

        configParam<HutaraBpmMultQuantity>(
            BPM_MULTIPLIER_PARAM, 0.f, static_cast<float>(NUM_BPM_MULTS - 1), 2.f,
            "BPM Multiplier", "", 0.01f, 1.f);
        configParam<HutaraPpqnQuantity>(
            CLOCK_PPQN_PARAM, 0.f, static_cast<float>(NUM_PPQN - 1), 0.f,
            "Clock PPQN");

        configSwitch(GATE_PATTERN_PARAM, 0.f,
            static_cast<float>(NUM_GATE_PATTERNS - 1), 1.f,
            "Gate Pattern", {"Darkpsy", "Psytrance", "Hi-Tech", "Break"});

        configParam(ENVELOPE_OFFSET_PARAM, -5.f, 5.f, 0.f, "Envelope Offset");
        configParam(ENVELOPE_RANGE_PARAM, 0.25f, 10.f, 10.f, "Envelope Range (V)");
        configParam(GATE_FILL_PARAM, 0.f, 1.f, 0.4f, "Fill Amount");
        configParam(GATE_PROBABILITY_PARAM, 0.f, 1.f, 0.75f, "Gate Probability", "%", 0.f, 100.f);

        configParam<HutaraInputPpqnQuantity>(
            INPUT_CLOCK_PPQN_PARAM, 0.f,
            static_cast<float>(NUM_INPUT_PPQN - 1), 0.f,
            "Input Clock PPQN");
        if (auto* pq = paramQuantities[INPUT_CLOCK_PPQN_PARAM]) {
            pq->snapEnabled = true;
            pq->smoothEnabled = false;
        }

        configParam(SWING_PARAM, 0.f, 1.f, 0.f, "Swing", "%", 0.f, 100.f);

        configParam<HutaraGateModeQuantity>(
            GATE_MODE_PARAM, 0.f, static_cast<float>(NUM_GATE_MODES - 1), 0.f,
            "Gate Mode");
        if (auto* pq = paramQuantities[GATE_MODE_PARAM]) {
            pq->snapEnabled = true;
            pq->smoothEnabled = false;
        }

        configOutput(GATE_OUTPUT, "Gate");
        configOutput(OUTPUT, "S&H / Random CV");
        configOutput(GATE_OUTPUT_INVERTED, "Inverted Gate (same width, end of step)");
        configInput(ON_INPUT, "Clock / Trigger (Gate)");
        configInput(MODULATION_INPUT, "Modulation CV");
        configInput(TIME_INPUT, "Time CV (10V = full range)");
        configInput(SHAPE_INPUT, "Shape CV (10V = full range)");
        configInput(WARP_INPUT, "Warp CV (5V = full range)");
        configInput(FLUX_INPUT, "Flux CV (10V = full range)");
        configInput(RANDOMNESS_INPUT, "Randomness CV (10V = full range)");
        configInput(TRIG_INPUT, "Enhanced Trigger");
        configInput(ENVELOPE_CLOCK_INPUT, "Envelope Clock (BPM Reference)");
        configOutput(ENVELOPE_OUTPUT, "Envelope");
        configOutput(EOC_OUTPUT, "End of Cycle");

        if (auto* q = paramQuantities[TIME_PARAM]) {
            if (auto* hq = dynamic_cast<HutaraTimeQuantity*>(q)) {
                hq->module = this;
            }
        }

        initDrawTable();
        publishDrawTable();
        audioDrawTableVersion.store(INVALID_SEQ, std::memory_order_relaxed);

        float initVal = 0.f;
        for (int i = 0; i < SH_HISTORY_SIZE; i++) shHistory[i] = initVal;
        dispShValue = initVal;
        shSampleTimer = 0.f;

        if (auto* pq = paramQuantities[SHAPE_PARAM])
            dispShape = pq->getValue();
    }

    // =====================================================================
    // Utilities
    // =====================================================================
    static float clampValue(float value, float minValue, float maxValue) {
        return std::min(std::max(value, minValue), maxValue);
    }
    static int clampInt(int value, int minValue, int maxValue) {
        return std::min(std::max(value, minValue), maxValue);
    }

    uint32_t nextRandomU32() {
        rngState ^= rngState << 13;
        rngState ^= rngState >> 17;
        rngState ^= rngState << 5;
        if (rngState == 0u) rngState = 0x6D2B79F5u;
        return rngState;
    }
    float randomFloat() {
        return nextRandomU32() * (1.f / 4294967296.f) * 2.f - 1.f;
    }
    float randomUnipolar() {
        return nextRandomU32() * (1.f / 4294967296.f);
    }

    // =====================================================================
    // Seqlock for draw table
    // =====================================================================
    void publishDrawTable() {
        uint32_t s = drawTableSeq.load(std::memory_order_relaxed);
        drawTableSeq.store(s + 1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        for (int i = 0; i < TABLE_N; ++i) {
            drawTableShared[i].store(drawTable[i], std::memory_order_relaxed);
        }
        std::atomic_thread_fence(std::memory_order_release);
        drawTableSeq.store(s + 2, std::memory_order_release);
    }

    void updateAudioDrawTable() {
        if (!drawModeAtomic.load(std::memory_order_acquire)) return;

        uint32_t s1 = drawTableSeq.load(std::memory_order_acquire);
        if ((s1 & 1u) != 0u) return;
        uint32_t cached = audioDrawTableVersion.load(std::memory_order_relaxed);
        if (s1 == cached) return;

        std::atomic_thread_fence(std::memory_order_acquire);
        for (int i = 0; i < TABLE_N; ++i) {
            audioDrawTable[i] = drawTableShared[i].load(std::memory_order_relaxed);
        }
        std::atomic_thread_fence(std::memory_order_acquire);
        uint32_t s2 = drawTableSeq.load(std::memory_order_relaxed);

        if (s1 == s2) {
            audioDrawTableVersion.store(s1, std::memory_order_relaxed);
        }
    }

    // =====================================================================
    // Static lookup tables
    // =====================================================================
    static float divisionToBeats(int index) {
        static constexpr float beats[NUM_DIVISIONS] = {
            32.0f, 24.0f, 16.0f, 12.0f, 8.0f, 4.0f, 2.0f,
            1.0f, 2.0f/3.0f, 1.5f, 0.5f, 1.0f/3.0f, 0.75f,
            0.25f, 1.0f/6.0f, 0.375f, 0.125f, 1.0f/12.0f, 0.1875f,
            0.0625f, 1.0f/24.0f, 0.09375f
        };
        if (index < 0) index = 0;
        if (index >= NUM_DIVISIONS) index = NUM_DIVISIONS - 1;
        return beats[index];
    }

    static const char* divisionToLabel(int index) {
        static const char* labels[NUM_DIVISIONS] = {
            "8/1", "6/1", "4/1", "3/1", "2/1", "1/1", "1/2",
            "1/4", "1/4T", "1/4D", "1/8", "1/8T", "1/8D",
            "1/16", "1/16T", "1/16D", "1/32", "1/32T", "1/32D",
            "1/64", "1/64T", "1/64D"
        };
        if (index < 0) index = 0;
        if (index >= NUM_DIVISIONS) index = NUM_DIVISIONS - 1;
        return labels[index];
    }

    static float multiplierToFactor(int idx) {
        static constexpr float factors[NUM_BPM_MULTS] = {
            0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f
        };
        if (idx < 0) idx = 0;
        if (idx >= NUM_BPM_MULTS) idx = NUM_BPM_MULTS - 1;
        return factors[idx];
    }

    static const char* multiplierToLabel(int idx) {
        static const char* labels[NUM_BPM_MULTS] = {
            "1/4x", "1/2x", "1x", "2x", "4x", "8x"
        };
        if (idx < 0) idx = 0;
        if (idx >= NUM_BPM_MULTS) idx = NUM_BPM_MULTS - 1;
        return labels[idx];
    }

    static int ppqnToInt(int idx) {
        static constexpr int values[NUM_PPQN] = { 0, 1, 4, 24 };
        if (idx < 0) idx = 0;
        if (idx >= NUM_PPQN) idx = NUM_PPQN - 1;
        return values[idx];
    }

    static const char* ppqnToLabel(int idx) {
        static const char* labels[NUM_PPQN] = {
            "Auto Detect", "1 PPQN", "4 PPQN", "24 PPQN (MIDI)"
        };
        if (idx < 0) idx = 0;
        if (idx >= NUM_PPQN) idx = NUM_PPQN - 1;
        return labels[idx];
    }

    static int inputPpqnToInt(int idx) {
        static constexpr int values[NUM_INPUT_PPQN] = { 0, 1, 2, 4, 6, 8, 12, 24, 48 };
        if (idx < 0) idx = 0;
        if (idx >= NUM_INPUT_PPQN) idx = NUM_INPUT_PPQN - 1;
        return values[idx];
    }

    static const char* inputPpqnToLabel(int idx) {
        static const char* labels[NUM_INPUT_PPQN] = {
            "Auto", "1", "2", "4", "6", "8", "12", "24", "48"
        };
        if (idx < 0) idx = 0;
        if (idx >= NUM_INPUT_PPQN) idx = NUM_INPUT_PPQN - 1;
        return labels[idx];
    }

    static const char* gateModeLabel(int idx) {
        static const char* labels[NUM_GATE_MODES] = {
            "Pattern", "Random", "Hybrid"
        };
        if (idx < 0) idx = 0;
        if (idx >= NUM_GATE_MODES) idx = NUM_GATE_MODES - 1;
        return labels[idx];
    }

    // =====================================================================
    // PPQN scoring
    // =====================================================================
    static float computePpqnScore(int c, float avgInterval) {
        float quarterDur = avgInterval * static_cast<float>(c);
        if (quarterDur < 1e-5f) return 1e18f;
        float bpm = 60.f / quarterDur;

        if (bpm < 55.f || bpm > 210.f) {
            float penalty = (bpm < 55.f) ? (55.f - bpm) : (bpm - 210.f);
            return 1e6f + penalty;
        }
        return std::fabs(bpm - 132.5f);
    }

    void resetPpqnDetection() {
        onInputPulseCount = 0;
        onInputIntervalHistIdx = 0;
        onInputIntervalHistSize = 0;
        for (int i = 0; i < 16; i++) onInputIntervalHistory[i] = 0.f;
        onInputLockedPpqn = 0;
        onInputDetectedPpqn = DEFAULT_DETECTED_PPQN;
    }

    void updateOnInputPpqnDetection(float interval) {
        if (interval < 1e-5f) return;

        onInputIntervalHistory[onInputIntervalHistIdx] = interval;
        onInputIntervalHistIdx = (onInputIntervalHistIdx + 1) % 16;
        if (onInputIntervalHistSize < 16) onInputIntervalHistSize++;
        if (onInputPulseCount < 16) onInputPulseCount++;

        if (onInputPulseCount < AUTO_DETECT_MIN_PULSES) return;

        int n = std::min(onInputIntervalHistSize, 16);
        if (n < 4) return;
        float avg = 0.f;
        for (int i = 0; i < n; i++) avg += onInputIntervalHistory[i];
        avg /= static_cast<float>(n);
        if (avg < 1e-6f) return;

        const int* candidates = ppqnCandidates();
        const int numCand = NUM_PPQN_CANDIDATES;

        float bestScore = 1e18f;
        int bestPpqn = onInputDetectedPpqn;
        for (int i = 0; i < numCand; i++) {
            int c = candidates[i];
            float s = computePpqnScore(c, avg);
            if (s < bestScore) { bestScore = s; bestPpqn = c; }
        }

        if (onInputLockedPpqn == 0) {
            onInputDetectedPpqn = bestPpqn;
            onInputLockedPpqn = bestPpqn;
            return;
        }

        float currentScore = computePpqnScore(onInputLockedPpqn, avg);
        if (bestScore * PPQN_HYSTERESIS < currentScore) {
            onInputDetectedPpqn = bestPpqn;
            onInputLockedPpqn = bestPpqn;
        } else {
            onInputDetectedPpqn = onInputLockedPpqn;
        }
    }

    static int timeParamToDivisionIndex(float timeParam) {
        float scaled = clampValue(timeParam, 0.f, 1.f) * (NUM_DIVISIONS - 1);
        int idx = static_cast<int>(std::round(scaled));
        if (idx < 0) idx = 0;
        if (idx >= NUM_DIVISIONS) idx = NUM_DIVISIONS - 1;
        return idx;
    }

    static const char* gatePatternLabel(int idx) {
        static const char* labels[NUM_GATE_PATTERNS] = {
            "Darkpsy", "Psytrance", "Hi-Tech", "Break"
        };
        if (idx < 0) idx = 0;
        if (idx >= NUM_GATE_PATTERNS) idx = NUM_GATE_PATTERNS - 1;
        return labels[idx];
    }

    static const char* shapeName(int index) {
        static const char* names[NUM_SHAPES] = {
            "EXP", "LIN", "AD", "TRI", "SIN", "BELL", "RISE", "CUBIC",
            "DBL", "AM", "DAMP", "STEP5D", "STEP5U", "GATE", "DCOS", "FM",
            "SCURVE", "PULSE", "BOUNCE", "PLUCK", "SWELL", "STEP8D",
            "TRAP", "MPEAK"
        };
        if (index < 0) index = 0;
        if (index >= NUM_SHAPES) index = NUM_SHAPES - 1;
        return names[index];
    }

    // =====================================================================
    // Draw-table helpers
    // =====================================================================
    void initDrawTable() {
        for (int i = 0; i < TABLE_N; i++) {
            float t = static_cast<float>(i) / static_cast<float>(TABLE_N - 1);
            drawTable[i] = 1.f - std::fabs(2.f * t - 1.f);
        }
    }

    void clearDrawHistory() {
        drawUndoCount = 0;
        drawResetAvailable = false;
        drawGestureActive = false;
    }

    void beginDrawGesture() {
        if (drawGestureActive) return;
        if (!drawModeAtomic.load(std::memory_order_relaxed)) {
            float shape = clampValue(params[SHAPE_PARAM].getValue(), 0.f, 1.f);
            for (int i = 0; i < TABLE_N; ++i) {
                float p = static_cast<float>(i) / static_cast<float>(TABLE_N - 1);
                float shapePosition = shape * static_cast<float>(NUM_SHAPES - 1);
                int shape0 = clampInt(static_cast<int>(shapePosition), 0, NUM_SHAPES - 1);
                int shape1 = std::min(shape0 + 1, NUM_SHAPES - 1);
                float morph = shapePosition - static_cast<float>(shape0);
                float v0 = shapeValue(shape0, p);
                float v1 = shapeValue(shape1, p);
                drawTable[i] = clampValue(v0 + (v1 - v0) * morph, 0.f, 1.f);
            }
            for (int i = 0; i < TABLE_N; ++i)
                drawResetTable[i] = drawTable[i];
            drawResetAvailable = true;
            drawUndoCount = 0;
            publishDrawTable();
            drawModeAtomic.store(true, std::memory_order_release);
        }
        if (drawUndoCount < DRAW_UNDO_LEVELS) {
            for (int i = drawUndoCount; i > 0; --i)
                for (int j = 0; j < TABLE_N; ++j)
                    drawUndoTables[i][j] = drawUndoTables[i - 1][j];
            for (int i = 0; i < TABLE_N; ++i)
                drawUndoTables[0][i] = drawTable[i];
            drawUndoCount++;
        } else {
            for (int i = DRAW_UNDO_LEVELS - 1; i > 0; --i)
                for (int j = 0; j < TABLE_N; ++j)
                    drawUndoTables[i][j] = drawUndoTables[i - 1][j];
            for (int i = 0; i < TABLE_N; ++i)
                drawUndoTables[0][i] = drawTable[i];
        }
        drawGestureActive = true;
    }

    void endDrawGesture() { drawGestureActive = false; }

    bool undoDraw() {
        if (drawUndoCount <= 0) return false;
        for (int i = 0; i < TABLE_N; ++i)
            drawTable[i] = drawUndoTables[0][i];
        for (int i = 0; i < drawUndoCount - 1; ++i)
            for (int j = 0; j < TABLE_N; ++j)
                drawUndoTables[i][j] = drawUndoTables[i + 1][j];
        drawUndoCount--;
        drawGestureActive = false;
        publishDrawTable();
        drawModeAtomic.store(true, std::memory_order_release);
        return true;
    }

    bool resetDraw() {
        if (!drawResetAvailable) return false;
        for (int i = 0; i < TABLE_N; ++i)
            drawTable[i] = drawResetTable[i];
        drawUndoCount = 0;
        drawGestureActive = false;
        drawResetAvailable = false;
        publishDrawTable();
        drawModeAtomic.store(false, std::memory_order_release);
        return true;
    }

    // =====================================================================
    // Shape generators
    // =====================================================================
    static float bell(float p, float center, float width) {
        float d = (p - center) / width;
        return std::exp(-0.5f * d * d);
    }

    static float shapeValue(int index, float p) {
        p = clampValue(p, 0.f, 1.f);
        switch (index) {
            case 0: return (std::exp(-9.f * p) - 1.2341e-4f) / (1.f - 1.2341e-4f);
            case 1: return 1.f - p;
            case 2: { float attack = 0.08f;
                return p < attack ? p / attack : 1.f - (p - attack) / (1.f - attack); }
            case 3: return 1.f - std::fabs(2.f * p - 1.f);
            case 4: return std::sin(M_PI * p);
            case 5: return bell(p, 0.5f, 0.15f);
            case 6: return p;
            case 7: return p * p * p;
            case 8: return std::max(bell(p, 0.28f, 0.1f), 0.75f * bell(p, 0.7f, 0.1f));
            case 9: return (1.f - p) * (0.6f + 0.4f * std::cos(2.f * M_PI * 4.f * p));
            case 10: return std::pow(1.f - p, 1.5f) * std::fabs(std::cos(M_PI * 3.5f * p));
            case 11: return std::min(static_cast<float>(static_cast<int>(p * 5.f)), 4.f) / 4.f;
            case 12: return 1.f - std::min(static_cast<float>(static_cast<int>(p * 5.f)), 4.f) / 4.f;
            case 13: return (std::fmod(p * 6.f, 1.f) < 0.5f ? 1.f : 0.f) * (1.f - p);
            case 14: return 0.5f + 0.5f * std::sin(2.f * M_PI * 3.f * p) * std::exp(-2.f * p);
            case 15: return clampValue(
                    0.5f + 0.35f * std::sin(2.f * M_PI * 1.7f * p + 1.f) +
                    0.25f * std::sin(2.f * M_PI * 3.1f * p + 2.f) +
                    0.15f * std::sin(2.f * M_PI * 5.3f * p + 4.f), 0.f, 1.f);
            case 16: return p * p * (3.f - 2.f * p);
            case 17: return (p > 0.375f && p < 0.625f) ? 1.f : 0.f;
            case 18: { float t = std::fmod(p * 3.f, 1.f);
                return 1.f - (2.f * t - 1.f) * (2.f * t - 1.f); }
            case 19: { if (p < 0.02f) return p / 0.02f;
                return std::exp(-6.f * (p - 0.02f)); }
            case 20: { if (p < 0.9f) { float t = p / 0.9f; return t * t; }
                return 1.f - (p - 0.9f) / 0.1f; }
            case 21: return 1.f - std::min(static_cast<float>(static_cast<int>(p * 8.f)), 7.f) / 7.f;
            case 22: { if (p < 0.25f) return p / 0.25f;
                if (p < 0.75f) return 1.f;
                return 1.f - (p - 0.75f) / 0.25f; }
            case 23: return std::max(bell(p, 0.3f, 0.12f), bell(p, 0.7f, 0.12f));
            default: return 0.f;
        }
    }

    static float envelopeValue(const float* table, float shape, float warp,
                               float flux, float p) {
        p = clampValue(p, 0.f, 1.f);
        float warpExponent = std::pow(2.f, 2.5f * warp);
        float warpedPhase = std::pow(p, warpExponent);
        float value = 0.f;
        if (table) {
            float x = warpedPhase * static_cast<float>(TABLE_N - 1);
            int index = std::min(static_cast<int>(x), TABLE_N - 2);
            float fraction = x - static_cast<float>(index);
            value = table[index] + (table[index + 1] - table[index]) * fraction;
        } else {
            float shapePosition = shape * static_cast<float>(NUM_SHAPES - 1);
            int shape0 = clampInt(static_cast<int>(shapePosition), 0, NUM_SHAPES - 1);
            int shape1 = std::min(shape0 + 1, NUM_SHAPES - 1);
            float morph = shapePosition - static_cast<float>(shape0);
            float value0 = shapeValue(shape0, warpedPhase);
            float value1 = shapeValue(shape1, warpedPhase);
            value = value0 + (value1 - value0) * morph;
        }
        float ripple = 0.5f - 0.5f * std::cos(2.f * M_PI * 8.f * warpedPhase);
        value *= 1.f - flux * ripple;
        return clampValue(value, 0.f, 1.f);
    }

    float randomizedEnvelope(float shape, float warp, float flux,
                             float randomness, float p, bool useAudioTable) const {
        const float* table = nullptr;
        if (useAudioTable) {
            if (drawModeAtomic.load(std::memory_order_acquire) &&
                audioDrawTableVersion.load(std::memory_order_relaxed) != INVALID_SEQ) {
                table = audioDrawTable;
            }
        } else {
            if (drawModeAtomic.load(std::memory_order_relaxed)) {
                table = drawTable;
            }
        }

        float base = envelopeValue(table, shape, warp, flux, p);
        if (randomness <= 0.f) return base;

        uint32_t seed = envelopeCycleSeed.load(std::memory_order_relaxed);
        float phaseOff = (static_cast<float>(seed & 0xFFFFu) / 65535.f) * 6.2831853f;

        float noise =
            0.50f + 0.25f * std::sin(2.f * M_PI * 1.73f * p + 1.31f + phaseOff) +
            0.15f * std::sin(2.f * M_PI * 3.91f * p + 2.73f + phaseOff * 1.3f) +
            0.10f * std::sin(2.f * M_PI * 7.17f * p + 4.91f + phaseOff * 0.7f);
        noise = clampValue(noise, 0.f, 1.f);

        float k = randomness * 0.9f;
        float gain = 1.f - k * noise;
        return clampValue(base * gain, 0.f, 1.f);
    }

    // =====================================================================
    // Gate engine
    // =====================================================================
    static StepDNA styleDNA(int style, int step) {
        static const StepDNA darkpsy[16] = {
            {1.00f, 0.30f}, {0.00f, 0.00f}, {0.75f, 0.12f}, {0.85f, 0.15f},
            {0.70f, 0.12f}, {0.85f, 0.15f}, {0.75f, 0.12f}, {0.90f, 0.18f},
            {1.00f, 0.30f}, {0.00f, 0.00f}, {0.75f, 0.12f}, {0.85f, 0.15f},
            {0.80f, 0.10f}, {0.85f, 0.10f}, {0.90f, 0.10f}, {1.00f, 0.20f},
        };
        static const StepDNA psytrance[16] = {
            {1.00f, 0.30f}, {0.85f, 0.15f}, {0.90f, 0.15f}, {0.70f, 0.15f},
            {0.00f, 0.00f}, {0.85f, 0.15f}, {0.90f, 0.15f}, {0.70f, 0.15f},
            {1.00f, 0.30f}, {0.85f, 0.15f}, {0.90f, 0.15f}, {0.70f, 0.15f},
            {0.00f, 0.00f}, {0.85f, 0.15f}, {0.90f, 0.15f}, {0.60f, 0.10f},
        };
        static const StepDNA hitech[16] = {
            {1.00f, 0.15f}, {0.70f, 0.08f}, {0.75f, 0.08f}, {0.70f, 0.08f},
            {0.90f, 0.12f}, {0.70f, 0.08f}, {0.75f, 0.08f}, {0.70f, 0.08f},
            {0.90f, 0.12f}, {0.70f, 0.08f}, {0.75f, 0.08f}, {0.70f, 0.08f},
            {0.90f, 0.12f}, {0.70f, 0.08f}, {0.75f, 0.08f}, {0.85f, 0.10f},
        };
        static const StepDNA breakbeat[16] = {
            {1.00f, 0.30f}, {0.00f, 0.00f}, {0.00f, 0.00f}, {0.30f, 0.10f},
            {1.00f, 0.25f}, {0.00f, 0.00f}, {0.60f, 0.15f}, {0.00f, 0.00f},
            {1.00f, 0.30f}, {0.30f, 0.10f}, {1.00f, 0.25f}, {0.00f, 0.00f},
            {1.00f, 0.25f}, {0.00f, 0.00f}, {0.60f, 0.15f}, {0.00f, 0.00f},
        };
        if (style < 0) style = 0;
        else if (style > 3) style = 3;
        if (step < 0) step = 0;
        else if (step > 15) step = 15;
        switch (style) {
            case 0: return darkpsy[step];
            case 1: return psytrance[step];
            case 2: return hitech[step];
            default: return breakbeat[step];
        }
    }

    void onPhraseBoundary() {
        float fillAmt = clampValue(params[GATE_FILL_PARAM].getValue(), 0.f, 1.f);
        float r = randomUnipolar();
        if (gateBar > 0 && gateBar % 32 == 0) {
            if (r < fillAmt * 0.9f)              gateVar = V_FILL32;
            else if (r < fillAmt * 0.9f + 0.3f)  gateVar = V_ROLL;
            else                                 gateVar = V_ORIG;
        } else if (gateBar > 0 && gateBar % 16 == 0) {
            if (r < fillAmt * 0.6f)              gateVar = V_ROLL;
            else if (r < fillAmt * 0.6f + 0.2f)  gateVar = V_GHOST;
            else                                 gateVar = V_ORIG;
        } else if (gateBar > 0 && gateBar % 8 == 0) {
            if (r < fillAmt * 0.4f)              gateVar = V_GHOST;
            else if (r < fillAmt * 0.4f + 0.15f) gateVar = V_DROP;
            else                                 gateVar = V_ORIG;
        } else if (gateBar > 0 && gateBar % 4 == 0) {
            if (r < fillAmt * 0.2f)              gateVar = V_GHOST;
            else                                 gateVar = V_ORIG;
        } else {
            gateVar = V_ORIG;
        }
    }

    // =====================================================================
    // Evaluate step (with all three gate modes)
    // =====================================================================
    void evaluateStep(float stepDur) {
        float prob = clampValue(params[GATE_PROBABILITY_PARAM].getValue(), 0.f, 1.f);
        int modeIdx = clampInt(
            static_cast<int>(std::round(params[GATE_MODE_PARAM].getValue())),
            0, NUM_GATE_MODES - 1);
        GateMode mode = static_cast<GateMode>(modeIdx);

        if (prob <= 0.001f) { gateWidthFrac = 0.f; return; }

        float gateLen = clampValue(params[GATE_LENGTH_PARAM].getValue(), 0.01f, 2.f);

        // ============================================================
        // GATE_RANDOM_MODE: pure random gate, independent of DNA
        // ============================================================
        if (mode == GATE_RANDOM_MODE) {
            if (randomUnipolar() >= prob) { gateWidthFrac = 0.f; return; }

            float widthMul = 0.15f + randomUnipolar() * 0.45f;
            float widthFrac = 0.30f * widthMul * gateLen * GATE_WIDTH_FACTOR;

            float absoluteMin = std::max(MIN_GATE_FRAC,
                MIN_GATE_WIDTH_SEC / std::max(stepDur, 1e-6f));
            absoluteMin = std::min(absoluteMin, 0.30f);
            gateWidthFrac = clampValue(widthFrac, absoluteMin, RANDOM_MAX_GATE_FRAC);
            return;
        }

        // Compute time-based lowerBound for Pattern and Hybrid modes
        float effectiveMinWidthSec = MIN_GATE_WIDTH_SEC;
        if (stepDur < 0.020f) effectiveMinWidthSec = 0.0015f;
        if (stepDur < 0.010f) effectiveMinWidthSec = 0.0008f;
        if (stepDur < 0.005f) effectiveMinWidthSec = 0.0004f;
        float minFracByTime = (stepDur > 1e-6f)
            ? (effectiveMinWidthSec / stepDur) : MIN_GATE_FRAC;
        float lowerBound = std::max(MIN_GATE_FRAC, minFracByTime);
        if (lowerBound > MAX_GATE_FRAC) lowerBound = MAX_GATE_FRAC;

        // Compute DNA (only used in Pattern and Hybrid modes)
        int style = clampInt(
            static_cast<int>(std::round(params[GATE_PATTERN_PARAM].getValue())),
            0, NUM_GATE_PATTERNS - 1);
        StepDNA dna = styleDNA(style, gateStep);

        // ============================================================
        // prob = 100%: force trigger, respect gateVar and DNA width
        // === FIX: Hybrid mode at 100% behaves exactly like Pattern mode
        //          (no random fill on rest steps) ===
        // ============================================================
        if (prob >= 0.999f) {
            gateWidthFrac = 0.f;
            if (dna.width <= 0.001f) return;  // DNA rest step

            float widthMul = 1.f;
            switch (gateVar) {
                case V_ORIG: break;
                case V_GHOST:
                    if (dna.trigger < 0.3f) widthMul = 0.5f;
                    else if (dna.trigger < 0.7f) widthMul = 0.7f;
                    break;
                case V_FILL32:
                    if (gateStep >= 8) widthMul = 0.35f;
                    break;
                case V_DROP:
                    if ((gateStep & 1) != 0) widthMul = 0.85f;
                    break;
                case V_ROLL:
                    if (gateStep >= 12) widthMul = 0.5f;
                    break;
            }

            float widthFrac = dna.width * widthMul * gateLen * GATE_WIDTH_FACTOR;
            gateWidthFrac = clampValue(widthFrac, lowerBound, MAX_GATE_FRAC);
            return;
        }

        // ============================================================
        // GATE_HYBRID_MODE: DNA + per-step random jitter
        // ============================================================
        if (mode == GATE_HYBRID_MODE) {
            if (dna.width <= 0.001f && dna.trigger <= 0.001f) {
                if (randomUnipolar() >= prob * 0.15f) {
                    gateWidthFrac = 0.f;
                    return;
                }
                float widthMul = 0.3f + randomUnipolar() * 0.5f;
                float widthFrac = 0.15f * widthMul * gateLen * GATE_WIDTH_FACTOR;
                gateWidthFrac = clampValue(widthFrac, lowerBound, MAX_GATE_FRAC);
                return;
            }
            float p = dna.trigger * prob;
            p += (randomUnipolar() - 0.5f) * 0.15f;
            p = clampValue(p, 0.f, 1.f);
            if (randomUnipolar() >= p) { gateWidthFrac = 0.f; return; }

            float widthMul = 0.7f + randomUnipolar() * 0.6f;
            switch (gateVar) {
                case V_ORIG: break;
                case V_GHOST:
                    if (dna.trigger < 0.7f) widthMul *= 0.7f;
                    break;
                case V_FILL32:
                    if (gateStep >= 8) widthMul *= 0.35f;
                    break;
                case V_DROP:
                    if ((gateStep & 1) != 0) widthMul *= 0.85f;
                    break;
                case V_ROLL:
                    if (gateStep >= 12) widthMul *= 0.5f;
                    break;
            }
            float widthFrac = dna.width * widthMul * gateLen * GATE_WIDTH_FACTOR;
            gateWidthFrac = clampValue(widthFrac, lowerBound, MAX_GATE_FRAC);
            return;
        }

        // ============================================================
        // GATE_PATTERN_MODE (default): DNA × probability with gateVar
        // ============================================================
        float triggerProb = dna.trigger;
        float widthMul = 1.f;

        switch (gateVar) {
            case V_ORIG: break;
            case V_GHOST:
                if (dna.trigger < 0.3f) {
                    triggerProb = std::max(dna.trigger, 0.40f); widthMul = 0.5f;
                } else if (dna.trigger < 0.7f) {
                    triggerProb = std::min(1.f, dna.trigger * 1.2f); widthMul = 0.7f;
                }
                break;
            case V_FILL32:
                if (gateStep >= 8) { triggerProb = 1.f; widthMul = 0.35f; }
                break;
            case V_DROP:
                if ((gateStep & 1) != 0) triggerProb *= 0.15f;
                break;
            case V_ROLL:
                if (gateStep >= 12) {
                    triggerProb = std::min(1.f, triggerProb * 1.6f);
                    widthMul = 0.5f;
                }
                break;
        }

        float p;
        if (triggerProb < 0.05f) {
            p = prob * 0.55f;
        } else if (triggerProb >= 0.95f) {
            p = (prob > 0.10f) ? 1.0f : triggerProb * prob;
        } else {
            p = triggerProb * prob;
        }

        if (randomUnipolar() >= p) { gateWidthFrac = 0.f; return; }

        float widthFrac = dna.width * widthMul * gateLen * GATE_WIDTH_FACTOR;
        gateWidthFrac = clampValue(widthFrac, lowerBound, MAX_GATE_FRAC);
    }

    void updateGateEngine(const ProcessArgs& args) {
        if (!inputs[ON_INPUT].isConnected() || !haveClockInterval) {
            gatePulseState = false;
            gateWidthFrac  = 0.f;
            gateLastStepDur = 0.f;
            dispGateQuarterDur = 0.f;
            dispGateStepDur = 0.f;
            outputs[GATE_OUTPUT].setVoltage(0.f);
            outputs[GATE_OUTPUT_INVERTED].setVoltage(0.f);
            return;
        }

        int currentMode = clampInt(
            static_cast<int>(std::round(params[GATE_MODE_PARAM].getValue())),
            0, NUM_GATE_MODES - 1);
        if (currentMode != lastGateMode) {
            gateStep = 0;
            gateStepPhase = 0.f;
            gateWidthFrac = 0.f;
            lastGateMode = currentMode;
        }

        int inputPpqnIdx = clampInt(
            static_cast<int>(std::round(params[INPUT_CLOCK_PPQN_PARAM].getValue())),
            0, NUM_INPUT_PPQN - 1);
        int inputPpqn = inputPpqnToInt(inputPpqnIdx);
        int effectivePpqn = (inputPpqn == 0) ? onInputDetectedPpqn : inputPpqn;
        if (effectivePpqn < 1) effectivePpqn = DEFAULT_DETECTED_PPQN;

        if (effectivePpqn != lastUsedPpqn) {
            gateStepPhase = 0.f;
            gateLastStepDur = 0.f;
            lastUsedPpqn = effectivePpqn;
        }

        float quarterDur = lastClockInterval * static_cast<float>(effectivePpqn);
        quarterDur = clampValue(quarterDur, 2e-5f, 8.f);

        int speedIdx = clampInt(
            static_cast<int>(std::round(params[RANDOM_GATE_SPEED_PARAM].getValue())),
            0, NUM_GATE_SPEEDS - 1);
        static constexpr float speedBeatMults[NUM_GATE_SPEEDS] = {
            4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f
        };
        float stepDur = quarterDur * speedBeatMults[speedIdx];
        stepDur = std::max(stepDur, 1e-5f);

        float swing = clampValue(params[SWING_PARAM].getValue(), 0.f, 1.f);
        float swingOffset = (gateStep & 1) ? (swing * 0.5f) : 0.f;

        if (gateLastStepDur > 0.f) {
            float durRatio = stepDur / gateLastStepDur;
            if (durRatio > 8.0f || durRatio < 0.125f) gateStepPhase = 0.f;
        }
        gateLastStepDur = stepDur;

        gateStepPhase += args.sampleTime / stepDur;

        while (gateStepPhase >= 1.f) {
            gateStepPhase -= 1.f;
            gateStep = (gateStep + 1) & 0x0F;
            if (gateStep == 0) {
                gateBar++;
                if (gateBar % 4 == 0) onPhraseBoundary();
            }
            evaluateStep(stepDur);
        }

        float w = gateWidthFrac;
        float effectivePhase = gateStepPhase - swingOffset;
        bool mainState = (w > 0.f) && (effectivePhase >= 0.f) && (effectivePhase < w);
        bool invState  = (w > 0.f) && (effectivePhase >= (1.f - w));
        gatePulseState = mainState;

        outputs[GATE_OUTPUT].setVoltage(mainState ? GATE_HIGH_VOLTAGE : 0.f);
        outputs[GATE_OUTPUT_INVERTED].setVoltage(invState ? GATE_HIGH_VOLTAGE : 0.f);

        dispGateQuarterDur = quarterDur;
        dispGateStepDur = stepDur;
        dispGateInputPpqn = effectivePpqn;
    }

    // =====================================================================
    // Legacy engine
    // =====================================================================
    float generateLegacySample() {
        float randomValue = randomFloat() * params[STRENGTH_PARAM].getValue();
        float legacyOutputVoltage = params[OUTPUT_VOLTAGE_PARAM].getValue();
        bool legacyBipolar = params[BIPOLAR_PARAM].getValue() > 0.5f;
        if (legacyBipolar) randomValue *= legacyOutputVoltage;
        else randomValue = (randomValue + 1.0f) * 0.5f * legacyOutputVoltage;
        randomValue = clampValue(randomValue,
            legacyBipolar ? -legacyOutputVoltage : 0.0f, legacyOutputVoltage);
        randomValue += params[S_H_OFFSET_PARAM].getValue();
        return randomValue;
    }

    int getLegacyDivider() {
        int divider = static_cast<int>(std::round(params[TRIGGER_DIVISION_DIVIDER_PARAM].getValue()));
        return std::max(1, std::min(divider, 3));
    }

    void updateLegacyRandomOutput(const ProcessArgs& args) {
        float smooth = params[SMOOTH_RANDOM_PARAM].getValue();
        if (smooth <= SMOOTH_DEADZONE) {
            currentOutput = targetRandomValue;
            smoothActive = false;
            return;
        }
        smooth = clampValue(smooth, 0.f, 1.f);
        smoothActive = true;
        float clockPeriod = haveClockInterval ? lastClockInterval : 0.25f;
        clockPeriod = clampValue(clockPeriod, MIN_CLOCK_INTERVAL, MAX_CLOCK_INTERVAL);
        float curve = smooth * smooth;
        float transitionTime = std::max(clockPeriod * (0.02f + 0.98f * curve), MIN_SMOOTH_TIME);
        float coefficient = 1.f - std::exp(-args.sampleTime / transitionTime);
        coefficient = clampValue(coefficient, 0.f, 1.f);
        float diff = targetRandomValue - currentOutput;
        if (std::fabs(diff) < 1e-15f) diff = 0.f;
        currentOutput += diff * coefficient;
    }

    void processLegacy(const ProcessArgs& args) {
        outputVoltage = params[OUTPUT_VOLTAGE_PARAM].getValue();
        bipolarOutput = params[BIPOLAR_PARAM].getValue() > 0.5f;
        float modulation = 0.f;
        if (inputs[MODULATION_INPUT].isConnected())
            modulation = inputs[MODULATION_INPUT].getVoltage() * params[MODULATION_INPUT_PARAM].getValue();
        float onVoltage = inputs[ON_INPUT].isConnected() ? inputs[ON_INPUT].getVoltage() : 0.f;

        int divider = getLegacyDivider();
        if (divider != lastDivider) {
            triggerCounter = 0;
            lastDivider = divider;
        }

        if (onVoltage >= CLOCK_HIGH_THRESHOLD && !sampleNext) {
            sampleNext = true;
            if (clockTimer > 0.0001f) {
                lastClockInterval = clampValue(clockTimer, MIN_CLOCK_INTERVAL, MAX_CLOCK_INTERVAL);
                haveClockInterval = true;
                updateOnInputPpqnDetection(lastClockInterval);
            }
            clockTimer = 0.f;
            triggerCounter++;
            if (triggerCounter >= divider) {
                triggerCounter = 0;
                targetRandomValue = generateLegacySample();
                targetRandomValue += modulation;
                bool bipolar = params[BIPOLAR_PARAM].getValue() > 0.5f;
                targetRandomValue = clampValue(targetRandomValue,
                    bipolar ? -outputVoltage : 0.f, outputVoltage);
            }
        }
        if (onVoltage <= CLOCK_LOW_THRESHOLD && sampleNext) sampleNext = false;

        if (inputs[ON_INPUT].isConnected()) {
            clockTimer += args.sampleTime;
            if (haveClockInterval) {
                float silenceTimeout = std::max(0.5f, lastClockInterval * 6.f);
                if (clockTimer > silenceTimeout) {
                    haveClockInterval = false;
                    resetPpqnDetection();
                    gateStepPhase = 0.f;
                    gateLastStepDur = 0.f;
                    gateWidthFrac = 0.f;
                    lastUsedPpqn = -1;
                    lastGateMode = -1;
                }
            }
        } else {
            clockTimer = 0.f;
            haveClockInterval = false;
            onInputPulseCount = 0;
            lastUsedPpqn = -1;
            lastGateMode = -1;
        }

        updateLegacyRandomOutput(args);
        outputs[OUTPUT].setVoltage(currentOutput);
        updateGateEngine(args);

        {
            float clockPeriod = haveClockInterval ? lastClockInterval : 0.25f;
            clockPeriod = clampValue(clockPeriod, MIN_CLOCK_INTERVAL, MAX_CLOCK_INTERVAL);
            float sampleInterval = clockPeriod / static_cast<float>(SH_DISPLAY_COUNT);
            if (sampleInterval < 1e-5f) sampleInterval = 1e-5f;

            shSampleTimer += args.sampleTime;
            while (shSampleTimer >= sampleInterval) {
                shSampleTimer -= sampleInterval;
                shHistory[shHistoryWritePos] = currentOutput;
                shHistoryWritePos = (shHistoryWritePos + 1) % SH_HISTORY_SIZE;
                if (shHistoryCount < SH_HISTORY_SIZE) shHistoryCount++;
            }
            dispShValue = currentOutput;
        }
    }

    // =====================================================================
    // BPM / clock tracking
    // =====================================================================
    void updateBpmFromClock(const ProcessArgs& args) {
        float onVoltage = inputs[ENVELOPE_CLOCK_INPUT].isConnected()
            ? inputs[ENVELOPE_CLOCK_INPUT].getVoltage() : 0.f;

        int ppqnIdx = static_cast<int>(std::round(params[CLOCK_PPQN_PARAM].getValue()));
        int ppqnVal = ppqnToInt(ppqnIdx);
        bool autoMode = (ppqnVal == 0);

        if (onVoltage >= CLOCK_HIGH_THRESHOLD && !envelopeClockHigh) {
            envelopeClockHigh = true;
            if (envelopeClockTimer > 0.005f) {
                int effectivePpqn = autoMode ? autoDetectedPpqn : ppqnVal;
                float secondsPerBeat = envelopeClockTimer * static_cast<float>(effectivePpqn);
                float instantBpm = 60.f / secondsPerBeat;
                if (instantBpm > 5.f && instantBpm < 400.f) {
                    if (!haveBpmMeasurement) {
                        measuredBpm = instantBpm;
                        haveBpmMeasurement = true;
                    } else {
                        float ratio = instantBpm / measuredBpm;
                        if (ratio > 0.4f && ratio < 2.5f) {
                            measuredBpm = measuredBpm * (1.f - BPM_SMOOTHING)
                                        + instantBpm * BPM_SMOOTHING;
                        } else {
                            measuredBpm = instantBpm;
                        }
                    }
                    float beatPeriod = 60.f / measuredBpm;
                    bpmTimeout = std::max(BPM_TIMEOUT_SECONDS, beatPeriod * 4.f);
                }

                if (autoMode) {
                    const int* candidates = ppqnCandidates();
                    const int numCand = NUM_PPQN_CANDIDATES;
                    float bestScore = 1e9f;
                    int bestPpqn = autoDetectedPpqn;
                    for (int i = 0; i < numCand; i++) {
                        int c = candidates[i];
                        float spb = envelopeClockTimer * static_cast<float>(c);
                        if (spb < 0.001f) continue;
                        float bpm = 60.f / spb;
                        if (bpm > 55.f && bpm < 210.f) {
                            float score = std::fabs(bpm - 132.5f);
                            if (score < bestScore) { bestScore = score; bestPpqn = c; }
                        }
                    }
                    if (bestScore < 1e8f) autoDetectedPpqn = bestPpqn;
                }
            }
            envelopeClockTimer = 0.f;
        } else if (onVoltage <= CLOCK_LOW_THRESHOLD && envelopeClockHigh) {
            envelopeClockHigh = false;
        }

        if (inputs[ENVELOPE_CLOCK_INPUT].isConnected()) {
            envelopeClockTimer += args.sampleTime;
            bpmTimeout -= args.sampleTime;
            if (bpmTimeout <= 0.f) {
                bpmTimeout = 0.f;
                haveBpmMeasurement = false;
            }
        } else {
            envelopeClockTimer = 0.f;
            bpmTimeout = 0.f;
            haveBpmMeasurement = false;
            if (measuredBpm <= 0.f) measuredBpm = DEFAULT_BPM;
        }
    }

    // =====================================================================
    // Enhanced envelope
    // =====================================================================
    void processEnhanced(const ProcessArgs& args) {
        updateBpmFromClock(args);

        syncMode = params[TIME_SYNC_MODE_PARAM].getValue() > 0.5f;

        float time = params[TIME_PARAM].getValue();
        if (!syncMode && inputs[TIME_INPUT].isConnected())
            time += inputs[TIME_INPUT].getVoltage() / 10.f;
        time = clampValue(time, 0.f, 1.f);

        float shape = params[SHAPE_PARAM].getValue();
        if (inputs[SHAPE_INPUT].isConnected())
            shape += inputs[SHAPE_INPUT].getVoltage() / 10.f;
        shape = clampValue(shape, 0.f, 1.f);

        float warp = params[WARP_PARAM].getValue();
        if (inputs[WARP_INPUT].isConnected())
            warp += inputs[WARP_INPUT].getVoltage() / 5.f;
        warp = clampValue(warp, -1.f, 1.f);

        float flux = params[FLUX_PARAM].getValue();
        if (inputs[FLUX_INPUT].isConnected())
            flux += inputs[FLUX_INPUT].getVoltage() / 10.f;
        flux = clampValue(flux, 0.f, 1.f);

        float randomness = params[RANDOMNESS_PARAM].getValue();
        if (inputs[RANDOMNESS_INPUT].isConnected())
            randomness += inputs[RANDOMNESS_INPUT].getVoltage() / 10.f;
        randomness = clampValue(randomness, 0.f, 1.f);

        float level = clampValue(params[LEVEL_PARAM].getValue(), 0.f, 1.f);
        bool loop = params[LOOP_PARAM].getValue() > 0.5f;

        bool externalTrigger = false;
        if (inputs[TRIG_INPUT].isConnected())
            externalTrigger = trigTrigger.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 1.f);
        bool manualTrigger = buttonTrigger.process(params[TRIG_BUTTON_PARAM].getValue() > 0.f);
        bool trigger = externalTrigger || manualTrigger;

        if (trigger) {
            envelopePhase = 0.0;
            envelopeCycleSeed.store(nextRandomU32(), std::memory_order_relaxed);
            running = true;
        }
        if (loop && !running) {
            if (envelopePhase >= 1.0) envelopePhase = 0.0;
            running = true;
        }

        double duration;
        if (syncMode) {
            currentDivisionIndex = timeParamToDivisionIndex(time);
            int multIdx = static_cast<int>(std::round(params[BPM_MULTIPLIER_PARAM].getValue()));
            float beats = divisionToBeats(currentDivisionIndex);
            float mult = multiplierToFactor(multIdx);
            duration = static_cast<double>(beats * mult * 60.f / measuredBpm);
        } else {
            duration = 0.005 * std::pow(360000.0, static_cast<double>(time));
            currentDivisionIndex = timeParamToDivisionIndex(time);
        }

        if (running) {
            envelopePhase += args.sampleTime / duration;
            if (envelopePhase >= 1.0) {
                eocPulse.trigger(1e-3f);
                envelopeCycleSeed.store(nextRandomU32(), std::memory_order_relaxed);
                if (loop) {
                    envelopePhase -= std::floor(envelopePhase);
                } else {
                    envelopePhase = 1.0;
                    running = false;
                }
            }
        }

        float phaseF = static_cast<float>(clampValue(static_cast<float>(envelopePhase), 0.f, 1.f));
        float value = randomizedEnvelope(shape, warp, flux, randomness, phaseF, true);
        value *= level;

        bool bipolar = params[BIPOLAR_PARAM].getValue() > 0.5f;
        float rangeVoltage = params[ENVELOPE_RANGE_PARAM].getValue();
        float output = 0.f;
        if (bipolar) output = (value * 2.f - 1.f) * rangeVoltage;
        else         output = value * rangeVoltage;
        output += params[ENVELOPE_OFFSET_PARAM].getValue();
        output = clampValue(output, -15.f, 15.f);

        outputs[ENVELOPE_OUTPUT].setVoltage(output);
        outputs[EOC_OUTPUT].setVoltage(eocPulse.process(args.sampleTime) ? 10.f : 0.f);

        dispTime = time;
        dispShape = shape;
        dispWarp = warp;
        dispFlux = flux;
        dispRandomness = randomness;
        dispPhase = envelopePhase;
        dispDuration = static_cast<float>(duration);
        dispLoop = loop;
        dispBpm = measuredBpm;
        dispDivisionIndex = currentDivisionIndex;
        dispSyncMode = syncMode;
        dispMultiplierIdx = static_cast<int>(std::round(params[BPM_MULTIPLIER_PARAM].getValue()));
        dispPpqnIdx = static_cast<int>(std::round(params[CLOCK_PPQN_PARAM].getValue()));
    }

    // =====================================================================
    // Main process
    // =====================================================================
    void process(const ProcessArgs& args) override {
        updateAudioDrawTable();

        processLegacy(args);
        if (params[ENHANCED_MODE_PARAM].getValue() > 0.5f) {
            processEnhanced(args);
        } else {
            outputs[ENVELOPE_OUTPUT].setVoltage(0.f);
            outputs[EOC_OUTPUT].setVoltage(0.f);
        }
    }

    // =====================================================================
    // Reset
    // =====================================================================
    void onReset() override {
        rngState = 0x7A31C9D5u;
        outputVoltage = 0.f;
        sampleNext = false;
        envelopePhase = 1.0;
        envelopeCycleSeed.store(0x9E3779B9u, std::memory_order_relaxed);
        running = false;
        eocPulse.reset();
        trigTrigger.reset();
        buttonTrigger.reset();
        clockTimer = 0.f;
        lastClockInterval = 0.25f;
        haveClockInterval = false;
        triggerCounter = 0;
        lastDivider = 1;
        currentOutput = 0.f;
        targetRandomValue = 0.f;
        smoothActive = false;

        gateStepPhase = 0.f;
        gateStep = 0;
        gateBar = 0;
        gateVar = V_ORIG;
        gateWidthFrac = 0.f;
        gatePulseState = false;
        lastGateMode = -1;

        resetPpqnDetection();

        gateLastStepDur = 0.f;
        lastUsedPpqn = -1;

        dispGateStepDur = 0.f;
        dispGateQuarterDur = 0.f;
        dispGateInputPpqn = DEFAULT_DETECTED_PPQN;

        syncMode = false;
        measuredBpm = DEFAULT_BPM;
        bpmTimeout = 0.f;
        envelopeClockHigh = false;
        haveBpmMeasurement = false;
        envelopeClockTimer = 0.f;
        currentDivisionIndex = 2;
        dispBpm = DEFAULT_BPM;
        dispDivisionIndex = 2;
        dispSyncMode = false;
        dispMultiplierIdx = 2;
        dispPpqnIdx = 0;
        autoDetectedPpqn = DEFAULT_DETECTED_PPQN;

        float initVal = 0.f;
        for (int i = 0; i < SH_HISTORY_SIZE; i++) shHistory[i] = initVal;
        shHistoryWritePos = 0;
        shHistoryCount = 0;
        shSampleTimer = 0.f;
        dispShValue = initVal;

        outputs[OUTPUT].setVoltage(0.f);
        outputs[ENVELOPE_OUTPUT].setVoltage(0.f);
        outputs[EOC_OUTPUT].setVoltage(0.f);
        outputs[GATE_OUTPUT].setVoltage(0.f);
        outputs[GATE_OUTPUT_INVERTED].setVoltage(0.f);

        initDrawTable();
        publishDrawTable();
        drawModeAtomic.store(false, std::memory_order_release);
        audioDrawTableVersion.store(INVALID_SEQ, std::memory_order_relaxed);
        for (int i = 0; i < TABLE_N; i++) audioDrawTable[i] = drawTable[i];
        clearDrawHistory();
    }

    // =====================================================================
    // Serialization
    // =====================================================================
    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        // === FIX: save module version for future migrations ===
        json_object_set_new(rootJ, "moduleVersion", json_integer(CURRENT_MODULE_VERSION));

        json_object_set_new(rootJ, "enhancedMode",
            json_boolean(params[ENHANCED_MODE_PARAM].getValue() > 0.5f));

        bool dm = drawModeAtomic.load(std::memory_order_relaxed);
        json_object_set_new(rootJ, "drawMode", json_boolean(dm));
        json_object_set_new(rootJ, "shapeVersion", json_integer(NUM_SHAPES));

        if (dm) {
            json_t* tableJ = json_array();
            for (int i = 0; i < TABLE_N; i++)
                json_array_append_new(tableJ, json_real(drawTable[i]));
            json_object_set_new(rootJ, "drawTable", tableJ);

            json_t* resetJ = json_array();
            for (int i = 0; i < TABLE_N; i++)
                json_array_append_new(resetJ, json_real(drawResetTable[i]));
            json_object_set_new(rootJ, "drawResetTable", resetJ);
            json_object_set_new(rootJ, "drawResetAvailable",
                json_boolean(drawResetAvailable));
        }

        json_object_set_new(rootJ, "measuredBpm", json_real(measuredBpm));
        json_object_set_new(rootJ, "haveBpmMeasurement",
            json_boolean(haveBpmMeasurement));
        json_object_set_new(rootJ, "autoDetectedPpqn", json_integer(autoDetectedPpqn));
        json_object_set_new(rootJ, "onInputDetectedPpqn",
            json_integer(onInputDetectedPpqn));

        json_object_set_new(rootJ, "gateBar", json_integer(gateBar));
        json_object_set_new(rootJ, "gateStep", json_integer(gateStep));
        json_object_set_new(rootJ, "gateVar", json_integer(static_cast<int>(gateVar)));

        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        // === FIX: migrate v1.0 patches (no moduleVersion field) ===
        json_t* versionJ = json_object_get(rootJ, "moduleVersion");
        int savedVersion = versionJ
            ? static_cast<int>(json_integer_value(versionJ))
            : 1;
        if (savedVersion < CURRENT_MODULE_VERSION) {
            // v1.0 RANDOM_GATE_SPEED: 1=slow, 2=medium, 3=fast
            // v2.0 RANDOM_GATE_SPEED: 0=1/1, 1=1/2, 2=1/4, 3=1/8, 4=1/16, 5=1/32
            // Map old musical intent to closest new value:
            //   1 (slow)   -> 1 (1/2)
            //   2 (medium) -> 2 (1/4)
            //   3 (fast)   -> 3 (1/8)
            float oldSpeed = params[RANDOM_GATE_SPEED_PARAM].getValue();
            int oldIdx = static_cast<int>(std::round(oldSpeed));
            int newIdx;
            switch (oldIdx) {
                case 1: newIdx = 1; break;
                case 2: newIdx = 2; break;
                case 3: newIdx = 3; break;
                default: newIdx = 1; break;
            }
            params[RANDOM_GATE_SPEED_PARAM].setValue(static_cast<float>(newIdx));
        }

        json_t* enhancedJ = json_object_get(rootJ, "enhancedMode");
        if (enhancedJ) {
            bool enhanced = json_boolean_value(enhancedJ);
            params[ENHANCED_MODE_PARAM].setValue(enhanced ? 1.f : 0.f);
        }

        bool dm = false;
        json_t* drawModeJ = json_object_get(rootJ, "drawMode");
        if (drawModeJ) dm = json_boolean_value(drawModeJ);

        json_t* shapeVersionJ = json_object_get(rootJ, "shapeVersion");
        int savedShapeVersion = shapeVersionJ
            ? static_cast<int>(json_integer_value(shapeVersionJ))
            : NUM_SHAPES;
        bool shapeSetChanged = (savedShapeVersion != NUM_SHAPES);

        if (dm) {
            json_t* tableJ = json_object_get(rootJ, "drawTable");
            if (tableJ) {
                size_t size = json_array_size(tableJ);
                for (int i = 0; i < TABLE_N && i < static_cast<int>(size); i++) {
                    json_t* valueJ = json_array_get(tableJ, i);
                    if (valueJ)
                        drawTable[i] = clampValue(
                            static_cast<float>(json_number_value(valueJ)), 0.f, 1.f);
                }
            }
            json_t* resetJ = json_object_get(rootJ, "drawResetTable");
            if (resetJ) {
                size_t size = json_array_size(resetJ);
                for (int i = 0; i < TABLE_N && i < static_cast<int>(size); i++) {
                    json_t* valueJ = json_array_get(resetJ, i);
                    if (valueJ)
                        drawResetTable[i] = clampValue(
                            static_cast<float>(json_number_value(valueJ)), 0.f, 1.f);
                }
            }
            json_t* resetAvailJ = json_object_get(rootJ, "drawResetAvailable");
            if (resetAvailJ)
                drawResetAvailable = json_boolean_value(resetAvailJ);

            if (shapeSetChanged) {
                drawResetAvailable = false;
            }

            publishDrawTable();
            drawModeAtomic.store(true, std::memory_order_release);
            audioDrawTableVersion.store(INVALID_SEQ, std::memory_order_relaxed);
        } else {
            drawModeAtomic.store(false, std::memory_order_release);
        }

        json_t* bpmJ = json_object_get(rootJ, "measuredBpm");
        if (bpmJ) {
            float bpm = static_cast<float>(json_number_value(bpmJ));
            if (bpm > 1.f && bpm < 400.f) measuredBpm = bpm;
            dispBpm = measuredBpm;
        }
        json_t* haveBpmJ = json_object_get(rootJ, "haveBpmMeasurement");
        if (haveBpmJ) {
            haveBpmMeasurement = json_boolean_value(haveBpmJ);
        } else {
            haveBpmMeasurement = (measuredBpm > 1.f && measuredBpm < 400.f);
        }

        json_t* ppqnJ = json_object_get(rootJ, "autoDetectedPpqn");
        if (ppqnJ) {
            int p = static_cast<int>(json_integer_value(ppqnJ));
            if (isValidPpqn(p)) autoDetectedPpqn = p;
        }
        json_t* onPpqnJ = json_object_get(rootJ, "onInputDetectedPpqn");
        if (onPpqnJ) {
            int p = static_cast<int>(json_integer_value(onPpqnJ));
            if (isValidPpqn(p)) {
                onInputDetectedPpqn = p;
                onInputLockedPpqn = p;
            }
        }

        json_t* gateBarJ = json_object_get(rootJ, "gateBar");
        if (gateBarJ) {
            int g = static_cast<int>(json_integer_value(gateBarJ));
            if (g >= 0) gateBar = g;
        }
        json_t* gateStepJ = json_object_get(rootJ, "gateStep");
        if (gateStepJ) {
            gateStep = clampInt(static_cast<int>(json_integer_value(gateStepJ)), 0, 15);
        }
        json_t* gateVarJ = json_object_get(rootJ, "gateVar");
        if (gateVarJ) {
            int v = static_cast<int>(json_integer_value(gateVarJ));
            if (v >= 0 && v <= 4) gateVar = static_cast<VarType>(v);
        }

        clearDrawHistory();
    }
};

// =========================================================================
// ParamQuantity out-of-line definitions
// =========================================================================
std::string HutaraTimeQuantity::getDisplayValueString() {
    bool syncActive = module &&
        module->params[Hutara_Random_CV::TIME_SYNC_MODE_PARAM].getValue() > 0.5f;
    if (syncActive) {
        int idx = Hutara_Random_CV::timeParamToDivisionIndex(getValue());
        return std::string(Hutara_Random_CV::divisionToLabel(idx));
    }
    float t = clamp(getValue(), 0.f, 1.f);
    float seconds = 0.005f * std::pow(360000.f, t);
    if (seconds < 1.f) return string::f("%.0f ms", seconds * 1000.f);
    if (seconds < 60.f) return string::f("%.2f s", seconds);
    int minutes = static_cast<int>(seconds / 60.f);
    int secs = static_cast<int>(seconds - 60.f * minutes);
    return string::f("%dm %02ds", minutes, secs);
}

void HutaraTimeQuantity::setValue(float v) {
    bool syncActive = module &&
        module->params[Hutara_Random_CV::TIME_SYNC_MODE_PARAM].getValue() > 0.5f;
    if (syncActive) {
        float scaled = clamp(v, 0.f, 1.f) * (Hutara_Random_CV::NUM_DIVISIONS - 1);
        int idx = static_cast<int>(std::round(scaled));
        if (idx < 0) idx = 0;
        if (idx >= Hutara_Random_CV::NUM_DIVISIONS) idx = Hutara_Random_CV::NUM_DIVISIONS - 1;
        v = static_cast<float>(idx) / static_cast<float>(Hutara_Random_CV::NUM_DIVISIONS - 1);
    }
    ParamQuantity::setValue(v);
}

std::string HutaraBpmMultQuantity::getDisplayValueString() {
    int idx = static_cast<int>(std::round(getValue()));
    idx = clamp(idx, 0, Hutara_Random_CV::NUM_BPM_MULTS - 1);
    return std::string(Hutara_Random_CV::multiplierToLabel(idx));
}

void HutaraBpmMultQuantity::setValue(float v) {
    ParamQuantity::setValue(clamp(v, 0.f,
        static_cast<float>(Hutara_Random_CV::NUM_BPM_MULTS - 1)));
}

std::string HutaraPpqnQuantity::getDisplayValueString() {
    int idx = static_cast<int>(std::round(getValue()));
    idx = clamp(idx, 0, Hutara_Random_CV::NUM_PPQN - 1);
    static const char* shortLabels[] = {
        "Auto", "1 PPQN", "4 PPQN", "24 PPQN"
    };
    return std::string(shortLabels[idx]);
}

void HutaraPpqnQuantity::setValue(float v) {
    ParamQuantity::setValue(clamp(v, 0.f,
        static_cast<float>(Hutara_Random_CV::NUM_PPQN - 1)));
}

std::string HutaraShapeQuantity::getDisplayValueString() {
    float v = clamp(getValue(), 0.f, 1.f);
    float scaled = v * static_cast<float>(Hutara_Random_CV::NUM_SHAPES - 1);
    int i0 = clamp(static_cast<int>(std::floor(scaled)), 0, Hutara_Random_CV::NUM_SHAPES - 1);
    int i1 = std::min(i0 + 1, Hutara_Random_CV::NUM_SHAPES - 1);
    float f = scaled - static_cast<float>(i0);
    if (f < 0.02f) return std::string(Hutara_Random_CV::shapeName(i0));
    if (f > 0.98f) return std::string(Hutara_Random_CV::shapeName(i1));
    return string::f("%s->%s",
        Hutara_Random_CV::shapeName(i0), Hutara_Random_CV::shapeName(i1));
}

void HutaraShapeQuantity::setValue(float v) {
    ParamQuantity::setValue(clamp(v, 0.f, 1.f));
}

std::string HutaraGateSpeedQuantity::getDisplayValueString() {
    int idx = static_cast<int>(std::round(getValue()));
    idx = clamp(idx, 0, Hutara_Random_CV::NUM_GATE_SPEEDS - 1);
    static const char* labels[] = {"1/1", "1/2", "1/4", "1/8", "1/16", "1/32"};
    return std::string(labels[idx]);
}

void HutaraGateSpeedQuantity::setValue(float v) {
    ParamQuantity::setValue(clamp(v, 0.f,
        static_cast<float>(Hutara_Random_CV::NUM_GATE_SPEEDS - 1)));
}

std::string HutaraInputPpqnQuantity::getDisplayValueString() {
    int idx = static_cast<int>(std::round(getValue()));
    idx = clamp(idx, 0, Hutara_Random_CV::NUM_INPUT_PPQN - 1);
    return std::string(Hutara_Random_CV::inputPpqnToLabel(idx));
}

void HutaraInputPpqnQuantity::setValue(float v) {
    ParamQuantity::setValue(clamp(v, 0.f,
        static_cast<float>(Hutara_Random_CV::NUM_INPUT_PPQN - 1)));
}

std::string HutaraGateModeQuantity::getDisplayValueString() {
    int idx = static_cast<int>(std::round(getValue()));
    idx = clamp(idx, 0, Hutara_Random_CV::NUM_GATE_MODES - 1);
    return std::string(Hutara_Random_CV::gateModeLabel(idx));
}

void HutaraGateModeQuantity::setValue(float v) {
    ParamQuantity::setValue(clamp(v, 0.f,
        static_cast<float>(Hutara_Random_CV::NUM_GATE_MODES - 1)));
}

// =========================================================================
// Labels
// =========================================================================
struct GateSpeedLabel : TransparentWidget {
    Hutara_Random_CV* module = nullptr;
    std::shared_ptr<Font> font;
    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) return;
        if (!module) return;
        if (!font) font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
        if (!font) return;
        int idx = static_cast<int>(std::round(
            module->params[Hutara_Random_CV::RANDOM_GATE_SPEED_PARAM].getValue()));
        idx = Hutara_Random_CV::clampInt(idx, 0, Hutara_Random_CV::NUM_GATE_SPEEDS - 1);
        static const char* labels[] = {"1/1", "1/2", "1/4", "1/8", "1/16", "1/32"};
        nvgFontFaceId(args.vg, font->handle);
        nvgFontSize(args.vg, 9.f);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, nvgRGB(0x00, 0xff, 0x41));
        nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.5f, labels[idx], nullptr);
    }
};

struct InputPpqnLabel : TransparentWidget {
    Hutara_Random_CV* module = nullptr;
    std::shared_ptr<Font> font;
    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) return;
        if (!module) return;
        if (!font) font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
        if (!font) return;
        int idx = Hutara_Random_CV::clampInt(
            static_cast<int>(std::round(module->params[Hutara_Random_CV::INPUT_CLOCK_PPQN_PARAM].getValue())),
            0, Hutara_Random_CV::NUM_INPUT_PPQN - 1);
        std::string txt;
        NVGcolor col;
        if (idx == 0) {
            if (module->onInputPulseCount >= Hutara_Random_CV::AUTO_DETECT_MIN_PULSES
                && module->haveClockInterval
                && module->onInputLockedPpqn != 0) {
                txt = string::f("A%d", module->onInputDetectedPpqn);
                col = nvgRGB(0x00, 0xff, 0x41);
            } else if (module->haveClockInterval) {
                txt = string::f("A~%d", module->onInputDetectedPpqn);
                col = nvgRGB(0xff, 0xb0, 0x40);
            } else {
                txt = "A--";
                col = nvgRGB(0xff, 0xb0, 0x40);
            }
        } else {
            static const char* labels[] = {
                "Auto", "1", "2", "4", "6", "8", "12", "24", "48"
            };
            txt = labels[idx];
            col = nvgRGB(0x00, 0xff, 0x41);
        }
        nvgFontFaceId(args.vg, font->handle);
        nvgFontSize(args.vg, 8.f);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, col);
        nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.5f, txt.c_str(), nullptr);
    }
};

struct GateModeLabel : TransparentWidget {
    Hutara_Random_CV* module = nullptr;
    std::shared_ptr<Font> font;
    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) return;
        if (!module) return;
        if (!font) font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
        if (!font) return;
        int idx = Hutara_Random_CV::clampInt(
            static_cast<int>(std::round(module->params[Hutara_Random_CV::GATE_MODE_PARAM].getValue())),
            0, Hutara_Random_CV::NUM_GATE_MODES - 1);
        // === FIX: use clearer 3-letter labels ===
        static const char* labels[] = {"PAT", "RND", "HYB"};
        nvgFontFaceId(args.vg, font->handle);
        nvgFontSize(args.vg, 8.f);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, nvgRGB(0xc0, 0x60, 0xff));
        nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.5f, labels[idx], nullptr);
    }
};
// =========================================================================
// Waveform display widget
// =========================================================================
struct HutaraRandomDisplay : TransparentWidget {
    Hutara_Random_CV* module = nullptr;
    Vec dragPos;
    Vec mousePos;
    bool hasMousePos = false;
    bool drawing = false;
    bool snapY = true;
    bool hoverUndo = false;
    bool hoverReset = false;

    static NVGcolor colBG()         { return nvgRGB (0x02, 0x08, 0x03); }
    static NVGcolor colGrid()       { return nvgRGBA(0x0a, 0x2a, 0x0a, 0xb0); }
    static NVGcolor colGridSub()    { return nvgRGBA(0x06, 0x1a, 0x06, 0x80); }
    static NVGcolor colCurve()      { return nvgRGBA(0x00, 0xff, 0x41, 0xe6); }
    static NVGcolor colCurveFill()  { return nvgRGBA(0x00, 0xff, 0x41, 0x28); }
    static NVGcolor colGhost()      { return nvgRGBA(0x00, 0xff, 0x41, 0x40); }
    static NVGcolor colPlayhead()   { return nvgRGBA(0x00, 0xff, 0x41, 0x91); }
    static NVGcolor colDot()        { return nvgRGB (0x39, 0xff, 0x14); }
    static NVGcolor colText()       { return nvgRGB (0x00, 0xff, 0x41); }
    static NVGcolor colTextDim()    { return nvgRGBA(0x00, 0xc0, 0x30, 0xc0); }
    static NVGcolor colBtnBg()      { return nvgRGBA(0x00, 0x66, 0x1a, 0x4d); }
    static NVGcolor colBtnBgH()     { return nvgRGBA(0x00, 0xff, 0x41, 0x66); }
    static NVGcolor colBtnBgD()     { return nvgRGBA(0x10, 0x10, 0x10, 0x33); }
    static NVGcolor colBtnBd()      { return nvgRGBA(0x00, 0xff, 0x41, 0xb3); }
    static NVGcolor colBtnBdD()     { return nvgRGBA(0x20, 0x40, 0x20, 0x80); }
    static NVGcolor colBrushOut()   { return nvgRGBA(0x00, 0xff, 0x41, 0x66); }
    static NVGcolor colSHCurve()    { return nvgRGBA(0x00, 0xc8, 0xff, 0xf0); }
    static NVGcolor colSHDot()      { return nvgRGB (0x60, 0xe0, 0xff); }
    static NVGcolor colSHLabel()    { return nvgRGB (0x00, 0xc8, 0xff); }
    static NVGcolor colGateInfo()   { return nvgRGB (0xff, 0xb0, 0x40); }
    static NVGcolor colZeroLine()   { return nvgRGBA(0x00, 0xc0, 0x30, 0x80); }
    static NVGcolor colRangeLine()  { return nvgRGBA(0xff, 0xb0, 0x40, 0x60); }
    static NVGcolor colModeInfo()   { return nvgRGB (0xc0, 0x60, 0xff); }

    float controlHeight() const { return mm2px(4.5f); }
    float controlWidth()  const { return mm2px(12.0f); }
    float controlGap()    const { return mm2px(1.0f); }
    float controlBottomMargin() const { return mm2px(1.0f); }

    Rect undoRect() const {
        float width = controlWidth(), height = controlHeight();
        return Rect(box.size.x - mm2px(1.5f) - width * 2.f - controlGap(),
                    box.size.y - height - controlBottomMargin(),
                    width, height);
    }
    Rect resetRect() const {
        float width = controlWidth(), height = controlHeight();
        return Rect(box.size.x - mm2px(1.5f) - width,
                    box.size.y - height - controlBottomMargin(),
                    width, height);
    }

    bool isInside(const Rect& rect, Vec p) const { return rect.contains(p); }
    float marginX() const { return mm2px(1.5f); }
    float marginY() const { return mm2px(1.5f); }

    bool isInsideDrawArea(Vec p) const {
        if (isInside(undoRect(), p)) return false;
        if (isInside(resetRect(), p)) return false;
        const float mx = marginX(), my = marginY();
        return (p.x >= mx && p.x <= box.size.x - mx &&
                p.y >= my && p.y <= box.size.y - my);
    }

    void panelToTable(Vec p, float& t, float& v) const {
        const float mx = marginX(), my = marginY();
        const float cw = box.size.x - 2.f * mx;
        const float ch = box.size.y - 2.f * my;
        float rawT = (cw > 1.f) ? (p.x - mx) / cw : 0.f;
        float rawV = (ch > 1.f) ? 1.f - (p.y - my) / ch : 0.f;
        t = Hutara_Random_CV::clampValue(rawT, 0.f, 1.f);
        v = Hutara_Random_CV::clampValue(rawV, 0.f, 1.f);
    }

    Vec tableToPanel(float t, float v) const {
        const float mx = marginX(), my = marginY();
        const float cw = box.size.x - 2.f * mx;
        const float ch = box.size.y - 2.f * my;
        float rawT = Hutara_Random_CV::clampValue(t, 0.f, 1.f);
        float rawV = Hutara_Random_CV::clampValue(v, 0.f, 1.f);
        return Vec(mx + rawT * cw, my + (1.f - rawV) * ch);
    }

    void plotLine(Vec a, Vec b) {
        if (!module || !module->drawGestureActive) return;
        float t0, v0, t1, v1;
        panelToTable(a, t0, v0);
        panelToTable(b, t1, v1);
        if (t0 > t1) { std::swap(t0, t1); std::swap(v0, v1); }
        const int last = Hutara_Random_CV::TABLE_N - 1;
        int i0 = std::max(0, std::min(last, static_cast<int>(std::round(t0 * last))));
        int i1 = std::max(0, std::min(last, static_cast<int>(std::round(t1 * last))));
        auto snapV = [&](float v) -> float {
            if (!snapY) return v;
            const float step = 1.f / 32.f;
            return std::round(v / step) * step;
        };
        if (i0 == i1) {
            const int r = 3;
            float sv = snapV(v0);
            for (int j = i0 - r; j <= i0 + r; ++j) {
                if (j < 0 || j > last) continue;
                float w = 1.f - std::fabs(float(j - i0)) / float(r + 1);
                module->drawTable[j] = (j == i0)
                    ? sv : module->drawTable[j] * (1.f - w) + sv * w;
                module->drawTable[j] = Hutara_Random_CV::clampValue(
                    module->drawTable[j], 0.f, 1.f);
            }
            module->publishDrawTable();
            return;
        }
        for (int i = i0; i <= i1; ++i) {
            float f = float(i - i0) / float(i1 - i0);
            float v = snapV(v0 + (v1 - v0) * f);
            module->drawTable[i] = Hutara_Random_CV::clampValue(v, 0.f, 1.f);
        }
        module->publishDrawTable();
    }

    void onButton(const ButtonEvent& e) override {
        if (!module || e.action != GLFW_PRESS || e.button != GLFW_MOUSE_BUTTON_LEFT) return;

        if (isInside(undoRect(), e.pos)) {
            e.consume(this);
            drawing = false; hasMousePos = false;
            module->endDrawGesture();
            module->undoDraw();
            return;
        }
        if (isInside(resetRect(), e.pos)) {
            e.consume(this);
            drawing = false; hasMousePos = false;
            module->endDrawGesture();
            module->resetDraw();
            return;
        }
        e.consume(this);
        module->beginDrawGesture();
        drawing = module->drawGestureActive;
        if (!drawing) return;
        dragPos = e.pos;
        dragPos.x = Hutara_Random_CV::clampValue(dragPos.x, marginX(), box.size.x - marginX());
        dragPos.y = Hutara_Random_CV::clampValue(dragPos.y, marginY(), box.size.y - marginY());
        mousePos = dragPos;
        hasMousePos = true;
        plotLine(dragPos, dragPos);
    }

    void onDragMove(const DragMoveEvent& e) override {
        if (!drawing || !module || !module->drawGestureActive) return;
        if (e.button != GLFW_MOUSE_BUTTON_LEFT) return;

        // === FIX: use mouseDelta for a quick update, but the authoritative
        // position comes from onHover. This ensures Y-axis tracking is exact
        // even if mouseDelta is slightly off. ===
        Vec newPos = dragPos.plus(e.mouseDelta);
        newPos.x = Hutara_Random_CV::clampValue(newPos.x, marginX(), box.size.x - marginX());
        newPos.y = Hutara_Random_CV::clampValue(newPos.y, marginY(), box.size.y - marginY());

        if ((newPos - dragPos).norm() > 0.01f) {
            plotLine(dragPos, newPos);
            dragPos = newPos;
        }

        e.consume(this);
    }

    void onDragEnd(const DragEndEvent& e) override {
        if (e.button != GLFW_MOUSE_BUTTON_LEFT) return;
        drawing = false;
        hasMousePos = false;
        if (module) module->endDrawGesture();
    }

    // === FIX: ONLY ONE onHover definition ===
    void onHover(const HoverEvent& e) override {
        TransparentWidget::onHover(e);
        hoverUndo  = isInside(undoRect(),  e.pos);
        hoverReset = isInside(resetRect(), e.pos);
        if (!drawing) {
            mousePos = e.pos;
            hasMousePos = true;
        }
    }
    // === FIX: DragHoverEvent has 'pos' and is sent every frame during drag ===
    void onDragHover(const DragHoverEvent& e) override {
        if (!drawing || !module || !module->drawGestureActive) return;
        if (e.origin != this) return;  // only handle drags originating from this widget

        // e.pos is the mouse position relative to THIS widget, exact and per-frame.
        Vec newPos = e.pos;
        newPos.x = Hutara_Random_CV::clampValue(newPos.x, marginX(), box.size.x - marginX());
        newPos.y = Hutara_Random_CV::clampValue(newPos.y, marginY(), box.size.y - marginY());

        if ((newPos - dragPos).norm() > 0.01f) {
            plotLine(dragPos, newPos);
            dragPos = newPos;
            mousePos = newPos;
            hasMousePos = true;
        }
        e.consume(this);
    }
    void onLeave(const LeaveEvent& e) override {
        TransparentWidget::onLeave(e);
        hoverUndo = hoverReset = false;
        hasMousePos = false;
    }

    static std::string formatTime(float seconds) {
        if (seconds < 1.f) return string::f("%.0f ms", seconds * 1000.f);
        if (seconds < 60.f) return string::f("%.2f s", seconds);
        int minutes = static_cast<int>(seconds / 60.f);
        int secs = static_cast<int>(seconds - 60.f * minutes);
        return string::f("%dm %02ds", minutes, secs);
    }

    static std::string formatDivision(int idx) {
        return std::string(Hutara_Random_CV::divisionToLabel(idx));
    }

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) return;

        float shape = module ? module->dispShape : 0.5f;
        float warp  = module ? module->dispWarp  : 0.f;
        float flux  = module ? module->dispFlux  : 0.f;
        float randomness = module ? module->dispRandomness : 0.f;
        double phase = module ? module->dispPhase : 0.4;
        float duration = module ? module->dispDuration : 1.f;
        bool  syncMode = module ? module->dispSyncMode : false;
        float bpm = module ? module->dispBpm : Hutara_Random_CV::DEFAULT_BPM;

        bool dm = module && module->drawModeAtomic.load(std::memory_order_relaxed);
        const float* table = dm ? module->drawTable : nullptr;

        float w = box.size.x, h = box.size.y;
        nvgScissor(args.vg, 0, 0, w, h);
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0, 0, w, h);
        nvgFillColor(args.vg, colBG());
        nvgFill(args.vg);

        float mx = marginX(), my = marginY();
        float cw = w - 2.f * mx, ch = h - 2.f * my;

        nvgStrokeColor(args.vg, colGrid());
        nvgStrokeWidth(args.vg, mm2px(0.15f));
        for (int i = 0; i <= 4; i++) {
            float x = mx + cw * static_cast<float>(i) / 4.f;
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, x, my);
            nvgLineTo(args.vg, x, my + ch);
            nvgStroke(args.vg);
        }
        if (snapY) {
            nvgStrokeColor(args.vg, colGridSub());
            for (int i = 1; i < 32; i++) {
                float y = my + ch * (static_cast<float>(i) / 32.f);
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, mx, y);
                nvgLineTo(args.vg, mx + cw, y);
                nvgStroke(args.vg);
            }
        }

        static constexpr int N = 128;

        float envRange  = module ? module->params[Hutara_Random_CV::ENVELOPE_RANGE_PARAM].getValue()  : 5.f;
        float envOffset = module ? module->params[Hutara_Random_CV::ENVELOPE_OFFSET_PARAM].getValue() : 0.f;
        float envLevel  = module ? module->params[Hutara_Random_CV::LEVEL_PARAM].getValue()           : 1.f;
        bool envBipolar = module ? (module->params[Hutara_Random_CV::BIPOLAR_PARAM].getValue() > 0.5f) : true;

        float vMin = -15.f, vMax = +15.f;
        auto voltageToY_local = [&](float volt) -> float {
            float norm = (volt - vMin) / (vMax - vMin);
            return my + (1.f - norm) * ch;
        };

        {
            float zeroY = voltageToY_local(0.f);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, mx, zeroY);
            nvgLineTo(args.vg, mx + cw, zeroY);
            nvgStrokeColor(args.vg, colZeroLine());
            nvgStrokeWidth(args.vg, mm2px(0.15f));
            nvgStroke(args.vg);
        }

        float upperVal = envOffset + envRange;
        float lowerVal = envBipolar ? (envOffset - envRange) : envOffset;
        if (upperVal >= vMin && upperVal <= vMax) {
            float yU = voltageToY_local(upperVal);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, mx, yU);
            nvgLineTo(args.vg, mx + cw, yU);
            nvgStrokeColor(args.vg, colRangeLine());
            nvgStrokeWidth(args.vg, mm2px(0.12f));
            nvgStroke(args.vg);
        }
        if (lowerVal >= vMin && lowerVal <= vMax && lowerVal != upperVal) {
            float yL = voltageToY_local(lowerVal);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, mx, yL);
            nvgLineTo(args.vg, mx + cw, yL);
            nvgStrokeColor(args.vg, colRangeLine());
            nvgStrokeWidth(args.vg, mm2px(0.12f));
            nvgStroke(args.vg);
        }

        auto valueToVoltage = [&](float v) -> float {
            float lv = v * envLevel;
            if (envBipolar) return (lv * 2.f - 1.f) * envRange + envOffset;
            else            return lv * envRange + envOffset;
        };

        if (module && randomness > 0.02f && !(drawing && module->drawGestureActive)) {
            nvgBeginPath(args.vg);
            for (int i = 0; i <= N; i++) {
                float p = static_cast<float>(i) / static_cast<float>(N);
                float v = module->envelopeValue(dm ? module->drawTable : nullptr,
                    shape, warp, flux, p);
                float volt = valueToVoltage(v);
                float px = mx + p * cw;
                float py = voltageToY_local(volt);
                if (i == 0) nvgMoveTo(args.vg, px, py);
                else        nvgLineTo(args.vg, px, py);
            }
            nvgStrokeColor(args.vg, colGhost());
            nvgStrokeWidth(args.vg, mm2px(0.18f));
            nvgStroke(args.vg);
        }

        nvgBeginPath(args.vg);
        for (int i = 0; i <= N; i++) {
            float p = static_cast<float>(i) / static_cast<float>(N);
            float v = 0.f;
            if (module) {
                if (drawing && module->drawGestureActive) {
                    float x = p * float(Hutara_Random_CV::TABLE_N - 1);
                    int k = std::max(0, std::min(
                        Hutara_Random_CV::TABLE_N - 2,
                        static_cast<int>(std::floor(x))));
                    float f = x - float(k);
                    v = module->drawTable[k] +
                        (module->drawTable[k + 1] - module->drawTable[k]) * f;
                } else {
                    v = module->randomizedEnvelope(
                        shape, warp, flux, randomness, p, false);
                }
            }
            float volt = valueToVoltage(v);
            float px = mx + p * cw;
            float py = voltageToY_local(volt);
            if (i == 0) nvgMoveTo(args.vg, px, py);
            else        nvgLineTo(args.vg, px, py);
        }
        nvgStrokeColor(args.vg, colCurve());
        nvgStrokeWidth(args.vg, mm2px(0.3f));
        nvgStroke(args.vg);

        float zeroLineY = voltageToY_local(0.f);
        zeroLineY = Hutara_Random_CV::clampValue(zeroLineY, my, my + ch);
        nvgLineTo(args.vg, mx + cw, zeroLineY);
        nvgLineTo(args.vg, mx, zeroLineY);
        nvgClosePath(args.vg);
        nvgFillColor(args.vg, colCurveFill());
        nvgFill(args.vg);

        if (module) {
            bool bipolar = module->bipolarOutput;
            float shMin = bipolar ? -Hutara_Random_CV::SH_DISPLAY_MAX : 0.f;
            float shMax = Hutara_Random_CV::SH_DISPLAY_MAX;
            float shRange = shMax - shMin;
            auto valueToNorm = [&](float v) -> float {
                float n = (v - shMin) / shRange;
                return Hutara_Random_CV::clampValue(n, 0.f, 1.f);
            };
            float smoothAmt = module->params[Hutara_Random_CV::SMOOTH_RANDOM_PARAM].getValue();
            bool smoothOn = (smoothAmt > Hutara_Random_CV::SMOOTH_DEADZONE);
            int totalAvail = module->shHistoryCount;
            int writePos = module->shHistoryWritePos;
            int displayCount = Hutara_Random_CV::SH_DISPLAY_COUNT;

            if (totalAvail == 0) {
                float v0 = valueToNorm(module->dispShValue);
                Vec p0 = tableToPanel(0.f, v0);
                Vec p1 = tableToPanel(1.f, v0);
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, p0.x, p0.y);
                nvgLineTo(args.vg, p1.x, p1.y);
                nvgStrokeColor(args.vg, colSHCurve());
                nvgStrokeWidth(args.vg, mm2px(0.35f));
                nvgStroke(args.vg);
            } else {
                int realCount = std::min(totalAvail, displayCount);
                int padCount  = displayCount - realCount;
                int oldestRealIdx = (writePos - realCount +
                    Hutara_Random_CV::SH_HISTORY_SIZE * 2) %
                    Hutara_Random_CV::SH_HISTORY_SIZE;
                nvgBeginPath(args.vg);
                bool first = true;
                Vec prevPt(0.f, 0.f);
                if (padCount > 0) {
                    float v = valueToNorm(module->shHistory[oldestRealIdx]);
                    for (int i = 0; i < padCount; ++i) {
                        float t = static_cast<float>(i) /
                            static_cast<float>(displayCount - 1);
                        Vec pt = tableToPanel(t, v);
                        if (first) { nvgMoveTo(args.vg, pt.x, pt.y); first = false; }
                        else       { nvgLineTo(args.vg, pt.x, pt.y); }
                        prevPt = pt;
                    }
                }
                for (int i = 0; i < realCount; ++i) {
                    int idx = (oldestRealIdx + i) %
                        Hutara_Random_CV::SH_HISTORY_SIZE;
                    int tIdx = padCount + i;
                    float t = static_cast<float>(tIdx) /
                        static_cast<float>(displayCount - 1);
                    float v = valueToNorm(module->shHistory[idx]);
                    Vec pt = tableToPanel(t, v);
                    if (first) { nvgMoveTo(args.vg, pt.x, pt.y); first = false; }
                    else {
                        if (smoothOn) nvgLineTo(args.vg, pt.x, pt.y);
                        else {
                            nvgLineTo(args.vg, pt.x, prevPt.y);
                            nvgLineTo(args.vg, pt.x, pt.y);
                        }
                    }
                    prevPt = pt;
                }
                if (!first) {
                    int lastIdx = (writePos - 1 +
                        Hutara_Random_CV::SH_HISTORY_SIZE) %
                        Hutara_Random_CV::SH_HISTORY_SIZE;
                    float lastV = valueToNorm(module->shHistory[lastIdx]);
                    Vec endPtSH = tableToPanel(1.f, lastV);
                    nvgLineTo(args.vg, endPtSH.x, endPtSH.y);
                }
                nvgStrokeColor(args.vg, colSHCurve());
                nvgStrokeWidth(args.vg, mm2px(0.35f));
                nvgStroke(args.vg);
                if (!smoothOn) {
                    for (int i = 0; i < realCount; ++i) {
                        int idx = (oldestRealIdx + i) %
                            Hutara_Random_CV::SH_HISTORY_SIZE;
                        int tIdx = padCount + i;
                        float t = static_cast<float>(tIdx) /
                            static_cast<float>(displayCount - 1);
                        float v = valueToNorm(module->shHistory[idx]);
                        Vec pt = tableToPanel(t, v);
                        nvgBeginPath(args.vg);
                        nvgCircle(args.vg, pt.x, pt.y, mm2px(0.35f));
                        nvgFillColor(args.vg, colSHCurve());
                        nvgFill(args.vg);
                    }
                }
                int lastIdx = (writePos - 1 +
                    Hutara_Random_CV::SH_HISTORY_SIZE) %
                    Hutara_Random_CV::SH_HISTORY_SIZE;
                float lastV = valueToNorm(module->shHistory[lastIdx]);
                Vec lastPt = tableToPanel(1.f, lastV);
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, lastPt.x, lastPt.y, mm2px(0.7f));
                nvgFillColor(args.vg, colSHDot());
                nvgFill(args.vg);
            }
        }

        float pPhase = Hutara_Random_CV::clampValue(
            static_cast<float>(phase), 0.f, 1.f);
        float pv = module ? module->randomizedEnvelope(
            shape, warp, flux, randomness, pPhase, false) : 0.f;
        float pvVolt = valueToVoltage(pv);
        float phX = mx + pPhase * cw;
        float phY = voltageToY_local(pvVolt);
        Vec ph(phX, phY);
        Vec phTop(phX, my);
        Vec phBot(phX, my + ch);
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, phTop.x, phTop.y);
        nvgLineTo(args.vg, phBot.x, phBot.y);
        nvgStrokeColor(args.vg, colPlayhead());
        nvgStrokeWidth(args.vg, mm2px(0.2f));
        nvgStroke(args.vg);
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, ph.x, ph.y, mm2px(0.7f));
        nvgFillColor(args.vg, colDot());
        nvgFill(args.vg);

        if (drawing && hasMousePos) {
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, mousePos.x, mousePos.y, mm2px(1.0f));
            nvgFillColor(args.vg, colBrushOut());
            nvgFill(args.vg);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, mousePos.x, mousePos.y, mm2px(0.45f));
            nvgFillColor(args.vg, colDot());
            nvgFill(args.vg);
        }

        auto drawControl = [&](Rect rect, const char* label, bool enabled, bool hover) {
            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, rect.pos.x, rect.pos.y,
                rect.size.x, rect.size.y, mm2px(0.8f));
            if (enabled) nvgFillColor(args.vg, hover ? colBtnBgH() : colBtnBg());
            else         nvgFillColor(args.vg, colBtnBgD());
            nvgFill(args.vg);
            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, rect.pos.x, rect.pos.y,
                rect.size.x, rect.size.y, mm2px(0.8f));
            nvgStrokeColor(args.vg, enabled ? colBtnBd() : colBtnBdD());
            nvgStrokeWidth(args.vg, mm2px(0.15f));
            nvgStroke(args.vg);
            nvgFontSize(args.vg, mm2px(1.8f));
            nvgFillColor(args.vg, enabled ? colText() : colTextDim());
            nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgText(args.vg, rect.pos.x + rect.size.x * 0.5f,
                rect.pos.y + rect.size.y * 0.5f, label, nullptr);
        };

        drawControl(undoRect(),  "UNDO",
            module && (module->drawUndoCount > 0), hoverUndo);
        drawControl(resetRect(), "RESET",
            module && module->drawResetAvailable, hoverReset);

        static std::shared_ptr<Font> font = APP->window->loadFont(
            asset::system("res/fonts/ShareTechMono-Regular.ttf"));
        if (font) {
            nvgFontFaceId(args.vg, font->handle);

            nvgFontSize(args.vg, mm2px(1.7f));
            nvgFillColor(args.vg, colText());
            nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
            const char* modeTxt = table ? "DRAW" : "CURVE";
            nvgText(args.vg, mx + mm2px(0.5f), my + mm2px(0.5f), modeTxt, nullptr);

            if (module) {
                float sv = module->dispShape;
                float scaled = Hutara_Random_CV::clampValue(sv, 0.f, 1.f) *
                    static_cast<float>(Hutara_Random_CV::NUM_SHAPES - 1);
                int sIdx = Hutara_Random_CV::clampInt(
                    static_cast<int>(std::round(scaled)),
                    0, Hutara_Random_CV::NUM_SHAPES - 1);
                std::string shapeTxt = string::f("SHP %s",
                    Hutara_Random_CV::shapeName(sIdx));
                nvgFontSize(args.vg, mm2px(1.5f));
                nvgFillColor(args.vg, colTextDim());
                nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
                nvgText(args.vg, mx + mm2px(0.5f), my + mm2px(2.2f),
                    shapeTxt.c_str(), nullptr);
            }

            if (syncMode && module) {
                int ppqnVal;
                if (module->dispPpqnIdx == 0) ppqnVal = module->autoDetectedPpqn;
                else ppqnVal = Hutara_Random_CV::ppqnToInt(module->dispPpqnIdx);
                std::string stateTxt;
                NVGcolor stateCol = colText();
                bool haveBpm = module->haveBpmMeasurement;
                bool clockConnected = module->inputs[
                    Hutara_Random_CV::ENVELOPE_CLOCK_INPUT].isConnected();
                if (!clockConnected) {
                    stateTxt = string::f("%d PPQN  NO CLK", ppqnVal);
                    stateCol = colTextDim();
                } else if (!haveBpm) {
                    stateTxt = string::f("%d PPQN  MEAS", ppqnVal);
                    stateCol = colTextDim();
                } else {
                    stateTxt = string::f("%d PPQN", ppqnVal);
                    stateCol = colText();
                }
                nvgFontSize(args.vg, mm2px(1.6f));
                nvgFillColor(args.vg, stateCol);
                nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
                nvgText(args.vg, w - mx - mm2px(0.5f), my + mm2px(0.5f),
                    stateTxt.c_str(), nullptr);
            }

            if (module) {
                int gs = Hutara_Random_CV::clampInt(
                    static_cast<int>(std::round(module->params[
                        Hutara_Random_CV::RANDOM_GATE_SPEED_PARAM].getValue())),
                    0, Hutara_Random_CV::NUM_GATE_SPEEDS - 1);
                static const char* gateSpeedLabels[] =
                    {"1/1", "1/2", "1/4", "1/8", "1/16", "1/32"};
                int patIdx = Hutara_Random_CV::clampInt(
                    static_cast<int>(std::round(module->params[
                        Hutara_Random_CV::GATE_PATTERN_PARAM].getValue())),
                    0, Hutara_Random_CV::NUM_GATE_PATTERNS - 1);
                int modeIdx = Hutara_Random_CV::clampInt(
                    static_cast<int>(std::round(module->params[
                        Hutara_Random_CV::GATE_MODE_PARAM].getValue())),
                    0, Hutara_Random_CV::NUM_GATE_MODES - 1);

                static const char* varLabels[] =
                    {"", "ghost", "fill", "drop", "roll"};
                int vIdx = static_cast<int>(module->gateVar);
                std::string varSuffix;
                if (vIdx > 0 && vIdx <= 4 && modeIdx == 0)
                    varSuffix = std::string(" ") + varLabels[vIdx];

                std::string gsTxt;
                if (modeIdx == 0) {
                    gsTxt = string::f("%s %s%s", gateSpeedLabels[gs],
                        Hutara_Random_CV::gatePatternLabel(patIdx),
                        varSuffix.c_str());
                } else {
                    gsTxt = string::f("%s %s", gateSpeedLabels[gs],
                        Hutara_Random_CV::gateModeLabel(modeIdx));
                }
                if (!module->haveClockInterval) gsTxt += " (no clk)";

                nvgFontSize(args.vg, mm2px(1.5f));
                nvgFillColor(args.vg, modeIdx == 0 ? colGateInfo() : colModeInfo());
                nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
                nvgText(args.vg, w - mx - mm2px(0.5f), my + mm2px(3.5f),
                    gsTxt.c_str(), nullptr);
            }

            nvgFontSize(args.vg, mm2px(2.1f));
            nvgFillColor(args.vg, colText());
            nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
            float textY = h - controlHeight() - controlBottomMargin() - mm2px(0.5f);
            if (syncMode) {
                std::string divTxt = formatDivision(
                    module ? module->dispDivisionIndex : 2);
                std::string multTxt = Hutara_Random_CV::multiplierToLabel(
                    module ? module->dispMultiplierIdx : 2);
                std::string combined = divTxt + " " + multTxt;
                nvgText(args.vg, mx + mm2px(0.5f), textY, combined.c_str(), nullptr);
            } else {
                nvgText(args.vg, mx + mm2px(0.5f), textY,
                    formatTime(duration).c_str(), nullptr);
            }

            nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
            if (syncMode) {
                std::string bpmTxt = string::f("%.1f BPM", bpm);
                nvgText(args.vg, w - mx - mm2px(0.5f), textY, bpmTxt.c_str(), nullptr);
            } else if (module && module->dispLoop) {
                nvgText(args.vg, w - mx - mm2px(0.5f), textY, "LOOP", nullptr);
            }

            if (module) {
                nvgFontSize(args.vg, mm2px(1.5f));
                nvgFillColor(args.vg, colSHLabel());
                nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
                float shLabelY = h - controlHeight() - controlBottomMargin() - mm2px(3.2f);
                std::string shTxt = string::f("S&H %.2fV", module->dispShValue);
                nvgText(args.vg, mx + mm2px(0.5f), shLabelY, shTxt.c_str(), nullptr);
            }
        }

        if (!drawing && hasMousePos && font && isInsideDrawArea(mousePos)) {
            float t, v;
            panelToTable(mousePos, t, v);
            nvgFontFaceId(args.vg, font->handle);
            nvgFontSize(args.vg, mm2px(1.6f));
            nvgFillColor(args.vg, colText());
            nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
            std::string txt = string::f("%.2f / %.2f", t, v);
            nvgText(args.vg, mousePos.x + mm2px(1.f),
                mousePos.y - mm2px(0.5f), txt.c_str(), nullptr);
        }

        nvgResetScissor(args.vg);
    }
};

// =========================================================================
// RandomParamRing (with DoubleClickEvent)
// =========================================================================
struct RandomParamRing : TransparentWidget {
    Hutara_Random_CV* module = nullptr;

    static constexpr int   NUM_SEGS        = 4;
    static constexpr float NAV_RADIUS_OFFSET = 3.f;
    static constexpr float NAV_HIT_RADIAL_TOL = 6.f;
    static constexpr float NAV_HIT_ANGULAR_TOL = 0.025f;
    static constexpr float NAV_STROKE_WIDTH = 4.f;

    struct SegDef {
        int paramId;
        const char* name;
        const char* shortName;
        NVGcolor color;
        float minVal;
        float maxVal;
    };

    static const SegDef& segDef(int i) {
        static const SegDef defs[NUM_SEGS] = {
            { Hutara_Random_CV::SHAPE_PARAM,      "SHAPE",  "SHP", nvgRGBA(  0, 255, 150, 255),  0.f, 1.f },
            { Hutara_Random_CV::WARP_PARAM,       "WARP",   "WRP", nvgRGBA(255,  80,  80, 255), -1.f, 1.f },
            { Hutara_Random_CV::FLUX_PARAM,       "FLUX",   "FLX", nvgRGBA(  0, 200, 255, 255),  0.f, 1.f },
            { Hutara_Random_CV::RANDOMNESS_PARAM, "RANDOM", "RND", nvgRGBA(255, 100, 255, 255),  0.f, 1.f },
        };
        return defs[i];
    }

    int   focusedSeg     = -1;
    int   draggingSeg    = -1;
    float dragStartVal   = 0.f;
    float dragAccum      = 0.f;
    float flashAlpha     = 0.f;
    int   lastTouchedSeg = -1;
    bool  hoverActive    = false;
    float focusHold      = 0.f;
    int   hoverNav       = -1;

    // === FIX: double-click state (was local static, now member) ===
    double lastPressTime = -1.0;
    int    lastPressSeg  = -1;
    static constexpr double DOUBLE_CLICK_TIME = 0.35;

    float prevValues[NUM_SEGS] = {0.f};
    bool  prevInit = false;

    std::shared_ptr<Font> font;

    void centerR(float& cx, float& cy, float& rIn, float& rOut) const {
        cx = box.size.x * 0.5f;
        cy = box.size.y * 0.5f;
        const float R = std::min(cx, cy) - 2.f;
        rIn  = R * 0.54f;
        rOut = R * 0.94f;
    }

    static void segAngles(int seg, float& a0, float& a1) {
        const float base = -M_PI * 0.5f + (float)seg * (M_PI * 0.5f);
        a0 = base - M_PI * 0.25f + 0.07f;
        a1 = base + M_PI * 0.25f - 0.07f;
    }

    void navAngles(int which, float& na0, float& na1) const {
        float a0, a1;
        segAngles(0, a0, a1);
        const float mid = (a0 + a1) * 0.5f;
        if (which == 0) { na0 = a0; na1 = mid; }
        else            { na0 = mid; na1 = a1; }
    }

    bool hitNav(Vec p, int& which) const {
        float cx, cy, rIn, rOut;
        centerR(cx, cy, rIn, rOut);
        (void)rIn;
        const float dx = p.x - cx;
        const float dy = p.y - cy;
        const float r2 = dx * dx + dy * dy;
        const float rMin = rOut + 0.5f;
        if (r2 < rMin * rMin) return false;
        const float rNav = rOut + NAV_RADIUS_OFFSET;
        const float r = std::sqrt(r2);
        if (std::fabs(r - rNav) > NAV_HIT_RADIAL_TOL) return false;
        const float ang = std::atan2(dy, dx);
        for (int w = 0; w < 2; ++w) {
            float na0, na1;
            navAngles(w, na0, na1);
            if (ang >= na0 - NAV_HIT_ANGULAR_TOL &&
                ang <= na1 + NAV_HIT_ANGULAR_TOL) {
                which = w;
                return true;
            }
        }
        return false;
    }

    void stepShape(int dir) {
        if (!module) return;
        if (auto* pq = module->paramQuantities[Hutara_Random_CV::SHAPE_PARAM]) {
            const int maxIdx = Hutara_Random_CV::NUM_SHAPES - 1;
            int cur = static_cast<int>(std::round(pq->getValue() * maxIdx));
            cur += dir;
            if (cur < 0)      cur = maxIdx;
            if (cur > maxIdx) cur = 0;
            pq->setValue(static_cast<float>(cur) / static_cast<float>(maxIdx));
        }
        focusedSeg     = 0;
        lastTouchedSeg = 0;
        flashAlpha     = 0.8f;
        focusHold      = 1.5f;
    }

    int hitSegment(Vec pos) const {
        float cx, cy, rIn, rOut;
        centerR(cx, cy, rIn, rOut);
        const float dx = pos.x - cx;
        const float dy = pos.y - cy;
        const float r = std::sqrt(dx*dx + dy*dy);
        if (r < rIn || r > rOut) return -1;
        const float ang = std::atan2(dy, dx);
        if (ang >= -M_PI * 0.75f && ang < -M_PI * 0.25f) return 0;
        if (ang >= -M_PI * 0.25f && ang <  M_PI * 0.25f) return 1;
        if (ang >=  M_PI * 0.25f && ang <  M_PI * 0.75f) return 2;
        return 3;
    }

    void resetSegToDefault(int seg) {
        if (!module || seg < 0 || seg >= NUM_SEGS) return;
        const SegDef& d = segDef(seg);
        if (auto* pq = module->paramQuantities[d.paramId])
            pq->setValue(pq->defaultValue);
        lastTouchedSeg = seg;
        flashAlpha     = 1.f;
        focusedSeg     = seg;
        focusHold      = 1.5f;
    }

    void onHover(const HoverEvent& e) override {
        TransparentWidget::onHover(e);
        if (!module) return;
        hoverActive = true;
        int nav = -1;
        if (hitNav(e.pos, nav)) { hoverNav = nav; return; }
        hoverNav = -1;
        if (draggingSeg >= 0) return;
        focusedSeg = hitSegment(e.pos);
        focusHold  = 0.f;
    }

    void onLeave(const LeaveEvent& e) override {
        TransparentWidget::onLeave(e);
        hoverActive = false;
        hoverNav    = -1;
    }

    void onHoverScroll(const HoverScrollEvent& e) override {
        if (!module) { TransparentWidget::onHoverScroll(e); return; }
        const int seg = hitSegment(e.pos);
        if (seg < 0) { TransparentWidget::onHoverScroll(e); return; }
        const SegDef& d = segDef(seg);
        const float range = d.maxVal - d.minVal;
        if (range <= 0.f) return;
        float stepFrac = 0.01f;
        if (APP->window) {
            int mods = APP->window->getMods();
            if (mods & RACK_MOD_SHIFT)      stepFrac = 0.05f;
            else if (mods & RACK_MOD_CTRL)  stepFrac = 0.001f;
        }
        const float step = range * stepFrac;
        float dir = 0.f;
        if (e.scrollDelta.y > 0.f)      dir = 1.f;
        else if (e.scrollDelta.y < 0.f) dir = -1.f;
        if (dir == 0.f) { e.consume(this); return; }
        const float cur = module->params[d.paramId].getValue();
        float nv = cur + dir * step;
        if (nv < d.minVal) nv = d.minVal;
        if (nv > d.maxVal) nv = d.maxVal;
        if (auto* pq = module->paramQuantities[d.paramId]) pq->setValue(nv);
        focusedSeg     = seg;
        lastTouchedSeg = seg;
        flashAlpha     = 0.6f;
        focusHold      = 1.5f;
        e.consume(this);
    }

    // === FIX: onDoubleClick REMOVED (DoubleClickEvent has no button/pos) ===
    // Double-click logic is now handled in onButton via glfwGetTime()

    void onButton(const ButtonEvent& e) override {
        if (!module) { TransparentWidget::onButton(e); return; }
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
            int nav = -1;
            if (hitNav(e.pos, nav)) {
                stepShape(nav == 0 ? -1 : +1);
                e.consume(this);
                return;
            }
            const int seg = hitSegment(e.pos);
            if (seg >= 0) {
                // === FIX: double-click detection via glfwGetTime ===
                const double now = glfwGetTime();
                if (lastPressSeg == seg && lastPressTime > 0.0 &&
                    (now - lastPressTime) < DOUBLE_CLICK_TIME) {
                    resetSegToDefault(seg);
                    lastPressSeg  = -1;
                    lastPressTime = -1.0;
                    draggingSeg   = -1;
                    e.consume(this);
                    return;
                }

                draggingSeg    = seg;
                focusedSeg     = seg;
                lastTouchedSeg = seg;
                const SegDef& d = segDef(seg);
                dragStartVal = module->params[d.paramId].getValue();
                dragAccum    = 0.f;
                flashAlpha   = 1.f;
                focusHold    = 0.f;
                lastPressSeg  = seg;
                lastPressTime = now;
                e.consume(this);
                return;
            }
        }
        if (e.action == GLFW_RELEASE) {
            if (draggingSeg >= 0) {
                draggingSeg = -1;
                focusHold   = 1.5f;
                e.consume(this);
                return;
            }
        }
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
            const int seg = hitSegment(e.pos);
            if (seg >= 0) { resetSegToDefault(seg); e.consume(this); }
        }
    }

    void onDragMove(const DragMoveEvent& e) override {
        if (!module || draggingSeg < 0) return;
        if (e.button != GLFW_MOUSE_BUTTON_LEFT) return;
        dragAccum += (-e.mouseDelta.y + e.mouseDelta.x);
        const SegDef& d = segDef(draggingSeg);
        const float range = d.maxVal - d.minVal;
        float nv = dragStartVal + dragAccum * (range / 100.f);
        if (nv < d.minVal) nv = d.minVal;
        if (nv > d.maxVal) nv = d.maxVal;
        if (auto* pq = module->paramQuantities[d.paramId]) pq->setValue(nv);
        e.consume(this);
    }

    void onDragEnd(const DragEndEvent& e) override {
        if (draggingSeg >= 0) {
            draggingSeg = -1;
            focusHold   = 1.5f;
            e.consume(this);
        }
    }

    void step() override {
        TransparentWidget::step();
        if (!module) return;
        float dt = APP->window ? APP->window->getFrameTime() : 1.f / 60.f;
        if (dt <= 0.f || !std::isfinite(dt)) dt = 1.f / 60.f;
        if (dt > 0.1f) dt = 0.1f;

        if (!prevInit) {
            for (int i = 0; i < NUM_SEGS; i++)
                prevValues[i] = module->params[segDef(i).paramId].getValue();
            prevInit = true;
        }

        for (int i = 0; i < NUM_SEGS; i++) {
            const SegDef& d = segDef(i);
            const float cur = module->params[d.paramId].getValue();
            const float thr = 0.02f * (d.maxVal - d.minVal);
            if (std::abs(cur - prevValues[i]) > thr) {
                prevValues[i] = cur;
                if (draggingSeg < 0 && !hoverActive) {
                    focusedSeg = i;
                    focusHold  = 1.5f;
                }
            }
        }

        if (focusHold > 0.f) {
            focusHold -= dt;
            if (focusHold < 0.f) focusHold = 0.f;
        }
        if (!hoverActive && draggingSeg < 0 && focusHold <= 0.f) {
            if (focusedSeg != -1) focusedSeg = -1;
        }
        if (flashAlpha > 0.f) {
            flashAlpha -= dt * 2.5f;
            if (flashAlpha < 0.f) flashAlpha = 0.f;
        }
    }

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) return;
        if (!module) return;

        if (!font) {
            font = APP->window->loadFont(
                asset::system("res/fonts/ShareTechMono-Regular.ttf"));
        }

        float cx, cy, rIn, rOut;
        centerR(cx, cy, rIn, rOut);
        const float rMid = (rIn + rOut) * 0.5f;
        const float thickness = (rOut - rIn);

        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, rOut + 5.f);
        nvgFillColor(args.vg, nvgRGBA(10, 2, 20, 200));
        nvgFill(args.vg);
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, rOut + 5.f);
        nvgStrokeColor(args.vg, nvgRGBA(160, 0, 255, 80));
        nvgStrokeWidth(args.vg, 3.f);
        nvgStroke(args.vg);
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, rOut + 5.f);
        nvgStrokeColor(args.vg, nvgRGBA(0, 255, 255, 100));
        nvgStrokeWidth(args.vg, 1.f);
        nvgStroke(args.vg);
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, rIn - 2.f);
        nvgStrokeColor(args.vg, nvgRGBA(255, 0, 255, 50));
        nvgStrokeWidth(args.vg, 1.f);
        nvgStroke(args.vg);

        for (int i = 0; i < NUM_SEGS; i++) {
            const SegDef& d = segDef(i);
            float a0, a1;
            segAngles(i, a0, a1);
            const bool focused = (focusedSeg == i);
            NVGcolor base = d.color;
            base.a = focused ? 0.3f : 0.1f;
            nvgBeginPath(args.vg);
            nvgArc(args.vg, cx, cy, rMid, a0, a1, NVG_CW);
            nvgStrokeColor(args.vg, base);
            nvgStrokeWidth(args.vg, thickness);
            nvgStroke(args.vg);

            const float val = module->params[d.paramId].getValue();
            float nv = (val - d.minVal) / (d.maxVal - d.minVal);
            if (nv < 0.f) nv = 0.f;
            if (nv > 1.f) nv = 1.f;
            if (nv > 0.002f) {
                NVGcolor fill = d.color;
                fill.a = focused ? 1.f : 0.7f;
                const float va1 = a0 + (a1 - a0) * nv;
                nvgBeginPath(args.vg);
                nvgArc(args.vg, cx, cy, rMid, a0, va1, NVG_CW);
                nvgStrokeColor(args.vg, fill);
                nvgStrokeWidth(args.vg, thickness * 0.8f);
                nvgStroke(args.vg);
            }

            if (lastTouchedSeg == i && flashAlpha > 0.01f) {
                NVGcolor fl = nvgRGBA(255, 255, 255,
                    (int)(200.f * flashAlpha + 0.5f));
                nvgBeginPath(args.vg);
                nvgArc(args.vg, cx, cy, rMid, a0, a1, NVG_CW);
                nvgStrokeColor(args.vg, fl);
                nvgStrokeWidth(args.vg, thickness * 0.95f);
                nvgStroke(args.vg);
            }

            if (font) {
                const float midAng = (a0 + a1) * 0.5f;
                float lx, ly;
                if (i == 0) {
                    const float rLabel = rIn + (rOut - rIn) * 0.5f;
                    lx = cx;
                    ly = cy - rLabel;
                } else {
                    const float labelR = rIn + (rOut - rIn) * 0.5f;
                    lx = cx + labelR * std::cos(midAng);
                    ly = cy + labelR * std::sin(midAng);
                }
                nvgFontFaceId(args.vg, font->handle);
                nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
                nvgFontSize(args.vg, 7.5f);
                NVGcolor lc = focused ? nvgRGB(255, 255, 255) :
                                       nvgRGBA(200, 200, 200, 200);
                nvgFillColor(args.vg, lc);
                nvgText(args.vg, lx, ly, d.shortName, nullptr);
            }
        }

        const float rNav = rOut + NAV_RADIUS_OFFSET;
        for (int w = 0; w < 2; ++w) {
            float na0, na1;
            navAngles(w, na0, na1);
            const bool hov = (hoverNav == w);
            const float mid = (na0 + na1) * 0.5f;
            const float tx = cx + rNav * std::cos(mid);
            const float ty = cy + rNav * std::sin(mid);
            nvgBeginPath(args.vg);
            nvgArc(args.vg, cx, cy, rNav, na0, na1, NVG_CW);
            nvgStrokeColor(args.vg, hov ? nvgRGBA(0, 255, 255, 255) :
                                          nvgRGBA(0, 200, 200, 150));
            nvgStrokeWidth(args.vg, NAV_STROKE_WIDTH);
            nvgStroke(args.vg);
            if (font) {
                nvgFontFaceId(args.vg, font->handle);
                nvgFontSize(args.vg, 9.f);
                nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
                nvgFillColor(args.vg, hov ? nvgRGB(255, 255, 255) :
                                            nvgRGB(0, 255, 255));
                nvgText(args.vg, tx, ty, (w == 0) ? "<" : ">", nullptr);
            }
        }

        if (font) {
            nvgFontFaceId(args.vg, font->handle);
            nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            if (focusedSeg >= 0) {
                const SegDef& d = segDef(focusedSeg);
                const float val = module->params[d.paramId].getValue();
                std::string valueStr;
                if (d.paramId == Hutara_Random_CV::SHAPE_PARAM) {
                    const int sIdx = Hutara_Random_CV::clampInt(
                        (int)std::round(val * (Hutara_Random_CV::NUM_SHAPES - 1)),
                        0, Hutara_Random_CV::NUM_SHAPES - 1);
                    valueStr = Hutara_Random_CV::shapeName(sIdx);
                } else if (d.paramId == Hutara_Random_CV::WARP_PARAM) {
                    valueStr = string::f("%+.2f", val);
                } else {
                    valueStr = string::f("%.0f%%", val * 100.f);
                }
                float asc1, desc1, lineH1;
                nvgFontSize(args.vg, 7.0f);
                nvgTextMetrics(args.vg, &asc1, &desc1, &lineH1);
                float asc2, desc2, lineH2;
                nvgFontSize(args.vg, 9.5f);
                nvgTextMetrics(args.vg, &asc2, &desc2, &lineH2);
                const float totalH = lineH1 + lineH2 + 2.f;
                const float y1 = cy - totalH * 0.5f + lineH1 * 0.5f;
                const float y2 = cy - totalH * 0.5f + lineH1 + 2.f + lineH2 * 0.5f;
                nvgFontSize(args.vg, 7.0f);
                nvgFillColor(args.vg, nvgRGB(255, 255, 0));
                nvgText(args.vg, cx, y1, d.name, nullptr);
                nvgFontSize(args.vg, 9.5f);
                nvgFillColor(args.vg, nvgRGB(0, 255, 255));
                nvgText(args.vg, cx, y2, valueStr.c_str(), nullptr);
            } else {
                nvgFontSize(args.vg, 7.5f);
                nvgFillColor(args.vg, nvgRGB(255, 0, 255));
                nvgText(args.vg, cx, cy - 4.f, "DRAG", nullptr);
                nvgText(args.vg, cx, cy + 4.f, "SEGMENT", nullptr);
            }
        }
    }
};

// =========================================================================
// Module widget
// =========================================================================
struct Hutara_Random_CVWidget : ModuleWidget {
    static constexpr int ADDITIONAL_PANEL_HP = 16;
    static constexpr float DISPLAY_X_OFFSET = 8.f;
    static constexpr float DISPLAY_Y        = 20.f;
    static constexpr float DISPLAY_W_MARGIN = 16.f;
    static constexpr float DISPLAY_H        = 68.f;
    static constexpr float RING_SIZE        = 60.f;
    static constexpr float RING_Y           = 180.f;
    static constexpr bool  RING_CENTERED_X  = true;
    static constexpr float CV_RING_DIST     = RING_SIZE * 0.5f + 14.f;

    Hutara_Random_CVWidget(Hutara_Random_CV* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Hutaraa.svg")));
        const float totalWidth = box.size.x;
        const float addedWidth = static_cast<float>(ADDITIONAL_PANEL_HP) * RACK_GRID_WIDTH;
        const float legacyWidth = std::max(0.f, totalWidth - addedWidth);
        const float rightX = legacyWidth;
        const float rightW = std::max(0.f, totalWidth - rightX);

        // === FIX: sanity check panel width ===
        const float expectedWidth = legacyWidth + addedWidth;
        if (std::fabs(totalWidth - expectedWidth) > 0.5f) {
            WARN("Hutara_Random_CV: panel width mismatch: box.size.x=%.2f expected=%.2f",
                totalWidth, expectedWidth);
        }

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0.f)));
        addChild(createWidget<ScrewSilver>(Vec(legacyWidth - 2.f * RACK_GRID_WIDTH, 0.f)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(legacyWidth - 2.f * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(rightX + RACK_GRID_WIDTH, 0.f)));
        addChild(createWidget<ScrewSilver>(Vec(totalWidth - 2.f * RACK_GRID_WIDTH, 0.f)));
        addChild(createWidget<ScrewSilver>(Vec(rightX + RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(totalWidth - 2.f * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParam<Rogan1PWhite>(Vec(45.f, 70.f), module, Hutara_Random_CV::STRENGTH_PARAM));
        addParam(createParam<RoundSmallBlackKnob>(Vec(85.f, 70.f), module, Hutara_Random_CV::SMOOTH_RANDOM_PARAM));
        addParam(createParam<RoundSmallBlackKnob>(Vec(20.f, 330.f), module, Hutara_Random_CV::RANDOM_GATE_SPEED_PARAM));
        {
            auto* lbl = new GateSpeedLabel;
            lbl->module = module;
            lbl->box.pos  = Vec(20.f - 15.f, 330.f - 12.f);
            lbl->box.size = Vec(30.f, 14.f);
            addChild(lbl);
        }
        addParam(createParam<RoundSmallBlackKnob>(Vec(60.f, 330.f), module, Hutara_Random_CV::INPUT_CLOCK_PPQN_PARAM));
        {
            auto* lbl = new InputPpqnLabel;
            lbl->module = module;
            lbl->box.pos  = Vec(60.f + 5.f, 330.f - 12.f);
            lbl->box.size = Vec(30.f, 14.f);
            addChild(lbl);
        }
        addParam(createParam<BefacoTinyKnob>(Vec(47.f, 215.f), module, Hutara_Random_CV::OUTPUT_VOLTAGE_PARAM));
        addParam(createParam<BefacoSwitch>(Vec(10.f, 175.f), module, Hutara_Random_CV::BIPOLAR_PARAM));
        addParam(createParam<RoundSmallBlackKnob>(Vec(68.f, 275.f), module, Hutara_Random_CV::GATE_LENGTH_PARAM));
        addParam(createParam<RoundBlackKnob>(Vec(17.f, 120.f), module, Hutara_Random_CV::TRIGGER_DIVISION_DIVIDER_PARAM));
        addParam(createParam<LEDSliderBlue>(Vec(89.f, 150.f), module, Hutara_Random_CV::S_H_OFFSET_PARAM));
        addParam(createParam<BefacoTinyKnob>(Vec(42.f, 305.f), module, Hutara_Random_CV::GATE_PROBABILITY_PARAM));
        addParam(createParam<MyHutaraSmallKnob>(Vec(64.f, 126.f), module, Hutara_Random_CV::MODULATION_INPUT_PARAM));

        addParam(createParam<CKSSThreeHorizontal>(Vec(42.f, 355.f), module, Hutara_Random_CV::GATE_MODE_PARAM));
        {
            auto* lbl = new GateModeLabel;
            lbl->module = module;
            lbl->box.pos  = Vec(42.f, 355.f - 10.f);
            lbl->box.size = Vec(30.f, 14.f);
            addChild(lbl);
        }

        addInput(createInput<PJ301MPort>(Vec(10.f, 55.f), module, Hutara_Random_CV::ON_INPUT));
        addInput(createInput<PJ301MPort>(Vec(88.f, 125.f), module, Hutara_Random_CV::MODULATION_INPUT));
        addOutput(createOutput<PJ3410Port>(Vec(46.f, 175.f), module, Hutara_Random_CV::OUTPUT));
        addOutput(createOutput<PJ3410Port>(Vec(95.f, 290.f), module, Hutara_Random_CV::GATE_OUTPUT));
        addOutput(createOutput<PJ3410Port>(Vec(95.f, 330.f), module, Hutara_Random_CV::GATE_OUTPUT_INVERTED));

        const float c1 = rightX + rightW * 0.18f;
        const float c2 = rightX + rightW * 0.50f;
        const float c3 = rightX + rightW * 0.82f;

        {
            auto* display = new HutaraRandomDisplay;
            display->module = module;
            display->box.pos  = Vec(rightX + DISPLAY_X_OFFSET, DISPLAY_Y);
            display->box.size = Vec(rightW - DISPLAY_W_MARGIN, DISPLAY_H);
            addChild(display);
        }

        float yRow1 = 110.f;
        addParam(createParamCentered<RoundBlackKnob>(Vec(c1, yRow1), module, Hutara_Random_CV::TIME_PARAM));
        addParam(createParamCentered<BefacoTinyKnob>(Vec(c2, yRow1), module, Hutara_Random_CV::BPM_MULTIPLIER_PARAM));
        addParam(createParamCentered<BefacoTinyKnob>(Vec(c3, yRow1), module, Hutara_Random_CV::CLOCK_PPQN_PARAM));

        float yRow2 = 145.f;
        addParam(createParamCentered<BefacoSwitchSmallKnob>(Vec(c1, yRow2), module, Hutara_Random_CV::TIME_SYNC_MODE_PARAM));
        addParam(createParamCentered<LEDSliderGreen>(Vec(c3, yRow2 + 55.f), module, Hutara_Random_CV::ENVELOPE_RANGE_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(Vec(c2, 310.f), module, Hutara_Random_CV::LEVEL_PARAM));

        const float ringCX = RING_CENTERED_X ? rightX + rightW * 0.5f : rightX + RING_SIZE * 0.5f;
        const float ringCY = RING_Y + RING_SIZE * 0.5f;
        {
            auto* ring = new RandomParamRing;
            ring->module = module;
            ring->box.pos  = Vec(ringCX - RING_SIZE * 0.5f, RING_Y);
            ring->box.size = Vec(RING_SIZE, RING_SIZE);
            addChild(ring);
        }

        addInput(createInputCentered<PJ301MPort>(Vec(ringCX, ringCY - CV_RING_DIST), module, Hutara_Random_CV::SHAPE_INPUT));
        addInput(createInputCentered<PJ301MPort>(Vec(ringCX + CV_RING_DIST, ringCY), module, Hutara_Random_CV::WARP_INPUT));
        addInput(createInputCentered<PJ301MPort>(Vec(ringCX, ringCY + CV_RING_DIST), module, Hutara_Random_CV::FLUX_INPUT));
        addInput(createInputCentered<PJ301MPort>(Vec(ringCX - CV_RING_DIST, ringCY), module, Hutara_Random_CV::RANDOMNESS_INPUT));

        float yRow3 = 275.f;
        addInput(createInputCentered<PJ301MPort>(Vec(c1, yRow3), module, Hutara_Random_CV::TIME_INPUT));
        addInput(createInputCentered<PJ301MPort>(Vec(c3, yRow3), module, Hutara_Random_CV::TRIG_INPUT));
        addInput(createInputCentered<PJ301MPort>(Vec(c3, yRow1), module, Hutara_Random_CV::ENVELOPE_CLOCK_INPUT));

        float yRow4 = 310.f;
        addParam(createParamCentered<BefacoSwitchSmallKnob>(Vec(c1, yRow4), module, Hutara_Random_CV::LOOP_PARAM));
        addParam(createParamCentered<BefacoSwitchSmallKnob>(Vec(c3, yRow4), module, Hutara_Random_CV::ENHANCED_MODE_PARAM));
        {
            auto* off = createParam<LEDSliderBlue>(Vec(c1 - 6.f, yRow2 + 25.f), module, Hutara_Random_CV::ENVELOPE_OFFSET_PARAM);
            off->box.size = Vec(12.f, 46.f);
            addParam(off);
        }
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(24.f, 285.f), module, Hutara_Random_CV::SWING_PARAM));

        addOutput(createOutputCentered<PJ3410Port>(Vec(c1 + 40.f, 355.f), module, Hutara_Random_CV::EOC_OUTPUT));
        addOutput(createOutputCentered<PJ3410Port>(Vec(c2 + 36.f, 355.f), module, Hutara_Random_CV::ENVELOPE_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        auto* module = getModule<Hutara_Random_CV>();
        if (!module) return;
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuItem("Reset Envelope Phase", "", [module]() {
            module->envelopePhase = 1.0;
            module->running = false;
        }));
        menu->addChild(createMenuItem("Reset Gate Phrase", "", [module]() {
            module->gateStep = 0;
            module->gateStepPhase = 0.f;
            module->gateBar = 0;
            module->gateVar = Hutara_Random_CV::V_ORIG;
            module->gateWidthFrac = 0.f;
        }));
        menu->addChild(createMenuItem("Reset PPQN Detection", "", [module]() {
            module->resetPpqnDetection();
        }));
        menu->addChild(new MenuSeparator);
        auto* undoItem = createMenuItem("Undo Draw", "", [module]() {
            module->undoDraw();
        });
        undoItem->disabled = (module->drawUndoCount <= 0);
        menu->addChild(undoItem);
        auto* resetItem = createMenuItem("Reset Draw", "", [module]() {
            module->resetDraw();
        });
        resetItem->disabled = (!module->drawResetAvailable);
        menu->addChild(resetItem);
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuItem("Sync Mode: Free", "", [module]() {
            module->params[Hutara_Random_CV::TIME_SYNC_MODE_PARAM].setValue(0.f);
        }));
        menu->addChild(createMenuItem("Sync Mode: BPM", "", [module]() {
            module->params[Hutara_Random_CV::TIME_SYNC_MODE_PARAM].setValue(1.f);
        }));
        menu->addChild(createMenuItem("Reset BPM to 120", "", [module]() {
            module->measuredBpm = Hutara_Random_CV::DEFAULT_BPM;
            module->dispBpm = module->measuredBpm;
            module->haveBpmMeasurement = false;
        }));

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuItem("Gate Mode: Pattern", "", [module]() {
            module->params[Hutara_Random_CV::GATE_MODE_PARAM].setValue(0.f);
        }));
        menu->addChild(createMenuItem("Gate Mode: Random", "", [module]() {
            module->params[Hutara_Random_CV::GATE_MODE_PARAM].setValue(1.f);
        }));
        menu->addChild(createMenuItem("Gate Mode: Hybrid", "", [module]() {
            module->params[Hutara_Random_CV::GATE_MODE_PARAM].setValue(2.f);
        }));
    }
};

Model* modelHutara_Random_CV = createModel<Hutara_Random_CV, Hutara_Random_CVWidget>("Hutara_Random_CV");