// =============================================================================
//  Hutara PsyOperatorFX  --  Revision 19
//
//  Panel + UI/UX optimisation:
//    * Panel shrunk from 42 HP (213.36 mm) to 36 HP (182.88 mm) -- the right
//      side of the previous build was mostly empty.
//    * Right column now holds ALL outputs AND the three remaining inputs
//      (CLOCK, TRIGGER, GATE_CV) with a short label under every jack.
//    * Left column keeps AUDIO_INPUT only.
//    * Removed the centre dot on PsyKnobSmall and PsyStepKnob.
//    * PsyStepKnob LED dots pushed from 0.46 to 0.62 of the box size so they
//      fully surround the knob.
//    * Every widget re-placed so nothing falls outside its section frame.
//    * Darkpsy palette preserved (near-black panel, neon-green borders,
//      purple accents, turquoise step cells).
//    * Crash fixes from Rev 18b retained: no duplicate port registrations,
//      no external SVG, single background widget.
// =============================================================================

#include "plugin.hpp"
#include "PsyFXCore.hpp"

using namespace rack;
using namespace psyfx;

namespace {

// -----------------------------------------------------------------------------
//  Darkpsy colour palette
// -----------------------------------------------------------------------------

namespace dp {
    static const NVGcolor kPanelBg       = nvgRGB(6, 8, 10);
    static const NVGcolor kSectionBg     = nvgRGBA(10, 18, 16, 220);
    static const NVGcolor kSectionBorder = nvgRGBA(0, 220, 140, 90);
    static const NVGcolor kSectionHeader = nvgRGBA(0, 220, 140, 45);
    static const NVGcolor kSectionTitle  = nvgRGBA(140, 255, 190, 255);

    static const NVGcolor kLabel         = nvgRGBA(200, 220, 210, 255);
    static const NVGcolor kLabelDim      = nvgRGBA(140, 180, 165, 220);
    static const NVGcolor kBrandTitle    = nvgRGBA(230, 255, 245, 255);
    static const NVGcolor kBrandSub      = nvgRGBA(0, 220, 140, 255);

    static const NVGcolor kRingOn        = nvgRGBA(0, 255, 160, 255);
    static const NVGcolor kRingOff       = nvgRGBA(20, 60, 45, 200);

    static const NVGcolor kStepOn        = nvgRGB(0, 200, 220);
    static const NVGcolor kStepOff       = nvgRGB(30, 45, 55);
    static const NVGcolor kStepBorder    = nvgRGBA(0, 150, 180, 200);
    static const NVGcolor kStepCurrent   = nvgRGB(255, 210, 110);
    static const NVGcolor kAccOn         = nvgRGB(180, 60, 220);
    static const NVGcolor kAccOff        = nvgRGB(50, 30, 60);
    static const NVGcolor kAccBorder     = nvgRGBA(140, 80, 180, 200);

    static const NVGcolor kDispBg        = nvgRGB(4, 10, 8);
    static const NVGcolor kDispBorder    = nvgRGBA(0, 200, 140, 110);
    static const NVGcolor kDispWave      = nvgRGB(120, 255, 180);
    static const NVGcolor kDispText      = nvgRGBA(160, 230, 200, 240);
    static const NVGcolor kDispWrite     = nvgRGB(220, 240, 230);
    static const NVGcolor kDispRead      = nvgRGB(0, 255, 160);
}

// Panel dimensions: 36 HP x 128.5 mm.
static const float PANEL_MM_W = 182.88f;
static const float PANEL_MM_H = 128.5f;

// -----------------------------------------------------------------------------
//  Module  (DSP identical to Rev 18)
// -----------------------------------------------------------------------------

struct PsyOperatorFX : Module {

    enum ParamIds {
        SPEED_PARAM, STRETCH_PARAM, PITCH_PARAM, SCATTER_PARAM, SPREAD_PARAM,
        STUTTER_SWITCH_PARAM, STUTTER_RATE_PARAM, CAPTURE_PARAM, REPEATS_PARAM,
        GATE_PARAM, PROBABILITY_PARAM,
        WOW_PARAM, FLUTTER_PARAM, STOP_SWITCH_PARAM, FREEZE_SWITCH_PARAM,
        REVERSE_SWITCH_PARAM,
        DRIVE_PARAM, TONE_PARAM, WIDTH_PARAM,
        MIX_PARAM, OUTPUT_PARAM,
        CHAOS_AMOUNT_PARAM, CHAOS_RATE_PARAM, CHAOS_TRIGGER_PARAM,
        TRANCE_GATE_SWITCH_PARAM, TRANCE_GATE_DEPTH_PARAM,
        TRANCE_GATE_PATTERN_PARAM, TRANCE_GATE_RATE_PARAM,
        TRANCE_GATE_SHAPE_PARAM, TRANCE_GATE_RESET_PARAM,
        DELAY_SWITCH_PARAM, DELAY_TIME_PARAM, DELAY_FEEDBACK_PARAM,
        DELAY_MIX_PARAM, DELAY_TONE_PARAM,
        PHASE1_MACRO_PARAM,
        FX_SRR_PARAM, FX_BITS_PARAM, FX_FOLD_PARAM, FX_RING_PARAM,
        FX_RFRQ_PARAM, FX_CUT_PARAM, FX_RES_PARAM, FX_MODE_PARAM,
        FX_COMB_PARAM, FX_DUCK_PARAM,
        SEQ_RATCHET_PARAM, SEQ_SWING_PARAM, SEQ_ACCENT_PARAM,
        SEQ_STEP_0_PARAM, SEQ_STEP_1_PARAM, SEQ_STEP_2_PARAM, SEQ_STEP_3_PARAM,
        SEQ_STEP_4_PARAM, SEQ_STEP_5_PARAM, SEQ_STEP_6_PARAM, SEQ_STEP_7_PARAM,
        SEQ_STEP_8_PARAM, SEQ_STEP_9_PARAM, SEQ_STEP_10_PARAM, SEQ_STEP_11_PARAM,
        SEQ_STEP_12_PARAM, SEQ_STEP_13_PARAM, SEQ_STEP_14_PARAM, SEQ_STEP_15_PARAM,
        SEQ_ACC_0_PARAM, SEQ_ACC_1_PARAM, SEQ_ACC_2_PARAM, SEQ_ACC_3_PARAM,
        SEQ_ACC_4_PARAM, SEQ_ACC_5_PARAM, SEQ_ACC_6_PARAM, SEQ_ACC_7_PARAM,
        SEQ_ACC_8_PARAM, SEQ_ACC_9_PARAM, SEQ_ACC_10_PARAM, SEQ_ACC_11_PARAM,
        SEQ_ACC_12_PARAM, SEQ_ACC_13_PARAM, SEQ_ACC_14_PARAM, SEQ_ACC_15_PARAM,
        NUM_PARAMS
    };

    enum InputIds {
        AUDIO_INPUT, SPEED_CV_INPUT, STRETCH_CV_INPUT, PITCH_CV_INPUT,
        SCATTER_CV_INPUT, STUTTER_CV_INPUT, WOW_CV_INPUT, FLUTTER_CV_INPUT,
        DRIVE_CV_INPUT, MIX_CV_INPUT, CLOCK_INPUT, TRIGGER_INPUT,
        EXT_MOD_INPUT, GATE_CV_INPUT, DELAY_CV_INPUT, PHASE1_MACRO_CV_INPUT,
        NUM_INPUTS
    };

    enum OutputIds {
        AUDIO_L_OUTPUT, AUDIO_R_OUTPUT,
        GATE_OUTPUT, EOC_OUTPUT, CLOCK_THRU_OUTPUT, ENV_OUTPUT,
        NUM_OUTPUTS
    };

    enum LightIds {
        FREEZE_LIGHT, STOP_LIGHT, STUTTER_LIGHT, SIGNAL_LIGHT,
        EXT_MOD_LIGHT, GATE_LIGHT, DELAY_LIGHT,
        NUM_LIGHTS
    };

    enum ModTarget {
        MOD_SPEED, MOD_GRAIN, MOD_PITCH, MOD_SCATTER, MOD_RATE,
        MOD_DRIVE, MOD_MIX, MOD_WOW, MOD_FLUTTER, MOD_GATE, MOD_DELAY,
        MOD_COUNT
    };

    enum StopPhase { STOP_NORMAL, STOP_DECAY, STOP_HOLD, STOP_RISE };
    enum { CHAOS_STREAMS = 3 };

    alignas(16) float bufL[TAPE_SIZE];
    alignas(16) float bufR[TAPE_SIZE];
    int writePos;
    float readLag, readPos, rateSlew, rateSlewCoeff, tapeMod;

    float wowPhase, wowWanderPhase, flutterPhase;
    float stopAmount;
    int stopPhase;
    bool freezeLatch;
    float winStart, winLen;

    float dispOldLag, dispFade, dispEventHold, scatterSlew;
    float dispFadeStep, sliceSwapStep, rateFlipCoeff, scatterSlewCoeff;
    float timelineSlewCoeff, timelineSlew;

    float grainLag, grainAnchor;
    int writtenSamples;
    float grainBackMax;

    float clockPeriod;
    int samplesSinceClock;
    bool clockLocked;
    float gridPeriod, gridPhase;
    bool gridTick;

    uint8_t userGate[8][32];
    uint8_t accentGate[8][32];
    int userGateLength;
    int userGateIndex;
    OnePole swingSmooth, accentSmooth;

    bool sliceActive;
    float sliceStart, sliceContent, sliceLen, sliceRate;
    float sliceProgress, sliceElapsed;
    int sliceRepeats;
    bool pendingReload;
    float sliceLayerGain, sliceLayerCoeff, sliceFade;
    bool oldActive;
    float oldStart, oldContent, oldProgress, oldRate, oldLen, oldEnv;
    bool stutterWasOn;

    Grain grains[MAX_GRAINS];
    float grainClock, grainInterval;

    ModSlot modView[MOD_COUNT];
    OnePole modSmooth[MOD_COUNT];
    OnePole toneSmooth, widthSmooth, gateSmooth;
    float chaosValue[CHAOS_STREAMS];
    float chaosPhase;
    float chaosLevel;

    float extModValue;
    float extModActive;

    float modSource[N_MOD_SOURCES];
    PsyLFO lfo;
    float lfoValue;
    float lfoRateHz;
    PsyEnvFollower envFollow;
    float envValue;
    OnePole macroSmooth;
    float macroIndex;
    int dispMacro;

    bool tranceGateOn, tranceGateReset;
    float tranceGateEnv, tranceGateDepth, tranceGateStepPhase;
    int tranceGateStep;
    uint16_t tranceGatePattern;
    float tranceGateStepSamples, tranceGatePhaseCounter;
    float tranceGateShape, tranceGateSmoothedShape, tranceGateShapeSlewCoeff;
    float tranceGateStepFlash;

    bool gateCvConnected, gateCvClockMode;
    dsp::SchmittTrigger gateCvTrigger;

    alignas(16) float delayL[TAPE_SIZE];
    alignas(16) float delayR[TAPE_SIZE];
    int delayWrite;
    float delayTimeSamples, delayTimeSamplesR;
    float delayFeedback, delayMix, delayToneCoeff;
    float delayToneL, delayToneR;
    float delayHpL, delayHpR;
    bool delayOn;
    float delayEnableFade;

    OnePole fxSrr, fxBits, fxFold, fxRing, fxRingFreq;
    OnePole fxCut, fxRes, fxMode, fxComb, fxDuck;
    float srrHoldL, srrHoldR, srrPhaseL, srrPhaseR;
    float svfLpL, svfBpL, svfLpR, svfBpR;
    float ringPhase;
    alignas(16) float combL[TAPE_SIZE];
    alignas(16) float combR[TAPE_SIZE];
    int combWrite;
    float duckLevel;
    int dispFx;

    float toneL1, toneL2, toneR1, toneR2;
    float dcXL, dcYL, dcXR, dcYR, dcR;

    float scopeMin[SCOPE_BUCKETS], scopeMax[SCOPE_BUCKETS];
    int scopeBucket, scopeCounter, scopeSamplesPerBucket;
    float dispWrite, dispRead, dispWinStart, dispWinLen;
    float dispSliceStart, dispSliceLen, dispSliceActive;
    float dispDivision, dispTimeline, dispReverse, dispStop, dispRate;
    float dispRms, dispChaos, dispExtMod;
    float dispGate;
    int dispGateStep;
    uint16_t dispGatePattern;
    float dispGateShape, dispGatePhase, dispGateStepFlash;
    float dispDelayTime, dispDelayMix, dispDelayFb;

    float sampleRate;
    float signalLevel;
    uint32_t rngState;

    dsp::SchmittTrigger clockTrigger, triggerInput, captureButton, chaosButton;
    dsp::PulseGenerator eocPulse, clockThruPulse;
    bool sliceEocEvent, clockThruEvent;
    float clockThruPhase;

    struct DspParams {
        float speed, tapeRate, tapeApplied, timelineW;
        float pitchSt, pitchRatio, grainNorm;
        float scatter, spread;
        float wow, flutter;
        float drive, tone, width, mix, output;
        float gate, probability;
        int repeatCount, rateIndex;
        bool stutterOn, stopOn, freezeOn, reverseOn;

        bool tranceGateOn, tranceGateReset;
        float tranceGateDepth;
        uint16_t tranceGatePattern, tranceGateAccent;
        int tranceGateRateIndex;
        float tranceGateShape;
        int seqRatchet;
        float seqSwing, seqAccent;

        bool delayOn;
        int delayDivIndex;
        float delayFeedback, delayMix, delayTone;

        float fSrr, fBits, fFold, fRing, fRingFreq;
        float fCut, fRes, fMode, fComb, fDuck;
    };

    // -------------------------------------------------------------------------
    //  Construction
    // -------------------------------------------------------------------------

    PsyOperatorFX() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

        configParam(SPEED_PARAM, -1.f, 1.f, 0.f, "Speed");
        configParam(STRETCH_PARAM, 0.f, 1.f, 0.f, "Grain / stretch");
        configParam(PITCH_PARAM, -12.f, 12.f, 0.f, "Pitch", " st");
        configParam(SCATTER_PARAM, 0.f, 1.f, 0.f, "Scatter");
        configParam(SPREAD_PARAM, 0.f, 1.f, 0.f, "Spread");

        configParam(STUTTER_SWITCH_PARAM, 0.f, 1.f, 0.f, "Stutter");
        configParam(STUTTER_RATE_PARAM, 0.f, 15.f, 3.f, "Stutter rate");
        configParam(CAPTURE_PARAM, 0.f, 1.f, 0.f, "Capture slice");
        configParam(REPEATS_PARAM, 0.f, 1.f, 0.34f, "Repeats");
        configParam(GATE_PARAM, 0.f, 1.f, 0.85f, "Gate / decay");
        configParam(PROBABILITY_PARAM, 0.f, 1.f, 1.f, "Capture probability");

        configParam(WOW_PARAM, 0.f, 1.f, 0.f, "Wow");
        configParam(FLUTTER_PARAM, 0.f, 1.f, 0.f, "Flutter");
        configParam(STOP_SWITCH_PARAM, 0.f, 1.f, 0.f, "Tape stop");
        configParam(FREEZE_SWITCH_PARAM, 0.f, 1.f, 0.f, "Freeze");
        configParam(REVERSE_SWITCH_PARAM, 0.f, 1.f, 0.f, "Reverse");

        configParam(DRIVE_PARAM, 0.f, 1.f, 0.f, "Drive");
        configParam(TONE_PARAM, 0.f, 1.f, 0.5f, "Tone");
        configParam(WIDTH_PARAM, 0.f, 1.f, 0.5f, "Width");

        configParam(MIX_PARAM, 0.f, 1.f, 1.f, "Dry / wet");
        configParam(OUTPUT_PARAM, 0.f, 2.f, 1.f, "Output level");

        configParam(CHAOS_AMOUNT_PARAM, 0.f, 1.f, 0.30f, "Modulation amount");
        configParam(CHAOS_RATE_PARAM, 0.f, 1.f, 0.35f, "Chaos rate");
        configParam(CHAOS_TRIGGER_PARAM, 0.f, 1.f, 0.f, "New random values");

        configParam(TRANCE_GATE_SWITCH_PARAM, 0.f, 1.f, 0.f, "Trance gate");
        configParam(TRANCE_GATE_DEPTH_PARAM, 0.f, 1.f, 0.85f, "Gate depth");
        configParam(TRANCE_GATE_PATTERN_PARAM, 0.f, 7.f, 0.f, "Gate pattern");
        configParam(TRANCE_GATE_RATE_PARAM, 0.f, 15.f, 3.f, "Gate rate");
        configParam(TRANCE_GATE_SHAPE_PARAM, 0.f, 1.f, 0.30f, "Gate shape");
        configParam(TRANCE_GATE_RESET_PARAM, 0.f, 1.f, 1.f, "Gate reset");

        configParam(DELAY_SWITCH_PARAM, 0.f, 1.f, 0.f, "Beat delay");
        configParam(DELAY_TIME_PARAM, 0.f, 15.f, 3.f, "Delay time");
        configParam(DELAY_FEEDBACK_PARAM, 0.f, 1.f, 0.45f, "Delay feedback");
        configParam(DELAY_MIX_PARAM, 0.f, 1.f, 0.f, "Delay mix");
        configParam(DELAY_TONE_PARAM, 0.f, 1.f, 0.5f, "Delay tone");

        static_assert(MOD_RATE == 4 && MOD_DELAY == 10 && MOD_COUNT == MOD_TARGET_COUNT,
                      "PsyFXCore modulation matrix must match the ModTarget enum order");
        configParam(PHASE1_MACRO_PARAM, 0.f, static_cast<float>(MACRO_COUNT - 1), 0.f,
                    "Macro", " (DARK / HITECH / GLITCH / RISER)");

        configParam(FX_SRR_PARAM, 0.f, 1.f, 0.f, "Sample rate reducer");
        configParam(FX_BITS_PARAM, 0.f, 1.f, 0.f, "Bit depth");
        configParam(FX_FOLD_PARAM, 0.f, 1.f, 0.f, "Wavefolder amount");
        configParam(FX_RING_PARAM, 0.f, 1.f, 0.f, "Ring modulation amount");
        configParam(FX_RFRQ_PARAM, 0.f, 1.f, 0.25f, "Ring modulator frequency");
        configParam(FX_CUT_PARAM, 0.f, 1.f, 1.f, "Morph filter cutoff");
        configParam(FX_RES_PARAM, 0.f, 1.f, 0.f, "Morph filter resonance");
        configParam(FX_MODE_PARAM, 0.f, 1.f, 0.f, "Morph filter tilt");
        configParam(FX_COMB_PARAM, 0.f, 1.f, 0.f, "Comb filter time");
        configParam(FX_DUCK_PARAM, 0.f, 1.f, 0.f, "Sidechain duck");

        configParam(SEQ_RATCHET_PARAM, 0.f, 7.f, 0.f, "Ratchet");
        configParam(SEQ_SWING_PARAM, 0.f, 1.f, 0.f, "Swing / shuffle");
        configParam(SEQ_ACCENT_PARAM, 0.f, 1.f, 0.f, "Accent amount");
        for (int s = 0; s < 16; ++s) {
            configParam(SEQ_STEP_0_PARAM + s, 0.f, 1.f, 0.f,
                        string::f("Gate step %d", s + 1));
            configParam(SEQ_ACC_0_PARAM + s, 0.f, 1.f, 0.f,
                        string::f("Accent step %d", s + 1));
        }

        configInput(AUDIO_INPUT, "Audio");
        configInput(SPEED_CV_INPUT, "Speed CV");
        configInput(STRETCH_CV_INPUT, "Grain CV");
        configInput(PITCH_CV_INPUT, "Pitch CV (1V/oct)");
        configInput(SCATTER_CV_INPUT, "Scatter CV");
        configInput(STUTTER_CV_INPUT, "Stutter gate CV");
        configInput(WOW_CV_INPUT, "Wow CV");
        configInput(FLUTTER_CV_INPUT, "Flutter CV");
        configInput(DRIVE_CV_INPUT, "Drive CV");
        configInput(MIX_CV_INPUT, "Mix CV");
        configInput(CLOCK_INPUT, "Clock (internal 120 BPM when unpatched)");
        configInput(TRIGGER_INPUT, "Slice trigger / new slice");
        configInput(EXT_MOD_INPUT, "External modulation");
        configInput(GATE_CV_INPUT, "Trance gate step advance");
        configInput(DELAY_CV_INPUT, "Delay mix CV");
        configInput(PHASE1_MACRO_CV_INPUT, "Macro CV (0 V = DARK ... 10 V = RISER)");

        configOutput(AUDIO_L_OUTPUT, "Left");
        configOutput(AUDIO_R_OUTPUT, "Right");
        configOutput(GATE_OUTPUT, "Trance gate");
        configOutput(EOC_OUTPUT, "End of slice trigger");
        configOutput(CLOCK_THRU_OUTPUT, "Clock thru");
        configOutput(ENV_OUTPUT, "Envelope follower");

        configLight(FREEZE_LIGHT, "Freeze");
        configLight(STOP_LIGHT, "Tape stop");
        configLight(STUTTER_LIGHT, "Slice");
        configLight(SIGNAL_LIGHT, "Output level");
        configLight(EXT_MOD_LIGHT, "Modulation bus active");
        configLight(GATE_LIGHT, "Trance gate step");
        configLight(DELAY_LIGHT, "Delay active");

        configBypass(AUDIO_INPUT, AUDIO_L_OUTPUT);
        configBypass(AUDIO_INPUT, AUDIO_R_OUTPUT);

        resetState();
        syncParamsFromUserGate();
    }

    void syncParamsFromUserGate() {
        if (paramQuantities.size() <= static_cast<size_t>(SEQ_ACC_15_PARAM))
            return;
        for (int i = 0; i < 16; ++i) {
            paramQuantities[SEQ_STEP_0_PARAM + i]->setValue(
                userGate[userGateIndex][i] ? 1.f : 0.f);
            paramQuantities[SEQ_ACC_0_PARAM + i]->setValue(
                accentGate[userGateIndex][i] ? 1.f : 0.f);
        }
    }

    void resetState() {
        std::memset(bufL, 0, sizeof(bufL));
        std::memset(bufR, 0, sizeof(bufR));
        std::memset(delayL, 0, sizeof(delayL));
        std::memset(delayR, 0, sizeof(delayR));
        std::memset(userGate, 0, sizeof(userGate));
        std::memset(accentGate, 0, sizeof(accentGate));
        for (int p = 0; p < 8; ++p)
            for (int s = 0; s < 16; ++s)
                userGate[p][s] = static_cast<uint8_t>((GATE_PATTERNS[p] >> s) & 1);
        userGateLength = 16;
        userGateIndex = 0;
        swingSmooth.set(0.f);
        accentSmooth.set(0.f);
        writePos = 0;
        writtenSamples = 0;
        grainBackMax = GRAN_LAG_MAX;
        readLag = NOMINAL_LAG;
        readPos = wrapTape(0.f - NOMINAL_LAG);
        rateSlew = 1.f;
        rateSlewCoeff = 0.f;
        tapeMod = 1.f;
        wowPhase = wowWanderPhase = flutterPhase = 0.f;
        stopAmount = 0.f;
        stopPhase = STOP_NORMAL;
        freezeLatch = false;
        winStart = 0.f;
        winLen = MIN_LAG * 4.f;
        dispOldLag = NOMINAL_LAG;
        dispFade = dispEventHold = scatterSlew = 0.f;
        dispFadeStep = sliceSwapStep = 1.f;
        rateFlipCoeff = scatterSlewCoeff = timelineSlewCoeff = timelineSlew = 0.f;
        grainAnchor = readPos;
        grainLag = NOMINAL_LAG;
        clockPeriod = 0.125f * 48000.f;
        samplesSinceClock = 0;
        clockLocked = false;
        gridPeriod = 0.03125f * 48000.f;
        gridPhase = 0.f;
        gridTick = false;
        sliceActive = oldActive = false;
        sliceStart = 0.f; sliceContent = 1.f; sliceLen = 1.f; sliceRate = 1.f;
        sliceProgress = sliceElapsed = 0.f;
        sliceRepeats = 1;
        pendingReload = false;
        sliceLayerGain = sliceFade = 0.f;
        sliceLayerCoeff = 0.f;
        oldStart = 0.f; oldContent = 1.f; oldProgress = 0.f;
        oldRate = 1.f; oldLen = 1.f; oldEnv = 0.f;
        stutterWasOn = false;
        for (int i = 0; i < MAX_GRAINS; ++i) grains[i].reset();
        grainClock = 0.f; grainInterval = 256.f;
        for (int i = 0; i < MOD_COUNT; ++i) {
            modView[i] = ModSlot();
            modSmooth[i].set(0.f);
        }
        modSmooth[MOD_SPEED].set(0.5f);
        modSmooth[MOD_PITCH].set(0.5f);
        modSmooth[MOD_RATE].set(0.5f);
        modSmooth[MOD_MIX].set(1.f);
        for (int i = 0; i < CHAOS_STREAMS; ++i) chaosValue[i] = 0.f;
        chaosPhase = 0.f;
        chaosLevel = 0.f;
        extModValue = extModActive = 0.f;
        toneSmooth.set(0.5f);
        widthSmooth.set(0.5f);
        gateSmooth.set(0.85f);
        for (int i = 0; i < N_MOD_SOURCES; ++i) modSource[i] = 0.f;
        lfo.reset();
        lfoValue = 0.f;
        lfoRateHz = 1.f;
        envFollow.reset();
        envValue = 0.f;
        macroIndex = 0.f;
        dispMacro = 0;
        macroSmooth.set(0.f);
        srrHoldL = srrHoldR = 0.f;
        srrPhaseL = srrPhaseR = 0.f;
        svfLpL = svfBpL = svfLpR = svfBpR = 0.f;
        ringPhase = 0.f;
        std::memset(combL, 0, sizeof(combL));
        std::memset(combR, 0, sizeof(combR));
        combWrite = 0;
        duckLevel = 0.f;
        dispFx = 0;
        fxSrr.set(0.f); fxBits.set(0.f); fxFold.set(0.f);
        fxRing.set(0.f); fxRingFreq.set(0.25f);
        fxCut.set(1.f); fxRes.set(0.f); fxMode.set(0.f);
        fxComb.set(0.f); fxDuck.set(0.f);
        tranceGateOn = false;
        tranceGateReset = true;
        tranceGateEnv = 0.f;
        tranceGateDepth = 0.f;
        tranceGateStepPhase = 0.f;
        tranceGateStep = 0;
        tranceGatePattern = GATE_PATTERNS[0];
        tranceGateStepSamples = 6000.f;
        tranceGatePhaseCounter = 0.f;
        tranceGateShape = 0.30f;
        tranceGateSmoothedShape = 0.30f;
        tranceGateShapeSlewCoeff = 0.f;
        tranceGateStepFlash = 0.f;
        gateCvConnected = gateCvClockMode = false;
        gateCvTrigger.reset();
        delayWrite = 0;
        delayTimeSamples = 24000.f;
        delayTimeSamplesR = 18000.f;
        delayFeedback = delayMix = delayToneCoeff = 0.f;
        delayToneL = delayToneR = 0.f;
        delayHpL = delayHpR = 0.f;
        delayOn = false;
        delayEnableFade = 0.f;
        toneL1 = toneL2 = toneR1 = toneR2 = 0.f;
        dcXL = dcYL = dcXR = dcYR = 0.f;
        dcR = 0.9995f;
        for (int i = 0; i < SCOPE_BUCKETS; ++i) {
            scopeMin[i] = 0.f;
            scopeMax[i] = 0.f;
        }
        scopeBucket = scopeCounter = 0;
        scopeSamplesPerBucket = 96;
        dispWrite = dispRead = dispWinStart = dispWinLen = 0.f;
        dispSliceStart = dispSliceLen = dispSliceActive = 0.f;
        dispDivision = 3.f;
        dispTimeline = dispReverse = dispStop = 0.f;
        dispRate = 1.f;
        dispRms = dispChaos = dispExtMod = 0.f;
        dispGate = 0.f;
        dispGateStep = 0;
        dispGatePattern = GATE_PATTERNS[0];
        dispGateShape = 0.30f;
        dispGatePhase = dispGateStepFlash = 0.f;
        dispDelayTime = dispDelayMix = dispDelayFb = 0.f;
        sampleRate = 48000.f;
        signalLevel = 0.f;
        rngState = 0x9E3779B9u;
        clockTrigger.reset();
        triggerInput.reset();
        captureButton.reset();
        chaosButton.reset();
        eocPulse.reset();
        clockThruPulse.reset();
        sliceEocEvent = false;
        clockThruEvent = false;
        clockThruPhase = 0.f;
        updateRates();
    }

    void onReset(const ResetEvent& e) override {
        Module::onReset(e);
        resetState();
        syncParamsFromUserGate();
    }

    void onRandomize(const RandomizeEvent& e) override {
        Module::onRandomize(e);
        for (int i = 0; i < MAX_GRAINS; ++i) grains[i].reset();
        sliceActive = false;
        oldActive = false;
        sliceFade = 0.f;
        sliceLayerGain = 0.f;
        sliceProgress = sliceElapsed = 0.f;
        pendingReload = false;
        grainClock = 0.f;
        dispFade = 0.f;
    }

    void onSampleRateChange(const SampleRateChangeEvent& e) override {
        enableFlushToZero();
        sampleRate = clampF(safeF(e.sampleRate, sampleRate), SR_MIN, SR_MAX);
        updateRates();
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "psyfxSchema", json_integer(1));
        json_object_set_new(rootJ, "gateLength", json_integer(userGateLength));
        json_object_set_new(rootJ, "gateIndex", json_integer(userGateIndex));
        json_t* patsJ = json_array();
        for (int p = 0; p < 8; ++p) {
            std::string hex;
            hex.reserve(32);
            for (int s = 0; s < userGateLength; ++s)
                hex.push_back(userGate[p][s] ? '1' : '0');
            json_array_append_new(patsJ, json_string(hex.c_str()));
        }
        json_object_set_new(rootJ, "gatePatterns", patsJ);
        json_t* accJ = json_array();
        for (int p = 0; p < 8; ++p) {
            std::string hex;
            hex.reserve(32);
            for (int s = 0; s < userGateLength; ++s)
                hex.push_back(accentGate[p][s] ? '1' : '0');
            json_array_append_new(accJ, json_string(hex.c_str()));
        }
        json_object_set_new(rootJ, "accentPatterns", accJ);
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        json_t* schemaJ = json_object_get(rootJ, "psyfxSchema");
        const int schema = (schemaJ && json_is_integer(schemaJ))
            ? static_cast<int>(json_integer_value(schemaJ)) : 0;
        if (schema != 1) {
            resetState();
            syncParamsFromUserGate();
            return;
        }
        json_t* lenJ = json_object_get(rootJ, "gateLength");
        int len = (lenJ && json_is_integer(lenJ))
            ? static_cast<int>(json_integer_value(lenJ)) : 16;
        len = clampI(len, 4, 32);
        json_t* idxJ = json_object_get(rootJ, "gateIndex");
        int idx = (idxJ && json_is_integer(idxJ))
            ? static_cast<int>(json_integer_value(idxJ)) : 0;
        idx = clampI(idx, 0, 7);
        json_t* patsJ = json_object_get(rootJ, "gatePatterns");
        if (patsJ && json_is_array(patsJ)) {
            const size_t n = json_array_size(patsJ);
            for (int p = 0; p < 8 && static_cast<size_t>(p) < n; ++p) {
                json_t* strJ = json_array_get(patsJ, p);
                if (!strJ || !json_is_string(strJ)) continue;
                const std::string s = json_string_value(strJ);
                for (int i = 0; i < len && i < static_cast<int>(s.size()) && i < 32; ++i)
                    userGate[p][i] = (s[i] == '1') ? 1 : 0;
            }
        }
        json_t* accJ = json_object_get(rootJ, "accentPatterns");
        if (accJ && json_is_array(accJ)) {
            const size_t n = json_array_size(accJ);
            for (int p = 0; p < 8 && static_cast<size_t>(p) < n; ++p) {
                json_t* strJ = json_array_get(accJ, p);
                if (!strJ || !json_is_string(strJ)) continue;
                const std::string s = json_string_value(strJ);
                for (int i = 0; i < len && i < static_cast<int>(s.size()) && i < 32; ++i)
                    accentGate[p][i] = (s[i] == '1') ? 1 : 0;
            }
        }
        userGateLength = len;
        userGateIndex = idx;
        paramQuantities[TRANCE_GATE_PATTERN_PARAM]->setValue(static_cast<float>(idx));
        syncParamsFromUserGate();
    }

    void updateRates() {
        const float sr = sampleRate;
        modSmooth[MOD_SPEED].setTime(0.015f, sr);
        modSmooth[MOD_GRAIN].setTime(0.030f, sr);
        modSmooth[MOD_PITCH].setTime(0.020f, sr);
        modSmooth[MOD_SCATTER].setTime(0.040f, sr);
        modSmooth[MOD_RATE].setTime(MOD_MIN_SMOOTH, sr);
        modSmooth[MOD_DRIVE].setTime(0.020f, sr);
        modSmooth[MOD_MIX].setTime(0.015f, sr);
        modSmooth[MOD_WOW].setTime(0.030f, sr);
        modSmooth[MOD_FLUTTER].setTime(0.015f, sr);
        modSmooth[MOD_GATE].setTime(0.020f, sr);
        modSmooth[MOD_DELAY].setTime(0.020f, sr);
        toneSmooth.setTime(0.020f, sr);
        widthSmooth.setTime(0.020f, sr);
        gateSmooth.setTime(0.020f, sr);
        rateSlewCoeff = std::exp(-1.f / (0.020f * sr));
        rateFlipCoeff = std::exp(-1.f / (0.070f * sr));
        dispFadeStep = clampF(1.f / (DISP_XFADE_SEC * sr), 0.f, 1.f);
        sliceSwapStep = clampF(1.f / (SLICE_SWAP_SEC * sr), 0.f, 1.f);
        scatterSlewCoeff = std::exp(-1.f / (SCATTER_SLEW_SEC * sr));
        timelineSlewCoeff = std::exp(-1.f / (0.060f * sr));
        sliceLayerCoeff = std::exp(-1.f / (0.003f * sr));
        dcR = clampF(1.f - TWO_PI_F * 8.f / sr, 0.9f, 0.9998f);
        scopeSamplesPerBucket = std::max(1, static_cast<int>(sr * 0.002f));
        clockPeriod = clampF(clockPeriod, 0.02f * sr, 4.f * sr);
        tranceGateShapeSlewCoeff = std::exp(-1.f / (0.030f * sr));
        envFollow.setTime(0.002f, 0.35f, sr);
        macroSmooth.setTime(0.060f, sr);
        fxSrr.setTime(0.030f, sr);
        fxBits.setTime(0.030f, sr);
        fxFold.setTime(0.030f, sr);
        fxRing.setTime(0.030f, sr);
        fxRingFreq.setTime(0.030f, sr);
        fxCut.setTime(0.030f, sr);
        fxRes.setTime(0.030f, sr);
        fxMode.setTime(0.030f, sr);
        fxComb.setTime(0.030f, sr);
        fxDuck.setTime(0.030f, sr);
        swingSmooth.setTime(0.040f, sr);
        accentSmooth.setTime(0.040f, sr);
    }

    inline float getBeatSamples() const {
        if (clockLocked)
            return clockPeriod * CLOCK_PULSES_PER_BEAT;
        return 0.5f * sampleRate;
    }

    uint32_t randomU32() {
        uint32_t x = rngState;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        if (x == 0u) x = 0x9E3779B9u;
        rngState = x;
        return x;
    }
    float randomBipolar() {
        return static_cast<float>(randomU32() >> 8) * (1.f / 8388607.5f) - 1.f;
    }
    float random01() {
        return static_cast<float>(randomU32() >> 8) * (1.f / 16777215.f);
    }

    void processChaos(float dt) {
        const float rateKnob = clamp01(safeF(params[CHAOS_RATE_PARAM].getValue()));
        bool redraw = chaosButton.process(params[CHAOS_TRIGGER_PARAM].getValue(), 0.1f, 0.5f);

        const float hz = 0.25f * std::pow(128.f, rateKnob)
                        * macroLerp(MACRO_CHAOS_RATE_BIAS, macroIndex);
        chaosPhase += dt * hz;
        if (chaosPhase >= 1.f) {
            chaosPhase -= std::floor(chaosPhase);
            redraw = true;
        }
        if (!(chaosPhase >= 0.f)) chaosPhase = 0.f;

        if (redraw) {
            for (int i = 0; i < CHAOS_STREAMS; ++i)
                chaosValue[i] = randomBipolar();
        }

        float sum = 0.f;
        for (int i = 0; i < CHAOS_STREAMS; ++i)
            sum += chaosValue[i] * chaosValue[i];
        const float rms = std::sqrt(sum / static_cast<float>(CHAOS_STREAMS));
        chaosLevel += 0.02f * (rms - chaosLevel);
        chaosLevel = clamp01(safeF(chaosLevel));
    }

    void processExtMod() {
        if (!inputs[EXT_MOD_INPUT].isConnected()) {
            extModValue = 0.f;
            extModActive = 0.f;
            return;
        }
        const float v = safeF(inputs[EXT_MOD_INPUT].getVoltage());
        extModValue = clampF(v * EXT_MOD_SCALE, -1.f, 1.f);
        const float target = (std::fabs(v) > 0.05f) ? 1.f : 0.f;
        extModActive += 0.005f * (target - extModActive);
        extModActive = clamp01(safeF(extModActive));
    }

    void updateMacro() {
        float knob = clampF(safeF(params[PHASE1_MACRO_PARAM].getValue()),
                            0.f, static_cast<float>(MACRO_COUNT - 1));
        float cv = 0.f;
        if (inputs[PHASE1_MACRO_CV_INPUT].isConnected())
            cv = clampF(safeF(inputs[PHASE1_MACRO_CV_INPUT].getVoltage()) * 0.3f,
                        -0.5f, 0.5f);
        const float target = clampF(knob + cv, 0.f, static_cast<float>(MACRO_COUNT - 1));
        macroIndex = macroSmooth.process(target);
        dispMacro = clampI(static_cast<int>(std::lround(macroIndex)), 0, MACRO_COUNT - 1);
    }

    void processModSources(float dt) {
        const float rateKnob = clamp01(safeF(params[CHAOS_RATE_PARAM].getValue()));
        lfoRateHz = 0.05f * std::pow(400.f, rateKnob * 0.4f)
                  * macroLerp(MACRO_CHAOS_RATE_BIAS, macroIndex);
        lfoRateHz = clampF(lfoRateHz, 0.01f, 40.f);
        lfoValue = lfo.tick(dt, lfoRateHz);

        float l = 0.f, r = 0.f;
        if (inputs[AUDIO_INPUT].isConnected()) {
            l = safeF(inputs[AUDIO_INPUT].getVoltage());
            r = (inputs[AUDIO_INPUT].getChannels() > 1)
                ? safeF(inputs[AUDIO_INPUT].getVoltage(1)) : l;
        }
        envValue = envFollow.tick(clampF(0.08f * (std::fabs(l) + std::fabs(r)), 0.f, 1.f));

        modSource[SRC_CHAOS0] = chaosValue[0];
        modSource[SRC_CHAOS1] = chaosValue[1];
        modSource[SRC_CHAOS2] = chaosValue[2];
        modSource[SRC_LFO]    = lfoValue;
        modSource[SRC_ENV]    = envValue;
    }

    void advanceTranceGateStep() {
        tranceGateStep = (tranceGateStep + 1) % GATE_STEPS;
        tranceGateStepFlash = 1.f;
    }

    void processGateCv() {
        gateCvConnected = inputs[GATE_CV_INPUT].isConnected();
        gateCvClockMode = false;
        if (!gateCvConnected) {
            gateCvTrigger.reset();
            return;
        }
        const float v = safeF(inputs[GATE_CV_INPUT].getVoltage());
        if (gateCvTrigger.process(v, 0.2f, 1.f)) {
            advanceTranceGateStep();
            tranceGatePhaseCounter = 0.f;
            gateCvClockMode = true;
        }
    }

    float evalParam(int target, int inputId,
                    float raw, float rawMin, float rawMax,
                    float cvScale, float modDepth, float smoothSeconds) {
        const float span = rawMax - rawMin;
        float baseN = (span > 0.0001f) ? ((raw - rawMin) / span) : 0.f;
        baseN = clamp01(baseN);

        float cvN = 0.f;
        if (inputId >= 0 && inputs[inputId].isConnected())
            cvN = clampF(safeF(inputs[inputId].getVoltage()) * cvScale, -1.f, 1.f);

        float modN = 0.f;
        if (modDepth > 0.f && target >= 0 && target < MOD_TARGET_COUNT) {
            const float amount = clamp01(safeF(params[CHAOS_AMOUNT_PARAM].getValue()));
            if (amount > 0.0001f) {
                const float bias = macroLerp(&MACRO_BIAS[target][0], macroIndex);
                float m = 0.f;
                for (int s = 0; s < N_MOD_SOURCES; ++s)
                    m += modSource[s] * MOD_MATRIX[target][s];
                float wsum = 0.f;
                for (int s = 0; s < N_MOD_SOURCES; ++s)
                    wsum += std::fabs(MOD_MATRIX[target][s]);
                if (wsum > 0.001f) m /= wsum;
                modN = m * modDepth * bias * amount;
            }
        }

        float extN = 0.f;
        if (modDepth > 0.f && std::fabs(extModValue) > 0.00001f) {
            const float amount = clamp01(safeF(params[CHAOS_AMOUNT_PARAM].getValue()));
            extN = extModValue * modDepth * amount;
        }

        const float v = modSmooth[target].process(clamp01(baseN + cvN + modN + extN));

        ModSlot& slot = modView[target];
        slot.base = baseN;
        slot.cv = cvN;
        slot.mod = modN;
        slot.ext = extN;
        slot.value = v;
        return v;
    }

    void updateParameters(DspParams& d, float dt) {
        updateMacro();
        processChaos(dt);
        processExtMod();
        processModSources(dt);

        const float speedN = evalParam(MOD_SPEED, SPEED_CV_INPUT,
            params[SPEED_PARAM].getValue(), -1.f, 1.f, 0.1f, CHAOS_SPEED_DEPTH, 0.015f);
        const float grainN = evalParam(MOD_GRAIN, STRETCH_CV_INPUT,
            params[STRETCH_PARAM].getValue(), 0.f, 1.f, 0.1f, CHAOS_GRAIN_DEPTH, 0.030f);
        const float pitchN = evalParam(MOD_PITCH, PITCH_CV_INPUT,
            params[PITCH_PARAM].getValue(), -12.f, 12.f, 0.5f, CHAOS_PITCH_DEPTH, 0.020f);
        const float scatterN = evalParam(MOD_SCATTER, SCATTER_CV_INPUT,
            params[SCATTER_PARAM].getValue(), 0.f, 1.f, 0.1f, CHAOS_SCATTER_DEPTH, 0.040f);
        const float driveN = evalParam(MOD_DRIVE, DRIVE_CV_INPUT,
            params[DRIVE_PARAM].getValue(), 0.f, 1.f, 0.1f, CHAOS_DRIVE_DEPTH, 0.020f);
        const float mixN = evalParam(MOD_MIX, MIX_CV_INPUT,
            params[MIX_PARAM].getValue(), 0.f, 1.f, 0.1f, 0.50f, 0.015f);
        const float wowN = evalParam(MOD_WOW, WOW_CV_INPUT,
            params[WOW_PARAM].getValue(), 0.f, 1.f, 0.1f, 0.60f, 0.030f);
        const float flutterN = evalParam(MOD_FLUTTER, FLUTTER_CV_INPUT,
            params[FLUTTER_PARAM].getValue(), 0.f, 1.f, 0.1f, 0.60f, 0.015f);
        const float gateDepthN = evalParam(MOD_GATE, -1,
            params[TRANCE_GATE_DEPTH_PARAM].getValue(), 0.f, 1.f, 0.f, CHAOS_GATE_DEPTH, 0.020f);
        const float delayMixN = evalParam(MOD_DELAY, DELAY_CV_INPUT,
            params[DELAY_MIX_PARAM].getValue(), 0.f, 1.f, 0.1f, CHAOS_DELAY_DEPTH, 0.020f);

        d.fSrr = fxSrr.process(clamp01(safeF(params[FX_SRR_PARAM].getValue())));
        d.fBits = fxBits.process(clamp01(safeF(params[FX_BITS_PARAM].getValue())));
        d.fFold = fxFold.process(clamp01(safeF(params[FX_FOLD_PARAM].getValue())));
        d.fRing = fxRing.process(clamp01(safeF(params[FX_RING_PARAM].getValue())));
        d.fRingFreq = fxRingFreq.process(clamp01(safeF(params[FX_RFRQ_PARAM].getValue())));
        d.fCut = fxCut.process(clamp01(safeF(params[FX_CUT_PARAM].getValue())));
        d.fRes = fxRes.process(clamp01(safeF(params[FX_RES_PARAM].getValue())));
        d.fMode = fxMode.process(clamp01(safeF(params[FX_MODE_PARAM].getValue())));
        d.fComb = fxComb.process(clamp01(safeF(params[FX_COMB_PARAM].getValue())));
        d.fDuck = fxDuck.process(clamp01(safeF(params[FX_DUCK_PARAM].getValue())));

        d.speed = 2.f * speedN - 1.f;
        d.grainNorm = grainN;
        d.pitchSt = -12.f + 24.f * pitchN;
        d.pitchRatio = std::pow(2.f, d.pitchSt * (1.f / 12.f));
        d.scatter = scatterN;
        d.spread = clamp01(safeF(params[SPREAD_PARAM].getValue()));
        d.drive = driveN;
        d.mix = mixN;
        d.wow = wowN;
        d.flutter = flutterN;
        d.tone = toneSmooth.process(clamp01(safeF(params[TONE_PARAM].getValue())));
        d.width = widthSmooth.process(clamp01(safeF(params[WIDTH_PARAM].getValue())));
        d.output = clampF(safeF(params[OUTPUT_PARAM].getValue(), 1.f), 0.f, 2.f);
        d.gate = gateSmooth.process(clamp01(safeF(params[GATE_PARAM].getValue())));
        d.probability = clamp01(safeF(params[PROBABILITY_PARAM].getValue()));
        d.repeatCount = repeatsFromKnob(params[REPEATS_PARAM].getValue());
        d.rateIndex = clampI(
            static_cast<int>(std::lround(clampF(
                safeF(params[STUTTER_RATE_PARAM].getValue()), 0.f, 15.f))),
            0, DIVISION_COUNT - 1);

        d.stutterOn = params[STUTTER_SWITCH_PARAM].getValue() > 0.5f;
        if (inputs[STUTTER_CV_INPUT].isConnected()
            && safeF(inputs[STUTTER_CV_INPUT].getVoltage()) > 1.f)
            d.stutterOn = true;
        d.stopOn = params[STOP_SWITCH_PARAM].getValue() > 0.5f;
        d.freezeOn = params[FREEZE_SWITCH_PARAM].getValue() > 0.5f;
        d.reverseOn = params[REVERSE_SWITCH_PARAM].getValue() > 0.5f;

        d.tranceGateOn = params[TRANCE_GATE_SWITCH_PARAM].getValue() > 0.5f;
        d.tranceGateReset = params[TRANCE_GATE_RESET_PARAM].getValue() > 0.5f;
        d.tranceGateDepth = gateDepthN;

        {
            int pat = static_cast<int>(std::lround(clampF(
                safeF(params[TRANCE_GATE_PATTERN_PARAM].getValue()), 0.f, 7.f)));
            pat = clampI(pat, 0, 7);
            userGateIndex = pat;

            uint16_t bits = 0, abits = 0;
            for (int i = 0; i < 16; ++i) {
                if (userGate[pat][i])   bits  |= static_cast<uint16_t>(1u << i);
                if (accentGate[pat][i]) abits |= static_cast<uint16_t>(1u << i);
            }
            d.tranceGatePattern = bits;
            d.tranceGateAccent = abits;

            d.seqRatchet = 1 + clampI(static_cast<int>(std::lround(clampF(
                safeF(params[SEQ_RATCHET_PARAM].getValue()), 0.f, 7.f))), 0, 7);
            d.seqSwing = clamp01(swingSmooth.process(clamp01(
                safeF(params[SEQ_SWING_PARAM].getValue()))));
            d.seqAccent = clamp01(accentSmooth.process(clamp01(
                safeF(params[SEQ_ACCENT_PARAM].getValue()))));
        }
        {
            int gr = static_cast<int>(std::lround(clampF(
                safeF(params[TRANCE_GATE_RATE_PARAM].getValue()), 0.f, 15.f)));
            d.tranceGateRateIndex = clampI(gr, 0, DIVISION_COUNT - 1);
        }
        d.tranceGateShape = clamp01(safeF(params[TRANCE_GATE_SHAPE_PARAM].getValue())
            + macroLerp(MACRO_GATE_SHAPE_ADD, macroIndex));

        d.delayOn = params[DELAY_SWITCH_PARAM].getValue() > 0.5f;
        {
            int di = static_cast<int>(std::lround(clampF(
                safeF(params[DELAY_TIME_PARAM].getValue()), 0.f, 15.f)));
            d.delayDivIndex = clampI(di, 0, DIVISION_COUNT - 1);
        }
        d.delayFeedback = clampF(safeF(params[DELAY_FEEDBACK_PARAM].getValue()), 0.f, 1.f) * DELAY_FB_MAX;
        d.delayMix = delayMixN;
        d.delayTone = clamp01(safeF(params[DELAY_TONE_PARAM].getValue()));

        modView[MOD_SPEED].dsp = speedToRate(d.speed);
        modView[MOD_GRAIN].dsp = grainN;
        modView[MOD_PITCH].dsp = d.pitchSt;
        modView[MOD_SCATTER].dsp = scatterN;
        modView[MOD_RATE].dsp = static_cast<float>(d.rateIndex);
        modView[MOD_WOW].dsp = wowN;
        modView[MOD_FLUTTER].dsp = flutterN;
        modView[MOD_DRIVE].dsp = driveN;
        modView[MOD_MIX].dsp = mixN;
        modView[MOD_GATE].dsp = gateDepthN;
        modView[MOD_DELAY].dsp = delayMixN;
    }

    void applyDisplacement(float amount) {
        amount = clamp01(amount);
        if (amount <= 0.0005f) return;
        if (dispEventHold > 0.f) return;
        dispEventHold = DISP_EVENT_SEC * sampleRate;
        const float span = amount * 0.5f * MAX_BACK;
        const float target = clampF(readLag + randomBipolar() * span, MIN_LAG, MAX_LAG);
        dispOldLag = readLag;
        dispFade = 1.f;
        readLag = target;
        grainAnchor = wrapTape(static_cast<float>(writePos) - target);
    }

    void processFreeze(bool freezeOn) {
        if (freezeOn && !freezeLatch) {
            freezeLatch = true;
            const float want = FREEZE_SECONDS * sampleRate;
            const float need = readLag + 0.05f * sampleRate;
            const float len = (want > need) ? want : need;
            winLen = clampF(len, MIN_LAG * 2.f, MAX_LAG - MIN_LAG);
            winStart = wrapTape(static_cast<float>(writePos) - MIN_LAG - winLen);
        } else if (!freezeOn && freezeLatch)
            freezeLatch = false;
    }

    void processClock(const DspParams& d) {
        if (inputs[CLOCK_INPUT].isConnected()) {
            const float v = safeF(inputs[CLOCK_INPUT].getVoltage());
            if (clockTrigger.process(v, 0.2f, 1.f)) {
                const float measured = static_cast<float>(samplesSinceClock);
                if (measured > 0.02f * sampleRate && measured < 4.f * sampleRate) {
                    if (clockLocked)
                        clockPeriod += (measured - clockPeriod) * 0.25f;
                    else {
                        clockPeriod = measured;
                        clockLocked = true;
                    }
                }
                samplesSinceClock = 0;
                clockThruEvent = true;
                realignSlice(d);
                if (!gateCvConnected && d.tranceGateReset) {
                    tranceGateStep = 0;
                    tranceGatePhaseCounter = 0.f;
                }
            }
        } else {
            clockLocked = false;
            clockTrigger.reset();
            clockThruPhase += 1.f;
            const float internalBeat = 0.5f * sampleRate;
            if (clockThruPhase >= internalBeat) {
                clockThruPhase -= internalBeat;
                if (!(clockThruPhase >= 0.f)) clockThruPhase = 0.f;
                clockThruEvent = true;
            }
        }
        ++samplesSinceClock;
        if (samplesSinceClock > 1000000) samplesSinceClock = 1000000;
        const float beat = getBeatSamples();
        gridPeriod = clampF(beat * divisionFraction(d.rateIndex), 64.f, 0.5f * MAX_LAG);
        gridTick = false;
        gridPhase += 1.f;
        if (gridPhase >= gridPeriod) {
            gridPhase -= gridPeriod;
            if (!(gridPhase >= 0.f)) gridPhase = 0.f;
            gridTick = true;
        }
    }

    void processTape(DspParams& d, float dt, float& outL, float& outR) {
        wowPhase += dt * TWO_PI_F * (0.18f + 1.4f * d.wow);
        if (wowPhase >= TWO_PI_F) wowPhase -= TWO_PI_F;
        wowWanderPhase += dt * TWO_PI_F * 0.07f;
        if (wowWanderPhase >= TWO_PI_F) wowWanderPhase -= TWO_PI_F;
        flutterPhase += dt * TWO_PI_F * (5.5f + 8.0f * d.flutter);
        if (flutterPhase >= TWO_PI_F) flutterPhase -= TWO_PI_F;

        const float wowMod = 1.f + 0.12f * d.wow
            * (0.75f * std::sin(wowPhase) + 0.25f * std::sin(wowWanderPhase));
        const float flutterMod = 1.f + 0.03f * d.flutter
            * (std::sin(flutterPhase) + 0.4f * std::sin(flutterPhase * 1.37f + 1.1f));
        tapeMod = wowMod * flutterMod;
        if (!std::isfinite(tapeMod)) tapeMod = 1.f;
        tapeMod = clampF(tapeMod, TAPE_MOD_LO, TAPE_MOD_HI);

        float transport = speedToRate(d.speed) * (1.f - stopAmount);
        if (d.reverseOn) transport = -transport;
        if (stopAmount > 0.9995f) transport = 0.f;
        transport = clampF(safeF(transport), -MAX_RATE, MAX_RATE);

        d.tapeRate = clampF(transport * tapeMod, -MAX_RATE_FULL, MAX_RATE_FULL);
        if (!std::isfinite(d.tapeRate)) d.tapeRate = 1.f;

        const float slackNear = smoothStep01(clamp01((readLag - MIN_LAG) / SLACK_ZONE));
        const float slackFar = smoothStep01(clamp01((MAX_LAG - readLag) / SLACK_ZONE));
        const float slack = (transport >= 0.f) ? slackNear : slackFar;
        d.tapeApplied = 1.f + (transport - 1.f) * slack;
        const float deficit = (transport > d.tapeApplied) ? (transport - d.tapeApplied)
                                                          : (d.tapeApplied - transport);
        const float goal = clamp01(deficit * 1.5f);
        timelineSlew = goal + timelineSlewCoeff * (timelineSlew - goal);
        if (!std::isfinite(timelineSlew)) timelineSlew = goal;
        d.timelineW = clamp01(timelineSlew);

        rateSlew = slewRate(rateSlew, d.tapeApplied, rateSlewCoeff, rateFlipCoeff, 0.05f);
        if (!std::isfinite(rateSlew)) rateSlew = d.tapeApplied;
        const float applied = clampF(rateSlew, -MAX_RATE, MAX_RATE);
        const float appliedFull = clampF(applied * tapeMod, -MAX_RATE_FULL, MAX_RATE_FULL);

        float tL = 0.f, tR = 0.f;

        if (freezeLatch) {
            readPos = wrapTape(readPos + appliedFull);
            float rel = std::fmod(readPos - winStart, winLen);
            if (rel < 0.f) rel += winLen;
            if (!(rel >= 0.f && rel < winLen)) rel = 0.f;
            readPos = winStart + rel;
            if (appliedFull >= 0.f) {
                const float dd = (winStart + winLen) - readPos;
                tL = readSeam(bufL, readPos, readPos - winLen, dd);
                tR = readSeam(bufR, readPos, readPos - winLen, dd);
            } else {
                const float dd = readPos - winStart;
                tL = readSeam(bufL, readPos, readPos + winLen, dd);
                tR = readSeam(bufR, readPos, readPos + winLen, dd);
            }
            readLag = clampF(wrapTape(static_cast<float>(writePos) - readPos),
                             MIN_LAG, MAX_LAG);
        } else {
            readLag += 1.f - appliedFull;
            if (d.timelineW > 0.999f) readLag = NOMINAL_LAG;
            readLag = clampF(readLag, MIN_LAG, MAX_LAG);
            readPos = wrapTape(static_cast<float>(writePos) - readLag);
            readLinearStereo(bufL, bufR, readPos, tL, tR);
        }

        if (dispEventHold > 0.f) dispEventHold -= 1.f;
        if (dispFade > 0.f) {
            dispOldLag = clampF(dispOldLag + 1.f - appliedFull, MIN_LAG, MAX_LAG);
            const float oldPos = wrapTape(static_cast<float>(writePos) - dispOldLag);
            const float wOld = clamp01(dispFade);
            const float wNew = 1.f - wOld;
            float oL = 0.f, oR = 0.f;
            readLinearStereo(bufL, bufR, oldPos, oL, oR);
            tL = oL * wOld + tL * wNew;
            tR = oR * wOld + tR * wNew;
            dispFade -= dispFadeStep;
            if (dispFade < 0.f) dispFade = 0.f;
        }

        const float amp = std::sqrt(clamp01(1.f - 0.98f * stopAmount));
        outL = tL * amp;
        outR = tR * amp;
    }

    void spawnGrain(const DspParams& d, float grainLen, float overlap, float rate) {
        int slot = 0;
        float oldest = -1.f;
        for (int i = 0; i < MAX_GRAINS; ++i) {
            if (!grains[i].active) { slot = i; break; }
            if (grains[i].age > oldest) { oldest = grains[i].age; slot = i; }
        }
        Grain& gr = grains[slot];
        float grainRate = clampF(rate, -4.f, 4.f);
        if (d.reverseOn && random01() < 0.30f + 0.45f * d.scatter)
            grainRate = -grainRate;
        const float pan = clamp01(0.5f + randomBipolar() * 0.5f * d.spread);
        const float panAngle = pan * PI_F * 0.5f;
        gr.panL = std::cos(panAngle);
        gr.panR = std::sin(panAngle);
        const float travel = std::fabs(grainRate) * grainLen;
        const float jitter = d.spread * grainLen * 0.5f * random01();
        const float scatterBack = d.scatter * 0.5f * MAX_BACK * random01();
        float back = travel + jitter + scatterBack + static_cast<float>(SEAM_XFADE);
        float backLimit = grainBackMax - grainLag;
        if (backLimit < travel) backLimit = travel;
        if (backLimit > MAX_BACK) backLimit = MAX_BACK;
        back = clampF(back, travel, backLimit);
        gr.pos = wrapTape(grainAnchor - back);
        gr.rate = grainRate;
        gr.age = 0.f;
        gr.len = grainLen;
        gr.gain = 1.2f / std::sqrt(overlap);
        gr.active = true;
    }

    void processGranular(DspParams& d, float& outL, float& outR) {
        const float g = clamp01(d.grainNorm);
        float grainHistoryMax = GRAN_LAG_MAX - READ_MARGIN;
        {
            const float written = static_cast<float>(writtenSamples);
            if (written < grainHistoryMax) grainHistoryMax = written - READ_MARGIN;
            if (grainHistoryMax < MIN_LAG) grainHistoryMax = MIN_LAG;
        }
        const float grainSpan = grainHistoryMax - MIN_LAG;
        grainBackMax = grainHistoryMax;

        if (d.timelineW < 0.02f) {
            grainLag = readLag;
        } else {
            const float anchorRate = clampF(1.f + (d.tapeRate - 1.f) * d.timelineW, -6.f, 6.f);
            grainLag += 1.f - anchorRate;
            if (!std::isfinite(grainLag)) grainLag = readLag;
            if (grainSpan > 1.f) {
                float rel = grainLag - MIN_LAG;
                rel = std::fmod(rel, grainSpan);
                if (rel < 0.f) rel += grainSpan;
                grainLag = MIN_LAG + rel;
            } else grainLag = MIN_LAG;
        }
        grainLag = clampF(grainLag, MIN_LAG, grainHistoryMax);
        grainAnchor = wrapTape(static_cast<float>(writePos) - grainLag);
        if (!std::isfinite(grainAnchor)) grainAnchor = readPos;

        const float grainLen = sampleRate * 0.02f * std::pow(12.5f, g);
        const float overlap = 1.f + 3.f * g;
        grainInterval = grainLen / overlap;
        if (grainInterval < 64.f) grainInterval = 64.f;

        const float anchorRate = 1.f + (d.tapeRate - 1.f) * d.timelineW;
        const float readRate = clampF(d.pitchRatio * anchorRate * tapeMod, -4.f, 4.f);
        const float pitchActive = (d.pitchSt > 0.01f || d.pitchSt < -0.01f) ? 1.f : 0.f;

        if (g > 0.015f || d.timelineW > 0.02f || pitchActive > 0.f) {
            grainClock += 1.f;
            if (grainClock >= grainInterval) {
                grainClock -= grainInterval;
                if (!(grainClock >= 0.f)) grainClock = 0.f;
                spawnGrain(d, grainLen, overlap, readRate);
            }
        }
        float envSum = 0.f;
        for (int i = 0; i < MAX_GRAINS; ++i) {
            Grain& gr = grains[i];
            if (!gr.active) continue;
            const float phase = gr.age / gr.len;
            if (!(phase < 1.f)) { gr.reset(); continue; }
            const float env = std::sin(PI_F * phase);
            envSum += env;
            outL += readLinear(bufL, gr.pos) * env * gr.gain * gr.panL;
            outR += readLinear(bufR, gr.pos) * env * gr.gain * gr.panR;
            gr.pos = wrapTape(gr.pos + gr.rate);
            gr.age += 1.f;
        }
        if (envSum > 1.6f) {
            const float norm = 1.6f / envSum;
            outL *= norm;
            outR *= norm;
        }
    }

    void beginSliceCrossfade() {
        if (!sliceActive) return;
        oldActive = true;
        oldStart = sliceStart;
        oldContent = sliceContent;
        oldProgress = sliceProgress;
        oldRate = sliceRate;
        oldLen = sliceLen;
        sliceFade = 1.f;
    }

    void latchOutgoingVoice(const DspParams& d) {
        oldEnv = sliceEnv(d, sliceProgress, sliceElapsed, true);
    }

    float readVoice(const float* buf, float start, float content, float progress, float rate) {
        const float travel = progress * content;
        const float pos = (rate >= 0.f) ? (start + travel) : (start + content - travel);
        const float cross = (rate >= 0.f) ? (pos - content) : (pos + content);
        return readSeam(buf, pos, cross, content - travel);
    }

    float sliceEnv(const DspParams& d, float progress, float elapsed, bool withAttack) {
        const float attack = clampF(SLICE_FADE_SEC * sampleRate, 1.f, sliceLen * 0.25f);
        float env = 1.f;
        if (withAttack && elapsed < attack)
            env = 0.5f - 0.5f * std::cos(PI_F * clamp01(elapsed / attack));
        const float gate = clampF(d.gate, 0.02f, 1.f);
        if (progress > gate) {
            const float k = clamp01((progress - gate) / (1.0001f - gate));
            env *= 0.5f + 0.5f * std::cos(PI_F * k);
        }
        return env;
    }

    void captureSlice(const DspParams& d, float periodSamples) {
        const float sc = clamp01(d.scatter);
        float semis = (sc > 0.001f) ? std::round(randomBipolar() * 12.f * sc) : 0.f;
        semis = clampF(semis, -12.f, 12.f);
        float rate = std::pow(2.f, semis * (1.f / 12.f));
        if (d.reverseOn && random01() < 0.35f + 0.45f * sc) rate = -rate;
        const float len = clampF(periodSamples, 64.f, 0.5f * MAX_BACK);
        float content = len * ((rate < 0.f) ? -rate : rate);
        if (content < len) content = len;
        content = clampF(content, 64.f, MAX_BACK);
        latchOutgoingVoice(d);
        beginSliceCrossfade();
        sliceStart = wrapTape(readPos - content - static_cast<float>(SEAM_XFADE));
        sliceContent = content;
        sliceLen = len;
        sliceRate = rate;
        sliceProgress = 0.f;
        sliceElapsed = 0.f;
        sliceActive = true;
        sliceRepeats = d.repeatCount;
        pendingReload = false;
    }

    void realignSlice(const DspParams& d) {
        if (!sliceActive) return;
        latchOutgoingVoice(d);
        beginSliceCrossfade();
        sliceProgress = 0.f;
        sliceElapsed = 0.f;
    }

    void processStutter(const DspParams& d, float& wetL, float& wetR) {
        const float target = d.stutterOn ? 1.f : 0.f;
        sliceLayerGain = target + sliceLayerCoeff * (sliceLayerGain - target);
        if (!std::isfinite(sliceLayerGain)) sliceLayerGain = target;
        sliceLayerGain = clamp01(sliceLayerGain);
        const float period = gridPeriod;
        scatterSlew = d.scatter + scatterSlewCoeff * (scatterSlew - d.scatter);
        if (!std::isfinite(scatterSlew)) scatterSlew = d.scatter;
        scatterSlew = clamp01(scatterSlew);
        if (gridTick) applyDisplacement(scatterSlew);
        const bool swapIdle = (sliceFade < 0.05f);
        const bool btnEdge = captureButton.process(params[CAPTURE_PARAM].getValue(), 0.1f, 0.5f);
        const bool trgEdge = inputs[TRIGGER_INPUT].isConnected()
            && triggerInput.process(safeF(inputs[TRIGGER_INPUT].getVoltage()), 0.2f, 1.f);
        const bool enableEdge = (d.stutterOn && !stutterWasOn);
        stutterWasOn = d.stutterOn;
        if (swapIdle && (btnEdge || trgEdge || enableEdge)) captureSlice(d, period);
        if (sliceLayerGain <= 0.0005f && !d.stutterOn) {
            sliceActive = false;
            oldActive = false;
            sliceFade = 0.f;
        }
        if (sliceActive && sliceRepeats < 0 && d.repeatCount > 0)
            sliceRepeats = d.repeatCount;
        float sl = 0.f, sr = 0.f;
        if (sliceActive) {
            sliceElapsed += 1.f;
            sliceProgress += 1.f / sliceLen;
            if (sliceProgress >= 1.f) {
                sliceProgress -= 1.f;
                sliceElapsed = 0.f;
                if (sliceRepeats > 0) {
                    --sliceRepeats;
                    if (sliceRepeats == 0) {
                        pendingReload = true;
                        sliceEocEvent = true;
                    }
                }
            }
            if (!(sliceProgress >= 0.f)) sliceProgress = 0.f;
            if (pendingReload) {
                pendingReload = false;
                if (swapIdle && random01() <= d.probability)
                    captureSlice(d, period);
                else sliceRepeats = d.repeatCount;
            }
            const float env = sliceEnv(d, sliceProgress, sliceElapsed, !oldActive);
            sl = readVoice(bufL, sliceStart, sliceContent, sliceProgress, sliceRate) * env;
            sr = readVoice(bufR, sliceStart, sliceContent, sliceProgress, sliceRate) * env;
        }
        if (oldActive) {
            sliceFade -= sliceSwapStep;
            if (sliceFade <= 0.f) {
                sliceFade = 0.f;
                oldActive = false;
            } else {
                oldProgress += 1.f / oldLen;
                if (oldProgress >= 1.f) oldProgress -= 1.f;
                if (!(oldProgress >= 0.f)) oldProgress = 0.f;
                const float wOld = clamp01(sliceFade);
                const float wNew = 1.f - wOld;
                sl = sl * wNew + readVoice(bufL, oldStart, oldContent, oldProgress, oldRate) * oldEnv * wOld;
                sr = sr * wNew + readVoice(bufR, oldStart, oldContent, oldProgress, oldRate) * oldEnv * wOld;
            }
        }
        const float tapeGain = std::sqrt(1.f - sliceLayerGain);
        const float sliceGain = std::sqrt(sliceLayerGain);
        wetL = wetL * tapeGain + sl * sliceGain;
        wetR = wetR * tapeGain + sr * sliceGain;
    }

    void processTranceGate(DspParams& d, float& wetL, float& wetR) {
        if (!d.tranceGateOn) {
            const float releaseTime = 0.005f;
            const float releaseCoeff = 1.f - std::exp(-1.f / (releaseTime * sampleRate));
            tranceGateEnv += (0.f - tranceGateEnv) * releaseCoeff;
            if (!std::isfinite(tranceGateEnv)) tranceGateEnv = 0.f;
            tranceGateEnv = clamp01(tranceGateEnv);
            const float beat = getBeatSamples();
            const float stepSamples = clampF(beat * divisionFraction(d.tranceGateRateIndex),
                                             16.f, 0.5f * MAX_LAG);
            tranceGateStepSamples = stepSamples;
            if (!gateCvConnected) {
                tranceGatePhaseCounter += 1.f;
                if (tranceGatePhaseCounter >= stepSamples) {
                    tranceGatePhaseCounter -= stepSamples;
                    if (!(tranceGatePhaseCounter >= 0.f)) tranceGatePhaseCounter = 0.f;
                    advanceTranceGateStep();
                }
            } else {
                tranceGatePhaseCounter += 1.f;
                if (tranceGatePhaseCounter >= stepSamples)
                    tranceGatePhaseCounter = stepSamples - 1.f;
            }
            tranceGateStepPhase = clamp01(tranceGatePhaseCounter / stepSamples);
            tranceGateOn = false;
            tranceGateReset = d.tranceGateReset;
            tranceGateDepth = clamp01(d.tranceGateDepth);
            tranceGatePattern = d.tranceGatePattern;
            tranceGateShape = clamp01(d.tranceGateShape);
            tranceGateStepFlash -= 1.f / (0.15f * sampleRate);
            if (tranceGateStepFlash < 0.f) tranceGateStepFlash = 0.f;
            return;
        }

        const float beat = getBeatSamples();
        const float stepSamples = clampF(beat * divisionFraction(d.tranceGateRateIndex),
                                         16.f, 0.5f * MAX_LAG);
        tranceGateStepSamples = stepSamples;
        if (!gateCvConnected) {
            tranceGatePhaseCounter += 1.f;
            if (tranceGatePhaseCounter >= stepSamples) {
                tranceGatePhaseCounter -= stepSamples;
                if (!(tranceGatePhaseCounter >= 0.f)) tranceGatePhaseCounter = 0.f;
                advanceTranceGateStep();
            }
        } else {
            tranceGatePhaseCounter += 1.f;
            if (tranceGatePhaseCounter >= stepSamples)
                tranceGatePhaseCounter = stepSamples - 1.f;
        }
        tranceGateStepPhase = clamp01(tranceGatePhaseCounter / stepSamples);

        tranceGateSmoothedShape = tranceGateShape
            + tranceGateShapeSlewCoeff * (tranceGateSmoothedShape - tranceGateShape);
        if (!std::isfinite(tranceGateSmoothedShape))
            tranceGateSmoothedShape = tranceGateShape;
        const float shape = clamp01(tranceGateSmoothedShape);

        float gateLength;
        if (shape > 0.95f) {
            gateLength = GATE_SHAPE_HOLD;
        } else {
            const float t = shape / 0.95f;
            gateLength = GATE_SHAPE_SHORT + (GATE_SHAPE_LONG - GATE_SHAPE_SHORT) * t;
        }
        gateLength = clampF(gateLength, 0.05f, 1.f);

        const bool stepOn = (d.tranceGatePattern >> tranceGateStep) & 1;
        float env = 0.f;
        if (stepOn) {
            float p = tranceGateStepPhase;
            const float sw = ((tranceGateStep & 1) == 1)
                ? clampF(d.seqSwing, 0.f, 0.95f) : 0.f;
            if (sw > 0.0005f) {
                p = (p - sw) / (1.f - sw);
                if (p < 0.f) p = 0.f;
                else if (p > 1.f) p = 1.f;
            }
            const int rat = clampI(d.seqRatchet, 1, 8);
            float pR = p * static_cast<float>(rat);
            if (pR >= static_cast<float>(rat))
                pR = static_cast<float>(rat) - 1e-4f;
            const float pp = pR - static_cast<float>(static_cast<int>(pR));
            const float gl = gateLength / static_cast<float>(rat);
            const float attack = GATE_ATTACK_FRAC * gl;
            const float release = GATE_RELEASE_FRAC * gl;
            const float sustainEnd = gl - release;
            const float activeEnd = gl;
            if (pp < attack) env = pp / attack;
            else if (pp < sustainEnd) env = 1.f;
            else if (pp < activeEnd) env = 1.f - (pp - sustainEnd) / release;
            else env = 0.f;
            const bool acc = ((d.tranceGateAccent >> tranceGateStep) & 1);
            const float level = acc ? 1.f : (1.f - 0.6f * d.seqAccent);
            env *= level;
            env = clamp01(safeF(env));
        }
        const float smoothingTime = 0.0004f;
        const float smoothCoeff = 1.f - std::exp(-1.f / (smoothingTime * sampleRate));
        tranceGateEnv += (env - tranceGateEnv) * smoothCoeff;
        if (!std::isfinite(tranceGateEnv)) tranceGateEnv = env;
        tranceGateEnv = clamp01(tranceGateEnv);

        const float depth = clamp01(d.tranceGateDepth);
        const float g = 1.f - depth * (1.f - tranceGateEnv);
        wetL *= g;
        wetR *= g;

        tranceGateOn = true;
        tranceGateReset = d.tranceGateReset;
        tranceGateDepth = depth;
        tranceGatePattern = d.tranceGatePattern;
        tranceGateShape = shape;
        tranceGateStepFlash -= 1.f / (0.15f * sampleRate);
        if (tranceGateStepFlash < 0.f) tranceGateStepFlash = 0.f;
    }

    void processDelay(DspParams& d, float& wetL, float& wetR) {
        static const float delayQN[16] = {
            1.0f,       0.5f,       0.75f,      0.25f,
            0.375f,     0.125f,     0.1875f,    0.0625f,
            0.6666667f, 0.3333333f, 0.1666667f, 0.0833333f,
            0.0416667f, 0.09375f,   0.03125f,   0.046875f
        };
        const float quarterNote = getBeatSamples();
        const int idx = clampI(d.delayDivIndex, 0, DIVISION_COUNT - 1);
        const float baseQN = delayQN[idx];

        const float timeL = clampF(quarterNote * baseQN, 64.f,
                                   DELAY_MAX_SEC * sampleRate);
        const float timeR = clampF(quarterNote * baseQN * 1.02f, 64.f,
                                   DELAY_MAX_SEC * sampleRate);

        delayTimeSamples  += 0.0015f * (timeL - delayTimeSamples);
        delayTimeSamplesR += 0.0015f * (timeR - delayTimeSamplesR);
        if (!std::isfinite(delayTimeSamples))  delayTimeSamples  = timeL;
        if (!std::isfinite(delayTimeSamplesR)) delayTimeSamplesR = timeR;
        delayTimeSamples  = clampF(delayTimeSamples,  64.f, DELAY_MAX_SEC * sampleRate);
        delayTimeSamplesR = clampF(delayTimeSamplesR, 64.f, DELAY_MAX_SEC * sampleRate);

        delayFeedback = d.delayFeedback;
        delayMix      = d.delayMix;
        delayOn       = d.delayOn;

        const float targetFade = d.delayOn ? 1.f : 0.f;
        delayEnableFade += 0.002f * (targetFade - delayEnableFade);
        if (!std::isfinite(delayEnableFade)) delayEnableFade = targetFade;
        delayEnableFade = clamp01(delayEnableFade);

        const float toneNorm = clamp01(d.delayTone);
        const float toneHz = 250.f * std::pow(40.f, toneNorm);
        const float toneCoeff = std::exp(-TWO_PI_F * toneHz / sampleRate);
        delayToneCoeff = clampF(toneCoeff, 0.f, 0.9999f);

        const float hpCoeff = std::exp(-TWO_PI_F * 120.f / sampleRate);

        const float readPosL = wrapTape(static_cast<float>(delayWrite) - delayTimeSamples);
        const float readPosR = wrapTape(static_cast<float>(delayWrite) - delayTimeSamplesR);
        const float tapL = readLinear(delayL, readPosL);
        const float tapR = readLinear(delayR, readPosR);

        delayHpL += (1.f - hpCoeff) * (tapL - delayHpL);
        delayHpR += (1.f - hpCoeff) * (tapR - delayHpR);
        if (!std::isfinite(delayHpL)) delayHpL = tapL;
        if (!std::isfinite(delayHpR)) delayHpR = tapR;

        const float hpL = tapL - delayHpL;
        const float hpR = tapR - delayHpR;

        delayToneL = hpL + delayToneCoeff * (delayToneL - hpL);
        delayToneR = hpR + delayToneCoeff * (delayToneR - hpR);
        if (!std::isfinite(delayToneL)) delayToneL = hpL;
        if (!std::isfinite(delayToneR)) delayToneR = hpR;

        const float fbAmt = clampF(d.delayFeedback * delayEnableFade, 0.f, DELAY_FB_MAX);
        const float fbL = softLimit(delayToneL * fbAmt);
        const float fbR = softLimit(delayToneR * fbAmt);

        const float inL = wetL * delayEnableFade;
        const float inR = wetR * delayEnableFade;
        delayL[delayWrite] = clampF(inL + fbL, -4.f, 4.f);
        delayR[delayWrite] = clampF(inR + fbR, -4.f, 4.f);
        delayWrite = (delayWrite + 1) & TAPE_MASK;

        const float m = clamp01(d.delayMix) * delayEnableFade;
        wetL = wetL * (1.f - m) + tapL * m;
        wetR = wetR * (1.f - m) + tapR * m;

        dispDelayTime = delayTimeSamples / sampleRate;
        dispDelayMix  = m;
        dispDelayFb   = fbAmt;
    }

    void processStop(float dt, bool stopOn) {
        if (stopOn) {
            if (stopPhase == STOP_NORMAL || stopPhase == STOP_RISE)
                stopPhase = STOP_DECAY;
        } else {
            if (stopPhase == STOP_DECAY || stopPhase == STOP_HOLD)
                stopPhase = STOP_RISE;
        }
        if (stopPhase == STOP_HOLD) { stopAmount = 1.f; return; }
        const float target = (stopPhase == STOP_DECAY) ? 1.f : 0.f;
        const float tau = (stopPhase == STOP_DECAY) ? 0.20f : 0.16f;
        const float a = 1.f - std::exp(-dt / tau);
        stopAmount += (target - stopAmount) * a;
        if (!std::isfinite(stopAmount)) stopAmount = target;
        stopAmount = clamp01(stopAmount);
        if (stopPhase == STOP_DECAY && stopAmount > 0.999f) {
            stopAmount = 1.f;
            stopPhase = STOP_HOLD;
        }
        if (stopPhase == STOP_RISE && stopAmount < 0.001f) {
            stopAmount = 0.f;
            stopPhase = STOP_NORMAL;
        }
    }

    float processDrive(float x, float d) {
        if (d <= 0.0005f) return x;
        const float amount = clamp01(d * 2.f);
        const float gain = 1.f + 15.f * d;
        const float asym = 0.15f * d;
        const float shaped = std::tanh(gain * x + asym * x * x) / (1.f + 3.2f * d);
        return x * (1.f - amount) + shaped * amount;
    }

    void processTone(float& l, float& r, float tone) {
        const float t = clamp01(tone);
        const float dark = (t < 0.5f) ? (1.f - 2.f * t) : 0.f;
        const float bright = (t > 0.5f) ? (2.f * t - 1.f) : 0.f;
        float fc = 1200.f;
        if (dark > 0.0001f) fc = 8000.f * std::pow(0.03125f, dark);
        else if (bright > 0.0001f) fc = 15.f * std::pow(60.f, bright);
        fc = clampF(fc, 20.f, 0.45f * sampleRate);
        const float w = std::tan(PI_F * fc / sampleRate);
        const float a = clampF(w / (1.f + w), 0.0002f, 0.95f);
        toneL1 += a * (l - toneL1);
        toneL2 += a * (toneL1 - toneL2);
        toneR1 += a * (r - toneR1);
        toneR2 += a * (toneR1 - toneR2);
        toneL1 = safeF(toneL1); toneL2 = safeF(toneL2);
        toneR1 = safeF(toneR1); toneR2 = safeF(toneR2);
        if (dark > 0.0001f) {
            l = l * (1.f - dark) + toneL2 * dark;
            r = r * (1.f - dark) + toneR2 * dark;
        } else if (bright > 0.0001f) {
            l = l * (1.f - bright) + (l - toneL2) * bright;
            r = r * (1.f - bright) + (r - toneR2) * bright;
        }
    }

    void processWidth(float& l, float& r, float width) {
        const float w = clampF(2.f * clamp01(width), 0.f, 1.6f);
        const float mid = 0.5f * (l + r);
        const float side = 0.5f * (l - r) * w;
        l = mid + side;
        r = mid - side;
    }

    void processDC(float& l, float& r) {
        const float xl = l;
        const float xr = r;
        l = xl - dcXL + dcR * dcYL;
        r = xr - dcXR + dcR * dcYR;
        dcXL = safeF(xl); dcYL = safeF(l);
        dcXR = safeF(xr); dcYR = safeF(r);
    }

    void processCharacter(float& l, float& r, const DspParams& d) {
        l = processDrive(l, d.drive);
        r = processDrive(r, d.drive);
        processTone(l, r, d.tone);
        processWidth(l, r, d.width);
        processDC(l, r);
    }

    void processFx(const DspParams& d, float& wetL, float& wetR) {
        int fxActive = 0;
        const float srrKnob = clamp01(d.fSrr);
        const float factor = std::pow(2.f, 6.f * srrKnob);
        const bool srrOn = factor > 1.02f;
        const float bitsKnob = clamp01(d.fBits);
        const float bits = 16.f - 12.f * bitsKnob;
        const bool crushOn = bitsKnob > 0.001f;
        if (srrOn) {
            srrPhaseL += 1.f;
            if (srrPhaseL >= factor) { srrPhaseL -= factor; srrHoldL = wetL; }
            wetL = srrHoldL;
            srrPhaseR += 1.f;
            if (srrPhaseR >= factor) { srrPhaseR -= factor; srrHoldR = wetR; }
            wetR = srrHoldR;
        }
        if (crushOn) {
            const float levels = std::exp2(bits);
            wetL = std::round(wetL * levels) / levels;
            wetR = std::round(wetR * levels) / levels;
        }
        if (srrOn || crushOn) fxActive = 1;

        const float foldAmt = clamp01(d.fFold);
        if (foldAmt > 0.002f) {
            const float g = 1.f + 4.f * foldAmt;
            const auto fold1 = [](float in, float amt, float gain) {
                float y = gain * in;
                y = 4.f * std::fabs((y * 0.25f) - std::floor(y * 0.25f + 0.5f)) - 1.f;
                return in * (1.f - amt) + y * amt * 2.f;
            };
            wetL = fold1(wetL, foldAmt, g);
            wetR = fold1(wetR, foldAmt, g);
            fxActive = 1;
        }

        const float ringAmt = clamp01(d.fRing);
        const float ringFreq = 3000.f * clamp01(d.fRingFreq) * clamp01(d.fRingFreq);
        if (ringAmt > 0.002f && ringFreq > 12.f) {
            ringPhase += TWO_PI_F * ringFreq / sampleRate;
            if (ringPhase >= TWO_PI_F) ringPhase -= TWO_PI_F;
            if (!(ringPhase >= 0.f)) ringPhase = 0.f;
            const float m = std::sin(ringPhase);
            wetL = wetL * (1.f - ringAmt) + wetL * m * ringAmt;
            wetR = wetR * (1.f - ringAmt) + wetR * m * ringAmt;
            fxActive = 1;
        }

        const float cutKnob = clamp01(d.fCut);
        if (cutKnob < 0.995f) {
            const float hz = 32.f * std::pow(400.f, clamp01(cutKnob));
            const float f = clampF(2.f * std::sin(PI_F * hz / sampleRate), 0.f, 0.9999f);
            const float damp = clampF(1.f - 0.92f * clamp01(d.fRes), 0.01f, 1.f);
            svfLpL += f * svfBpL;
            const float hpL = wetL - svfLpL - damp * svfBpL;
            svfBpL += f * hpL;
            svfLpR += f * svfBpR;
            const float hpR = wetR - svfLpR - damp * svfBpR;
            svfBpR += f * hpR;
            const float mode = clamp01(d.fMode);
            float mL = svfLpL, mR = svfLpR;
            if (mode < 0.5f) {
                const float a = mode * 2.f;
                mL = svfLpL * (1.f - a) + svfBpL * a;
                mR = svfLpR * (1.f - a) + svfBpR * a;
            } else {
                const float a = (mode - 0.5f) * 2.f;
                mL = svfBpL * (1.f - a) + hpL * a;
                mR = svfBpR * (1.f - a) + hpR * a;
            }
            wetL = mL;
            wetR = mR;
            fxActive = 1;
        }

        const float combKnob = clamp01(d.fComb);
        if (combKnob > 0.02f) {
            const float combSamples = clampF(
                (0.005f * std::pow(60.f, combKnob)) * sampleRate, 4.f, 0.30f * SR_MAX);
            const float readPos = wrapTape(static_cast<float>(combWrite) - combSamples);
            const float dl = readLinear(combL, readPos);
            const float dr = readLinear(combR, readPos);
            combL[combWrite] = clampF(wetL + dl * 0.55f, -4.f, 4.f);
            combR[combWrite] = clampF(wetR + dr * 0.55f, -4.f, 4.f);
            combWrite = (combWrite + 1) & TAPE_MASK;
            wetL = wetL * 0.5f + dl * 0.5f;
            wetR = wetR * 0.5f + dr * 0.5f;
            fxActive = 1;
        }

        const float duckAmt = clamp01(d.fDuck);
        if (duckAmt > 0.002f) {
            duckLevel += 0.002f * (envValue - duckLevel);
            if (!std::isfinite(duckLevel)) duckLevel = envValue;
            const float gain = 1.f - duckAmt * clamp01(duckLevel * 1.5f);
            wetL *= gain;
            wetR *= gain;
            fxActive = 1;
        }
        dispFx = fxActive;
    }

    void updateDisplayState(const DspParams& d, float outL, float outR) {
        const float mono = 0.5f * (outL + outR);
        if (++scopeCounter >= scopeSamplesPerBucket) {
            scopeCounter = 0;
            scopeBucket = (scopeBucket + 1) & (SCOPE_BUCKETS - 1);
            scopeMin[scopeBucket] = mono;
            scopeMax[scopeBucket] = mono;
        } else {
            if (mono < scopeMin[scopeBucket]) scopeMin[scopeBucket] = mono;
            if (mono > scopeMax[scopeBucket]) scopeMax[scopeBucket] = mono;
        }
        dispWrite = static_cast<float>(writePos) / TAPE_F;
        dispRead = readPos / TAPE_F;
        dispWinStart = freezeLatch ? (winStart / TAPE_F) : 0.f;
        dispWinLen = freezeLatch ? (winLen / TAPE_F) : 0.f;
        dispSliceStart = sliceActive ? (sliceStart / TAPE_F) : 0.f;
        dispSliceLen = sliceActive ? (sliceContent / TAPE_F) : 0.f;
        dispSliceActive = sliceActive ? clamp01(sliceLayerGain * 1.5f) : 0.f;
        dispDivision = static_cast<float>(d.rateIndex);
        dispTimeline = d.timelineW;
        dispReverse = d.reverseOn ? 1.f : 0.f;
        dispStop = stopAmount;
        dispRate = d.tapeRate;
        dispChaos = chaosLevel;
        dispExtMod = extModActive;
        dispGate = tranceGateEnv;
        dispGateStep = tranceGateStep;
        dispGatePattern = tranceGatePattern;
        dispGateShape = tranceGateShape;
        dispGatePhase = tranceGateStepPhase;
        dispGateStepFlash = tranceGateStepFlash;

        dispRms += 0.0015f * (std::fabs(mono) - dispRms);
        dispRms = clamp01(safeF(dispRms));
        signalLevel = dispRms;

        lights[FREEZE_LIGHT].setBrightness(freezeLatch ? 1.f : 0.f);
        lights[STOP_LIGHT].setBrightness(stopPhase == STOP_HOLD ? 1.f : stopAmount);
        lights[STUTTER_LIGHT].setBrightness(d.stutterOn ? clampF(sliceLayerGain, 0.2f, 1.f) : 0.f);
        lights[SIGNAL_LIGHT].setBrightness(signalLevel);

        const float amount = clamp01(safeF(params[CHAOS_AMOUNT_PARAM].getValue()));
        const float busActive = (amount > 0.02f)
            ? clamp01(0.4f + 0.6f * (std::fabs(lfoValue) + envValue + chaosLevel))
            : 0.f;
        lights[EXT_MOD_LIGHT].setBrightness(std::fmax(extModActive, busActive));

        lights[GATE_LIGHT].setBrightness(tranceGateOn ? tranceGateEnv : 0.f);
        lights[DELAY_LIGHT].setBrightness(delayOn ? clamp01(delayMix * 1.5f) : 0.f);
    }

    void process(const ProcessArgs& args) override {
        enableFlushToZero();
        const float sr = clampF(safeF(args.sampleRate, sampleRate), SR_MIN, SR_MAX);
        if (std::fabs(sr - sampleRate) > 0.5f) {
            sampleRate = sr;
            updateRates();
        }
        const float dt = clampF(safeF(args.sampleTime, 1.f / sampleRate),
                                1.f / SR_MAX, 1.f / SR_MIN);

        float inL = 0.f, inR = 0.f;
        if (inputs[AUDIO_INPUT].isConnected()) {
            inL = inputs[AUDIO_INPUT].getVoltage() * 0.2f;
            inR = (inputs[AUDIO_INPUT].getChannels() > 1)
                ? inputs[AUDIO_INPUT].getVoltage(1) * 0.2f : inL;
        }
        inL = clampF(safeF(inL), -4.f, 4.f);
        inR = clampF(safeF(inR), -4.f, 4.f);

        DspParams d;
        updateParameters(d, dt);
        processStop(dt, d.stopOn);
        processFreeze(d.freezeOn);
        processGateCv();
        processClock(d);

        if (!freezeLatch) {
            bufL[writePos] = inL;
            bufR[writePos] = inR;
            writePos = (writePos + 1) & TAPE_MASK;
            if (writtenSamples < TAPE_SIZE) ++writtenSamples;
        }

        float tapeL = 0.f, tapeR = 0.f;
        processTape(d, dt, tapeL, tapeR);
        float grainL = 0.f, grainR = 0.f;
        processGranular(d, grainL, grainR);

        const float pitchW = clamp01((d.pitchSt < 0.f ? -d.pitchSt : d.pitchSt) * (1.f / 6.f));
        const float granW = clamp01(max3(d.timelineW, pitchW, clamp01(d.grainNorm * 1.25f)));
        const float tapeG = std::sqrt(1.f - granW);
        const float granG = std::sqrt(granW);
        float wetL = tapeL * tapeG + grainL * granG;
        float wetR = tapeR * tapeG + grainR * granG;

        processStutter(d, wetL, wetR);
        processTranceGate(d, wetL, wetR);
        processCharacter(wetL, wetR, d);
        processFx(d, wetL, wetR);
        wetL = clampF(safeF(wetL), -4.f, 4.f);
        wetR = clampF(safeF(wetR), -4.f, 4.f);
        processDelay(d, wetL, wetR);

        const float mix = clamp01(d.mix);
        const float dryGain = std::cos(mix * PI_F * 0.5f);
        const float wetGain = std::sin(mix * PI_F * 0.5f);
        const float outL = softLimit((inL * dryGain + wetL * wetGain) * d.output);
        const float outR = softLimit((inR * dryGain + wetR * wetGain) * d.output);

        outputs[AUDIO_L_OUTPUT].setVoltage(outL * 5.f);
        outputs[AUDIO_R_OUTPUT].setVoltage(outR * 5.f);
        outputs[GATE_OUTPUT].setVoltage(clamp01(safeF(tranceGateEnv)) * 10.f);

        if (sliceEocEvent) { sliceEocEvent = false; eocPulse.trigger(1e-3f); }
        outputs[EOC_OUTPUT].setVoltage(eocPulse.process(dt) ? 10.f : 0.f);
        if (clockThruEvent) { clockThruEvent = false; clockThruPulse.trigger(1e-3f); }
        outputs[CLOCK_THRU_OUTPUT].setVoltage(clockThruPulse.process(dt) ? 10.f : 0.f);
        outputs[ENV_OUTPUT].setVoltage(clamp01(safeF(envValue)) * 10.f);

        updateDisplayState(d, outL, outR);
    }
};

// -----------------------------------------------------------------------------
//  Panel  --  36 HP Darkpsy layout with port labels
// -----------------------------------------------------------------------------

struct PsyFXPanel : Widget {
    PsyFXPanel() { box.size = mm2px(Vec(PANEL_MM_W, PANEL_MM_H)); }

    void text(NVGcontext* vg, float mmX, float mmY, float px, NVGcolor col,
              const char* s, int align = NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE) {
        const Vec p = mm2px(Vec(mmX, mmY));
        nvgFontSize(vg, px);
        nvgTextAlign(vg, align);
        nvgFillColor(vg, col);
        nvgText(vg, p.x, p.y, s, nullptr);
    }

    void section(NVGcontext* vg, float x0, float y0, float w, float h, const char* title) {
        nvgBeginPath(vg);
        nvgRoundedRect(vg, mm2px(x0), mm2px(y0), mm2px(w), mm2px(h), mm2px(1.0f));
        nvgFillColor(vg, dp::kSectionBg);
        nvgFill(vg);
        nvgStrokeColor(vg, dp::kSectionBorder);
        nvgStrokeWidth(vg, 1.f);
        nvgStroke(vg);

        nvgBeginPath(vg);
        nvgRoundedRect(vg, mm2px(x0), mm2px(y0), mm2px(w), mm2px(4.0f), mm2px(1.0f));
        nvgFillColor(vg, dp::kSectionHeader);
        nvgFill(vg);

        const Vec dotP = mm2px(Vec(x0 + 2.0f, y0 + 2.0f));
        nvgBeginPath(vg);
        nvgCircle(vg, dotP.x, dotP.y, mm2px(0.7f));
        nvgFillColor(vg, dp::kSectionTitle);
        nvgFill(vg);

        const Vec p = mm2px(Vec(x0 + 3.6f, y0 + 2.0f));
        nvgFontSize(vg, 6.2f);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, dp::kSectionTitle);
        nvgText(vg, p.x, p.y, title, nullptr);
    }

    // Small label under a port jack.
    void portLabel(NVGcontext* vg, float mmX, float mmY, const char* s) {
        text(vg, mmX, mmY, 4.4f, dp::kLabel, s,
             NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    }

    void draw(const DrawArgs& args) override {
        const float W = box.size.x;
        const float H = box.size.y;

        // Panel base + vertical gradient.
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0.f, 0.f, W, H);
        nvgFillColor(args.vg, dp::kPanelBg);
        nvgFill(args.vg);
        {
            NVGpaint g = nvgLinearGradient(args.vg, 0.f, 0.f, 0.f, H,
                                           nvgRGBA(18, 30, 26, 220),
                                           nvgRGBA(4, 8, 10, 240));
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, W, H);
            nvgFillPaint(args.vg, g);
            nvgFill(args.vg);
        }

        // Neon-green + purple double rule under the brand.
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, 0.f, mm2px(12.f));
        nvgLineTo(args.vg, W, mm2px(12.f));
        nvgStrokeColor(args.vg, nvgRGBA(0, 230, 150, 220));
        nvgStrokeWidth(args.vg, 1.4f);
        nvgStroke(args.vg);
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, 0.f, mm2px(12.6f));
        nvgLineTo(args.vg, W, mm2px(12.6f));
        nvgStrokeColor(args.vg, nvgRGBA(170, 60, 220, 180));
        nvgStrokeWidth(args.vg, 0.8f);
        nvgStroke(args.vg);

        // ---- Sections ------------------------------------------------------
        section(args.vg, 3.0f, 31.0f, 79.0f, 14.0f, "DIGITAL FX");
        section(args.vg, 86.0f, 31.0f, 76.0f, 14.0f, "SPECTRAL FX");

        section(args.vg, 3.0f, 48.0f, 54.0f, 26.0f, "TIME / PITCH");
        section(args.vg, 60.0f, 48.0f, 54.0f, 26.0f, "TAPE");
        section(args.vg, 117.0f, 48.0f, 45.0f, 26.0f, "BEAT DELAY");

        section(args.vg, 3.0f, 76.0f, 54.0f, 26.0f, "RHYTHM / SLICE");
        section(args.vg, 60.0f, 76.0f, 54.0f, 26.0f, "CHARACTER / OUTPUT");
        section(args.vg, 117.0f, 76.0f, 45.0f, 26.0f, "MOD / MACRO");

        section(args.vg, 3.0f, 104.0f, 108.0f, 21.0f, "16-STEP GATE");
        section(args.vg, 114.0f, 104.0f, 48.0f, 21.0f, "TRANCE");

        // ---- Brand ---------------------------------------------------------
        text(args.vg, PANEL_MM_W * 0.5f, 4.5f, 14.f, dp::kBrandTitle, "HUTARA");
        text(args.vg, PANEL_MM_W * 0.5f, 10.5f, 6.4f, dp::kBrandSub, "PSYOPERATOR FX");

        // ---- Port column headers ------------------------------------------
        text(args.vg, 4.0f, 20.0f, 6.2f, dp::kLabelDim, "IN",
             NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        text(args.vg, PANEL_MM_W - 4.0f, 20.0f, 6.2f, dp::kLabelDim, "OUT",
             NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);

        // ---- Left column port label ---------------------------------------
        portLabel(args.vg, 8.0f, 26.0f, "AUDIO IN");

        // ---- Right column port labels --------------------------------------
        portLabel(args.vg, 172.0f, 25.5f, "SIG");
        portLabel(args.vg, 172.0f, 36.0f, "L OUT");
        portLabel(args.vg, 172.0f, 45.0f, "R OUT");
        portLabel(args.vg, 172.0f, 57.0f, "GATE");
        portLabel(args.vg, 172.0f, 66.0f, "EOC");
        portLabel(args.vg, 172.0f, 78.0f, "CLK");
        portLabel(args.vg, 172.0f, 87.0f, "ENV");
        portLabel(args.vg, 172.0f, 101.0f, "CLK IN");
        portLabel(args.vg, 172.0f, 110.0f, "TRIG IN");
        portLabel(args.vg, 172.0f, 119.0f, "GATE CV");

        // ---- TIME / PITCH labels -------------------------------------------
        text(args.vg, 13.0f, 55.5f, 4.8f, dp::kLabel, "SPEED");
        text(args.vg, 40.0f, 55.5f, 4.8f, dp::kLabel, "GRAIN");
        text(args.vg, 13.0f, 65.5f, 4.8f, dp::kLabel, "PITCH");
        text(args.vg, 40.0f, 65.5f, 4.8f, dp::kLabel, "SCATTER");

        // ---- TAPE labels ---------------------------------------------------
        text(args.vg, 70.0f, 55.5f, 4.8f, dp::kLabel, "WOW");
        text(args.vg, 98.0f, 55.5f, 4.8f, dp::kLabel, "FLUTTER");
        text(args.vg, 70.0f, 65.5f, 4.8f, dp::kLabel, "SPREAD");
        text(args.vg, 81.0f, 65.5f, 4.8f, dp::kLabel, "STOP");
        text(args.vg, 93.0f, 65.5f, 4.8f, dp::kLabel, "FRZ");
        text(args.vg, 106.0f, 65.5f, 4.8f, dp::kLabel, "REV");

        // ---- BEAT DELAY labels ---------------------------------------------
        text(args.vg, 124.0f, 55.5f, 4.8f, dp::kLabel, "ON");
        text(args.vg, 138.0f, 55.5f, 4.8f, dp::kLabel, "TIME");
        text(args.vg, 149.0f, 55.5f, 4.8f, dp::kLabel, "FB");
        text(args.vg, 159.0f, 55.5f, 4.8f, dp::kLabel, "TONE");
        text(args.vg, 128.0f, 65.5f, 4.8f, dp::kLabel, "MIX");

        // ---- RHYTHM / SLICE labels -----------------------------------------
        text(args.vg, 12.0f, 83.5f, 4.8f, dp::kLabel, "STUT");
        text(args.vg, 28.0f, 83.5f, 4.8f, dp::kLabel, "RATE");
        text(args.vg, 49.0f, 83.5f, 4.8f, dp::kLabel, "REPEAT");
        text(args.vg, 15.0f, 91.5f, 4.8f, dp::kLabel, "GATE");
        text(args.vg, 30.0f, 91.5f, 4.8f, dp::kLabel, "PROB");
        text(args.vg, 48.0f, 91.5f, 4.8f, dp::kLabel, "CAPT");

        // ---- CHARACTER / OUTPUT labels -------------------------------------
        text(args.vg, 70.0f, 83.5f, 4.8f, dp::kLabel, "DRIVE");
        text(args.vg, 98.0f, 83.5f, 4.8f, dp::kLabel, "TONE");
        text(args.vg, 70.0f, 91.5f, 4.8f, dp::kLabel, "WIDTH");
        text(args.vg, 82.0f, 91.5f, 4.8f, dp::kLabel, "MIX");
        text(args.vg, 104.0f, 91.5f, 4.8f, dp::kLabel, "OUT");

        // ---- MOD / MACRO labels --------------------------------------------
        text(args.vg, 126.0f, 83.5f, 4.8f, dp::kLabel, "MACRO");
        text(args.vg, 149.0f, 83.5f, 4.8f, dp::kLabel, "AMOUNT");
        text(args.vg, 124.0f, 91.5f, 4.8f, dp::kLabel, "RATE");
        text(args.vg, 136.0f, 91.5f, 4.8f, dp::kLabel, "RANDOM");
        text(args.vg, 153.0f, 91.5f, 4.8f, dp::kLabel, "EXT");

        // ---- 16-STEP GATE labels -------------------------------------------
        text(args.vg, 6.0f, 114.0f, 5.0f, dp::kLabel, "STEP");
        text(args.vg, 6.0f, 121.0f, 5.0f, dp::kLabel, "ACC");

        // ---- TRANCE labels -------------------------------------------------
        text(args.vg, 119.0f, 110.0f, 4.8f, dp::kLabel, "ON");
        text(args.vg, 133.0f, 110.0f, 4.8f, dp::kLabel, "DEPTH");
        text(args.vg, 146.0f, 110.0f, 4.8f, dp::kLabel, "SHAPE");
        text(args.vg, 158.0f, 110.0f, 4.8f, dp::kLabel, "RST");
        text(args.vg, 119.0f, 118.0f, 4.8f, dp::kLabel, "PAT");
        text(args.vg, 130.0f, 118.0f, 4.8f, dp::kLabel, "RATE");
        text(args.vg, 141.0f, 118.0f, 4.8f, dp::kLabel, "RATCH");
        text(args.vg, 151.0f, 118.0f, 4.8f, dp::kLabel, "SWING");
        text(args.vg, 160.0f, 118.0f, 4.8f, dp::kLabel, "ACC");

        // ---- FX labels -----------------------------------------------------
        text(args.vg, 12.0f, 43.5f, 4.8f, dp::kLabel, "SRR");
        text(args.vg, 26.0f, 43.5f, 4.8f, dp::kLabel, "BITS");
        text(args.vg, 40.0f, 43.5f, 4.8f, dp::kLabel, "FOLD");
        text(args.vg, 54.0f, 43.5f, 4.8f, dp::kLabel, "RING");
        text(args.vg, 68.0f, 43.5f, 4.8f, dp::kLabel, "RFRQ");
        text(args.vg, 97.0f, 43.5f, 4.8f, dp::kLabel, "CUT");
        text(args.vg, 113.0f, 43.5f, 4.8f, dp::kLabel, "RES");
        text(args.vg, 129.0f, 43.5f, 4.8f, dp::kLabel, "TILT");
        text(args.vg, 145.0f, 43.5f, 4.8f, dp::kLabel, "COMB");
        text(args.vg, 158.0f, 43.5f, 4.8f, dp::kLabel, "DUCK");

        Widget::draw(args);
    }
};

// -----------------------------------------------------------------------------
//  Display  --  full-width scope / history / mod bus
// -----------------------------------------------------------------------------

struct PsyFXDisplay : Widget {
    PsyOperatorFX* module;
    PsyFXDisplay() : module(nullptr) {}

    void label(NVGcontext* vg, float x, float y, float px, NVGcolor col,
               int align, const char* s) {
        nvgFontSize(vg, px);
        nvgTextAlign(vg, align | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, col);
        nvgText(vg, x, y, s, nullptr);
    }

    void historyBand(NVGcontext* vg, float x0, float y0, float w, float h,
                     float start, float len, NVGcolor col) {
        len = clampF(len, 0.f, 1.f);
        if (len <= 0.0005f) return;
        float a0 = std::fmod(start, 1.f);
        if (a0 < 0.f) a0 += 1.f;
        if (!(a0 >= 0.f && a0 < 1.f)) a0 = 0.f;
        const float a1 = a0 + len;
        nvgBeginPath(vg);
        if (a1 <= 1.f) nvgRect(vg, x0 + a0 * w, y0, len * w, h);
        else {
            nvgRect(vg, x0 + a0 * w, y0, (1.f - a0) * w, h);
            nvgRect(vg, x0, y0, (a1 - 1.f) * w, h);
        }
        nvgFillColor(vg, col);
        nvgFill(vg);
    }

    void drawWave(NVGcontext* vg, float x0, float y0, float w, float h) {
        const float mid = y0 + 0.5f * h;
        const float amp = 0.46f * h;
        nvgBeginPath(vg);
        nvgMoveTo(vg, x0, mid);
        nvgLineTo(vg, x0 + w, mid);
        nvgStrokeColor(vg, nvgRGBA(40, 80, 60, 130));
        nvgStrokeWidth(vg, 1.f);
        nvgStroke(vg);
        nvgBeginPath(vg);
        for (int i = 0; i < SCOPE_BUCKETS; ++i) {
            const int idx = (module->scopeBucket + 1 + i) & (SCOPE_BUCKETS - 1);
            const float mn = clampF(module->scopeMin[idx], -1.3f, 1.3f);
            const float mx = clampF(module->scopeMax[idx], -1.3f, 1.3f);
            const float x = x0 + (static_cast<float>(i) + 0.5f) * w / static_cast<float>(SCOPE_BUCKETS);
            nvgMoveTo(vg, x, mid - mx * amp);
            nvgLineTo(vg, x, mid - mn * amp);
        }
        nvgStrokeColor(vg, dp::kDispWave);
        nvgStrokeWidth(vg, 1.f);
        nvgStroke(vg);
    }

    void drawHistory(NVGcontext* vg, float x0, float y0, float w, float h) {
        nvgBeginPath(vg);
        nvgRect(vg, x0, y0, w, h);
        nvgFillColor(vg, dp::kDispBg);
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgRect(vg, x0 + 0.5f, y0 + 0.5f, w - 1.f, h - 1.f);
        nvgStrokeColor(vg, dp::kDispBorder);
        nvgStrokeWidth(vg, 1.f);
        nvgStroke(vg);
        const float inner = 2.f;
        const float bw = w - 2.f * inner;
        const float bh = h - 2.f * inner;
        const float by = y0 + inner;
        historyBand(vg, x0 + inner, by, bw, bh, module->dispWinStart, module->dispWinLen,
                    nvgRGBA(0, 190, 140, 60));
        historyBand(vg, x0 + inner, by, bw, bh * module->dispSliceActive,
                    module->dispSliceStart, module->dispSliceLen,
                    nvgRGBA(180, 60, 220, 120));
        const float wx = x0 + inner + clampF(module->dispWrite, 0.f, 1.f) * bw;
        const float rx = x0 + inner + clampF(module->dispRead, 0.f, 1.f) * bw;
        nvgBeginPath(vg);
        nvgRect(vg, wx - 0.5f, by, 1.5f, bh);
        nvgFillColor(vg, dp::kDispWrite);
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgRect(vg, rx - 0.5f, by - 1.f, 2.f, bh + 2.f);
        nvgFillColor(vg, dp::kDispRead);
        nvgFill(vg);
        const char* dir = (module->dispRate >= 0.f) ? ">>" : "<<";
        label(vg, x0 + inner + 8.f, by - 1.f, 8.f, dp::kDispRead, NVG_ALIGN_LEFT, dir);
        label(vg, x0 + w - inner - 4.f, by - 1.f, 8.f, dp::kDispText, NVG_ALIGN_RIGHT,
              divisionName(static_cast<int>(module->dispDivision)));
        {
            const float gx0 = x0 + inner + 2.f;
            const float gw = (bw - 4.f) / static_cast<float>(GATE_STEPS);
            const bool gateOn = module->tranceGateOn;
            for (int i = 0; i < GATE_STEPS; ++i) {
                const bool on = (module->dispGatePattern >> i) & 1;
                const bool cur = (i == module->dispGateStep) && (module->dispGate > 0.01f);
                const float cx = gx0 + (static_cast<float>(i) + 0.5f) * gw;
                const float cy = y0 - 3.f;
                const float rad = cur ? 1.8f : 1.2f;
                nvgBeginPath(vg);
                nvgCircle(vg, cx, cy, rad);
                if (!gateOn) nvgFillColor(vg, nvgRGB(30, 40, 35));
                else if (cur) nvgFillColor(vg, nvgRGB(255, 220, 120));
                else if (on) nvgFillColor(vg, dp::kDispRead);
                else nvgFillColor(vg, nvgRGB(30, 55, 45));
                nvgFill(vg);
            }
        }
    }

    void drawModBus(NVGcontext* vg, float x0, float y0, float w, float h) {
        static const char* names[4] = { "SPD", "PIT", "GTE", "DLY" };
        static const int slots[4] = {
            PsyOperatorFX::MOD_SPEED, PsyOperatorFX::MOD_PITCH,
            PsyOperatorFX::MOD_GATE, PsyOperatorFX::MOD_DELAY
        };
        nvgBeginPath(vg);
        nvgRect(vg, x0, y0, w, h);
        nvgFillColor(vg, dp::kDispBg);
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgRect(vg, x0 + 0.5f, y0 + 0.5f, w - 1.f, h - 1.f);
        nvgStrokeColor(vg, dp::kDispBorder);
        nvgStrokeWidth(vg, 1.f);
        nvgStroke(vg);
        label(vg, x0 + 4.f, y0 + 5.f, 7.f, dp::kDispText, NVG_ALIGN_LEFT, "MOD BUS");
        const float rowH = (h - 8.f) / 4.f;
        for (int i = 0; i < 4; ++i) {
            const ModSlot& s = module->modView[slots[i]];
            const float ry = y0 + 8.f + static_cast<float>(i) * rowH;
            label(vg, x0 + 4.f, ry + rowH * 0.5f, 6.5f,
                  nvgRGBA(120, 200, 170, 220), NVG_ALIGN_LEFT, names[i]);
            const float tx = x0 + 26.f;
            const float tw = w - 30.f;
            nvgBeginPath(vg);
            nvgRect(vg, tx, ry + 1.f, tw, rowH - 3.f);
            nvgFillColor(vg, nvgRGB(10, 20, 16));
            nvgFill(vg);
            const float bx = tx + clamp01(s.base) * tw;
            const float cvx = tx + clamp01(s.base + s.cv) * tw;
            const float mdx = tx + clamp01(s.base + s.mod) * tw;
            const float exx = tx + clamp01(s.base + s.ext) * tw;
            nvgBeginPath(vg);
            nvgRect(vg, (mdx < bx) ? mdx : bx, ry + rowH * 0.5f - 0.5f,
                    (mdx < bx) ? (bx - mdx) : (mdx - bx), 2.f);
            nvgFillColor(vg, nvgRGBA(180, 60, 220, 200));
            nvgFill(vg);
            nvgBeginPath(vg);
            nvgRect(vg, (exx < bx) ? exx : bx, ry + rowH * 0.5f - 0.5f,
                    (exx < bx) ? (bx - exx) : (exx - bx), 2.f);
            nvgFillColor(vg, nvgRGBA(220, 90, 220, 200));
            nvgFill(vg);
            nvgBeginPath(vg);
            nvgRect(vg, (cvx < bx) ? cvx : bx, ry + 1.f,
                    (cvx < bx) ? (bx - cvx) : (cvx - bx), rowH - 3.f);
            nvgFillColor(vg, nvgRGBA(0, 200, 140, 130));
            nvgFill(vg);
            const float vx = tx + clamp01(s.value) * tw;
            nvgBeginPath(vg);
            nvgMoveTo(vg, vx, ry + 0.5f);
            nvgLineTo(vg, vx + 2.5f, ry + 5.f);
            nvgLineTo(vg, vx - 2.5f, ry + 5.f);
            nvgClosePath(vg);
            nvgFillColor(vg, nvgRGB(230, 255, 240));
            nvgFill(vg);
        }
    }

    void draw(const DrawArgs& args) override {
        const float w = box.size.x;
        const float h = box.size.y;
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0.f, 0.f, w, h);
        nvgFillColor(args.vg, dp::kDispBg);
        nvgFill(args.vg);
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0.5f, 0.5f, w - 1.f, h - 1.f);
        nvgStrokeColor(args.vg, dp::kDispBorder);
        nvgStrokeWidth(args.vg, 1.f);
        nvgStroke(args.vg);
        if (!module) return;

        drawWave(args.vg, 6.f, 2.f, 262.f, 28.f);
        drawHistory(args.vg, 6.f, 34.f, 262.f, 12.f);
        drawModBus(args.vg, 275.f, 2.f, 128.f, 44.f);

        char line[200];
        const char* stopName = "IDLE";
        if (module->stopPhase == PsyOperatorFX::STOP_DECAY) stopName = "DECAY";
        else if (module->stopPhase == PsyOperatorFX::STOP_HOLD) stopName = "HOLD";
        else if (module->stopPhase == PsyOperatorFX::STOP_RISE) stopName = "RISE";
        std::snprintf(line, sizeof(line),
                      "MAC %s  STOP %s  TM %.2f  CHAOS %.2f  EXT %.2f  GATE %.2f  DLY %.2f",
                      MACRO_NAMES[clampI(module->dispMacro, 0, MACRO_COUNT - 1)],
                      stopName, module->dispTimeline, module->dispChaos, module->dispExtMod,
                      module->dispGate, module->dispDelayMix);
        label(args.vg, 6.f, 51.f, 8.f, dp::kDispText, NVG_ALIGN_LEFT, line);
        float bx = 6.f;
        if (module->freezeLatch) {
            label(args.vg, bx, 8.f, 9.f, nvgRGB(255, 255, 255), NVG_ALIGN_LEFT, "FRZ");
            bx += 26.f;
        }
        if (module->dispReverse > 0.5f) {
            label(args.vg, bx, 8.f, 9.f, dp::kDispRead, NVG_ALIGN_LEFT, "REV");
            bx += 26.f;
        }
        if (module->dispSliceActive > 0.05f) {
            label(args.vg, bx, 8.f, 9.f, nvgRGB(180, 60, 220), NVG_ALIGN_LEFT, "SLICE");
            bx += 34.f;
        }
        if (module->dispStop > 0.02f) {
            label(args.vg, bx, 8.f, 9.f, nvgRGB(255, 200, 90), NVG_ALIGN_LEFT, "STOP");
            bx += 30.f;
        }
        if (module->dispExtMod > 0.05f)
            label(args.vg, bx, 8.f, 9.f, nvgRGB(220, 90, 220), NVG_ALIGN_LEFT, "EXT");
        if (module->dispFx > 0) {
            label(args.vg, bx, 8.f, 9.f, nvgRGB(0, 220, 140), NVG_ALIGN_LEFT, "FX");
            bx += 24.f;
        }
    }
};

// -----------------------------------------------------------------------------
//  Knob / switch / button widgets
// -----------------------------------------------------------------------------

struct PsyKnobBig : componentlibrary::Davies1900hWhiteKnob {
    PsyKnobBig() { box.size = box.size.mult(0.60f); }
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        const float cx = box.size.x * 0.5f;
        const float cy = box.size.y * 0.5f;
        nvgTranslate(args.vg, cx, cy);
        nvgScale(args.vg, 0.60f, 0.60f);
        nvgTranslate(args.vg, -cx, -cy);
        componentlibrary::Davies1900hWhiteKnob::draw(args);
        nvgRestore(args.vg);
    }
};

// Small knob, no centre dot (Rev 19).
struct PsyKnobSmall : componentlibrary::RoundSmallBlackKnob {
    PsyKnobSmall() { box.size = box.size.mult(0.85f); }
    void draw(const DrawArgs& args) override {
        componentlibrary::RoundSmallBlackKnob::draw(args);
    }
};

// Step knob with enlarged outer LED dots (Rev 19).  No centre dot.
struct PsyStepKnob : componentlibrary::RoundSmallBlackKnob {
    int steps = DIVISION_COUNT;
    int currentStep = 0;
    ParamQuantity* quantity = nullptr;
    const char* const* labelNames = DIVISION_NAMES;
    int labelCount = DIVISION_COUNT;
    bool isMacro = false;

    PsyStepKnob() { box.size = box.size.mult(0.85f); }

    void setSteps(int n) { steps = clampI(n, 2, DIVISION_COUNT); currentStep = 0; }
    void setStepLabels(const char* const* names, int count) {
        labelNames = names;
        labelCount = clampI(count, 2, 64);
    }
    void setCurrentStep(int s) { currentStep = clampI(s, 0, steps - 1); }

    void snapToNearest() {
        if (!quantity) return;
        const float v = quantity->getValue();
        if (!std::isfinite(v)) return;
        int s = clampI(static_cast<int>(std::lround(v)), 0, steps - 1);
        if (std::fabs(v - static_cast<float>(s)) > 1e-4f)
            quantity->setValue(static_cast<float>(s));
        currentStep = s;
    }

    void onDragStart(const DragStartEvent& e) override {
        componentlibrary::RoundSmallBlackKnob::onDragStart(e);
        snapToNearest();
    }
    void onDragMove(const DragMoveEvent& e) override {
        componentlibrary::RoundSmallBlackKnob::onDragMove(e);
        snapToNearest();
    }
    void onDragEnd(const DragEndEvent& e) override {
        componentlibrary::RoundSmallBlackKnob::onDragEnd(e);
        snapToNearest();
    }
    void onDragHover(const DragHoverEvent& e) override {
        componentlibrary::RoundSmallBlackKnob::onDragHover(e);
        snapToNearest();
    }

    void draw(const DrawArgs& args) override {
        if (quantity) {
            const float v = quantity->getValue();
            if (std::isfinite(v))
                currentStep = clampI(static_cast<int>(std::lround(v)), 0, steps - 1);
        }

        componentlibrary::RoundSmallBlackKnob::draw(args);

        const float cx = box.size.x * 0.5f;
        const float cy = box.size.y * 0.5f;
        // Enlarged LED ring (0.62 instead of 0.46) so the dots sit just
        // outside the knob body and fully surround it.
        const float radius = box.size.x * 0.62f;
        const float minV = quantity ? quantity->getMinValue() : 0.f;
        const float maxV = quantity ? quantity->getMaxValue() : static_cast<float>(steps - 1);
        const float spanV = (maxV - minV) > 1e-6f ? (maxV - minV) : 1.f;

        for (int i = 0; i < steps; ++i) {
            const float stepValue = static_cast<float>(i);
            const float norm = clampF((stepValue - minV) / spanV, 0.f, 1.f);
            const float rot = norm * PI_F * 1.5f - PI_F * 0.75f;
            const float lx = cx + std::sin(rot) * radius;
            const float ly = cy - std::cos(rot) * radius;
            const bool active = (i == currentStep);

            nvgBeginPath(args.vg);
            nvgCircle(args.vg, lx, ly, active ? 2.0f : 1.2f);
            nvgFillColor(args.vg, active ? dp::kRingOn : dp::kRingOff);
            nvgFill(args.vg);
        }

        nvgFontSize(args.vg, 7.0f);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, nvgRGBA(140, 255, 190, 255));
        nvgText(args.vg, cx, cy - box.size.y * 0.78f,
                labelNames[clampI(currentStep, 0, labelCount - 1)], nullptr);
    }
};

struct PsyTrimKnob : componentlibrary::Rogan1PRed {
    PsyTrimKnob() { box.size = box.size.mult(0.80f); }
};

struct PsySwitch : componentlibrary::CKSS {
    PsySwitch() { box.size = box.size.mult(1.0f); }
};

struct PsyButton : componentlibrary::LEDButton {
    PsyButton() { box.size = box.size.mult(1.05f); }
};

// -----------------------------------------------------------------------------
//  PsyGateGrid -- single OpaqueWidget owning the whole 16-step / 16-accent grid.
//  Rev 19: re-tuned for the 108 mm section.
// -----------------------------------------------------------------------------

struct PsyGateGrid : OpaqueWidget {
    PsyOperatorFX* module = nullptr;

    // Panel-absolute cell geometry.
    static constexpr float kBaseX     = 12.0f;   // first cell centre
    static constexpr float kStepPitch = 6.30f;   // spacing between cells
    static constexpr float kStepY     = 113.5f;  // step row centre
    static constexpr float kAccY      = 120.5f;  // accent row centre
    static constexpr float kStepHalf  = 1.90f;   // half-size of a step cell
    static constexpr float kAccHalf   = 1.40f;   // radius of an accent cell

    static constexpr float kOriginX  = 4.0f;
    static constexpr float kOriginY  = 105.0f;
    static constexpr float kWidthMM  = 106.0f;
    static constexpr float kHeightMM = 19.0f;

    void draw(const DrawArgs& args) override {
        if (!module) return;

        const float stepYpx = mm2px(Vec(0.f, kStepY - kOriginY)).y;
        const float accYpx  = mm2px(Vec(0.f, kAccY  - kOriginY)).y;
        const float halfStepPx = mm2px(Vec(kStepHalf, 0.f)).x;
        const float accHalfPx  = mm2px(Vec(kAccHalf,  0.f)).x;

        for (int s = 0; s < 16; ++s) {
            const float xpx = mm2px(Vec(kBaseX + s * kStepPitch - kOriginX, 0.f)).x;

            const bool onStep = module->userGate[module->userGateIndex][s] != 0;
            const bool onAcc  = module->accentGate[module->userGateIndex][s] != 0;
            const bool cur    = module->tranceGateOn && (s == module->tranceGateStep);

            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, xpx - halfStepPx, stepYpx - halfStepPx,
                           2.f * halfStepPx, 2.f * halfStepPx, 3.0f);
            nvgFillColor(args.vg, onStep ? dp::kStepOn : dp::kStepOff);
            nvgFill(args.vg);

            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, xpx - halfStepPx, stepYpx - halfStepPx,
                           2.f * halfStepPx, 2.f * halfStepPx, 3.0f);
            nvgStrokeColor(args.vg, dp::kStepBorder);
            nvgStrokeWidth(args.vg, 1.f);
            nvgStroke(args.vg);

            if (cur) {
                nvgBeginPath(args.vg);
                nvgRoundedRect(args.vg,
                               xpx - halfStepPx - 1.5f, stepYpx - halfStepPx - 1.5f,
                               2.f * halfStepPx + 3.f, 2.f * halfStepPx + 3.f, 4.0f);
                nvgStrokeColor(args.vg, dp::kStepCurrent);
                nvgStrokeWidth(args.vg, 1.4f);
                nvgStroke(args.vg);
            }

            nvgBeginPath(args.vg);
            nvgCircle(args.vg, xpx, accYpx, accHalfPx);
            nvgFillColor(args.vg, onAcc ? dp::kAccOn : dp::kAccOff);
            nvgFill(args.vg);

            nvgBeginPath(args.vg);
            nvgCircle(args.vg, xpx, accYpx, accHalfPx);
            nvgStrokeColor(args.vg, dp::kAccBorder);
            nvgStrokeWidth(args.vg, 1.f);
            nvgStroke(args.vg);
        }
    }

    void onButton(const ButtonEvent& e) override {
        if (e.action != GLFW_PRESS || e.button != GLFW_MOUSE_BUTTON_LEFT) {
            OpaqueWidget::onButton(e);
            return;
        }
        if (!module) {
            OpaqueWidget::onButton(e);
            return;
        }

        const float stepYpx = mm2px(Vec(0.f, kStepY - kOriginY)).y;
        const float accYpx  = mm2px(Vec(0.f, kAccY  - kOriginY)).y;
        const float halfStepPx = mm2px(Vec(kStepHalf, 0.f)).x;
        const float accHalfPx  = mm2px(Vec(kAccHalf,  0.f)).x;

        for (int s = 0; s < 16; ++s) {
            const float xpx = mm2px(Vec(kBaseX + s * kStepPitch - kOriginX, 0.f)).x;

            if (std::fabs(e.pos.x - xpx) <= halfStepPx &&
                std::fabs(e.pos.y - stepYpx) <= halfStepPx) {
                uint8_t& cell = module->userGate[module->userGateIndex][s];
                cell = cell ? 0 : 1;
                module->paramQuantities[PsyOperatorFX::SEQ_STEP_0_PARAM + s]
                    ->setValue(cell ? 1.f : 0.f);
                e.consume(this);
                return;
            }

            const float dx = e.pos.x - xpx;
            const float dy = e.pos.y - accYpx;
            if (dx * dx + dy * dy <= accHalfPx * accHalfPx) {
                uint8_t& cell = module->accentGate[module->userGateIndex][s];
                cell = cell ? 1 : 0;
                module->paramQuantities[PsyOperatorFX::SEQ_ACC_0_PARAM + s]
                    ->setValue(cell ? 1.f : 0.f);
                e.consume(this);
                return;
            }
        }

        OpaqueWidget::onButton(e);
    }
};

// -----------------------------------------------------------------------------
//  Module widget  --  36 HP layout, every port registered exactly once
// -----------------------------------------------------------------------------

struct PsyOperatorFXWidget : ModuleWidget {
    PsyOperatorFXWidget(PsyOperatorFX* module) {
        setModule(module);
        setPanel(createWidget<PsyFXPanel>(Vec(0, 0)));

        // Corner screws for a 36 HP panel.
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0.f)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2.f * RACK_GRID_WIDTH, 0.f)));
        addChild(createWidget<ScrewSilver>(mm2px(Vec(3.0f, 123.5f))));
        addChild(createWidget<ScrewSilver>(mm2px(Vec(PANEL_MM_W - 3.0f, 123.5f))));

        // Display.
        PsyFXDisplay* disp = createWidget<PsyFXDisplay>(mm2px(Vec(3.0f, 13.5f)));
        disp->box.size = mm2px(Vec(PANEL_MM_W - 6.0f, 17.0f));
        disp->module = module;
        addChild(disp);

        auto big   = [&](float x, float y, int id) { addParam(createParamCentered<PsyKnobBig>(mm2px(Vec(x, y)), module, id)); };
        auto small = [&](float x, float y, int id) { addParam(createParamCentered<PsyKnobSmall>(mm2px(Vec(x, y)), module, id)); };
        auto trim  = [&](float x, float y, int id) { addParam(createParamCentered<PsyTrimKnob>(mm2px(Vec(x, y)), module, id)); };
        auto sw    = [&](float x, float y, int id) { addParam(createParamCentered<PsySwitch>(mm2px(Vec(x, y)), module, id)); };
        auto btn   = [&](float x, float y, int id) { addParam(createParamCentered<PsyButton>(mm2px(Vec(x, y)), module, id)); };
        auto jin   = [&](float x, float y, int id) { addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x, y)), module, id)); };
        auto jout  = [&](float x, float y, int id) { addOutput(createOutputCentered<PJ3410Port>(mm2px(Vec(x, y)), module, id)); };
        auto lamp  = [&](float x, float y, int id) { addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(x, y)), module, id)); };

        auto step = [&](float x, float y, int id, int defaultStep) {
            auto* k = createParamCentered<PsyStepKnob>(mm2px(Vec(x, y)), module, id);
            k->setSteps(DIVISION_COUNT);
            k->setCurrentStep(defaultStep);
            if (module) k->quantity = module->paramQuantities[id];
            addParam(k);
            return k;
        };
        auto macroStep = [&](float x, float y, int id) {
            auto* k = createParamCentered<PsyStepKnob>(mm2px(Vec(x, y)), module, id);
            k->setSteps(MACRO_COUNT);
            k->setStepLabels(MACRO_NAMES, MACRO_COUNT);
            k->setCurrentStep(0);
            k->isMacro = true;
            if (module) k->quantity = module->paramQuantities[id];
            addParam(k);
            return k;
        };

        // =====================================================================
        //  LEFT PORT  --  audio in
        // =====================================================================
        jin(8.0f, 22.0f, PsyOperatorFX::AUDIO_INPUT);

        // =====================================================================
        //  RIGHT COLUMN  --  outputs + remaining inputs, all with labels
        // =====================================================================
        lamp(172.0f, 22.0f, PsyOperatorFX::SIGNAL_LIGHT);
        jout(172.0f, 32.0f, PsyOperatorFX::AUDIO_L_OUTPUT);
        jout(172.0f, 41.0f, PsyOperatorFX::AUDIO_R_OUTPUT);
        jout(172.0f, 53.0f, PsyOperatorFX::GATE_OUTPUT);
        jout(172.0f, 62.0f, PsyOperatorFX::EOC_OUTPUT);
        jout(172.0f, 74.0f, PsyOperatorFX::CLOCK_THRU_OUTPUT);
        jout(172.0f, 83.0f, PsyOperatorFX::ENV_OUTPUT);
        jin(172.0f, 97.0f,  PsyOperatorFX::CLOCK_INPUT);
        jin(172.0f, 106.0f, PsyOperatorFX::TRIGGER_INPUT);
        jin(172.0f, 115.0f, PsyOperatorFX::GATE_CV_INPUT);

        // =====================================================================
        //  DIGITAL FX  (section 3..82, y 31..45)
        // =====================================================================
        small(12.0f, 40.0f, PsyOperatorFX::FX_SRR_PARAM);
        small(26.0f, 40.0f, PsyOperatorFX::FX_BITS_PARAM);
        small(40.0f, 40.0f, PsyOperatorFX::FX_FOLD_PARAM);
        small(54.0f, 40.0f, PsyOperatorFX::FX_RING_PARAM);
        small(68.0f, 40.0f, PsyOperatorFX::FX_RFRQ_PARAM);

        // =====================================================================
        //  SPECTRAL FX  (section 86..162, y 31..45)
        // =====================================================================
        small(97.0f,  40.0f, PsyOperatorFX::FX_CUT_PARAM);
        small(113.0f, 40.0f, PsyOperatorFX::FX_RES_PARAM);
        small(129.0f, 40.0f, PsyOperatorFX::FX_MODE_PARAM);
        small(145.0f, 40.0f, PsyOperatorFX::FX_COMB_PARAM);
        small(158.0f, 40.0f, PsyOperatorFX::FX_DUCK_PARAM);

        // =====================================================================
        //  TIME / PITCH  (section 3..57, y 48..74)
        // =====================================================================
        small(13.0f, 60.0f, PsyOperatorFX::SPEED_PARAM);
        jin(23.0f, 60.0f, PsyOperatorFX::SPEED_CV_INPUT);
        small(40.0f, 60.0f, PsyOperatorFX::STRETCH_PARAM);
        jin(50.0f, 60.0f, PsyOperatorFX::STRETCH_CV_INPUT);
        big(13.0f, 70.0f, PsyOperatorFX::PITCH_PARAM);
        jin(23.0f, 70.0f, PsyOperatorFX::PITCH_CV_INPUT);
        big(40.0f, 70.0f, PsyOperatorFX::SCATTER_PARAM);
        jin(50.0f, 70.0f, PsyOperatorFX::SCATTER_CV_INPUT);

        // =====================================================================
        //  TAPE  (section 60..114, y 48..74)
        // =====================================================================
        small(70.0f, 60.0f, PsyOperatorFX::WOW_PARAM);
        jin(79.0f, 60.0f, PsyOperatorFX::WOW_CV_INPUT);
        small(98.0f, 60.0f, PsyOperatorFX::FLUTTER_PARAM);
        jin(108.0f, 60.0f, PsyOperatorFX::FLUTTER_CV_INPUT);
        small(70.0f, 70.0f, PsyOperatorFX::SPREAD_PARAM);
        sw(81.0f, 70.0f, PsyOperatorFX::STOP_SWITCH_PARAM);
        lamp(85.5f, 70.0f, PsyOperatorFX::STOP_LIGHT);
        sw(93.0f, 70.0f, PsyOperatorFX::FREEZE_SWITCH_PARAM);
        lamp(97.5f, 70.0f, PsyOperatorFX::FREEZE_LIGHT);
        sw(106.0f, 70.0f, PsyOperatorFX::REVERSE_SWITCH_PARAM);

        // =====================================================================
        //  BEAT DELAY  (section 117..162, y 48..74)
        // =====================================================================
        sw(124.0f, 60.0f, PsyOperatorFX::DELAY_SWITCH_PARAM);
        lamp(128.5f, 60.0f, PsyOperatorFX::DELAY_LIGHT);
        step(138.0f, 60.0f, PsyOperatorFX::DELAY_TIME_PARAM, 4);
        small(149.0f, 60.0f, PsyOperatorFX::DELAY_FEEDBACK_PARAM);
        small(159.0f, 60.0f, PsyOperatorFX::DELAY_TONE_PARAM);
        small(128.0f, 70.0f, PsyOperatorFX::DELAY_MIX_PARAM);
        jin(142.0f, 70.0f, PsyOperatorFX::DELAY_CV_INPUT);

        // =====================================================================
        //  RHYTHM / SLICE  (section 3..57, y 76..102)
        // =====================================================================
        sw(12.0f, 87.0f, PsyOperatorFX::STUTTER_SWITCH_PARAM);
        lamp(16.5f, 87.0f, PsyOperatorFX::STUTTER_LIGHT);
        step(28.0f, 87.0f, PsyOperatorFX::STUTTER_RATE_PARAM, 3);
        jin(38.0f, 87.0f, PsyOperatorFX::STUTTER_CV_INPUT);
        small(49.0f, 87.0f, PsyOperatorFX::REPEATS_PARAM);
        small(15.0f, 96.0f, PsyOperatorFX::GATE_PARAM);
        small(30.0f, 96.0f, PsyOperatorFX::PROBABILITY_PARAM);
        btn(48.0f, 96.0f, PsyOperatorFX::CAPTURE_PARAM);

        // =====================================================================
        //  CHARACTER / OUTPUT  (section 60..114, y 76..102)
        // =====================================================================
        small(70.0f, 87.0f, PsyOperatorFX::DRIVE_PARAM);
        jin(79.0f, 87.0f, PsyOperatorFX::DRIVE_CV_INPUT);
        small(98.0f, 87.0f, PsyOperatorFX::TONE_PARAM);
        small(70.0f, 96.0f, PsyOperatorFX::WIDTH_PARAM);
        big(82.0f, 96.0f, PsyOperatorFX::MIX_PARAM);
        jin(92.0f, 96.0f, PsyOperatorFX::MIX_CV_INPUT);
        trim(104.0f, 96.0f, PsyOperatorFX::OUTPUT_PARAM);

        // =====================================================================
        //  MOD / MACRO  (section 117..162, y 76..102)
        // =====================================================================
        macroStep(126.0f, 87.0f, PsyOperatorFX::PHASE1_MACRO_PARAM);
        jin(136.0f, 87.0f, PsyOperatorFX::PHASE1_MACRO_CV_INPUT);
        small(149.0f, 87.0f, PsyOperatorFX::CHAOS_AMOUNT_PARAM);
        small(124.0f, 96.0f, PsyOperatorFX::CHAOS_RATE_PARAM);
        btn(136.0f, 96.0f, PsyOperatorFX::CHAOS_TRIGGER_PARAM);
        jin(153.0f, 96.0f, PsyOperatorFX::EXT_MOD_INPUT);
        lamp(158.0f, 96.0f, PsyOperatorFX::EXT_MOD_LIGHT);

        // =====================================================================
        //  16-STEP GATE grid
        // =====================================================================
        {
            auto* grid = new PsyGateGrid;
            grid->box.pos  = mm2px(Vec(PsyGateGrid::kOriginX, PsyGateGrid::kOriginY));
            grid->box.size = mm2px(Vec(PsyGateGrid::kWidthMM, PsyGateGrid::kHeightMM));
            grid->module   = module;
            addChild(grid);
        }

        // =====================================================================
        //  TRANCE  (section 114..162, y 104..125)
        // =====================================================================
        sw(119.0f, 113.5f, PsyOperatorFX::TRANCE_GATE_SWITCH_PARAM);
        lamp(123.5f, 113.5f, PsyOperatorFX::GATE_LIGHT);
        small(133.0f, 113.5f, PsyOperatorFX::TRANCE_GATE_DEPTH_PARAM);
        small(146.0f, 113.5f, PsyOperatorFX::TRANCE_GATE_SHAPE_PARAM);
        sw(158.0f, 113.5f, PsyOperatorFX::TRANCE_GATE_RESET_PARAM);
        {
            auto* k = createParamCentered<PsyStepKnob>(mm2px(Vec(119.0f, 121.0f)),
                                                       module,
                                                       PsyOperatorFX::TRANCE_GATE_PATTERN_PARAM);
            k->setSteps(8);
            k->setCurrentStep(0);
            if (module) k->quantity = module->paramQuantities[PsyOperatorFX::TRANCE_GATE_PATTERN_PARAM];
            addParam(k);
        }
        step(130.0f, 121.0f, PsyOperatorFX::TRANCE_GATE_RATE_PARAM, 3);
        small(141.0f, 121.0f, PsyOperatorFX::SEQ_RATCHET_PARAM);
        small(151.0f, 121.0f, PsyOperatorFX::SEQ_SWING_PARAM);
        small(160.0f, 121.0f, PsyOperatorFX::SEQ_ACCENT_PARAM);
    }
};

} // namespace

Model* modelPsyOperatorFX = createModel<PsyOperatorFX, PsyOperatorFXWidget>("PsyOperatorFX");