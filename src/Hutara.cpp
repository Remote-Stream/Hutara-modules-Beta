#include "plugin.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <string>
#include <array>
#include <cstdio>
#include <atomic>

#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <pmmintrin.h>
#include <xmmintrin.h>
#define FMOP_X86_FPU 1
#endif

static inline void fmopFlushToZero() {
#ifdef FMOP_X86_FPU
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
#ifdef _MM_SET_DENORMALS_ZERO_MODE
	_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif
#endif
}

// ============================================================
// UI widgets
// ============================================================

struct MyPrivateSmallKnob : componentlibrary::RoundSmallBlackKnob {
	MyPrivateSmallKnob() { box.size = box.size.mult(0.8f); }
	void draw(const DrawArgs& args) override {
		nvgSave(args.vg);
		nvgScale(args.vg, 0.8f, 0.8f);
		nvgGlobalTint(args.vg, nvgRGBA(0, 255, 255, 255));
		componentlibrary::RoundSmallBlackKnob::draw(args);
		nvgRestore(args.vg);
	}
};

struct NKKSmall : componentlibrary::NKK {
	NKKSmall() { box.size = box.size.mult(0.7f); }
	void draw(const DrawArgs& args) override {
		nvgSave(args.vg);
		nvgScale(args.vg, 0.7f, 0.7f);
		componentlibrary::NKK::draw(args);
		nvgRestore(args.vg);
	}
};

struct NKKTiny : componentlibrary::NKK {
	NKKTiny() { box.size = box.size.mult(0.5f); }
	void draw(const DrawArgs& args) override {
		nvgSave(args.vg);
		nvgScale(args.vg, 0.5f, 0.5f);
		componentlibrary::NKK::draw(args);
		nvgRestore(args.vg);
	}
};

struct PitchStepModeButton : app::SvgSwitch {
	PitchStepModeButton() {
		momentary = false;
		addFrame(APP->window->loadSvg(
			asset::system("res/ComponentLibrary/VCVButton_0.svg")));
		addFrame(APP->window->loadSvg(
			asset::system("res/ComponentLibrary/VCVButton_1.svg")));
		box.size = Vec(12.f, 12.f);
	}
	void draw(const DrawArgs& args) override {
		nvgSave(args.vg);
		nvgScale(args.vg, 0.7f, 0.7f);
		nvgGlobalTint(args.vg, nvgRGBA(255, 160, 60, 255));
		app::SvgSwitch::draw(args);
		nvgRestore(args.vg);
	}
};

struct PitchBusModeSwitch : componentlibrary::RoundSmallBlackKnob {
	PitchBusModeSwitch() { box.size = box.size.mult(0.7f); }
	void draw(const DrawArgs& args) override {
		nvgSave(args.vg);
		nvgScale(args.vg, 0.7f, 0.7f);
		nvgGlobalTint(args.vg, nvgRGBA(255, 160, 60, 255));
		componentlibrary::RoundSmallBlackKnob::draw(args);
		nvgRestore(args.vg);
	}
};

struct MyPrivateSmallRedKnob : componentlibrary::Rogan1PRed {
	MyPrivateSmallRedKnob() { box.size = box.size.mult(0.8f); }
	void draw(const DrawArgs& args) override {
		nvgSave(args.vg);
		nvgScale(args.vg, 0.8f, 0.8f);
		nvgGlobalTint(args.vg, nvgRGBA(255, 80, 80, 255));
		componentlibrary::Rogan1PRed::draw(args);
		nvgRestore(args.vg);
	}
};

struct MySynthTechAlco : componentlibrary::SynthTechAlco {
	MySynthTechAlco() { box.size = box.size.mult(0.9f); }
	void draw(const DrawArgs& args) override {
		nvgSave(args.vg);
		nvgScale(args.vg, 0.9f, 0.9f);
		componentlibrary::SynthTechAlco::draw(args);
		nvgRestore(args.vg);
	}
};

struct PsyKnobSmall : componentlibrary::Davies1900hWhiteKnob {
	PsyKnobSmall() { box.size = box.size.mult(0.65f); }
	void draw(const DrawArgs& args) override {
		nvgSave(args.vg);
		nvgScale(args.vg, 0.65f, 0.65f);
		componentlibrary::Davies1900hWhiteKnob::draw(args);
		nvgRestore(args.vg);
	}
};

struct PitchBusDepthKnob : componentlibrary::RoundSmallBlackKnob {
	PitchBusDepthKnob() { box.size = box.size.mult(0.55f); }
	void draw(const DrawArgs& args) override {
		nvgSave(args.vg);
		nvgScale(args.vg, 0.55f, 0.55f);
		nvgGlobalTint(args.vg, nvgRGBA(255, 160, 60, 255));
		componentlibrary::RoundSmallBlackKnob::draw(args);
		nvgRestore(args.vg);
	}
};

struct CVAMOUNTKnob : componentlibrary::RoundSmallBlackKnob {
	CVAMOUNTKnob() { box.size = box.size.mult(0.75f); }
	void draw(const DrawArgs& args) override {
		nvgSave(args.vg);
		nvgScale(args.vg, 0.75f, 0.75f);
		nvgGlobalTint(args.vg, nvgRGBA(60, 255, 120, 255));
		componentlibrary::RoundSmallBlackKnob::draw(args);
		nvgRestore(args.vg);
	}
};

struct PDKnob : componentlibrary::SynthTechAlco {
	PDKnob() { box.size = box.size.mult(0.52f); }
	void draw(const DrawArgs& args) override {
		nvgSave(args.vg);
		nvgScale(args.vg, 0.52f, 0.52f);
		componentlibrary::SynthTechAlco::draw(args);
		nvgRestore(args.vg);
	}
};

struct CrushKnob : componentlibrary::RoundSmallBlackKnob {
	CrushKnob() { box.size = box.size.mult(0.72f); }
	void draw(const DrawArgs& args) override {
		nvgSave(args.vg);
		nvgScale(args.vg, 0.72f, 0.72f);
		nvgGlobalTint(args.vg, nvgRGBA(200, 120, 255, 255));
		componentlibrary::RoundSmallBlackKnob::draw(args);
		nvgRestore(args.vg);
	}
};

// ============================================================
// FM OPERATOR
// ============================================================

struct FmOperator : Module {

	static constexpr float PI2 = 2.f * M_PI;

	static constexpr float FM_OCTAVES   = 2.0f;
	static constexpr float PHASE_MOD_DEPTH = 0.25f;
	static constexpr float DC_CUTOFF    = 20.f;
	static constexpr float FINAL_DC_CUTOFF = 5.f;

	static constexpr float SUM_OUTPUT_SCALE = 5.f;
	static constexpr float MACRO_CV_SCALE = 0.1f;

	static constexpr float TURB_MIN_RATE = 0.5f;
	static constexpr float TURB_MAX_RATE = 12.0f;

	static constexpr float MACRO_SMOOTH_TIME  = 0.008f;
	static constexpr float FERAL_SMOOTH_TIME  = 0.012f;

	static constexpr float FERAL_FEEDBACK_MAX = 0.42f;

	static constexpr float DARK_CUTOFF_MAX = 18000.f;
	static constexpr float DARK_CUTOFF_MIN = 1800.f;

	static constexpr float DENORM = 1.0e-20f;
	static constexpr float PITCH_BUS_MAX_OCTAVES = 4.f;
	static constexpr float FM_PHASE_MAX = 8.f;
	static constexpr float INTERNAL_ENV_TAU = 0.20f;
	static constexpr float TRI_LEAK_TAU = 0.5f;
	static constexpr float OCT_SLEW_TAU = 0.004f;

	static constexpr float MIN_BIT_DEPTH = 1.f;
	static constexpr float MAX_BIT_DEPTH = 16.f;
	static constexpr float MIN_SRR       = 1.f;
	static constexpr float MAX_SRR       = 64.f;

	static constexpr float FM_INDEX_HARD_CLAMP = 30.f;
	static constexpr float FM_INDEX_POST_SAT_CLAMP = 6.f;
	static constexpr float FM_FREQ_SOFT_NYQ = 0.50f;

	static constexpr int WAVE_BUF_SIZE      = 512;
	static constexpr int WAVE_SNAPSHOT_SIZE = 256;
	static constexpr int SNAP_INTERVAL      = 3840;

	static constexpr int FB_TAPS = 4;
	static constexpr float DARK_SUB_MAX_GAIN = 0.5f;

	static constexpr float FM_SYNC_RATIO   = 2.5f;
	static constexpr float FM_FORMANT_AM   = 4.f;

	enum ModPitchRange {
		MOD_PITCH_RANGE_SMALL  = 0,
		MOD_PITCH_RANGE_MEDIUM = 1,
		MOD_PITCH_RANGE_LARGE  = 2,
	};

	std::atomic<int> modPitchRangePM{ MOD_PITCH_RANGE_LARGE };

	static float modPitchRangeMax(int r) {
		switch (r) {
			case MOD_PITCH_RANGE_SMALL:  return 2.f;
			case MOD_PITCH_RANGE_MEDIUM: return 4.f;
			default:                     return 6.f;
		}
	}

	float getModPitchMax() const {
		return modPitchRangeMax(modPitchRangePM.load(std::memory_order_relaxed));
	}
	float getModPitchMin() const { return -getModPitchMax(); }

	enum FmMode { FM_MODE_EXP = 0, FM_MODE_LINEAR = 1, FM_MODE_PHASE = 2 };
	enum FmRouting { FM_ROUTE_GLOBAL = 0, FM_ROUTE_SELF = 1, FM_ROUTE_CHAIN = 2 };
	enum FmSync { FM_SYNC_OFF = 0, FM_SYNC_SOFT = 1, FM_SYNC_HARD = 2, FM_SYNC_REVERSE = 3 };
	enum TurbulenceMode { TURB_LORENZ = 0, TURB_ENVELOPE = 1, TURB_RHYTHMIC = 2 };
	enum PitchBusMode { PITCH_BUS_OFF = 0, PITCH_BUS_BIPOLAR = 1,
	                    PITCH_BUS_QUANTIZE = 2, PITCH_BUS_GLIDE = 3, PITCH_BUS_BOTH = 4 };

	// Sub-oscillator frequency modes
	enum SubFreqMode {
		SUB_FREQ_OCTAVE   = 0,
		SUB_FREQ_DIV3     = 1,
		SUB_FREQ_DIV4     = 2,
		SUB_FREQ_DIV5     = 3,
		SUB_FREQ_DIV7     = 4,
		SUB_FREQ_UNISON   = 5,
		SUB_FREQ_FIXED    = 6,
		SUB_FREQ_MODRATIO = 7,
	};

	// Sub-oscillator FM routing
	enum SubFmRoute {
		SUB_ROUTE_LAYERED   = 0,
		SUB_ROUTE_MODULATOR = 1,
		SUB_ROUTE_CARRIER   = 2,
		SUB_ROUTE_OFF       = 3,
	};

	enum SineMorphShape {
		SM_SINE = 0,
		SM_FOLDED,
		SM_CHEBYSHEV,
		SM_ASYMMETRIC,
		SM_SYNC,
		SM_FORMANT,
		SM_COUNT
	};

	static constexpr int CHAIN_PATTERNS[8][4] = {
		{ 3, 0, 1, 2 }, { 1, 2, 3, 0 }, { 2, 3, 0, 1 }, { 1, 0, 3, 2 },
		{ 0, 1, 2, 3 }, { 2, 0, 3, 1 }, { 3, 1, 0, 2 }, { 0, 3, 2, 1 },
	};

	struct LorenzMotion {
		float x = 0.1f, y = 0.f, z = 0.f;
		float speed = 1.f;

		float step(float dt) {
			const float sigma = 10.f, rho = 28.f, beta = 8.f / 3.f;
			const float dx = sigma * (y - x);
			const float dy = x * (rho - z) - y;
			const float dz = x * y - beta * z;
			x += dx * dt * speed;
			y += dy * dt * speed;
			z += dz * dt * speed;
			x = clampSafe(x, -30.f, 30.f);
			y = clampSafe(y, -30.f, 30.f);
			z = clampSafe(z, 0.f, 60.f);
			return clampSafe(x / 20.f, -1.f, 1.f);
		}
		void reset() { x = 0.1f; y = 0.f; z = 0.f; speed = 1.f; }
		static float clampSafe(float v, float lo, float hi) {
			return (v < lo) ? lo : ((v > hi) ? hi : v);
		}
	};

	struct TurbulenceState {
		LorenzMotion lorenz;
		float envelope = 0.f, envelopeTarget = 0.f, phase = 0.f, lastGate = 0.f;
		void reset() {
			lorenz.reset();
			envelope = 0.f; envelopeTarget = 0.f; phase = 0.f; lastGate = 0.f;
		}
	};

	float phaseSine = 0.f, phaseSaw = 0.f, phaseTriangle = 0.f, phaseSquare = 0.f;
	float subPhase = 0.f;
	float prevPhaseSine = 0.f;
	float prevRawSine = 0.f;
	float modPhase = 0.f;
	float modPitchSmooth = 0.f;
	bool  lastSyncWrap = false;

	TurbulenceState turbState;
	float internalEnvelope = 0.f;

	float psySmooth = 0.f, darkSmooth = 0.f, motionSmooth = 0.f, feralSmooth = 0.f;
	float feralFeedback = 0.f;
	float darkFilterState = 0.f;
	float darkCoeffCached = 0.f, prevDarkForCoeff = -1.f;

	float audioDcX = 0.f, audioDcY = 0.f;
	float foldDcX = 0.f,  foldDcY = 0.f;
	float finalDcX = 0.f, finalDcY = 0.f;
	float finalDcCoeff = std::exp(-2.f * M_PI * FINAL_DC_CUTOFF / 48000.f);
	float finalDcSr = 0.f;
	float dcCoeff = 0.995f;

	float prevSine = 0.f, prevSaw = 0.f, prevTriangle = 0.f, prevSquare = 0.f;
	float pitchBusGlideState = 0.f;
	float srrCounter = 0.f, srrHold = 0.f;
	float outDriveSmooth = 0.f, pdSmooth = 0.f;
	float sineOctSmooth = 0.f, sawOctSmooth = 0.f, triOctSmooth = 0.f, sqOctSmooth = 0.f;
	float fmEnvFollower = 0.f;

	float fbHistorySine[FB_TAPS]   = {0.f};
	float fbHistorySaw[FB_TAPS]    = {0.f};
	float fbHistoryTri[FB_TAPS]    = {0.f};
	float fbHistorySquare[FB_TAPS] = {0.f};

	float fbAP_z_sine = 0.f, fbAP_z_saw = 0.f, fbAP_z_tri = 0.f, fbAP_z_sqr = 0.f;

	std::atomic<uint32_t> chainDepthPM[4];
	std::atomic<uint32_t> displayChainPacked;

	float waveBufSine[WAVE_BUF_SIZE]   = {0.f};
	float waveBufSaw[WAVE_BUF_SIZE]    = {0.f};
	float waveBufTri[WAVE_BUF_SIZE]    = {0.f};
	float waveBufSquare[WAVE_BUF_SIZE] = {0.f};
	float waveBufSum[WAVE_BUF_SIZE]    = {0.f};
	int   waveIdx = 0;

	float snapSine[WAVE_SNAPSHOT_SIZE]   = {0.f};
	float snapSaw[WAVE_SNAPSHOT_SIZE]    = {0.f};
	float snapTri[WAVE_SNAPSHOT_SIZE]    = {0.f};
	float snapSquare[WAVE_SNAPSHOT_SIZE] = {0.f};
	float snapSum[WAVE_SNAPSHOT_SIZE]    = {0.f};
	int   snapCounter = 0;

	// LEGACY PARAMETER IDS -- DO NOT REORDER / INSERT BEFORE PARAMS_LEN
	enum ParamId {
		PITCH_PARAM_SINE, PITCH_PARAM_SAW, PITCH_PARAM_TRIANGLE, PITCH_PARAM_SQUARE,
		FM_PARAM,
		SINE_WAVESHAPER_PARAM,
		SAW_WAVESHAPER_PARAM, PSYCHEDELIC_PARAM_TRIANGLE, PSYCHEDELIC_CV_KNOB_PARAM,
		FM_AMOUNT_PARAM,
		VOLUME_PARAM_SINE, VOLUME_PARAM_SAW, VOLUME_PARAM_TRIANGLE, VOLUME_PARAM_SQUARE,
		RESAMPLE, SAW_WAVESHAPER_AMOUNT, SQUARE_WAVESHAPER_PARAM, SQUARE_WAVESHAPER_AMOUNT,
		TRIANGLE_WAVESHAPER_PARAM, TRIANGLE_WAVESHAPER_AMOUNT,
		PHASE_MOD_PARAM_SINE, PHASE_MOD_PARAM_SAW, PHASE_MOD_PARAM_TRIANGLE, PHASE_MOD_PARAM_SQUARE,
		CV_MODE_PARAM, FM_MODE_PARAM, FM_ROUTING_PARAM,
		HARMONIC_SUM_PARAM,
		PSY_MACRO_PARAM, DARK_MACRO_PARAM, MOTION_MACRO_PARAM, FERAL_MACRO_PARAM,
		TURBULENCE_MODE_PARAM,
		PITCH_BUS_DEPTH_SINE, PITCH_BUS_DEPTH_SAW, PITCH_BUS_DEPTH_TRIANGLE, PITCH_BUS_DEPTH_SQUARE,
		PITCH_BUS_MODE_PARAM, PITCH_STEP_MODE_PARAM,
		PHASE_DIST_PARAM, BIT_DEPTH_PARAM, SRR_RATE_PARAM, OUT_DRIVE_PARAM,
		FM_SYNC_PARAM, FM_ENV_PARAM, FM_CHAIN_PATTERN_PARAM, FM_SYNC_STRENGTH_PARAM,
		FM_INTERNAL_SCALE_PARAM, FM_LEGACY_STATIC_PARAM, FM_INTERNAL_AMT_PARAM,
		FM_MOD_RATIO_PARAM, FM_MOD_WAVE_PARAM,
		DARK_SUB_LEVEL_PARAM, MASTER_MACRO_PARAM,
		FM_MOD_PITCH_PARAM, FM_MOD_PSY_CV_DEPTH_PARAM,
		FM_MOD_MORPH_PARAM,
		// === NEW SUB-OSCILLATOR PARAMS (appended for backward compatibility) ===
		SUB_WAVE_PARAM,
		SUB_FREQ_MODE_PARAM,
		SUB_PITCH_PARAM,
		SUB_PHASE_PARAM,
		SUB_FM_ROUTE_PARAM,
		SUB_RING_PARAM,
		SUB_DRIFT_PARAM,
		PARAMS_LEN
	};

	enum InputId {
		PITCH_INPUT_SINE, PITCH_INPUT_SAW, PITCH_INPUT_TRIANGLE, PITCH_INPUT_SQUARE,
		FM_INPUT, FM_AMOUNT_INPUT,
		PSYCHEDELIC_CV_INPUT_SAW, PSYCHEDELIC_CV_INPUT_FOR_All, PSYCHEDELIC_CV_INPUT_TRIANGLE,
		RESAMPLE_INPUT, PITCH_INPUT_ALL, PSYCHEDELIC_CV_INPUT_SQUARE,
		PHASE_MOD_INPUT_SINE, PHASE_MOD_INPUT_SAW, PHASE_MOD_INPUT_TRIANGLE, PHASE_MOD_INPUT_SQUARE,
		PSY_MACRO_CV_INPUT, DARK_MACRO_CV_INPUT, MOTION_MACRO_CV_INPUT, FERAL_MACRO_CV_INPUT,
		INPUTS_LEN
	};

	enum OutputId {
		SINE_OUTPUT, SAW_OUTPUT, TRIANGLE_OUTPUT, SQUARE_OUTPUT,
		FINAL_OUTPUT, SUM_OUTPUT, OUTPUTS_LEN
	};

	enum LightId {
		FINAL_OUTPUT_LIGHT, LED_LIGHT, LED_RED_LIGHT, HEXAGRAM_LIGHT, FM_AMOUNT_LIGHT,
		PITCH_STEP_MODE_LIGHT,
		CV_MODE_LIGHT_0, CV_MODE_LIGHT_1, CV_MODE_LIGHT_2,
		LIGHTS_LEN
	};

	float getChainDepth(int modIdx) const {
		if (modIdx < 0 || modIdx > 3) return 1.f;
		return chainDepthPM[modIdx].load(std::memory_order_relaxed) / 1000.f;
	}
	void setChainDepth(int modIdx, float v) {
		if (modIdx < 0 || modIdx > 3) return;
		v = clamp(v, 0.f, 1.f);
		chainDepthPM[modIdx].store((uint32_t)(v * 1000.f + 0.5f), std::memory_order_relaxed);
	}

	void setModPitchRange(int rangeIdx) {
		rangeIdx = clamp(rangeIdx, (int)MOD_PITCH_RANGE_SMALL, (int)MOD_PITCH_RANGE_LARGE);
		modPitchRangePM.store(rangeIdx, std::memory_order_relaxed);
		const float newMax  = modPitchRangeMax(rangeIdx);
		const float cur     = params[FM_MOD_PITCH_PARAM].getValue();
		const float clamped = clamp(cur, -newMax, newMax);
		if (auto* pq = paramQuantities[FM_MOD_PITCH_PARAM]) {
			pq->minValue = -newMax;
			pq->maxValue =  newMax;
			pq->setValue(clamped);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_t* arr = json_array();
		for (int i = 0; i < 4; i++) {
			json_array_append_new(arr, json_integer((json_int_t)chainDepthPM[i].load()));
		}
		json_object_set_new(rootJ, "chainDepthPM", arr);
		json_object_set_new(rootJ, "modPitchRange",
			json_integer((json_int_t)modPitchRangePM.load(std::memory_order_relaxed)));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* arr = json_object_get(rootJ, "chainDepthPM");
		if (arr) {
			for (int i = 0; i < 4; i++) {
				json_t* v = json_array_get(arr, i);
				if (v) {
					int32_t pm = (int32_t)json_integer_value(v);
					pm = clamp(pm, 0, 1000);
					chainDepthPM[i].store((uint32_t)pm);
				}
			}
		}
		json_t* rangeJ = json_object_get(rootJ, "modPitchRange");
		if (rangeJ) setModPitchRange((int)json_integer_value(rangeJ));
	}

	FmOperator() : displayChainPacked(0u) {
		for (int i = 0; i < 4; i++) chainDepthPM[i].store(1000u);

		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configParam(PITCH_PARAM_SINE,     -4.f, 4.f, 0.f, "Sine Pitch");
		configParam(PITCH_PARAM_SAW,      -4.f, 4.f, 0.f, "Saw Pitch");
		configParam(PITCH_PARAM_TRIANGLE, -4.f, 4.f, 0.f, "Triangle Pitch");
		configParam(PITCH_PARAM_SQUARE,   -4.f, 4.f, 0.f, "Square Pitch");

		configParam(FM_PARAM,        -1.2f, 3.23f, 0.f, "FM Index");
		configParam(FM_AMOUNT_PARAM, -0.5f, 0.6f,  0.f, "FM CV Amount");

		configParam(SINE_WAVESHAPER_PARAM, 0.f, 1.f, 0.f, "FM Source Morph");

		configParam(SAW_WAVESHAPER_PARAM,        -0.5f, 1.f, 0.f, "Saw Shape");
		configParam(PSYCHEDELIC_PARAM_TRIANGLE,  -0.5f, 1.f, 0.f, "Triangle Psychedelic");
		configParam(SAW_WAVESHAPER_AMOUNT,       -0.5f, 1.f, 0.f, "Saw Shape CV Amount");
		configParam(TRIANGLE_WAVESHAPER_PARAM,   -0.5f, 1.f, 0.f, "Triangle Shape");
		configParam(TRIANGLE_WAVESHAPER_AMOUNT,  -0.5f, 1.f, 0.f, "Triangle Shape CV Amount");
		configParam(SQUARE_WAVESHAPER_PARAM,     -0.5f, 1.f, 0.f, "Square Shape");
		configParam(SQUARE_WAVESHAPER_AMOUNT,    -0.5f, 1.f, 0.f, "Square Shape CV Amount");
		configParam(PSYCHEDELIC_CV_KNOB_PARAM,   -0.5f, 1.f, 0.f, "Psychedelic CV");

		configParam(VOLUME_PARAM_SINE,     0.f, 1.f, 1.f, "Sine Volume");
		configParam(VOLUME_PARAM_SAW,      0.f, 1.f, 1.f, "Saw Volume");
		configParam(VOLUME_PARAM_TRIANGLE, 0.f, 1.f, 1.f, "Triangle Volume");
		configParam(VOLUME_PARAM_SQUARE,   0.f, 1.f, 1.f, "Square Volume");

		configParam(RESAMPLE,           0.f, 0.8f, 0.f, "Hard Fold Mix");
		configParam(HARMONIC_SUM_PARAM, 0.f, 1.f,  0.f, "Harmonic Sum");

		configParam(PHASE_MOD_PARAM_SINE,     -2.f, 2.f, 0.f, "Sine Phase Mod");
		configParam(PHASE_MOD_PARAM_SAW,      -2.f, 2.f, 0.f, "Saw Phase Mod");
		configParam(PHASE_MOD_PARAM_TRIANGLE, -2.f, 2.f, 0.f, "Triangle Phase Mod");
		configParam(PHASE_MOD_PARAM_SQUARE,   -2.f, 2.f, 0.f, "Square Phase Mod");

		configSwitch(CV_MODE_PARAM, 0.f, 2.f, 0.f, "CV Mode",
		             {"Bipolar (-5V..+5V)", "Unipolar (0V..+5V)", "Reverse (-5V..+5V inverted)"});

		configParam(FM_MODE_PARAM,    0.f, 2.f, 0.f, "FM Mode (Exp/Lin/Phase)");
		configParam(FM_ROUTING_PARAM, 0.f, 2.f, 0.f, "FM Routing (Global/Self/Chain)");

		configSwitch(FM_SYNC_PARAM, 0.f, 3.f, 0.f, "FM Sync",
		             {"Off", "Soft", "Hard", "Reverse"});
		configParam(FM_ENV_PARAM, 0.f, 1.f, 0.f, "FM Envelope Follower", "%", 0.f, 100.f);
		configSwitch(FM_CHAIN_PATTERN_PARAM, 0.f, 7.f, 0.f, "FM Chain Pattern",
		             {"A","B","C","D","E","F","G","H"});
		configParam(FM_SYNC_STRENGTH_PARAM, 0.f, 1.f, 1.f, "FM Sync Strength", "%", 0.f, 100.f);

		configParam(PSY_MACRO_PARAM,    0.f, 1.f, 0.f, "PSY Macro");
		configParam(DARK_MACRO_PARAM,   0.f, 1.f, 0.f, "DARK Macro");
		configParam(MOTION_MACRO_PARAM, 0.f, 1.f, 0.f, "TURBULENCE Amount");
		configParam(FERAL_MACRO_PARAM,  0.f, 1.f, 0.f, "FERAL Macro");
		configParam(TURBULENCE_MODE_PARAM, 0.f, 2.f, 0.f,
		            "Turbulence Mode (Chaos/Envelope/Rhythm)");

		configParam(PITCH_BUS_DEPTH_SINE,     -1.f, 1.f, 0.f, "Pitch Bus Depth (Sine)",     "%", 0.f, 100.f);
		configParam(PITCH_BUS_DEPTH_SAW,      -1.f, 1.f, 0.f, "Pitch Bus Depth (Saw)",      "%", 0.f, 100.f);
		configParam(PITCH_BUS_DEPTH_TRIANGLE, -1.f, 1.f, 0.f, "Pitch Bus Depth (Triangle)", "%", 0.f, 100.f);
		configParam(PITCH_BUS_DEPTH_SQUARE,   -1.f, 1.f, 0.f, "Pitch Bus Depth (Square)",   "%", 0.f, 100.f);
		configSwitch(PITCH_BUS_MODE_PARAM, 0.f, 4.f, 1.f, "Pitch Bus Mode",
		             {"Off", "Bipolar", "Quantize", "Glide", "Both"});
		configButton(PITCH_STEP_MODE_PARAM, "Freq/OCT (Octave Quantize)");

		configParam(PHASE_DIST_PARAM, 0.f, 1.f, 0.f, "Phase Distortion", "%", 0.f, 100.f);
		configParam(BIT_DEPTH_PARAM, MIN_BIT_DEPTH, MAX_BIT_DEPTH, MAX_BIT_DEPTH, "Bit Depth", " bit");
		configParam(SRR_RATE_PARAM, MIN_SRR, MAX_SRR, MIN_SRR, "Sample Rate Reduction", "x");
		configParam(OUT_DRIVE_PARAM, 0.f, 2.f, 0.f, "Output Drive", "%", 0.f, 50.f);

		configParam(FM_INTERNAL_SCALE_PARAM, 0.f, 1.f, 0.5f,
		            "FM Internal Scale (needs FM Index > 0)", "%", 0.f, 100.f);
		configSwitch(FM_LEGACY_STATIC_PARAM, 0.f, 1.f, 0.f, "Legacy Static Transposition",
		             {"Off (internal modulator)", "On (legacy +5V)"});
		configParam(FM_INTERNAL_AMT_PARAM, 0.f, 1.f, 0.f, "FM Internal Amount", "%", 0.f, 100.f);

		configParam(FM_MOD_RATIO_PARAM, 0.25f, 8.f, 2.f, "Modulator Ratio", ":1");
		configSwitch(FM_MOD_WAVE_PARAM, 0.f, 3.f, 0.f, "Modulator Base Waveform",
		             {"Sine (follows FM Src Morph)", "Triangle", "Saw", "Square"});
		configParam(DARK_SUB_LEVEL_PARAM, 0.f, 1.f, 0.6f, "DARK Sub Level", "%", 0.f, 100.f);
		configParam(MASTER_MACRO_PARAM, 0.f, 1.f, 0.f, "Master Macro (all 4 macros)", "%", 0.f, 100.f);

		const float defMax = modPitchRangeMax(MOD_PITCH_RANGE_LARGE);
		configParam(FM_MOD_PITCH_PARAM, -defMax, defMax, 0.f, "Modulator Pitch Offset", " oct");
		configParam(FM_MOD_PSY_CV_DEPTH_PARAM, -1.f, 1.f, 0.f,
		            "Modulator Pitch via Shape/Pitch-Bus CV", "%", 0.f, 100.f);
		configParam(FM_MOD_MORPH_PARAM, 0.f, 1.f, 0.f,
		            "Modulator Warmth (non-sine base)", "%", 0.f, 100.f);

		// === NEW SUB-OSCILLATOR PARAMS ===
		configSwitch(SUB_WAVE_PARAM, 0.f, 5.f, 1.f, "Sub Waveform",
		             {"Sine", "Triangle", "Saw", "Square", "Fold", "Sync"});
		configSwitch(SUB_FREQ_MODE_PARAM, 0.f, 7.f, 0.f, "Sub Frequency Mode",
		             {"÷2 (Sub-Octave)", "÷3", "÷4", "÷5", "÷7", "Unison",
		              "Fixed Drone (5-80Hz)", "Ratio to Modulator"});
		configParam(SUB_PITCH_PARAM, -2.f, 2.f, 0.f, "Sub Pitch Offset", " oct");
		configParam(SUB_PHASE_PARAM, 0.f, 1.f, 0.f, "Sub Phase Offset", "°", 0.f, 360.f);
		configSwitch(SUB_FM_ROUTE_PARAM, 0.f, 3.f, 0.f, "Sub FM Route",
		             {"Layered Output", "As Modulator", "As Carrier", "Off"});
		configParam(SUB_RING_PARAM, 0.f, 1.f, 0.f, "Sub Ring Mod (with Sine)", "%", 0.f, 100.f);
		configParam(SUB_DRIFT_PARAM, 0.f, 1.f, 0.f, "Sub Drift (Space Atmosphere)", "%", 0.f, 100.f);

		configInput(PITCH_INPUT_ALL,      "Pitch CV for All Oscillators");
		configInput(PITCH_INPUT_SINE,     "Sine Pitch CV");
		configInput(PITCH_INPUT_SAW,      "Saw Pitch CV");
		configInput(PITCH_INPUT_TRIANGLE, "Triangle Pitch CV");
		configInput(PITCH_INPUT_SQUARE,   "Square Pitch CV");
		configInput(FM_INPUT,             "FM CV");
		configInput(FM_AMOUNT_INPUT,      "FM Index CV");
		configInput(PSYCHEDELIC_CV_INPUT_FOR_All,  "Shape/Pitch Bus CV");
		configInput(PSYCHEDELIC_CV_INPUT_SAW,      "Saw Shape CV");
		configInput(PSYCHEDELIC_CV_INPUT_TRIANGLE, "Triangle Shape CV");
		configInput(PSYCHEDELIC_CV_INPUT_SQUARE,   "Square Shape CV");
		configInput(RESAMPLE_INPUT,                "Audio Input");
		configInput(PHASE_MOD_INPUT_SINE,          "Sine Phase Modulation CV");
		configInput(PHASE_MOD_INPUT_SAW,           "Saw Phase Modulation CV");
		configInput(PHASE_MOD_INPUT_TRIANGLE,      "Triangle Phase Modulation CV");
		configInput(PHASE_MOD_INPUT_SQUARE,        "Square Phase Modulation CV");
		configInput(PSY_MACRO_CV_INPUT,            "PSY Macro CV");
		configInput(DARK_MACRO_CV_INPUT,           "DARK Macro CV");
		configInput(MOTION_MACRO_CV_INPUT,         "TURBULENCE CV");
		configInput(FERAL_MACRO_CV_INPUT,          "FERAL Macro CV");

		configOutput(SINE_OUTPUT,     "Sine Output");
		configOutput(SAW_OUTPUT,      "Saw Output");
		configOutput(TRIANGLE_OUTPUT, "Triangle Output");
		configOutput(SQUARE_OUTPUT,   "Square Output");
		configOutput(FINAL_OUTPUT,    "Processed Output");
		configOutput(SUM_OUTPUT,      "Clean Oscillator Sum");
	}

	void onReset() override {
		phaseSine = phaseSaw = phaseTriangle = phaseSquare = 0.f;
		prevPhaseSine = 0.f;
		subPhase = 0.f;
		modPhase = 0.f;
		modPitchSmooth = 0.f;

		turbState.reset();
		internalEnvelope = 0.f;
		psySmooth = darkSmooth = motionSmooth = feralSmooth = 0.f;
		feralFeedback = 0.f;
		darkFilterState = 0.f;
		darkCoeffCached = 0.f;
		prevDarkForCoeff = -1.f;

		audioDcX = audioDcY = 0.f;
		foldDcX  = foldDcY  = 0.f;
		finalDcX = finalDcY = 0.f;

		prevSine = prevSaw = prevTriangle = prevSquare = 0.f;
		prevRawSine = 0.f;
		lastSyncWrap = false;
		pitchBusGlideState = 0.f;
		srrCounter = 0.f;
		srrHold = 0.f;
		outDriveSmooth = 0.f;
		pdSmooth = 0.f;
		sineOctSmooth = 0.f;
		sawOctSmooth = 0.f;
		triOctSmooth = 0.f;
		sqOctSmooth = 0.f;
		fmEnvFollower = 0.f;

		for (int i = 0; i < FB_TAPS; i++) {
			fbHistorySine[i] = fbHistorySaw[i] = 0.f;
			fbHistoryTri[i]  = fbHistorySquare[i] = 0.f;
		}
		fbAP_z_sine = fbAP_z_saw = fbAP_z_tri = fbAP_z_sqr = 0.f;
		displayChainPacked.store(0u, std::memory_order_relaxed);

		for (int i = 0; i < WAVE_BUF_SIZE; i++) {
			waveBufSine[i] = waveBufSaw[i] = 0.f;
			waveBufTri[i]  = waveBufSquare[i] = 0.f;
			waveBufSum[i]  = 0.f;
		}
		for (int i = 0; i < WAVE_SNAPSHOT_SIZE; i++) {
			snapSine[i] = snapSaw[i] = 0.f;
			snapTri[i]  = snapSquare[i] = 0.f;
			snapSum[i]  = 0.f;
		}
		waveIdx = 0;
		snapCounter = 0;

		if (APP && APP->engine) {
			updateDcCoeff(APP->engine->getSampleRate());
		}
	}

	void onSampleRateChange() override {
		updateDcCoeff(APP->engine->getSampleRate());
		prevDarkForCoeff = -1.f;
		srrCounter = 0.f;
		srrHold = 0.f;
	}

	void updateDcCoeff(float sampleRate) {
		if (sampleRate <= 1.f) return;
		dcCoeff = std::exp(-2.f * M_PI * DC_CUTOFF / sampleRate);
		dcCoeff = clamp(dcCoeff, 0.9f, 0.99999f);
		updateFinalDcCoeff(sampleRate);
	}
	void updateFinalDcCoeff(float sampleRate) {
		if (sampleRate <= 1.f) return;
		finalDcCoeff = std::exp(-2.f * M_PI * FINAL_DC_CUTOFF / sampleRate);
		finalDcCoeff = clamp(finalDcCoeff, 0.9f, 0.99999f);
		finalDcSr = sampleRate;
	}

	static inline float flushDenorm(float x) { return (std::abs(x) < DENORM) ? 0.f : x; }
	static float finite(float x, float fallback = 0.f) { return std::isfinite(x) ? x : fallback; }
	static float clampSafe(float x, float lo, float hi) { return clamp(finite(x), lo, hi); }

	static inline float processCvModeVoltage(float voltage, int mode) {
		const float cv = clampSafe(voltage, -5.f, 5.f);
		switch (mode) {
			case 1:  return (cv + 5.f) * 0.5f;
			case 2:  return -cv;
			case 0:
			default: return cv;
		}
	}
	static inline float wrap(float p) {
		if (p >= 0.f && p < 1.f) return p;
		return p - std::floor(p);
	}
	static float macroCurve(float x) {
		x = clampSafe(x, 0.f, 1.f);
		return x * x * (3.f - 2.f * x);
	}
	static inline float lateCurve(float x) {
		x = clampSafe(x, 0.f, 1.f);
		return x * x * x;
	}
	static float macroWithCV(float knob, float cv) {
		knob = clampSafe(knob, 0.f, 1.f);
		cv   = clampSafe(cv,   -10.f, 10.f);
		return clampSafe(knob + cv * MACRO_CV_SCALE, 0.f, 1.f);
	}
	static float phaseMod(float amount, float cv, float feralAmt = 0.f) {
		const float depth = PHASE_MOD_DEPTH * (1.f + 9.f * clampSafe(feralAmt, 0.f, 1.f));
		const float offset = amount * clampSafe(cv, -10.f, 10.f) * depth;
		return clampSafe(offset, -6.f, 6.f);
	}
	static inline float phaseDistort(float phase, float amount) {
		if (amount < 0.001f) return wrap(phase);
		const float distorted = phase + amount * 0.5f * std::sin(PI2 * phase);
		return wrap(distorted);
	}
	static inline float advance(float phase, float freq, float sr) {
		freq = clampSafe(freq, -sr * 0.49f, sr * 0.49f);
		return wrap(phase + freq / sr);
	}
	static float onePoleCoeff(float cutoff, float sr) {
		cutoff = clampSafe(cutoff, 1.f, sr * 0.45f);
		const float c = 1.f - std::exp(-2.f * M_PI * cutoff / sr);
		return clampSafe(c, 0.0001f, 1.f);
	}
	float smoothMacro(float current, float target, float dt,
	                  float tau = MACRO_SMOOTH_TIME) const {
		const float t = clampSafe(dt / tau, 0.f, 1.f);
		const float coeff = 1.f - std::exp(-6.907755f * t);
		return current + (target - current) * coeff;
	}
	static float rateIndependentSmooth(float current, float target, float dt, float tau) {
		tau = clampSafe(tau, 1e-5f, 10.f);
		const float coeff = 1.f - std::exp(-dt / tau);
		return current + (target - current) * coeff;
	}
	static inline float multiTapAverage(const float* hist, int taps) {
		float s = 0.f;
		for (int i = 0; i < taps; i++) s += hist[i];
		return s / (float)taps;
	}
	static inline void pushFeedback(float* hist, int taps, float v) {
		for (int i = taps - 1; i > 0; i--) hist[i] = hist[i-1];
		hist[0] = clampSafe(v, -1.5f, 1.5f);
	}
	static inline float allpassStep(float x, float& z, float coef) {
		const float y = -coef * x + z;
		z = x + coef * y;
		z = flushDenorm(z);
		return y;
	}
	static inline float softSaturateFM(float x) {
		constexpr float KNEE = 5.f, K = 2.f;
		const float ax = std::abs(x);
		if (ax < KNEE) return x;
		const float sign = (x >= 0.f) ? 1.f : -1.f;
		const float a = ax - KNEE;
		return sign * (KNEE + K * std::tanh(a / K));
	}
	static inline float syncSlave(float slavePhase, float masterPhase,
	                              float masterPrev, int mode, float strength,
	                              bool masterWrappedRecently) {
		if (mode == FM_SYNC_OFF) return slavePhase;
		strength = clampSafe(strength, 0.f, 1.f);
		if (strength < 0.001f) return slavePhase;
		if (!masterWrappedRecently) return slavePhase;
		float target = slavePhase;
		switch (mode) {
			case FM_SYNC_HARD: target = 0.5f; break;
			case FM_SYNC_SOFT:
				target = (slavePhase > 0.5f) ? 0.5f : slavePhase * 0.5f;
				break;
			case FM_SYNC_REVERSE: target = wrap(1.f - slavePhase); break;
			default: return slavePhase;
		}
		(void)masterPhase;
		(void)masterPrev;
		return wrap(slavePhase + (target - slavePhase) * strength);
	}

	static const char* shortParamName(int paramId) {
		switch (paramId) {
			case PITCH_STEP_MODE_PARAM: return "PITCH MODE";
			case PITCH_BUS_MODE_PARAM:  return "BUS MODE";
			case FM_MODE_PARAM:         return "FM MODE";
			case FM_ROUTING_PARAM:      return "ROUTING";
			case FM_SYNC_PARAM:         return "FM SYNC";
			case FM_ENV_PARAM:          return "FM ENV";
			case FM_CHAIN_PATTERN_PARAM:return "FM CHAIN";
			case FM_SYNC_STRENGTH_PARAM:return "SYNC AMT";
			case TURBULENCE_MODE_PARAM: return "TURB MODE";
			case CV_MODE_PARAM:         return "CV MODE";
			case PITCH_PARAM_SINE:      return "SINE";
			case PITCH_PARAM_SAW:       return "SAW";
			case PITCH_PARAM_TRIANGLE:  return "TRI";
			case PITCH_PARAM_SQUARE:    return "SQR";
			case FM_PARAM:              return "FM IDX";
			case FM_AMOUNT_PARAM:       return "FM AMT";
			case SINE_WAVESHAPER_PARAM: return "FM SRC MORPH";
			case SAW_WAVESHAPER_PARAM:  return "SAW SHAPE";
			case TRIANGLE_WAVESHAPER_PARAM: return "TRI SHAPE";
			case SQUARE_WAVESHAPER_PARAM: return "SQR SHAPE";
			case SAW_WAVESHAPER_AMOUNT: return "SAW CV";
			case TRIANGLE_WAVESHAPER_AMOUNT: return "TRI CV";
			case SQUARE_WAVESHAPER_AMOUNT: return "SQR CV";
			case PSYCHEDELIC_PARAM_TRIANGLE: return "TRI PSY";
			case PSYCHEDELIC_CV_KNOB_PARAM: return "SHAPE CV";
			case VOLUME_PARAM_SINE:     return "SINE VOL";
			case VOLUME_PARAM_SAW:      return "SAW VOL";
			case VOLUME_PARAM_TRIANGLE: return "TRI VOL";
			case VOLUME_PARAM_SQUARE:   return "SQR VOL";
			case RESAMPLE:              return "FOLD MIX";
			case HARMONIC_SUM_PARAM:    return "HARMONIC";
			case PHASE_MOD_PARAM_SINE:  return "SINE PM";
			case PHASE_MOD_PARAM_SAW:   return "SAW PM";
			case PHASE_MOD_PARAM_TRIANGLE: return "TRI PM";
			case PHASE_MOD_PARAM_SQUARE: return "SQR PM";
			case PSY_MACRO_PARAM:       return "PSY";
			case DARK_MACRO_PARAM:      return "DARK";
			case MOTION_MACRO_PARAM:    return "MOTION";
			case FERAL_MACRO_PARAM:     return "FERAL";
			case PITCH_BUS_DEPTH_SINE:  return "BUS SINE";
			case PITCH_BUS_DEPTH_SAW:   return "BUS SAW";
			case PITCH_BUS_DEPTH_TRIANGLE: return "BUS TRI";
			case PITCH_BUS_DEPTH_SQUARE: return "BUS SQR";
			case PHASE_DIST_PARAM:      return "PHASE DIST";
			case BIT_DEPTH_PARAM:       return "BIT DEPTH";
			case SRR_RATE_PARAM:        return "SRR";
			case OUT_DRIVE_PARAM:       return "OUT DRIVE";
			case FM_INTERNAL_SCALE_PARAM: return "INT SCALE";
			case FM_LEGACY_STATIC_PARAM: return "LEGACY ST";
			case FM_INTERNAL_AMT_PARAM: return "INT AMT";
			case FM_MOD_RATIO_PARAM:    return "MOD RATIO";
			case FM_MOD_WAVE_PARAM:     return "MOD WAVE";
			case DARK_SUB_LEVEL_PARAM:  return "SUB LEVEL";
			case MASTER_MACRO_PARAM:    return "MASTER";
			case FM_MOD_PITCH_PARAM:    return "MOD PITCH";
			case FM_MOD_PSY_CV_DEPTH_PARAM: return "MOD CV";
			case FM_MOD_MORPH_PARAM:    return "MOD WARMTH";
			case SUB_WAVE_PARAM:        return "SUB WAVE";
			case SUB_FREQ_MODE_PARAM:   return "SUB FREQ";
			case SUB_PITCH_PARAM:       return "SUB PITCH";
			case SUB_PHASE_PARAM:       return "SUB PHASE";
			case SUB_FM_ROUTE_PARAM:    return "SUB ROUTE";
			case SUB_RING_PARAM:        return "SUB RING";
			case SUB_DRIFT_PARAM:       return "SUB DRIFT";
			default:                    return "PARAM";
		}
	}

	std::string shortValueText(int paramId, float value, bool pitchStepMode) const {
		switch (paramId) {
			case PITCH_STEP_MODE_PARAM: return value > 0.5f ? "OCT" : "FREQ";
			case PITCH_BUS_MODE_PARAM: {
				const char* names[] = {"OFF", "BIP", "QNT", "GLD", "BTH"};
				return names[clamp((int)std::round(value), 0, 4)];
			}
			case FM_MODE_PARAM: {
				const char* names[] = {"EXP", "LIN", "PHS"};
				return names[clamp((int)std::round(value), 0, 2)];
			}
			case FM_ROUTING_PARAM: {
				const char* names[] = {"GLB", "SLF", "CHN"};
				return names[clamp((int)std::round(value), 0, 2)];
			}
			case FM_SYNC_PARAM: {
				const char* names[] = {"OFF", "SFT", "HRD", "REV"};
				return names[clamp((int)std::round(value), 0, 3)];
			}
			case FM_ENV_PARAM: return string::f("%.0f%%", value * 100.f);
			case FM_CHAIN_PATTERN_PARAM: {
				const char* names[] = {"A","B","C","D","E","F","G","H"};
				return names[clamp((int)std::round(value), 0, 7)];
			}
			case FM_SYNC_STRENGTH_PARAM: return string::f("%.0f%%", value * 100.f);
			case TURBULENCE_MODE_PARAM: {
				const char* names[] = {"CHA", "ENV", "RTH"};
				return names[clamp((int)std::round(value), 0, 2)];
			}
			case CV_MODE_PARAM: {
				const char* names[] = {"BIP", "UNI", "REV"};
				return names[clamp((int)std::round(value), 0, 2)];
			}
			case PITCH_PARAM_SINE:
			case PITCH_PARAM_SAW:
			case PITCH_PARAM_TRIANGLE:
			case PITCH_PARAM_SQUARE:
				if (pitchStepMode) {
					return string::f("%+d oct", (int)std::round(value));
				} else {
					const float hz = dsp::FREQ_C4 * std::exp2(value);
					if (hz >= 1000.f) return string::f("%.2fk", hz / 1000.f);
					return string::f("%.0fHz", hz);
				}
			case FM_PARAM:        return string::f("%.3f", value);
			case FM_AMOUNT_PARAM: return string::f("%+.0f%%", value * 100.f);
			case SINE_WAVESHAPER_PARAM: {
				static const char* morphNames[] = {
					"SINE", "FOLD", "CHEB", "ASYM", "SYNC", "FORM"
				};
				const float pos = clamp(value, 0.f, 1.f) * 5.f;
				const int idx = clamp((int)std::round(pos), 0, 5);
				return string::f("%s %.0f%%", morphNames[idx], value * 100.f);
			}
			case SAW_WAVESHAPER_PARAM:
			case TRIANGLE_WAVESHAPER_PARAM:
			case SQUARE_WAVESHAPER_PARAM:
			case PSYCHEDELIC_PARAM_TRIANGLE:
			case PSYCHEDELIC_CV_KNOB_PARAM:
				return string::f("%.2f", value);
			case SAW_WAVESHAPER_AMOUNT:
			case TRIANGLE_WAVESHAPER_AMOUNT:
			case SQUARE_WAVESHAPER_AMOUNT:
				return string::f("%+.0f%%", value * 100.f);
			case VOLUME_PARAM_SINE:
			case VOLUME_PARAM_SAW:
			case VOLUME_PARAM_TRIANGLE:
			case VOLUME_PARAM_SQUARE:
			case PSY_MACRO_PARAM:
			case DARK_MACRO_PARAM:
			case MOTION_MACRO_PARAM:
			case FERAL_MACRO_PARAM:
			case RESAMPLE:
			case HARMONIC_SUM_PARAM:
			case PHASE_DIST_PARAM:
				return string::f("%.0f%%", value * 100.f);
			case PHASE_MOD_PARAM_SINE:
			case PHASE_MOD_PARAM_SAW:
			case PHASE_MOD_PARAM_TRIANGLE:
			case PHASE_MOD_PARAM_SQUARE:
				return string::f("%+.2f", value);
			case PITCH_BUS_DEPTH_SINE:
			case PITCH_BUS_DEPTH_SAW:
			case PITCH_BUS_DEPTH_TRIANGLE:
			case PITCH_BUS_DEPTH_SQUARE:
			case FM_MOD_PSY_CV_DEPTH_PARAM:
				return string::f("%+.0f%%", value * 100.f);
			case BIT_DEPTH_PARAM:   return string::f("%.1f bit", value);
			case SRR_RATE_PARAM:    return string::f("%.2fx", value);
			case OUT_DRIVE_PARAM:   return string::f("%.0f%%", value * 50.f);
			case FM_INTERNAL_SCALE_PARAM: return string::f("%.0f%%", value * 100.f);
			case FM_LEGACY_STATIC_PARAM:  return value > 0.5f ? "LEGACY" : "MODERN";
			case FM_INTERNAL_AMT_PARAM:   return string::f("%.0f%%", value * 100.f);
			case FM_MOD_RATIO_PARAM:      return string::f("%.2f:1", value);
			case FM_MOD_WAVE_PARAM: {
				const char* names[] = {"SIN", "TRI", "SAW", "SQR"};
				return names[clamp((int)std::round(value), 0, 3)];
			}
			case DARK_SUB_LEVEL_PARAM:
			case MASTER_MACRO_PARAM:
			case FM_MOD_MORPH_PARAM:
				return string::f("%.0f%%", value * 100.f);
			case FM_MOD_PITCH_PARAM: {
				const float hz = dsp::FREQ_C4 * std::exp2(value);
				if (hz >= 1000.f) return string::f("%.2fk", hz / 1000.f);
				return string::f("%.0fHz", hz);
			}
			case SUB_WAVE_PARAM: {
				const char* names[] = {"SINE", "TRI", "SAW", "SQR", "FOLD", "SYNC"};
				return names[clamp((int)std::round(value), 0, 5)];
			}
			case SUB_FREQ_MODE_PARAM: {
				const char* names[] = {"÷2", "÷3", "÷4", "÷5", "÷7", "UNI", "FIX", "MOD"};
				return names[clamp((int)std::round(value), 0, 7)];
			}
			case SUB_PITCH_PARAM: return string::f("%+.2f oct", value);
			case SUB_PHASE_PARAM: return string::f("%.0f°", value * 360.f);
			case SUB_FM_ROUTE_PARAM: {
				const char* names[] = {"LAYER", "MOD", "CARRIER", "OFF"};
				return names[clamp((int)std::round(value), 0, 3)];
			}
			case SUB_RING_PARAM:
			case SUB_DRIFT_PARAM:
				return string::f("%.0f%%", value * 100.f);
			default: return string::f("%.2f", value);
		}
	}

	float computeTurbulence(TurbulenceState& t, float internalEnv,
	                        float macro, int mode, float sr, float dt) {
		macro = clampSafe(macro, 0.f, 1.f);
		switch (mode) {
			case TURB_LORENZ: {
				t.lorenz.speed = 1.2f + 1.6f * macro;
				return t.lorenz.step(dt);
			}
			case TURB_ENVELOPE: {
				t.envelopeTarget = clampSafe(internalEnv, 0.f, 1.f);
				const float attackTime  = 0.001f + 0.05f * (1.f - macro);
				const float releaseTime = 0.020f + 0.30f * macro;
				const float tau = (t.envelopeTarget > t.envelope)
					? attackTime : releaseTime;
				t.envelope = rateIndependentSmooth(t.envelope, t.envelopeTarget, dt, tau);
				t.envelope = flushDenorm(t.envelope);
				return clampSafe(t.envelope * 2.f - 1.f, -1.f, 1.f);
			}
			case TURB_RHYTHMIC:
			default: {
				const float rate = TURB_MIN_RATE + (TURB_MAX_RATE - TURB_MIN_RATE) * macro;
				t.phase = wrap(t.phase + rate / sr);
				const float duty = 0.3f + 0.4f * macro;
				const float gate = (t.phase < duty) ? 1.f : 0.f;
				t.lastGate = rateIndependentSmooth(t.lastGate, gate, dt, 0.020f);
				t.lastGate = flushDenorm(t.lastGate);
				return clampSafe(t.lastGate * 2.f - 1.f, -1.f, 1.f);
			}
		}
	}

	static inline float polyBlep(float t, float dt) {
		if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
		if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
		return 0.f;
	}
	static inline float sawBL(float t, float dt) {
		float v = 2.f * t - 1.f;
		v -= polyBlep(t, dt);
		return v;
	}
	static inline float squareBL(float t, float dt) {
		float v = (t < 0.5f) ? -1.f : 1.f;
		v -= polyBlep(t, dt);
		v += polyBlep(wrap(t + 0.5f), dt);
		return v;
	}
	static inline float triangleBL(float t) { return 4.f * std::fabs(t - 0.5f) - 1.f; }
	static inline float pulseBL(float t, float duty, float dt) {
		duty = clampSafe(duty, 0.05f, 0.95f);
		float v = (t < duty) ? -1.f : 1.f;
		v -= polyBlep(t, dt);
		v += polyBlep(wrap(t - duty + 1.f), dt);
		return v - (1.f - 2.f * duty);
	}

	enum Shape { PSYCH_SHAPE, SAW_SHAPE, SQUARE_SHAPE };

	static float shape(float x, float amount, Shape type) {
		x = clampSafe(x, -1.5f, 1.5f);
		amount = clampSafe(amount, -1.5f, 1.5f);
		if (std::abs(amount) < 0.0001f) return x;
		float k;
		switch (type) {
			case SAW_SHAPE:
				k = 0.8f * (std::sin(4.f * M_PI * x) + std::cos(6.f * M_PI * x));
				break;
			case SQUARE_SHAPE:
				k = std::sin(2.f * M_PI * x) + std::cos(2.f * M_PI * x);
				break;
			default:
				k = std::sin(3.f * M_PI * x) + std::cos(5.f * M_PI * x);
				break;
		}
		return clampSafe((1.f - amount) * x + 0.5f * amount * k, -2.f, 2.f);
	}

	static inline float evalFmSourceShape(int idx, float phase,
	                                       float dt, float rawSine) {
		(void)dt;
		switch (idx) {
			case SM_FOLDED:
				return std::tanh(2.4f * rawSine);
			case SM_CHEBYSHEV: {
				const float s3 = std::sin(3.f * PI2 * phase);
				const float s5 = std::sin(5.f * PI2 * phase);
				return clampSafe(0.72f * rawSine + 0.25f * s3 + 0.10f * s5, -1.f, 1.f);
			}
			case SM_ASYMMETRIC: {
				const float s2 = std::sin(2.f * PI2 * phase);
				const float s4 = std::sin(4.f * PI2 * phase);
				return clampSafe(0.85f * rawSine + 0.28f * s2 + 0.10f * s4, -1.f, 1.f);
			}
			case SM_SYNC: {
				const float slave = wrap(FM_SYNC_RATIO * phase);
				return std::sin(PI2 * slave);
			}
			case SM_FORMANT: {
				const float am = 0.5f + 0.5f * std::cos(FM_FORMANT_AM * PI2 * phase);
				return clampSafe(rawSine * am, -1.f, 1.f);
			}
			case SM_SINE:
			default:
				return rawSine;
		}
	}

	static float fmSourceMorph(float phase, float rawSine,
	                           float morph, float dt) {
		morph = clampSafe(morph, 0.f, 1.f);
		const int maxShape = (int)SM_COUNT - 1;
		const float pos = morph * (float)maxShape;
		int i0 = (int)std::floor(pos);
		if (i0 >= maxShape) {
			return evalFmSourceShape(maxShape, phase, dt, rawSine);
		}
		const float frac = pos - (float)i0;
		const float a = evalFmSourceShape(i0,     phase, dt, rawSine);
		const float b = evalFmSourceShape(i0 + 1, phase, dt, rawSine);
		return a + (b - a) * frac;
	}

	static float modulatorMorph(int startShape,
	                            float warmth,
	                            float sineMorphAmt,
	                            float phase, float dt, float rawSine) {
		if (startShape == 0) {
			return fmSourceMorph(phase, rawSine, sineMorphAmt, dt);
		}

		warmth = clampSafe(warmth, 0.f, 1.f);

		float base;
		switch (startShape) {
			case 1: base = triangleBL(phase); break;
			case 2: base = sawBL(phase, dt);  break;
			case 3: base = squareBL(phase, dt); break;
			default: base = rawSine; break;
		}

		if (warmth < 0.001f) return base;

		const float target = std::tanh(2.4f * rawSine);
		return base + (target - base) * warmth;
	}

	static float harmonics(float x, float amount) {
		x = clampSafe(x, -1.f, 1.f);
		amount = clampSafe(amount, 0.f, 1.f);
		if (amount < 0.0001f) return x;
		const float x2 = x * x, x3 = x2 * x, x4 = x2 * x2;
		const float h2 = 2.f * x2 - 1.f;
		const float h3 = 4.f * x3 - 3.f * x;
		const float h4 = 8.f * x4 - 8.f * x2 + 1.f;
		const float h5 = 16.f * x4 * x - 20.f * x2 * x + 5.f * x;
		return clampSafe(
			x + 0.45f * amount * h2
			  + 0.35f * amount * h3
			  + 0.25f * amount * h4
			  + 0.15f * amount * h5, -2.f, 2.f);
	}

	static float fold(float x, float amount) {
		x = clampSafe(x, -2.f, 2.f);
		amount = clampSafe(amount, 0.f, 1.f);
		x *= 1.f + 6.f * amount;
		x = std::fmod(x + 1.f, 4.f);
		if (x < 0.f) x += 4.f;
		if (x > 2.f) x = 4.f - x;
		return clampSafe(x - 1.f, -1.f, 1.f);
	}

	static float asymmetricDrive(float x, float amount) {
		x = clampSafe(x, -1.5f, 1.5f);
		amount = clampSafe(amount, 0.f, 1.f);
		const float asym = 0.30f * amount * x * std::abs(x);
		return clampSafe(x + asym, -1.75f, 1.75f);
	}

	inline float dcBlockAt(float input, float& x1, float& y1, float coeff) {
		input = finite(input);
		const float y = input - x1 + coeff * y1;
		x1 = input;
		y1 = flushDenorm(finite(y));
		return clampSafe(y1, -2.f, 2.f);
	}
	inline float dcBlock(float input, float& x1, float& y1) {
		return dcBlockAt(input, x1, y1, dcCoeff);
	}

	static inline float softLimitFreq(float raw, float limit) {
		const float thresh = 0.85f * limit;
		const float a = std::fabs(raw);
		if (a < thresh) return raw;
		const float sign  = (raw >= 0.f) ? 1.f : -1.f;
		const float range = limit - thresh;
		const float t     = (a - thresh) / range;
		return sign * (thresh + range * std::tanh(t));
	}

	static float fmFrequencyAdvanced(float baseFreq, float fmIndex, float fmCv,
	                                 int fmMode, float sr) {
		const float cv = clampSafe(fmCv, -5.f, 5.f) / 5.f;
		const float nyq = sr * 0.49f;
		const float safeNyq = nyq * FM_FREQ_SOFT_NYQ;
		switch (fmMode) {
			case FM_MODE_LINEAR: {
				const float ratio = 1.f + fmIndex * FM_OCTAVES * cv;
				return softLimitFreq(baseFreq * ratio, safeNyq);
			}
			case FM_MODE_PHASE:
				return clampSafe(baseFreq, -nyq, nyq);
			case FM_MODE_EXP:
			default: {
				const float ratio = std::exp2(fmIndex * FM_OCTAVES * cv);
				return softLimitFreq(baseFreq * ratio, safeNyq);
			}
		}
	}

	static float fmFrequencyBlended(float baseFreq, float fmIndex, float fmCv,
	                                int fmMode, float darkBias, float sr) {
		const float fPrimary = fmFrequencyAdvanced(baseFreq, fmIndex, fmCv, fmMode, sr);
		if (darkBias < 0.01f || fmMode != FM_MODE_EXP) return fPrimary;
		const float fLinear = fmFrequencyAdvanced(baseFreq, fmIndex, fmCv, FM_MODE_LINEAR, sr);
		return fPrimary + (fLinear - fPrimary) * clampSafe(darkBias, 0.f, 1.f);
	}

	static float fmPhaseOffset(float fmIndex, float fmCv) {
		const float cv = clampSafe(fmCv, -5.f, 5.f) / 5.f;
		return clampSafe(fmIndex * cv * 0.75f, -FM_PHASE_MAX, FM_PHASE_MAX);
	}

	inline float darkLowpass(float input, float dark, float sr) {
		dark = clampSafe(dark, 0.f, 1.f);
		if (std::abs(dark - prevDarkForCoeff) > 0.001f) {
			const float cutoff = DARK_CUTOFF_MAX *
				std::exp2(std::log2(DARK_CUTOFF_MIN / DARK_CUTOFF_MAX) * dark);
			darkCoeffCached = onePoleCoeff(cutoff, sr);
			prevDarkForCoeff = dark;
		}
		darkFilterState += darkCoeffCached * (input - darkFilterState);
		darkFilterState = flushDenorm(finite(darkFilterState));
		return clampSafe(darkFilterState, -1.5f, 1.5f);
	}

	inline float bitCrush(float input, float bitDepth) {
		bitDepth = clampSafe(bitDepth, MIN_BIT_DEPTH, MAX_BIT_DEPTH);
		if (bitDepth >= 15.999f) return input;
		const float levels = std::exp2(bitDepth);
		const float invLevels = 1.f / levels;
		return std::round(input * levels) * invLevels;
	}

	inline float sampleRateReduce(float input, float rate) {
		rate = clampSafe(rate, MIN_SRR, MAX_SRR);
		if (rate <= 1.001f) {
			srrHold = input;
			srrCounter = 0.f;
			return input;
		}
		srrCounter += 1.f;
		if (srrCounter >= rate) {
			srrCounter = std::fmod(srrCounter, rate);
			srrHold = input;
		}
		return srrHold;
	}

	void process(const ProcessArgs& args) override {
		fmopFlushToZero();
		const float sr = args.sampleRate;
		if (sr <= 1.f) return;
		const float dt = 1.f / sr;
		if (sr != finalDcSr) updateDcCoeff(sr);

		// 1) MACRO READ + CV
		const float psyCv    = finite(inputs[PSY_MACRO_CV_INPUT   ].getVoltage());
		const float darkCv   = finite(inputs[DARK_MACRO_CV_INPUT  ].getVoltage());
		const float motionCv = finite(inputs[MOTION_MACRO_CV_INPUT].getVoltage());
		const float feralCv  = finite(inputs[FERAL_MACRO_CV_INPUT ].getVoltage());

		const float psyMacro    = macroWithCV(params[PSY_MACRO_PARAM   ].getValue(), psyCv);
		const float darkMacro   = macroWithCV(params[DARK_MACRO_PARAM  ].getValue(), darkCv);
		const float motionMacro = macroWithCV(params[MOTION_MACRO_PARAM].getValue(), motionCv);
		const float feralMacro  = macroWithCV(params[FERAL_MACRO_PARAM ].getValue(), feralCv);

		psySmooth    = smoothMacro(psySmooth,    macroCurve(psyMacro),    dt);
		darkSmooth   = smoothMacro(darkSmooth,   macroCurve(darkMacro),   dt);
		motionSmooth = smoothMacro(motionSmooth, macroCurve(motionMacro), dt);
		feralSmooth  = smoothMacro(feralSmooth,  macroCurve(feralMacro),  dt, FERAL_SMOOTH_TIME);

		const float masterMacro = clampSafe(params[MASTER_MACRO_PARAM].getValue(), 0.f, 1.f);
		const float masterBlend = 0.5f * masterMacro;

		const float psy    = clampSafe(psySmooth    + (1.f - psySmooth)    * masterBlend, 0.f, 1.f);
		const float dark   = clampSafe(darkSmooth   + (1.f - darkSmooth)   * masterBlend, 0.f, 1.f);
		const float motion = clampSafe(motionSmooth + (1.f - motionSmooth) * masterBlend, 0.f, 1.f);
		const float feral  = clampSafe(feralSmooth  + (1.f - feralSmooth)  * masterBlend, 0.f, 1.f);

		const float psyLate   = lateCurve(psy);
		const float darkLate  = lateCurve(dark);
		const float feralLate = lateCurve(feral);

		// 2) CROSS-MODULATION
		const float feralBoost     = 1.f + 0.40f * psy;
		const float feralEffective = clampSafe(feral * feralBoost, 0.f, 1.20f);
		const float motionSlow     = 1.f - 0.30f * dark;
		const float motionEffective = clampSafe(motion * motionSlow, 0.f, 1.f);

		// 3) INTERNAL ENVELOPE
		const float envTarget = clampSafe(
			std::abs(prevRawSine)  * 0.55f +
			std::abs(prevSaw)      * 0.35f +
			std::abs(prevTriangle) * 0.30f +
			std::abs(prevSquare)   * 0.35f, 0.f, 1.f);

		internalEnvelope = rateIndependentSmooth(internalEnvelope, envTarget, dt, INTERNAL_ENV_TAU);
		internalEnvelope = flushDenorm(internalEnvelope);

		// 4) TURBULENCE
		const int turbMode = static_cast<int>(params[TURBULENCE_MODE_PARAM].getValue());
		const float turbulence = computeTurbulence(
			turbState, internalEnvelope, motionEffective, turbMode, sr, dt);

		// 5) PITCH BUS + GLIDE
		const float pitchAll  = finite(inputs[PITCH_INPUT_ALL].getVoltage());
		const float shapeKnob = finite(params[PSYCHEDELIC_CV_KNOB_PARAM].getValue());
		const int cvMode = clamp(
			static_cast<int>(std::round(params[CV_MODE_PARAM].getValue())), 0, 2);

		const float sharedCvRaw = finite(inputs[PSYCHEDELIC_CV_INPUT_FOR_All].getVoltage());
		const float shapeCv = processCvModeVoltage(sharedCvRaw, cvMode);

		const int pitchBusMode = static_cast<int>(params[PITCH_BUS_MODE_PARAM].getValue());
		const float pitchBusCv = processCvModeVoltage(sharedCvRaw, cvMode);
		float pitchBusProcessed = pitchBusCv;
		const float glideTau = 0.020f * (1.f - 0.90f * motionEffective) + 0.002f;

		if (pitchBusMode == PITCH_BUS_OFF) {
			pitchBusProcessed = 0.f;
			pitchBusGlideState = rateIndependentSmooth(
				pitchBusGlideState, 0.f, dt, 0.020f);
			pitchBusGlideState = flushDenorm(finite(pitchBusGlideState));
		} else {
			const bool doQuant = (pitchBusMode == PITCH_BUS_QUANTIZE ||
			                      pitchBusMode == PITCH_BUS_BOTH);
			const bool doGlide = (pitchBusMode == PITCH_BUS_GLIDE ||
			                      pitchBusMode == PITCH_BUS_BOTH);
			if (doQuant) {
				constexpr float SEMITONE = 1.f / 12.f;
				pitchBusProcessed = std::round(pitchBusProcessed / SEMITONE) * SEMITONE;
			}
			if (doGlide) {
				pitchBusGlideState = rateIndependentSmooth(
					pitchBusGlideState, pitchBusProcessed, dt, glideTau);
				pitchBusGlideState = flushDenorm(finite(pitchBusGlideState));
				pitchBusProcessed = pitchBusGlideState;
			} else {
				pitchBusGlideState = pitchBusProcessed;
			}
		}

		const float busDepthSine     = params[PITCH_BUS_DEPTH_SINE    ].getValue();
		const float busDepthSaw      = params[PITCH_BUS_DEPTH_SAW     ].getValue();
		const float busDepthTriangle = params[PITCH_BUS_DEPTH_TRIANGLE].getValue();
		const float busDepthSquare   = params[PITCH_BUS_DEPTH_SQUARE  ].getValue();
		constexpr float PITCH_BUS_SCALE = PITCH_BUS_MAX_OCTAVES / 10.f;

		const float pitchBusOffsetSine     = busDepthSine     * pitchBusProcessed * PITCH_BUS_SCALE;
		const float pitchBusOffsetSaw      = busDepthSaw      * pitchBusProcessed * PITCH_BUS_SCALE;
		const float pitchBusOffsetTriangle = busDepthTriangle * pitchBusProcessed * PITCH_BUS_SCALE;
		const float pitchBusOffsetSquare   = busDepthSquare   * pitchBusProcessed * PITCH_BUS_SCALE;

		// 6) FM CONFIG
		const int fmMode       = static_cast<int>(params[FM_MODE_PARAM].getValue());
		const int fmRouting    = static_cast<int>(params[FM_ROUTING_PARAM].getValue());
		const int fmSyncMode   = static_cast<int>(params[FM_SYNC_PARAM].getValue());
		const int chainPattern = clamp(
			(int)params[FM_CHAIN_PATTERN_PARAM].getValue(), 0, 7);

		const float syncStrengthBase = clampSafe(
			params[FM_SYNC_STRENGTH_PARAM].getValue(), 0.f, 1.f);

		float syncStrength = syncStrengthBase;
		if (fmSyncMode != FM_SYNC_OFF && syncStrengthBase > 0.001f) {
			syncStrength = clampSafe(
				syncStrengthBase * (1.f + 0.30f * motionEffective), 0.f, 1.f);
		}

		{
			const int* pat = CHAIN_PATTERNS[chainPattern];
			uint32_t packed = ((uint32_t)(chainPattern & 0x07) << 8)
			                | ((uint32_t)(pat[0] & 0x03) << 0)
			                | ((uint32_t)(pat[1] & 0x03) << 2)
			                | ((uint32_t)(pat[2] & 0x03) << 4)
			                | ((uint32_t)(pat[3] & 0x03) << 6);
			displayChainPacked.store(packed, std::memory_order_relaxed);
		}

		const bool pitchStepMode = params[PITCH_STEP_MODE_PARAM].getValue() > 0.5f;

		// 7) FM INDEX
		float fmIndex = params[FM_PARAM].getValue();
		fmIndex += finite(inputs[FM_AMOUNT_INPUT].getVoltage()) *
		           params[FM_AMOUNT_PARAM].getValue();
		fmIndex += 0.30f * psy;
		fmIndex += 0.20f * turbulence * motionEffective;
		fmIndex += 0.10f * feral;

		// SHARED FM SRC MORPH AMOUNT
		float sineMorphAmt = params[SINE_WAVESHAPER_PARAM].getValue();
		sineMorphAmt += shapeCv * 0.30f * shapeKnob;
		sineMorphAmt += 0.55f * psy * (1.f - 0.30f * psyLate);
		sineMorphAmt += 0.25f * turbulence * motionEffective;
		sineMorphAmt = clampSafe(sineMorphAmt, 0.f, 1.f);

		const float sineKnobEarly = params[PITCH_PARAM_SINE].getValue();
		const float sineKnobEarlyUsed = pitchStepMode
			? std::round(sineKnobEarly) : sineKnobEarly;
		const float sinePitchEarly = clampSafe(
			sineKnobEarlyUsed
			+ inputs[PITCH_INPUT_SINE].getVoltage()
			+ pitchAll
			+ pitchBusOffsetSine, -12.f, 12.f);

		// INDEPENDENT MODULATOR PITCH
		const int modPitchRange = modPitchRangePM.load(std::memory_order_relaxed);
		const float modPitchMax = modPitchRangeMax(modPitchRange);
		const float modPitchMin = -modPitchMax;

		const float modPitchKnob = clampSafe(
			params[FM_MOD_PITCH_PARAM].getValue(), modPitchMin, modPitchMax);
		const float modCvDepth = clampSafe(
			params[FM_MOD_PSY_CV_DEPTH_PARAM].getValue(), -1.f, 1.f);
		const float modCvPitchOct = modCvDepth * pitchBusProcessed * PITCH_BUS_SCALE;

		modPitchSmooth = rateIndependentSmooth(modPitchSmooth, modPitchKnob, dt, OCT_SLEW_TAU);
		modPitchSmooth = flushDenorm(modPitchSmooth);

		float modPitch = sinePitchEarly + modPitchSmooth + modCvPitchOct;
		if (pitchStepMode) modPitch = std::round(modPitch);
		modPitch = clampSafe(modPitch, -12.f, 12.f);

		const float modBaseFreqRaw = dsp::FREQ_C4 *
			std::exp2(clampSafe(modPitch, -10.f, 10.f));
		const float modBaseFreq = modBaseFreqRaw * (1.f + 0.040f * feralEffective);

		const float modRatio = clampSafe(
			params[FM_MOD_RATIO_PARAM].getValue(), 0.25f, 8.f);
		const int   modWave  = clamp(
			(int)std::round(params[FM_MOD_WAVE_PARAM].getValue()), 0, 3);

		const float modFreq = modBaseFreq * modRatio;
		modPhase = wrap(modPhase + modFreq / sr);

		const float modDt = clampSafe(std::abs(modFreq) / sr, 1e-6f, 0.45f);
		const float modRawSine = std::sin(PI2 * modPhase);
		const float modMorphAmt = clampSafe(
			params[FM_MOD_MORPH_PARAM].getValue(), 0.f, 1.f);

		float modulator = modulatorMorph(
			modWave, modMorphAmt, sineMorphAmt, modPhase, modDt, modRawSine);

		// fmCv three-way branch
		const bool  fmCvPatched   = inputs[FM_INPUT].isConnected();
		const float fmCvRawJack   = finite(inputs[FM_INPUT].getVoltage());
		const float internalScale = clampSafe(
			params[FM_INTERNAL_SCALE_PARAM].getValue(), 0.f, 1.f);
		const bool  legacyStatic  = params[FM_LEGACY_STATIC_PARAM].getValue() > 0.5f;

		float fmCv;
		if (fmCvPatched) {
			fmCv = clampSafe(fmCvRawJack, -5.f, 5.f);
		} else if (legacyStatic) {
			fmCv = 5.f;
		} else {
			fmCv = 5.f * internalScale * modulator;
		}

		float fmEnvTarget;
		if (fmCvPatched) {
			fmEnvTarget = std::abs(clampSafe(fmCvRawJack, -5.f, 5.f)) / 5.f;
		} else if (legacyStatic) {
			fmEnvTarget = 0.f;
		} else {
			fmEnvTarget = std::abs(internalScale * modulator);
		}
		fmEnvFollower = rateIndependentSmooth(fmEnvFollower, fmEnvTarget, dt, 0.005f);
		fmEnvFollower = flushDenorm(fmEnvFollower);
		fmIndex += params[FM_ENV_PARAM].getValue() * fmEnvFollower * 6.f;

		const float boundedFeedback =
			clampSafe(feralFeedback, -FERAL_FEEDBACK_MAX, FERAL_FEEDBACK_MAX);
		fmIndex += boundedFeedback * feralEffective * turbulence;
		fmIndex = clampSafe(fmIndex, -FM_INDEX_HARD_CLAMP, FM_INDEX_HARD_CLAMP);
		fmIndex = softSaturateFM(fmIndex);
		fmIndex = clampSafe(fmIndex, -FM_INDEX_POST_SAT_CLAMP, FM_INDEX_POST_SAT_CLAMP);

		// 8) TONAL RATIOS
		const float sineRatio     = 1.f + 0.040f * feralEffective;
		const float triangleRatio = 1.f - 0.050f * feralEffective
		                              + 0.020f * motionEffective * turbulence * feralEffective;
		const float sawRatio      = 1.f + 0.075f * feralEffective;
		const float squareRatio   = 1.f - 0.060f * feralEffective
		                              - 0.020f * dark * feralEffective;

		// 9) PITCH COMPUTE
		float sineKnob     = params[PITCH_PARAM_SINE    ].getValue();
		float sawKnob      = params[PITCH_PARAM_SAW     ].getValue();
		float triangleKnob = params[PITCH_PARAM_TRIANGLE].getValue();
		float squareKnob   = params[PITCH_PARAM_SQUARE  ].getValue();

		if (pitchStepMode) {
			sineKnob = std::round(sineKnob);
			sawKnob = std::round(sawKnob);
			triangleKnob = std::round(triangleKnob);
			squareKnob = std::round(squareKnob);
		}

		sineOctSmooth = rateIndependentSmooth(sineOctSmooth, sineKnob,     dt, OCT_SLEW_TAU);
		sawOctSmooth  = rateIndependentSmooth(sawOctSmooth,  sawKnob,      dt, OCT_SLEW_TAU);
		triOctSmooth  = rateIndependentSmooth(triOctSmooth,  triangleKnob, dt, OCT_SLEW_TAU);
		sqOctSmooth   = rateIndependentSmooth(sqOctSmooth,   squareKnob,   dt, OCT_SLEW_TAU);

		float sinePitch = sineOctSmooth +
			inputs[PITCH_INPUT_SINE].getVoltage() + pitchAll + pitchBusOffsetSine;
		float sawPitch = sawOctSmooth +
			inputs[PITCH_INPUT_SAW].getVoltage() + pitchAll + pitchBusOffsetSaw;
		float trianglePitch = triOctSmooth +
			inputs[PITCH_INPUT_TRIANGLE].getVoltage() + pitchAll + pitchBusOffsetTriangle;
		float squarePitch = sqOctSmooth +
			inputs[PITCH_INPUT_SQUARE].getVoltage() + pitchAll + pitchBusOffsetSquare;

		sinePitch     = clampSafe(sinePitch,     -12.f, 12.f);
		sawPitch      = clampSafe(sawPitch,      -12.f, 12.f);
		trianglePitch = clampSafe(trianglePitch, -12.f, 12.f);
		squarePitch   = clampSafe(squarePitch,   -12.f, 12.f);

		const float sineBase     = dsp::FREQ_C4 * std::exp2(clampSafe(sinePitch, -10.f, 10.f));
		const float sawBase      = dsp::FREQ_C4 * std::exp2(clampSafe(sawPitch, -10.f, 10.f));
		const float triangleBase = dsp::FREQ_C4 * std::exp2(clampSafe(trianglePitch, -10.f, 10.f));
		const float squareBase   = dsp::FREQ_C4 * std::exp2(clampSafe(squarePitch, -10.f, 10.f));

		const float sineBaseF     = sineBase     * sineRatio;
		const float sawBaseF      = sawBase      * sawRatio;
		const float triangleBaseF = triangleBase * triangleRatio;
		const float squareBaseF   = squareBase   * squareRatio;

		// 10) SELF-FM DEPTH
		const float fmCvNorm = fmCv;
		const float selfDepth = clampSafe(
			std::abs(fmIndex) * (1.f + 3.0f * feralEffective), 0.f, 6.f);
		const float apCoef = clampSafe(0.25f + 0.65f * feralEffective, 0.f, 0.90f);

		const float avgSine = multiTapAverage(fbHistorySine,   FB_TAPS);
		const float avgSaw  = multiTapAverage(fbHistorySaw,    FB_TAPS);
		const float avgTri  = multiTapAverage(fbHistoryTri,    FB_TAPS);
		const float avgSqr  = multiTapAverage(fbHistorySquare, FB_TAPS);

		const float fbSine = allpassStep(avgSine, fbAP_z_sine, apCoef);
		const float fbSaw  = allpassStep(avgSaw,  fbAP_z_saw,  apCoef);
		const float fbTri  = allpassStep(avgTri,  fbAP_z_tri,  apCoef);
		const float fbSqr  = allpassStep(avgSqr,  fbAP_z_sqr,  apCoef);

		const float cdSine = getChainDepth(0);
		const float cdSaw  = getChainDepth(1);
		const float cdTri  = getChainDepth(2);
		const float cdSqr  = getChainDepth(3);

		const float selfMods[4] = {
			clampSafe(fbSine * selfDepth * (fmRouting == FM_ROUTE_CHAIN ? cdSine : 1.f), -5.f, 5.f),
			clampSafe(fbSaw  * selfDepth * (fmRouting == FM_ROUTE_CHAIN ? cdSaw  : 1.f), -5.f, 5.f),
			clampSafe(fbTri  * selfDepth * (fmRouting == FM_ROUTE_CHAIN ? cdTri  : 1.f), -5.f, 5.f),
			clampSafe(fbSqr  * selfDepth * (fmRouting == FM_ROUTE_CHAIN ? cdSqr  : 1.f), -5.f, 5.f),
		};
		const float selfModS = selfMods[0];
		const float selfModW = selfMods[1];
		const float selfModT = selfMods[2];
		const float selfModQ = selfMods[3];

		const float globalAmt = clampSafe(
			params[FM_INTERNAL_AMT_PARAM].getValue(), 0.f, 1.f);
		const float globalSelfScale = (globalAmt > 0.f && feralEffective > 0.f)
			? (0.10f * globalAmt * feralEffective) : 0.f;
		const float globalSelfInjS = clampSafe(globalSelfScale * selfModS, -1.5f, 1.5f);
		const float globalSelfInjW = clampSafe(globalSelfScale * selfModW, -1.5f, 1.5f);
		const float globalSelfInjT = clampSafe(globalSelfScale * selfModT, -1.5f, 1.5f);
		const float globalSelfInjQ = clampSafe(globalSelfScale * selfModQ, -1.5f, 1.5f);

		// ==========================================================
		// 10.5) SUB AS MODULATOR — inject into fmIndex
		// ==========================================================
		const int subFmRoute = clamp((int)params[SUB_FM_ROUTE_PARAM].getValue(), 0, 3);
		float fmIndexSub = fmIndex;
		if (subFmRoute == SUB_ROUTE_MODULATOR) {
			const float subPrev = std::sin(PI2 * subPhase);
			fmIndexSub = clampSafe(
				fmIndex + subPrev * 0.20f * feralEffective,
				-FM_INDEX_HARD_CLAMP, FM_INDEX_HARD_CLAMP);
			fmIndexSub = softSaturateFM(fmIndexSub);
			fmIndexSub = clampSafe(fmIndexSub, -FM_INDEX_POST_SAT_CLAMP, FM_INDEX_POST_SAT_CLAMP);
		}

		// 11) FM ROUTING
		float sineFreq, sawFreq, triangleFreq, squareFreq;
		switch (fmRouting) {
			case FM_ROUTE_SELF: {
				sineFreq     = fmFrequencyBlended(sineBaseF,     fmIndexSub, fmCvNorm + selfModS, fmMode, darkLate, sr);
				sawFreq      = fmFrequencyBlended(sawBaseF,      fmIndexSub, fmCvNorm + selfModW, fmMode, darkLate, sr);
				triangleFreq = fmFrequencyBlended(triangleBaseF, fmIndexSub, fmCvNorm + selfModT, fmMode, darkLate, sr);
				squareFreq   = fmFrequencyBlended(squareBaseF,   fmIndexSub, fmCvNorm + selfModQ, fmMode, darkLate, sr);
				break;
			}
			case FM_ROUTE_CHAIN: {
				const int* pat = CHAIN_PATTERNS[chainPattern];
				sineFreq     = fmFrequencyBlended(sineBaseF,     fmIndexSub, fmCvNorm + selfMods[pat[0]], fmMode, darkLate, sr);
				sawFreq      = fmFrequencyBlended(sawBaseF,      fmIndexSub, fmCvNorm + selfMods[pat[1]], fmMode, darkLate, sr);
				triangleFreq = fmFrequencyBlended(triangleBaseF, fmIndexSub, fmCvNorm + selfMods[pat[2]], fmMode, darkLate, sr);
				squareFreq   = fmFrequencyBlended(squareBaseF,   fmIndexSub, fmCvNorm + selfMods[pat[3]], fmMode, darkLate, sr);
				break;
			}
			case FM_ROUTE_GLOBAL:
			default: {
				sineFreq     = fmFrequencyBlended(sineBaseF,     fmIndexSub, fmCvNorm + globalSelfInjS, fmMode, darkLate, sr);
				sawFreq      = fmFrequencyBlended(sawBaseF,      fmIndexSub, fmCvNorm + globalSelfInjW, fmMode, darkLate, sr);
				triangleFreq = fmFrequencyBlended(triangleBaseF, fmIndexSub, fmCvNorm + globalSelfInjT, fmMode, darkLate, sr);
				squareFreq   = fmFrequencyBlended(squareBaseF,   fmIndexSub, fmCvNorm + globalSelfInjQ, fmMode, darkLate, sr);
				break;
			}
		}

		float sinePhaseFm = 0.f, sawPhaseFm = 0.f, trianglePhaseFm = 0.f, squarePhaseFm = 0.f;
		if (fmMode == FM_MODE_PHASE) {
			switch (fmRouting) {
				case FM_ROUTE_SELF: {
					sinePhaseFm     = fmPhaseOffset(fmIndexSub, fmCvNorm + selfModS);
					sawPhaseFm      = fmPhaseOffset(fmIndexSub, fmCvNorm + selfModW);
					trianglePhaseFm = fmPhaseOffset(fmIndexSub, fmCvNorm + selfModT);
					squarePhaseFm   = fmPhaseOffset(fmIndexSub, fmCvNorm + selfModQ);
					break;
				}
				case FM_ROUTE_CHAIN: {
					const int* pat = CHAIN_PATTERNS[chainPattern];
					sinePhaseFm     = fmPhaseOffset(fmIndexSub, fmCvNorm + selfMods[pat[0]]);
					sawPhaseFm      = fmPhaseOffset(fmIndexSub, fmCvNorm + selfMods[pat[1]]);
					trianglePhaseFm = fmPhaseOffset(fmIndexSub, fmCvNorm + selfMods[pat[2]]);
					squarePhaseFm   = fmPhaseOffset(fmIndexSub, fmCvNorm + selfMods[pat[3]]);
					break;
				}
				case FM_ROUTE_GLOBAL:
				default: {
					sinePhaseFm     = fmPhaseOffset(fmIndexSub, fmCvNorm + globalSelfInjS);
					sawPhaseFm      = fmPhaseOffset(fmIndexSub, fmCvNorm + globalSelfInjW);
					trianglePhaseFm = fmPhaseOffset(fmIndexSub, fmCvNorm + globalSelfInjT);
					squarePhaseFm   = fmPhaseOffset(fmIndexSub, fmCvNorm + globalSelfInjQ);
					break;
				}
			}
		}

		// 12) PHASE DISTORTION
		const float pdTarget = clampSafe(
			params[PHASE_DIST_PARAM].getValue()
			+ 0.40f * psy
			+ 0.15f * darkLate, 0.f, 1.f);
		pdSmooth = rateIndependentSmooth(pdSmooth, pdTarget, dt, 0.010f);
		pdSmooth = clampSafe(pdSmooth, 0.f, 1.f);

		// 13) PHASE ADVANCE (subPhase now advances in section 20)
		prevPhaseSine = phaseSine;
		phaseSine     = advance(phaseSine,     sineFreq, sr);
		phaseSaw      = advance(phaseSaw,      sawFreq, sr);
		phaseTriangle = advance(phaseTriangle, triangleFreq, sr);
		phaseSquare   = advance(phaseSquare,   squareFreq, sr);

		const float dPhase = phaseSine - prevPhaseSine;
		const bool wrappedNow = (dPhase < -0.5f) || (dPhase > 0.5f);
		const bool validWrap  = wrappedNow && !lastSyncWrap;
		lastSyncWrap = wrappedNow;

		if (fmSyncMode != FM_SYNC_OFF && validWrap) {
			phaseSaw = syncSlave(phaseSaw, phaseSine, prevPhaseSine,
			                     fmSyncMode, syncStrength, true);
			phaseTriangle = syncSlave(phaseTriangle, phaseSine, prevPhaseSine,
			                          fmSyncMode, syncStrength, true);
			phaseSquare = syncSlave(phaseSquare, phaseSine, prevPhaseSine,
			                        fmSyncMode, syncStrength, true);
		}

		// 14) WAVEFORM GENERATION
		float sinePhase = phaseSine + sinePhaseFm +
			phaseMod(params[PHASE_MOD_PARAM_SINE].getValue(),
			         inputs[PHASE_MOD_INPUT_SINE].getVoltage(), feral);
		sinePhase += 0.045f * feralEffective *
			(0.70f * turbulence * motionEffective + 0.30f * boundedFeedback);
		sinePhase = wrap(sinePhase);
		sinePhase = phaseDistort(sinePhase, pdSmooth);

		const float sineDt = clampSafe(std::abs(sineFreq) / sr, 1e-6f, 0.45f);
		const float rawSineThis = std::sin(PI2 * sinePhase);

		float sineOut = fmSourceMorph(sinePhase, rawSineThis, sineMorphAmt, sineDt);

		float sawPhase = phaseSaw + sawPhaseFm +
			phaseMod(params[PHASE_MOD_PARAM_SAW].getValue(),
			         inputs[PHASE_MOD_INPUT_SAW].getVoltage(), feral);
		sawPhase += 0.060f * feralEffective * turbulence * motionEffective;
		sawPhase = wrap(sawPhase);
		sawPhase = phaseDistort(sawPhase, pdSmooth * 0.5f);

		const float sawDt = clampSafe(std::abs(sawFreq) / sr, 1e-6f, 0.45f);
		float sawOut = sawBL(sawPhase, sawDt);

		float sawShape = params[SAW_WAVESHAPER_PARAM].getValue();
		sawShape += params[SAW_WAVESHAPER_AMOUNT].getValue() *
		            (inputs[PSYCHEDELIC_CV_INPUT_SAW].getVoltage() + shapeCv * shapeKnob);
		sawShape += 0.55f * psy * (1.f - 0.45f * psyLate);
		sawShape += 0.30f * turbulence * motionEffective;
		sawOut = shape(sawOut, sawShape, SAW_SHAPE);

		float trianglePhase = phaseTriangle + trianglePhaseFm +
			phaseMod(params[PHASE_MOD_PARAM_TRIANGLE].getValue(),
			         inputs[PHASE_MOD_INPUT_TRIANGLE].getVoltage(), feral);
		trianglePhase += 0.050f * feralEffective *
			(0.50f * turbulence * motionEffective + 0.50f * boundedFeedback);
		trianglePhase = wrap(trianglePhase);
		trianglePhase = phaseDistort(trianglePhase, pdSmooth);

		float triangleOut = triangleBL(trianglePhase);

		float triangleShape = params[TRIANGLE_WAVESHAPER_PARAM].getValue();
		triangleShape += params[PSYCHEDELIC_PARAM_TRIANGLE].getValue();
		triangleShape += params[TRIANGLE_WAVESHAPER_AMOUNT].getValue() *
		                 (inputs[PSYCHEDELIC_CV_INPUT_TRIANGLE].getVoltage() + shapeCv * shapeKnob);
		triangleShape += 0.60f * psy * (1.f - 0.40f * psyLate);
		triangleShape += 0.30f * turbulence * motionEffective;
		triangleOut = shape(triangleOut, triangleShape, PSYCH_SHAPE);

		float squarePhase = phaseSquare + squarePhaseFm +
			phaseMod(params[PHASE_MOD_PARAM_SQUARE].getValue(),
			         inputs[PHASE_MOD_INPUT_SQUARE].getVoltage(), feral);
		squarePhase += 0.070f * feralEffective * turbulence * motionEffective;
		squarePhase = wrap(squarePhase);
		squarePhase = phaseDistort(squarePhase, pdSmooth * 0.35f);

		const float sqDt = clampSafe(std::abs(squareFreq) / sr, 1e-6f, 0.45f);
		float squareOut = squareBL(squarePhase, sqDt);

		float squareShape = params[SQUARE_WAVESHAPER_PARAM].getValue();
		squareShape += params[SQUARE_WAVESHAPER_AMOUNT].getValue() *
		               (inputs[PSYCHEDELIC_CV_INPUT_SQUARE].getVoltage() + shapeCv * shapeKnob);
		squareShape += 0.35f * psy * (1.f - 0.55f * psyLate);
		squareShape += 0.28f * turbulence * motionEffective;
		squareOut = shape(squareOut, squareShape, SQUARE_SHAPE);

		// 15) FEEDBACK HISTORY
		prevSine     = clampSafe(sineOut,     -1.5f, 1.5f);
		prevSaw      = clampSafe(sawOut,      -1.5f, 1.5f);
		prevTriangle = clampSafe(triangleOut, -1.5f, 1.5f);
		prevSquare   = clampSafe(squareOut,   -1.5f, 1.5f);
		prevRawSine  = clampSafe(rawSineThis, -1.f, 1.f);

		pushFeedback(fbHistorySine,   FB_TAPS, prevSine);
		pushFeedback(fbHistorySaw,    FB_TAPS, prevSaw);
		pushFeedback(fbHistoryTri,    FB_TAPS, prevTriangle);
		pushFeedback(fbHistorySquare, FB_TAPS, prevSquare);

		// 16) OUTPUT GAINS
		float sineGain     = params[VOLUME_PARAM_SINE].getValue();
		float sawGain      = params[VOLUME_PARAM_SAW].getValue();
		float triangleGain = params[VOLUME_PARAM_TRIANGLE].getValue();
		float squareGain   = params[VOLUME_PARAM_SQUARE].getValue();
		sawGain    *= 1.f + 0.08f * feral;
		squareGain *= 1.f + 0.06f * feral;
		sineOut     *= sineGain;
		sawOut      *= sawGain;
		triangleOut *= triangleGain;
		squareOut   *= squareGain;

		// 17) WAVE BUFFER + INDIVIDUAL OUTPUTS
		waveBufSine[waveIdx]   = clamp(sineOut,     -1.f, 1.f);
		waveBufSaw[waveIdx]    = clamp(sawOut,      -1.f, 1.f);
		waveBufTri[waveIdx]    = clamp(triangleOut, -1.f, 1.f);
		waveBufSquare[waveIdx] = clamp(squareOut,   -1.f, 1.f);

		outputs[SINE_OUTPUT    ].setVoltage(clamp(sineOut     * 5.f, -5.f, 5.f));
		outputs[SAW_OUTPUT     ].setVoltage(clamp(sawOut      * 5.f, -5.f, 5.f));
		outputs[TRIANGLE_OUTPUT].setVoltage(clamp(triangleOut * 5.f, -5.f, 5.f));
		outputs[SQUARE_OUTPUT  ].setVoltage(clamp(squareOut   * 5.f, -5.f, 5.f));

		const float sum = clampSafe(
			sineOut + sawOut + triangleOut + squareOut, -4.f, 4.f);
		waveBufSum[waveIdx] = clamp(sum * 0.35f, -1.f, 1.f);
		waveIdx = (waveIdx + 1) % WAVE_BUF_SIZE;

		if (++snapCounter >= SNAP_INTERVAL) {
			snapCounter = 0;
			for (int i = 0; i < WAVE_SNAPSHOT_SIZE; i++) {
				const int srcIdx =
					(waveIdx + (i * WAVE_BUF_SIZE) / WAVE_SNAPSHOT_SIZE) % WAVE_BUF_SIZE;
				snapSine[i]   = waveBufSine[srcIdx];
				snapSaw[i]    = waveBufSaw[srcIdx];
				snapTri[i]    = waveBufTri[srcIdx];
				snapSquare[i] = waveBufSquare[srcIdx];
				snapSum[i]    = waveBufSum[srcIdx];
			}
		}

		outputs[SUM_OUTPUT].setVoltage(SUM_OUTPUT_SCALE * std::tanh(sum * 0.5f));

		// 18) POST-PROCESS
		const float internal = clampSafe(sum * 0.25f, -1.f, 1.f);

		float source;
		if (inputs[RESAMPLE_INPUT].isConnected()) {
			source = inputs[RESAMPLE_INPUT].getVoltage() * 0.2f;
		} else {
			source = internal;
		}
		source = clampSafe(source, -1.f, 1.f);

		const float blocked = dcBlock(source, audioDcX, audioDcY);

		float wet = params[RESAMPLE].getValue() + 0.10f * psy;
		wet = clampSafe(wet, 0.f, 1.f);

		float harmonicAmount = params[HARMONIC_SUM_PARAM].getValue() + 0.50f * psyLate;
		harmonicAmount = clampSafe(harmonicAmount, 0.f, 1.f);

		float processInput = clampSafe(blocked + internal, -1.f, 1.f);
		processInput = clampSafe(
			processInput + 0.18f * feralEffective
			             * processInput * std::abs(processInput), -1.f, 1.f);

		float harmonic = harmonics(processInput, harmonicAmount);

		float foldAmount = clampSafe(
			wet + 0.10f * psy + 0.10f * feralEffective, 0.f, 1.f);

		float foldedInput = asymmetricDrive(harmonic, feralEffective);
		float folded      = fold(foldedInput, foldAmount);
		float foldBlocked = dcBlock(folded, foldDcX, foldDcY);

		float processed = source * (1.f - wet) + foldBlocked * wet;

		const float srrRate  = params[SRR_RATE_PARAM ].getValue();
		const float bitDepth = params[BIT_DEPTH_PARAM].getValue();
		processed = sampleRateReduce(processed, srrRate);
		processed = bitCrush(processed, bitDepth);

		const float darkFiltered = darkLowpass(processed, dark, sr);
		processed = processed * (1.f - dark) + darkFiltered * dark;

		// 19) FERAL FEEDBACK LOOP
		const float feedbackSource = clampSafe(
			0.55f * processed + 0.25f * foldBlocked + 0.20f * internal, -1.f, 1.f);
		const float feedbackCoeff = (feralEffective > 0.001f)
			? (0.02f + 0.40f * feralEffective) : 0.f;
		feralFeedback += (feedbackSource - feralFeedback) * feedbackCoeff;
		feralFeedback = std::tanh(feralFeedback * 1.5f) * FERAL_FEEDBACK_MAX;
		feralFeedback = flushDenorm(feralFeedback);

		// ==========================================================
		// 20) ENHANCED SUB-OSCILLATOR (lead & space)
		// ==========================================================
		const int subWave       = clamp((int)params[SUB_WAVE_PARAM].getValue(), 0, 5);
		const int subFreqMode   = clamp((int)params[SUB_FREQ_MODE_PARAM].getValue(), 0, 7);
		const float subPitch    = clampSafe(params[SUB_PITCH_PARAM].getValue(), -2.f, 2.f);
		const float subPhaseOff = clampSafe(params[SUB_PHASE_PARAM].getValue(), 0.f, 1.f);
		const float subRingAmt  = clampSafe(params[SUB_RING_PARAM].getValue(), 0.f, 1.f);
		const float subDriftAmt = clampSafe(params[SUB_DRIFT_PARAM].getValue(), 0.f, 1.f);
		const float subLevelParam = clampSafe(params[DARK_SUB_LEVEL_PARAM].getValue(), 0.f, 1.f);

		float subFreq;
		switch (subFreqMode) {
			case SUB_FREQ_DIV3:     subFreq = sineFreq / 3.f; break;
			case SUB_FREQ_DIV4:     subFreq = sineFreq * 0.25f; break;
			case SUB_FREQ_DIV5:     subFreq = sineFreq / 5.f; break;
			case SUB_FREQ_DIV7:     subFreq = sineFreq / 7.f; break;
			case SUB_FREQ_UNISON:   subFreq = sineFreq; break;
			case SUB_FREQ_FIXED:    subFreq = 5.f + 75.f * clampSafe(subPitch * 0.5f + 0.5f, 0.f, 1.f); break;
			case SUB_FREQ_MODRATIO: subFreq = modFreq * 0.5f; break;
			case SUB_FREQ_OCTAVE:
			default:                subFreq = sineFreq * 0.5f; break;
		}
		if (subFreqMode != SUB_FREQ_FIXED) {
			subFreq *= std::exp2(subPitch);
		}
		if (subFmRoute == SUB_ROUTE_CARRIER) {
			const float subModDepth = 0.08f * fmIndex * feralEffective;
			subFreq *= (1.f + modulator * subModDepth);
		}
		if (subDriftAmt > 0.001f && motionEffective > 0.01f) {
			subFreq *= (1.f + subDriftAmt * 0.02f * turbulence * motionEffective);
		}
		subFreq = clampSafe(subFreq, -sr * 0.49f, sr * 0.49f);

		subPhase = wrap(subPhase + subFreq / sr + subPhaseOff * 0.0001f);

		const float subDt = clampSafe(std::abs(subFreq) / sr, 1e-6f, 0.45f);
		const float subRawSine = std::sin(PI2 * subPhase);
		float subRaw;
		switch (subWave) {
			case 0: subRaw = subRawSine; break;
			case 1: subRaw = triangleBL(subPhase); break;
			case 2: subRaw = sawBL(subPhase, subDt); break;
			case 3: subRaw = squareBL(subPhase, subDt); break;
			case 4: subRaw = std::tanh(2.4f * subRawSine); break;
			case 5: subRaw = evalFmSourceShape(SM_SYNC, subPhase, subDt, subRawSine); break;
			default: subRaw = triangleBL(subPhase); break;
		}

		float subShaped = subRaw;
		if (dark > 0.01f) {
			subShaped = asymmetricDrive(subShaped, dark * 0.5f);
			subShaped = fmSourceMorph(subPhase, subShaped, sineMorphAmt * dark * 0.6f, subDt);
			subShaped = std::tanh(subShaped * (1.f + 1.5f * dark));
		}

		if (feralEffective > 0.01f) {
			const float subFmDepth = 0.03f * feralEffective * boundedFeedback;
			subShaped = clampSafe(
				subShaped + subFmDepth * std::sin(PI2 * wrap(subPhase * 3.f)),
				-1.5f, 1.5f);
		}

		if (subRingAmt > 0.001f) {
			const float ringed = subShaped * sineOut;
			subShaped = subShaped * (1.f - subRingAmt) + ringed * subRingAmt;
		}

		const float darkShape = dark * (0.5f + 0.5f * dark);
		float subMix = 0.f;
		if (subFmRoute != SUB_ROUTE_OFF) {
			subMix = subShaped * subLevelParam * darkShape * DARK_SUB_MAX_GAIN;
		}

		// 21) FINAL STAGE
		const float outDriveTarget = clampSafe(
			params[OUT_DRIVE_PARAM].getValue()
			+ 0.15f * darkLate
			+ 0.20f * feralLate, 0.f, 2.f);

		outDriveSmooth = rateIndependentSmooth(outDriveSmooth, outDriveTarget, dt, 0.015f);
		outDriveSmooth = clampSafe(outDriveSmooth, 0.f, 2.f);

		const float mixed = 0.5f * (internal + processed) + subMix;
		const float driven = mixed * (1.f + outDriveSmooth);

		const float needSat = std::max(outDriveSmooth, 2.f * feralEffective);
		float finalNorm;
		if (needSat < 0.005f) {
			finalNorm = driven;
		} else {
			finalNorm = std::tanh(driven);
			finalNorm = std::tanh(finalNorm + 0.035f * feralEffective * feralFeedback);
		}

		const float totalDrive = 0.05f * psy + 0.05f * dark
		                       + 0.15f * motion + 0.15f * feral;
		const float autoTrim = 1.f / (1.f + 0.15f * totalDrive);
		finalNorm *= autoTrim;
		finalNorm *= 1.f + 0.20f * psy;

		const float finalClean = dcBlockAt(finalNorm, finalDcX, finalDcY, finalDcCoeff);
		const float finalVoltage = clamp(finalClean * 5.f, -10.f, 10.f);
		outputs[FINAL_OUTPUT].setVoltage(finalVoltage);

		// 22) LIGHTS
		const float finalBrightness = clamp(std::abs(finalVoltage) / 5.f, 0.f, 1.f);
		const float fmModeBright =
			(fmMode == FM_MODE_EXP)    ? 0.35f :
			(fmMode == FM_MODE_LINEAR) ? 0.65f : 1.0f;
		lights[LED_LIGHT].setBrightness(fmModeBright);

		const float fmRoutingBright =
			(fmRouting == FM_ROUTE_GLOBAL) ? 0.35f :
			(fmRouting == FM_ROUTE_SELF)   ? 0.65f : 1.0f;
		lights[LED_RED_LIGHT].setBrightness(fmRoutingBright);
		lights[FINAL_OUTPUT_LIGHT].setBrightness(finalBrightness);

		float hexagramBrightness;
		if (pitchBusMode == PITCH_BUS_OFF) {
			const float rawShapeCv = inputs[PSYCHEDELIC_CV_INPUT_FOR_All].getVoltage();
			hexagramBrightness = clamp(std::abs(rawShapeCv) / 5.f, 0.f, 1.f);
		} else {
			const float busActivity = clampSafe(
				std::abs(pitchBusProcessed) / 5.f, 0.f, 1.f);
			const float busFlicker = clamp(
				0.6f * busActivity + 0.4f * std::abs(turbulence), 0.f, 1.f);
			hexagramBrightness = busFlicker;
		}
		lights[HEXAGRAM_LIGHT].setBrightness(hexagramBrightness);

		const float fmCvRaw = inputs[FM_INPUT].getVoltage();
		const float fmAmountKnob = std::abs(params[FM_AMOUNT_PARAM].getValue());
		lights[FM_AMOUNT_LIGHT].setBrightness(clamp(
			std::abs(fmCvRaw) / 5.f * (0.3f + 0.7f * fmAmountKnob * 2.f), 0.f, 1.f));

		lights[PITCH_STEP_MODE_LIGHT].setBrightness(pitchStepMode ? 1.f : 0.f);
		lights[CV_MODE_LIGHT_0].setBrightness(cvMode == 0 ? 1.f : 0.f);
		lights[CV_MODE_LIGHT_1].setBrightness(cvMode == 1 ? 1.f : 0.f);
		lights[CV_MODE_LIGHT_2].setBrightness(cvMode == 2 ? 1.f : 0.f);
	}
};

constexpr int FmOperator::CHAIN_PATTERNS[8][4];

// ============================================================
// FM ENGINE DISPLAY
// ============================================================

struct FmEngineDisplay : TransparentWidget {
	FmOperator* module = nullptr;

	int   focusedSeg     = 0;
	int   lastClickedSeg = -1;
	float flashAlpha     = 0.f;

	int   draggingNode   = -1;
	float dragStartDepth = 1.f;
	Vec   dragAccum      = Vec(0.f, 0.f);

	bool  syncDragActive = false;
	float syncDragStart  = 1.f;
	float syncDragAccumY = 0.f;

	float seenValue[4]   = {0.f, 0.f, 0.f, 0.f};
	bool  seenInit[4]    = {false, false, false, false};

	std::shared_ptr<Font> font;

	static constexpr float SIZE_MM  = 21.f;
	static constexpr float UI_SCALE = 1.f;

	static int segParamId(int seg) {
		switch (seg) {
			case 0: return FmOperator::FM_MODE_PARAM;
			case 1: return FmOperator::FM_ROUTING_PARAM;
			case 2: return FmOperator::FM_SYNC_PARAM;
			case 3: return FmOperator::FM_CHAIN_PATTERN_PARAM;
			case 4: case 5: case 6: case 7: return FmOperator::FM_ROUTING_PARAM;
		}
		return -1;
	}
	static int segMaxValue(int seg) {
		switch (seg) {
			case 0: return 2;
			case 1: return 2;
			case 2: return 3;
			case 3: return 7;
		}
		return 0;
	}
	static const char* segLabel(int seg) {
		switch (seg) {
			case 0: return "MODE";
			case 1: return "ROUTE";
			case 2: return "SYNC";
			case 3: return "CHAIN";
		}
		return "";
	}
	static NVGcolor segColor(int seg) {
		switch (seg) {
			case 0: return nvgRGBA( 60, 255, 255, 255);
			case 1: return nvgRGBA(255, 160,  60, 255);
			case 2: return nvgRGBA(255, 100, 200, 255);
			case 3: return nvgRGBA( 60, 255, 120, 255);
		}
		return nvgRGBA(255, 255, 255, 255);
	}

	FmEngineDisplay() {
		const float mm = SIZE_MM * UI_SCALE;
		const float d  = mm2px(Vec(mm, mm)).x;
		box.size = Vec(d, d);
	}

	static const int* nodeLayout() { static const int L[4] = { 0, 1, 3, 2 }; return L; }

	Vec chainNodePos(int slot) const {
		const float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
		const float R  = std::min(cx, cy);
		const float Rinner = R * 0.66f;
		const float layoutR = Rinner * 0.58f;
		const float ang = -M_PI * 0.5f + (M_PI * 0.5f) * slot;
		return Vec(cx + layoutR * std::cos(ang), cy + layoutR * std::sin(ang));
	}
	float chainNodeRadius() const {
		const float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
		const float R = std::min(cx, cy);
		return R * 0.66f * 0.24f;
	}

	int hitChainNode(Vec pos) const {
		if (focusedSeg != 3) return -1;
		const int* L = nodeLayout();
		const float nodeR = chainNodeRadius();
		const float hitR = nodeR + 3.f;
		for (int i = 0; i < 4; i++) {
			Vec p = chainNodePos(i);
			Vec d = pos.minus(p);
			if (d.x*d.x + d.y*d.y < hitR*hitR) return L[i];
		}
		return -1;
	}

	bool hitSyncCenter(Vec pos) const {
		if (focusedSeg != 2) return false;
		const float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
		const float R = std::min(cx, cy);
		const float Rinner = R * 0.66f;
		const Vec d = pos.minus(Vec(cx, cy));
		return (d.x*d.x + d.y*d.y) < (Rinner * Rinner);
	}

	int getPatIdx() const {
		if (!module) return 0;
		const uint32_t p = module->displayChainPacked.load(std::memory_order_relaxed);
		return (int)((p >> 8) & 0x07);
	}
	int getOrder(int i) const {
		if (!module) return i;
		const uint32_t p = module->displayChainPacked.load(std::memory_order_relaxed);
		return (int)((p >> (i * 2)) & 0x03);
	}

	void segAngles(int seg, float& a0, float& a1) const {
		const float PI = M_PI;
		const float base = -PI * 0.5f + (float)seg * (PI * 0.5f);
		a0 = base - PI * 0.25f;
		a1 = base + PI * 0.25f;
	}

	int hitSegment(Vec pos, float cx, float cy, float rIn, float rOut) const {
		const float dx = pos.x - cx, dy = pos.y - cy;
		const float r = std::sqrt(dx*dx + dy*dy);
		if (r < rIn || r > rOut) return -1;
		const float ang = std::atan2(dy, dx);
		const float PI = M_PI;
		if (ang >= -PI*0.75f && ang < -PI*0.25f) return 0;
		if (ang >= -PI*0.25f && ang <  PI*0.25f) return 1;
		if (ang >=  PI*0.25f && ang <  PI*0.75f) return 2;
		return 3;
	}

	void onHover(const event::Hover& e) override {
		TransparentWidget::onHover(e);
		if (!module) return;
		if (draggingNode >= 0 || syncDragActive) return;
		if (focusedSeg == 3) {
			if (hitChainNode(e.pos) >= 0) { e.consume(this); return; }
		}
		const float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
		const float R = std::min(cx, cy);
		int seg = hitSegment(e.pos, cx, cy, R * 0.68f, R * 0.98f);
		if (seg >= 0) focusedSeg = seg;
	}

	void onButton(const event::Button& e) override {
		TransparentWidget::onButton(e);
		if (!module) return;

		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			if (hitSyncCenter(e.pos)) {
				syncDragActive = true;
				syncDragStart  = module->params[FmOperator::FM_SYNC_STRENGTH_PARAM].getValue();
				syncDragAccumY = 0.f;
				e.consume(this);
				return;
			}
			if (focusedSeg == 3) {
				int nodeIdx = hitChainNode(e.pos);
				if (nodeIdx >= 0) {
					draggingNode   = nodeIdx;
					dragStartDepth = module->getChainDepth(nodeIdx);
					dragAccum      = Vec(0.f, 0.f);
					e.consume(this);
					return;
				}
			}
			const float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
			const float R = std::min(cx, cy);
			int seg = hitSegment(e.pos, cx, cy, R * 0.68f, R * 0.98f);
			if (seg < 0) return;
			int paramId = segParamId(seg);
			int maxV = segMaxValue(seg);
			if (paramId < 0 || maxV <= 0) return;
			int curI = (int)std::round(module->params[paramId].getValue());
			int next = (curI + 1) % (maxV + 1);
			if (auto* pq = module->paramQuantities[paramId]) pq->setValue((float)next);
			focusedSeg     = seg;
			lastClickedSeg = seg;
			flashAlpha     = 1.f;
			seenValue[seg] = (float)next;
			seenInit[seg]  = true;
			e.consume(this);
			return;
		}

		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			if (focusedSeg == 3) {
				int nodeIdx = hitChainNode(e.pos);
				if (nodeIdx >= 0) {
					module->setChainDepth(nodeIdx, 1.f);
					lastClickedSeg = 3;
					flashAlpha = 1.f;
					e.consume(this);
					return;
				}
			}
			if (hitSyncCenter(e.pos)) {
				if (auto* pq = module->paramQuantities[FmOperator::FM_SYNC_STRENGTH_PARAM]) {
					pq->setValue(1.f);
				}
				lastClickedSeg = 2;
				flashAlpha = 1.f;
				e.consume(this);
				return;
			}
		}

		if (e.action == GLFW_RELEASE) {
			if (draggingNode >= 0) {
				draggingNode = -1;
				dragAccum = Vec(0.f, 0.f);
				e.consume(this);
			}
			if (syncDragActive) {
				syncDragActive = false;
				e.consume(this);
			}
		}
	}

	void onDragMove(const event::DragMove& e) override {
		if (!module) return;
		if (syncDragActive) {
			syncDragAccumY += e.mouseDelta.y;
			const float newStrength = clamp(
				syncDragStart - syncDragAccumY / 50.f, 0.f, 1.f);
			if (auto* pq = module->paramQuantities[FmOperator::FM_SYNC_STRENGTH_PARAM]) {
				pq->setValue(newStrength);
			}
			e.consume(this);
			return;
		}
		if (draggingNode < 0) return;
		dragAccum.x += e.mouseDelta.x;
		dragAccum.y += e.mouseDelta.y;
		const float newDepth = clamp(dragStartDepth - dragAccum.y / 50.f, 0.f, 1.f);
		module->setChainDepth(draggingNode, newDepth);
		e.consume(this);
	}

	void onDragEnd(const event::DragEnd& e) override {
		(void)e;
		draggingNode = -1;
		dragAccum = Vec(0.f, 0.f);
		syncDragActive = false;
	}

	void step() override {
		TransparentWidget::step();
		if (!module) return;
		for (int s = 0; s < 4; s++) {
			const int pid = segParamId(s);
			if (pid < 0) continue;
			const float cur = module->params[pid].getValue();
			if (!seenInit[s]) { seenValue[s] = cur; seenInit[s] = true; continue; }
			if (std::abs(cur - seenValue[s]) > 0.5f
			    && focusedSeg != s && flashAlpha < 0.05f) focusedSeg = s;
			seenValue[s] = cur;
		}
		if (flashAlpha > 0.f) {
			float dt = APP->window->getFrameTime();
			if (dt <= 0.f || !std::isfinite(dt) || dt > 0.1f) dt = 1.f/60.f;
			flashAlpha -= dt * 2.5f;
			if (flashAlpha < 0.f) flashAlpha = 0.f;
		}
	}

	void draw(const DrawArgs& args) override {
		if (!module) return;
		if (!font) {
			font = APP->window->loadFont(
				asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		}
		const float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
		const float R = std::min(cx, cy) - 0.5f;
		drawBackground(args.vg, cx, cy, R);
		drawSegments(args.vg, cx, cy, R);
		drawCenter(args.vg, cx, cy, R);
	}

	void drawBackground(NVGcontext* vg, float cx, float cy, float R) {
		nvgBeginPath(vg); nvgCircle(vg, cx, cy, R);
		nvgFillColor(vg, nvgRGBA(6, 8, 12, 240)); nvgFill(vg);
		nvgBeginPath(vg); nvgCircle(vg, cx, cy, R * 0.94f);
		nvgFillColor(vg, nvgRGBA(2, 3, 5, 245)); nvgFill(vg);
		nvgBeginPath(vg); nvgCircle(vg, cx, cy, R);
		nvgStrokeColor(vg, nvgRGBA(255, 160, 60, 150));
		nvgStrokeWidth(vg, 1.2f); nvgStroke(vg);
		nvgBeginPath(vg); nvgCircle(vg, cx, cy, R * 0.68f);
		nvgStrokeColor(vg, nvgRGBA(255, 160, 60, 70));
		nvgStrokeWidth(vg, 0.7f); nvgStroke(vg);
	}

	void drawSegments(NVGcontext* vg, float cx, float cy, float R) {
		const float rIn = R * 0.70f, rOut = R * 0.96f;
		const float rMid = (rIn + rOut) * 0.5f;
		const float ringThickness = rOut - rIn;
		for (int seg = 0; seg < 4; seg++) {
			float a0, a1;
			segAngles(seg, a0, a1);
			const float gap = 0.07f;
			a0 += gap; a1 -= gap;
			NVGcolor col = segColor(seg);
			const bool focused = (focusedSeg == seg);
			{
				NVGcolor glow = col;
				glow.a = focused ? 0.28f : 0.08f;
				nvgBeginPath(vg); nvgArc(vg, cx, cy, rMid, a0, a1, NVG_CW);
				nvgStrokeWidth(vg, ringThickness + 4.f);
				nvgStrokeColor(vg, glow); nvgStroke(vg);
			}
			{
				NVGcolor c = col;
				c.a = focused ? 1.f : 0.45f;
				nvgBeginPath(vg); nvgArc(vg, cx, cy, rMid, a0, a1, NVG_CW);
				nvgStrokeWidth(vg, ringThickness * 0.72f);
				nvgStrokeColor(vg, c); nvgStroke(vg);
			}
			if (lastClickedSeg == seg && flashAlpha > 0.01f) {
				NVGcolor fl = nvgRGBA(255, 255, 255, (int)(255 * flashAlpha));
				nvgBeginPath(vg); nvgArc(vg, cx, cy, rMid, a0, a1, NVG_CW);
				nvgStrokeWidth(vg, ringThickness * 0.92f);
				nvgStrokeColor(vg, fl); nvgStroke(vg);
			}
		}
		if (font) {
			static const char* patLetters[8] = {"A","B","C","D","E","F","G","H"};
			const int patIdx = clamp(getPatIdx(), 0, 7);
			const float lx = cx + rMid * std::cos(M_PI);
			const float ly = cy + rMid * std::sin(M_PI);
			nvgFontFaceId(vg, font->handle);
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			const bool chainFocused = (focusedSeg == 3);
			nvgFontSize(vg, ringThickness * 1.15f);
			nvgFillColor(vg, nvgRGBA(0, 15, 8, chainFocused ? 255 : 220));
			nvgText(vg, lx, ly, patLetters[patIdx], nullptr);
		}
	}

	void drawCenter(NVGcontext* vg, float cx, float cy, float R) {
		if (!font) return;
		const float Rinner = R * 0.66f;
		if (focusedSeg == 2) { drawSyncCenter(vg, cx, cy, Rinner); return; }
		if (focusedSeg == 3) { drawMiniChain(vg, cx, cy, Rinner); return; }
		const int paramId = segParamId(focusedSeg);
		if (paramId < 0) return;
		const float val = module->params[paramId].getValue();
		std::string valStr = module->shortValueText(paramId, val, false);
		NVGcolor col = segColor(focusedSeg);
		nvgFontFaceId(vg, font->handle);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFontSize(vg, Rinner * 0.30f);
		nvgFillColor(vg, nvgRGBA(
			(int)(col.r * 220), (int)(col.g * 220), (int)(col.b * 220), 220));
		nvgText(vg, cx, cy - Rinner * 0.30f, segLabel(focusedSeg), nullptr);
		nvgFontSize(vg, Rinner * 0.55f);
		nvgFillColor(vg, nvgRGBA(220, 255, 230, 240));
		nvgText(vg, cx, cy + Rinner * 0.15f, valStr.c_str(), nullptr);
	}

	void drawSyncCenter(NVGcontext* vg, float cx, float cy, float R) {
		if (!font) return;
		const NVGcolor syncCol = segColor(2);
		const int syncMode = clamp(
			(int)std::round(module->params[FmOperator::FM_SYNC_PARAM].getValue()), 0, 3);
		static const char* syncNames[4] = { "OFF", "SFT", "HRD", "REV" };
		nvgFontFaceId(vg, font->handle);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFontSize(vg, R * 0.36f);
		nvgFillColor(vg, nvgRGBA(0, 255, 100, 245));
		nvgText(vg, cx, cy - R * 0.66f, syncNames[syncMode], nullptr);
		const float strength = clamp(
			module->params[FmOperator::FM_SYNC_STRENGTH_PARAM].getValue(), 0.f, 1.f);
		const float ringR = R * 0.42f;
		const float thick = R * 0.09f;
		{
			NVGcolor base = syncCol; base.a = 0.14f;
			nvgBeginPath(vg); nvgCircle(vg, cx, cy, ringR);
			nvgStrokeColor(vg, base); nvgStrokeWidth(vg, thick); nvgStroke(vg);
		}
		if (strength > 0.005f) {
			const float a0 = -M_PI * 0.5f;
			const float a1 = a0 + strength * 2.f * M_PI;
			NVGcolor arcCol = syncCol; arcCol.a = 1.f;
			nvgBeginPath(vg); nvgArc(vg, cx, cy, ringR, a0, a1, NVG_CW);
			nvgStrokeColor(vg, arcCol); nvgStrokeWidth(vg, thick);
			nvgLineCap(vg, NVG_ROUND); nvgStroke(vg);
		}
		char buf[16];
		snprintf(buf, sizeof(buf), "%d%%", (int)(strength * 100.f + 0.5f));
		nvgFontSize(vg, R * 0.42f);
		nvgFillColor(vg, nvgRGBA(240, 255, 250, 250));
		nvgText(vg, cx, cy + R * 0.02f, buf, nullptr);
		nvgFontSize(vg, R * 0.20f);
		nvgFillColor(vg, nvgRGBA(0, 255, 100, 245));
		nvgText(vg, cx, cy + R * 0.70f, "SYNC STRENGTH", nullptr);
		if (syncDragActive) {
			nvgFontSize(vg, R * 0.14f);
			nvgFillColor(vg, nvgRGBA(255, 255, 255, 200));
			nvgText(vg, cx, cy + R * 0.95f, "release to lock", nullptr);
		}
	}

	void drawWaveIcon(NVGcontext* vg, float x, float y, float r, int osc) {
		if (r < 1.5f) return;
		nvgSave(vg); nvgTranslate(vg, x, y); nvgBeginPath(vg);
		const float w = r * 1.35f, h = r * 0.85f;
		const float hw = w * 0.5f, hh = h * 0.5f;
		switch (osc) {
			case 0: {
				const int N = 10;
				for (int i = 0; i <= N; i++) {
					const float t = (float)i / N;
					const float px = -hw + t * w;
					const float py = -std::sin(t * 2.f * M_PI) * hh;
					if (i == 0) nvgMoveTo(vg, px, py); else nvgLineTo(vg, px, py);
				}
				break;
			}
			case 1:
				nvgMoveTo(vg, -hw, hh); nvgLineTo(vg, -hw*0.5f, -hh);
				nvgLineTo(vg, -hw*0.5f, hh); nvgLineTo(vg, 0.f, -hh);
				nvgLineTo(vg, 0.f, hh); nvgLineTo(vg, hw*0.5f, -hh);
				nvgLineTo(vg, hw*0.5f, hh); nvgLineTo(vg, hw, -hh);
				break;
			case 2:
				nvgMoveTo(vg, -hw, 0.f); nvgLineTo(vg, -hw*0.5f, -hh);
				nvgLineTo(vg, 0.f, 0.f); nvgLineTo(vg, hw*0.5f, hh);
				nvgLineTo(vg, hw, 0.f);
				break;
			case 3:
				nvgMoveTo(vg, -hw, -hh); nvgLineTo(vg, -hw*0.5f, -hh);
				nvgLineTo(vg, -hw*0.5f, hh); nvgLineTo(vg, 0.f, hh);
				nvgLineTo(vg, 0.f, -hh); nvgLineTo(vg, hw*0.5f, -hh);
				nvgLineTo(vg, hw*0.5f, hh); nvgLineTo(vg, hw, hh);
				break;
		}
		nvgStrokeColor(vg, nvgRGBA(8, 12, 18, 245));
		nvgStrokeWidth(vg, 1.1f);
		nvgLineJoin(vg, NVG_ROUND);
		nvgLineCap(vg, NVG_ROUND);
		nvgStroke(vg);
		nvgRestore(vg);
	}

	void drawMiniChain(NVGcontext* vg, float cx, float cy, float R) {
		if (!font) return;
		int order[4];
		for (int i = 0; i < 4; i++) order[i] = getOrder(i);
		const int* L = nodeLayout();
		const float layoutR = R * 0.58f;
		const float nodeR   = R * 0.24f;
		Vec pos[4];
		for (int i = 0; i < 4; i++) {
			const float ang = -M_PI * 0.5f + (M_PI * 0.5f) * i;
			pos[i] = Vec(cx + layoutR * std::cos(ang), cy + layoutR * std::sin(ang));
		}
		const NVGcolor nodeCol[4] = {
			nvgRGBA( 60, 255, 255, 255),
			nvgRGBA(255, 170,  60, 255),
			nvgRGBA( 60, 255, 120, 255),
			nvgRGBA(255, 100, 200, 255),
		};
		for (int osc = 0; osc < 4; osc++) {
			const int mod = order[osc];
			if (mod == osc) continue;
			int pOsc = -1, pMod = -1;
			for (int i = 0; i < 4; i++) {
				if (L[i] == osc) pOsc = i;
				if (L[i] == mod) pMod = i;
			}
			if (pOsc < 0 || pMod < 0) continue;
			const Vec p1 = pos[pMod], p2 = pos[pOsc];
			Vec d = p2.minus(p1);
			const float len = std::sqrt(d.x*d.x + d.y*d.y);
			if (len < 0.001f) continue;
			Vec dir = Vec(d.x/len, d.y/len);
			const Vec start = p1.plus(dir.mult(nodeR + 0.5f));
			const Vec end   = p2.minus(dir.mult(nodeR + 3.0f));
			NVGcolor col = nodeCol[mod]; col.a = 0.9f;
			nvgBeginPath(vg); nvgMoveTo(vg, start.x, start.y);
			nvgLineTo(vg, end.x, end.y);
			nvgStrokeColor(vg, col); nvgStrokeWidth(vg, 1.1f); nvgStroke(vg);
			const float aSz = 2.3f;
			const Vec back = Vec(-dir.x, -dir.y);
			const Vec pA = end.plus(dir.mult(aSz));
			const Vec pB = end.plus(back.mult(aSz).rotate( M_PI/6.f));
			const Vec pC = end.plus(back.mult(aSz).rotate(-M_PI/6.f));
			nvgBeginPath(vg);
			nvgMoveTo(vg, pA.x, pA.y);
			nvgLineTo(vg, pB.x, pB.y);
			nvgLineTo(vg, pC.x, pC.y);
			nvgClosePath(vg);
			nvgFillColor(vg, col); nvgFill(vg);
		}
		for (int osc = 0; osc < 4; osc++) {
			const int mod = order[osc];
			if (mod != osc) continue;
			int pOsc = -1;
			for (int i = 0; i < 4; i++) if (L[i] == osc) pOsc = i;
			if (pOsc < 0) continue;
			const Vec p = pos[pOsc];
			const float ang = -M_PI * 0.5f + (M_PI * 0.5f) * pOsc;
			const Vec out = Vec(std::cos(ang), std::sin(ang));
			const Vec c = p.plus(out.mult(nodeR + 3.5f));
			NVGcolor col = nodeCol[mod]; col.a = 0.9f;
			nvgBeginPath(vg); nvgCircle(vg, c.x, c.y, 2.2f);
			nvgStrokeColor(vg, col); nvgStrokeWidth(vg, 1.f); nvgStroke(vg);
		}
		for (int i = 0; i < 4; i++) {
			const Vec p = pos[i];
			const int osc = L[i];
			const float depth = module->getChainDepth(osc);
			const bool isHovered = (draggingNode == osc);
			const bool isDragging = (draggingNode == osc);
			nvgBeginPath(vg);
			nvgCircle(vg, p.x, p.y, nodeR + (isHovered ? 2.4f : 1.2f));
			NVGcolor halo = nodeCol[osc];
			halo.a = isHovered ? 0.35f : 0.18f;
			nvgFillColor(vg, halo); nvgFill(vg);
			nvgBeginPath(vg); nvgCircle(vg, p.x, p.y, nodeR);
			NVGcolor fill = nodeCol[osc]; fill.a = 0.55f;
			nvgFillColor(vg, fill); nvgFill(vg);
			nvgBeginPath(vg); nvgCircle(vg, p.x, p.y, nodeR);
			NVGcolor stroke = nodeCol[osc]; stroke.a = 1.f;
			nvgStrokeColor(vg, stroke); nvgStrokeWidth(vg, 0.9f); nvgStroke(vg);
			{
				NVGcolor base = nodeCol[osc]; base.a = 0.15f;
				nvgBeginPath(vg); nvgCircle(vg, p.x, p.y, nodeR + 2.2f);
				nvgStrokeColor(vg, base); nvgStrokeWidth(vg, 1.6f); nvgStroke(vg);
			}
			if (depth > 0.005f) {
				const float arcR = nodeR + 2.2f;
				const float a0 = -M_PI * 0.5f;
				const float a1 = a0 + depth * 2.f * M_PI;
				NVGcolor arcCol = nodeCol[osc]; arcCol.a = 1.f;
				nvgBeginPath(vg); nvgArc(vg, p.x, p.y, arcR, a0, a1, NVG_CW);
				nvgStrokeColor(vg, arcCol); nvgStrokeWidth(vg, 1.6f); nvgStroke(vg);
			}
			if (isDragging) {
				nvgFontFaceId(vg, font->handle);
				nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
				char buf[16];
				snprintf(buf, sizeof(buf), "%d%%", (int)(depth * 100.f + 0.5f));
				nvgFontSize(vg, nodeR * 0.72f);
				nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
				nvgText(vg, p.x, p.y, buf, nullptr);
			} else {
				drawWaveIcon(vg, p.x, p.y, nodeR, osc);
			}
		}
		if (draggingNode < 0) {
			nvgFontSize(vg, R * 0.16f);
			nvgFillColor(vg, nvgRGBA(150, 200, 255, 180));
			nvgText(vg, cx, cy + R * 0.86f, "drag node to set %", nullptr);
		}
	}
};

// ============================================================
// PSYDISPLAY
// ============================================================

struct PsyDisplay : TransparentWidget {
	FmOperator* module = nullptr;

	int    shownParamId = -1;
	float  shownValue   = 0.f;
	float  displayAge   = 999.f;
	float  textAlpha    = 0.f;

	std::shared_ptr<Font> fontMain;

	float prevValues[FmOperator::PARAMS_LEN] = {0.f};
	bool  prevInitialized = false;
	bool  lastPitchStepMode = false;

	static constexpr float HOLD_TIME       = 3.5f;
	static constexpr float FADE_TIME       = 1.0f;
	static constexpr float CHANGE_THRESHOLD = 0.002f;

	static constexpr float SIZE_MM  = 26.f;
	static constexpr float UI_SCALE = 0.75f;

	PsyDisplay() {
		const float mm = SIZE_MM * UI_SCALE;
		const float d  = mm2px(Vec(mm, mm)).x;
		box.size = Vec(d, d);
	}

	// 61 = 54 legacy + 7 new sub params
	static const std::array<int, 61>& watchedParams() {
		static const std::array<int, 61> watched = {{
			FmOperator::PITCH_STEP_MODE_PARAM,
			FmOperator::PITCH_BUS_MODE_PARAM,
			FmOperator::FM_MODE_PARAM,
			FmOperator::FM_ROUTING_PARAM,
			FmOperator::FM_SYNC_PARAM,
			FmOperator::FM_ENV_PARAM,
			FmOperator::FM_CHAIN_PATTERN_PARAM,
			FmOperator::FM_SYNC_STRENGTH_PARAM,
			FmOperator::TURBULENCE_MODE_PARAM,
			FmOperator::CV_MODE_PARAM,
			FmOperator::PITCH_PARAM_SINE,
			FmOperator::PITCH_PARAM_SAW,
			FmOperator::PITCH_PARAM_TRIANGLE,
			FmOperator::PITCH_PARAM_SQUARE,
			FmOperator::FM_PARAM,
			FmOperator::FM_AMOUNT_PARAM,
			FmOperator::SINE_WAVESHAPER_PARAM,
			FmOperator::SAW_WAVESHAPER_PARAM,
			FmOperator::TRIANGLE_WAVESHAPER_PARAM,
			FmOperator::SQUARE_WAVESHAPER_PARAM,
			FmOperator::SAW_WAVESHAPER_AMOUNT,
			FmOperator::TRIANGLE_WAVESHAPER_AMOUNT,
			FmOperator::SQUARE_WAVESHAPER_AMOUNT,
			FmOperator::PSYCHEDELIC_PARAM_TRIANGLE,
			FmOperator::PSYCHEDELIC_CV_KNOB_PARAM,
			FmOperator::VOLUME_PARAM_SINE,
			FmOperator::VOLUME_PARAM_SAW,
			FmOperator::VOLUME_PARAM_TRIANGLE,
			FmOperator::VOLUME_PARAM_SQUARE,
			FmOperator::RESAMPLE,
			FmOperator::HARMONIC_SUM_PARAM,
			FmOperator::PHASE_MOD_PARAM_SINE,
			FmOperator::PHASE_MOD_PARAM_SAW,
			FmOperator::PHASE_MOD_PARAM_TRIANGLE,
			FmOperator::PHASE_MOD_PARAM_SQUARE,
			FmOperator::PSY_MACRO_PARAM,
			FmOperator::DARK_MACRO_PARAM,
			FmOperator::MOTION_MACRO_PARAM,
			FmOperator::FERAL_MACRO_PARAM,
			FmOperator::PITCH_BUS_DEPTH_SINE,
			FmOperator::PITCH_BUS_DEPTH_SAW,
			FmOperator::PITCH_BUS_DEPTH_TRIANGLE,
			FmOperator::PITCH_BUS_DEPTH_SQUARE,
			FmOperator::PHASE_DIST_PARAM,
			FmOperator::BIT_DEPTH_PARAM,
			FmOperator::SRR_RATE_PARAM,
			FmOperator::OUT_DRIVE_PARAM,
			FmOperator::FM_MOD_RATIO_PARAM,
			FmOperator::FM_MOD_WAVE_PARAM,
			FmOperator::DARK_SUB_LEVEL_PARAM,
			FmOperator::MASTER_MACRO_PARAM,
			FmOperator::FM_MOD_PITCH_PARAM,
			FmOperator::FM_MOD_PSY_CV_DEPTH_PARAM,
			FmOperator::FM_MOD_MORPH_PARAM,
			// NEW SUB PARAMS
			FmOperator::SUB_WAVE_PARAM,
			FmOperator::SUB_FREQ_MODE_PARAM,
			FmOperator::SUB_PITCH_PARAM,
			FmOperator::SUB_PHASE_PARAM,
			FmOperator::SUB_FM_ROUTE_PARAM,
			FmOperator::SUB_RING_PARAM,
			FmOperator::SUB_DRIFT_PARAM,
		}};
		return watched;
	}

	void step() override {
		TransparentWidget::step();
		if (!module) return;
		float dt = APP->window->getFrameTime();
		if (dt <= 0.f || !std::isfinite(dt)) dt = 1.f / 60.f;
		if (dt > 0.1f) dt = 0.1f;

		if (!prevInitialized) {
			for (int i = 0; i < FmOperator::PARAMS_LEN; i++) {
				prevValues[i] = module->params[i].getValue();
			}
			prevInitialized = true;
		}

		const auto& watched = watchedParams();
		int changedId = -1;
		float changedValue = 0.f;
		for (size_t i = 0; i < watched.size(); i++) {
			const int id = watched[i];
			const float cur = module->params[id].getValue();
			if (std::abs(cur - prevValues[id]) > CHANGE_THRESHOLD) {
				prevValues[id] = cur;
				changedId = id;
				changedValue = cur;
				break;
			}
		}

		if (changedId >= 0) {
			const bool psm =
				module->params[FmOperator::PITCH_STEP_MODE_PARAM].getValue() > 0.5f;
			shownParamId = changedId;
			shownValue = changedValue;
			lastPitchStepMode = psm;
			displayAge = 0.f;
			textAlpha = 1.f;
		}

		if (shownParamId >= FmOperator::PITCH_PARAM_SINE &&
		    shownParamId <= FmOperator::PITCH_PARAM_SQUARE) {
			const bool psm =
				module->params[FmOperator::PITCH_STEP_MODE_PARAM].getValue() > 0.5f;
			if (psm != lastPitchStepMode) {
				shownValue = module->params[shownParamId].getValue();
				lastPitchStepMode = psm;
				displayAge = 0.f;
				textAlpha = 1.f;
			}
		}

		displayAge += dt;
		if (displayAge > 100.f) displayAge = 100.f;

		float targetAlpha = 0.f;
		if (displayAge < HOLD_TIME) targetAlpha = 1.f;
		else if (displayAge < HOLD_TIME + FADE_TIME) {
			const float p = (displayAge - HOLD_TIME) / FADE_TIME;
			targetAlpha = clamp(1.f - p, 0.f, 1.f);
		}

		if (shownParamId < 0) {
			textAlpha = 0.f;
		} else {
			const float coeff = 1.f - std::exp(-dt / 0.10f);
			textAlpha += (targetAlpha - textAlpha) * coeff;
			textAlpha = clamp(textAlpha, 0.f, 1.f);
			if (textAlpha < 0.01f) textAlpha = 0.f;
		}
	}

	void draw(const DrawArgs& args) override {
		if (!module) return;
		const float cx = box.size.x * 0.5f;
		const float cy = box.size.y * 0.5f;
		const float R  = std::min(cx, cy) - 1.f;
		drawBackground(args.vg, cx, cy, R);
		drawStatusDots(args.vg, cx, cy, R);
		drawSumRing(args.vg, cx, cy, R);
		drawQuadWaves(args.vg, cx, cy, R);
		drawCenterWave(args.vg, cx, cy, R);
		drawCenterText(args.vg, cx, cy, R);
	}

	void drawBackground(NVGcontext* vg, float cx, float cy, float R) {
		nvgBeginPath(vg); nvgCircle(vg, cx, cy, R);
		nvgFillColor(vg, nvgRGBA(6, 8, 12, 235)); nvgFill(vg);
		nvgBeginPath(vg); nvgCircle(vg, cx, cy, R * 0.94f);
		nvgFillColor(vg, nvgRGBA(2, 3, 5, 240)); nvgFill(vg);
		nvgBeginPath(vg); nvgCircle(vg, cx, cy, R);
		nvgStrokeColor(vg, nvgRGBA(255, 160, 60, 150));
		nvgStrokeWidth(vg, 1.2f); nvgStroke(vg);
		nvgBeginPath(vg); nvgCircle(vg, cx, cy, R * 0.94f);
		nvgStrokeColor(vg, nvgRGBA(255, 160, 60, 40));
		nvgStrokeWidth(vg, 0.6f); nvgStroke(vg);
	}

	void drawStatusDots(NVGcontext* vg, float cx, float cy, float R) {
		struct DotInfo { int paramId; NVGcolor color; };
		const DotInfo dots[8] = {
			{ FmOperator::PSY_MACRO_PARAM,        nvgRGBA( 60, 255, 120, 255) },
			{ FmOperator::DARK_MACRO_PARAM,       nvgRGBA( 60, 160, 255, 255) },
			{ FmOperator::MOTION_MACRO_PARAM,     nvgRGBA(255, 220,  60, 255) },
			{ FmOperator::FERAL_MACRO_PARAM,      nvgRGBA(255,  70,  70, 255) },
			{ FmOperator::FM_PARAM,               nvgRGBA(255, 160,  60, 255) },
			{ FmOperator::PITCH_BUS_MODE_PARAM,   nvgRGBA( 60, 255, 255, 255) },
			{ FmOperator::CV_MODE_PARAM,          nvgRGBA(180, 100, 255, 255) },
			{ FmOperator::TURBULENCE_MODE_PARAM,  nvgRGBA(255, 100, 180, 255) },
		};
		const float dotR = R * 0.92f;
		const float dotSize = 1.6f;
		for (int i = 0; i < 8; i++) {
			const float angle = -M_PI * 0.5f + 2.f * (float)M_PI * i / 8.f;
			const float x = cx + dotR * std::cos(angle);
			const float y = cy + dotR * std::sin(angle);
			float b = 0.f;
			const int pid = dots[i].paramId;
			if (pid == FmOperator::FM_PARAM) {
				const float v = module->params[pid].getValue();
				b = clamp((v + 1.2f) / 4.43f, 0.f, 1.f);
			} else if (pid == FmOperator::PITCH_BUS_MODE_PARAM ||
			           pid == FmOperator::CV_MODE_PARAM ||
			           pid == FmOperator::TURBULENCE_MODE_PARAM) {
				const float v = module->params[pid].getValue();
				b = 0.35f + 0.65f * clamp(v / 4.f, 0.f, 1.f);
			} else {
				b = clamp(module->params[pid].getValue(), 0.f, 1.f);
			}
			NVGcolor col = dots[i].color;
			col.a = 0.25f + 0.75f * b;
			nvgBeginPath(vg); nvgCircle(vg, x, y, dotSize * 2.2f);
			NVGcolor glowCol = col; glowCol.a = 0.15f * b;
			nvgFillColor(vg, glowCol); nvgFill(vg);
			nvgBeginPath(vg); nvgCircle(vg, x, y, dotSize);
			nvgFillColor(vg, col); nvgFill(vg);
		}
	}

	void drawSumRing(NVGcontext* vg, float cx, float cy, float R) {
		const int N = 128;
		const float rMin = R * 0.72f, rMax = R * 0.90f;
		const float rMid = (rMin + rMax) * 0.5f;
		const float rAmp = (rMax - rMin) * 0.5f;
		const int ss = FmOperator::WAVE_SNAPSHOT_SIZE;
		const float* buf = module->snapSum;
		float peak = 0.f;
		for (int i = 0; i < ss; i++) {
			const float a = std::abs(buf[i]);
			if (a > peak) peak = a;
		}
		const float gain = (peak > 0.02f) ? (0.65f / std::max(peak, 0.05f)) : 1.f;
		nvgBeginPath(vg);
		for (int i = 0; i <= N; i++) {
			const int ii = i % N;
			const int idx = (ii * ss) / N;
			float s = clamp(buf[idx] * gain, -1.f, 1.f);
			const float ang = -M_PI * 0.5f + 2.f * (float)M_PI * ii / N;
			const float r = rMid + s * rAmp;
			const float x = cx + r * std::cos(ang);
			const float y = cy + r * std::sin(ang);
			if (i == 0) nvgMoveTo(vg, x, y); else nvgLineTo(vg, x, y);
		}
		nvgClosePath(vg);
		nvgStrokeColor(vg, nvgRGBA(20, 100, 20, 90));
		nvgStrokeWidth(vg, 3.0f); nvgStroke(vg);
		nvgBeginPath(vg);
		for (int i = 0; i <= N; i++) {
			const int ii = i % N;
			const int idx = (ii * ss) / N;
			float s = clamp(buf[idx] * gain, -1.f, 1.f);
			const float ang = -M_PI * 0.5f + 2.f * (float)M_PI * ii / N;
			const float r = rMid + s * rAmp;
			const float x = cx + r * std::cos(ang);
			const float y = cy + r * std::sin(ang);
			if (i == 0) nvgMoveTo(vg, x, y); else nvgLineTo(vg, x, y);
		}
		nvgClosePath(vg);
		nvgStrokeColor(vg, nvgRGBA(0, 200, 40, 240));
		nvgStrokeWidth(vg, 1.1f); nvgStroke(vg);
	}

	void drawQuadWaves(NVGcontext* vg, float cx, float cy, float R) {
		const float rMin = R * 0.44f, rMax = R * 0.74f;
		const float rMid = (rMin + rMax) * 0.5f;
		const float rAmp = (rMax - rMin) * 0.5f;
		struct Quad { float a0, a1; const float* buf; NVGcolor color; };
		const Quad quads[4] = {
			{ -M_PI*0.5f, 0.f,        module->snapSine,   nvgRGBA( 60, 255, 255, 125) },
			{ 0.f,        M_PI*0.5f,  module->snapSquare, nvgRGBA(255, 100, 200, 125) },
			{ M_PI*0.5f,  M_PI,       module->snapTri,    nvgRGBA( 60, 255, 120, 125) },
			{ M_PI,       M_PI*1.5f,  module->snapSaw,    nvgRGBA(255, 170,  60, 125) },
		};
		const int N = 48;
		const int ss = FmOperator::WAVE_SNAPSHOT_SIZE;
		for (int q = 0; q < 4; q++) {
			const float* buf = quads[q].buf;
			float peak = 0.f;
			for (int i = 0; i < ss; i++) {
				const float a = std::abs(buf[i]);
				if (a > peak) peak = a;
			}
			const float gain = (peak > 0.02f) ? (0.55f / std::max(peak, 0.05f)) : 1.f;
			{
				NVGcolor glow = quads[q].color; glow.a = 0.22f;
				nvgBeginPath(vg);
				for (int i = 0; i <= N; i++) {
					const int ii = i % N;
					const int idx = (ii * ss) / N;
					float s = clamp(buf[idx] * gain, -1.f, 1.f);
					const float t   = (float)ii / N;
					const float ang = quads[q].a0 + (quads[q].a1 - quads[q].a0) * t;
					const float r = rMid + s * rAmp;
					const float x = cx + r * std::cos(ang);
					const float y = cy + r * std::sin(ang);
					if (i == 0) nvgMoveTo(vg, x, y); else nvgLineTo(vg, x, y);
				}
				nvgStrokeColor(vg, glow); nvgStrokeWidth(vg, 2.5f); nvgStroke(vg);
			}
			nvgBeginPath(vg);
			for (int i = 0; i <= N; i++) {
				const int ii = i % N;
				const int idx = (ii * ss) / N;
				float s = clamp(buf[idx] * gain, -1.f, 1.f);
				const float t   = (float)ii / N;
				const float ang = quads[q].a0 + (quads[q].a1 - quads[q].a0) * t;
				const float r = rMid + s * rAmp;
				const float x = cx + r * std::cos(ang);
				const float y = cy + r * std::sin(ang);
				if (i == 0) nvgMoveTo(vg, x, y); else nvgLineTo(vg, x, y);
			}
			nvgStrokeColor(vg, quads[q].color);
			nvgStrokeWidth(vg, 0.7f); nvgStroke(vg);
		}
	}

	void drawCenterWave(NVGcontext* vg, float cx, float cy, float R) {
		const float alpha = clamp(1.f - textAlpha, 0.f, 1.f);
		if (alpha < 0.02f) return;
		const int ss = FmOperator::WAVE_SNAPSHOT_SIZE;
		const float* buf = module->snapSum;
		float peak = 0.f;
		for (int i = 0; i < ss; i++) {
			const float a = std::abs(buf[i]);
			if (a > peak) peak = a;
		}
		const float gain = (peak > 0.02f) ? (0.85f / std::max(peak, 0.05f)) : 1.f;
		const float w = R * 0.66f;
		const float h = R * 0.26f;
		const float x0 = cx - w * 0.5f;
		nvgBeginPath(vg);
		nvgMoveTo(vg, x0, cy);
		nvgLineTo(vg, x0 + w, cy);
		nvgStrokeColor(vg, nvgRGBA(0, 255, 100, (int)(60 * alpha)));
		nvgStrokeWidth(vg, 0.5f); nvgStroke(vg);
		nvgBeginPath(vg);
		for (int i = 0; i < ss; i++) {
			const float t = (float)i / (ss - 1);
			const float px = x0 + t * w;
			const float s = clamp(buf[i] * gain, -1.f, 1.f);
			const float py = cy - s * h;
			if (i == 0) nvgMoveTo(vg, px, py); else nvgLineTo(vg, px, py);
		}
		nvgStrokeColor(vg, nvgRGBA(20, 255, 80, (int)(100 * alpha)));
		nvgStrokeWidth(vg, 2.5f); nvgStroke(vg);
		nvgBeginPath(vg);
		for (int i = 0; i < ss; i++) {
			const float t = (float)i / (ss - 1);
			const float px = x0 + t * w;
			const float s = clamp(buf[i] * gain, -1.f, 1.f);
			const float py = cy - s * h;
			if (i == 0) nvgMoveTo(vg, px, py); else nvgLineTo(vg, px, py);
		}
		nvgStrokeColor(vg, nvgRGBA(0, 255, 100, (int)(240 * alpha)));
		nvgStrokeWidth(vg, 1.1f); nvgStroke(vg);
	}

	void drawCenterText(NVGcontext* vg, float cx, float cy, float R) {
		(void)R;
		if (!fontMain) {
			fontMain = APP->window->loadFont(
				asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		}
		if (!fontMain) return;
		if (shownParamId < 0) return;
		if (textAlpha < 0.01f) return;
		nvgFontFaceId(vg, fontMain->handle);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		const float a = clamp(textAlpha, 0.f, 1.f);
		const char* name = FmOperator::shortParamName(shownParamId);
		nvgFontSize(vg, 6.5f);
		nvgFillColor(vg, nvgRGBA(0, 255, 70, (int)(255 * a)));
		nvgText(vg, cx, cy - 6.5f, name, nullptr);
		const bool psm =
			module->params[FmOperator::PITCH_STEP_MODE_PARAM].getValue() > 0.5f;
		std::string val = module->shortValueText(shownParamId, shownValue, psm);
		nvgFontSize(vg, 11.f);
		nvgFillColor(vg, nvgRGBA(0, 255, 100, (int)(240 * a)));
		nvgText(vg, cx, cy + 2.f, val.c_str(), nullptr);
	}
};

// ============================================================
// HUTARA SVG LOGO LIGHT
// ============================================================

struct HutaraLogoSvgLight : TransparentWidget {
	FmOperator* module = nullptr;
	std::shared_ptr<Svg> svgLed;
	float svgW = 280.f, svgH = 300.f, scale = 1.0f;
	HutaraLogoSvgLight() {
		svgLed = Svg::load(asset::plugin(pluginInstance, "res/cv-all.svg"));
		if (!svgLed) WARN("Hutara: failed to load res/cv-all.svg");
		box.size = Vec(svgW * scale, svgH * scale);
	}
	void draw(const DrawArgs& args) override {
		if (!svgLed) return;
		float b = 0.f;
		if (module) b = clamp(module->lights[FmOperator::HEXAGRAM_LIGHT].getBrightness(), 0.f, 1.f);
		const float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
		nvgSave(args.vg);
		nvgTranslate(args.vg, cx, cy);
		nvgScale(args.vg, scale, scale);
		nvgTranslate(args.vg, -svgW * 0.5f, -svgH * 0.5f);
		nvgGlobalAlpha(args.vg, 1.0f);
		svgLed->draw(args.vg);
		nvgRestore(args.vg);
		if (b > 0.005f) {
			nvgSave(args.vg);
			nvgTranslate(args.vg, cx, cy);
			nvgScale(args.vg, scale, scale);
			nvgTranslate(args.vg, -svgW * 0.5f, -svgH * 0.5f);
			nvgGlobalCompositeBlendFunc(args.vg, NVG_LIGHTER, NVG_ONE);
			nvgGlobalAlpha(args.vg, b);
			svgLed->draw(args.vg);
			nvgRestore(args.vg);
		}
	}
};

struct FmAmountSvgLight : TransparentWidget {
	FmOperator* module = nullptr;
	std::shared_ptr<Svg> svg;
	float svgW = 280.f, svgH = 300.f, scale = 0.42f;
	FmAmountSvgLight() {
		svg = Svg::load(asset::plugin(pluginInstance, "res/Hutara-vcv-logo3.svg"));
		if (!svg) WARN("Hutara: failed to load res/Hutara-vcv-logo3.svg");
		box.size = Vec(svgW * scale, svgH * scale);
	}
	void draw(const DrawArgs& args) override {
		if (!svg) return;
		float b = 0.f;
		if (module) b = clamp(module->lights[FmOperator::FM_AMOUNT_LIGHT].getBrightness(), 0.f, 1.f);
		const float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
		nvgSave(args.vg);
		nvgTranslate(args.vg, cx, cy);
		nvgScale(args.vg, scale, scale);
		nvgTranslate(args.vg, -svgW * 0.5f, -svgH * 0.5f);
		nvgGlobalAlpha(args.vg, 0.32f + 0.40f * b);
		svg->draw(args.vg);
		nvgRestore(args.vg);
		if (b > 0.05f) {
			nvgSave(args.vg);
			nvgTranslate(args.vg, cx, cy);
			nvgScale(args.vg, scale, scale);
			nvgTranslate(args.vg, -svgW * 0.5f, -svgH * 0.5f);
			nvgGlobalCompositeBlendFunc(args.vg, NVG_LIGHTER, NVG_ONE);
			nvgGlobalAlpha(args.vg, b);
			svg->draw(args.vg);
			nvgRestore(args.vg);
		}
	}
};

// ============================================================
// WIDGET
// ============================================================

struct FmOperatorWidget : ModuleWidget {

	FmOperatorWidget(FmOperator* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/HutaraFm.svg")));

		SvgLight* svgLight = createWidget<SvgLight>(mm2px(Vec(60.f, 50.f)));
		svgLight->setSvg(APP->window->loadSvg(
			asset::plugin(pluginInstance, "res/mandala.svg")));
		addChild(svgLight);

		HutaraLogoSvgLight* hutaraLogo = new HutaraLogoSvgLight();
		hutaraLogo->module = module;
		{
			const Vec center = mm2px(Vec(57.24f, 40.263f));
			hutaraLogo->box.pos = center.minus(hutaraLogo->box.size.mult(0.5f));
		}
		addChild(hutaraLogo);

		{
			FmAmountSvgLight* fmLed = new FmAmountSvgLight();
			fmLed->module = module;
			const Vec center = mm2px(Vec(57.24f, 96.4f));
			fmLed->box.pos = center.minus(fmLed->box.size.mult(0.5f));
			addChild(fmLed);
		}

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		{
			PsyDisplay* display = new PsyDisplay();
			display->module = module;
			const Vec center = mm2px(Vec(57.24f, 16.f));
			display->box.pos = center.minus(display->box.size.mult(0.5f));
			addChild(display);
		}
		{
			FmEngineDisplay* fmDisp = new FmEngineDisplay();
			fmDisp->module = module;
			const Vec center = mm2px(Vec(57.24f, 70.f));
			fmDisp->box.pos = center.minus(fmDisp->box.size.mult(0.5f));
			addChild(fmDisp);
		}

		addParam(createParamCentered<MyPrivateSmallKnob>(
			mm2px(Vec(65.f, 55.f)), module, FmOperator::FM_MOD_PITCH_PARAM));
		addParam(createParamCentered<PitchBusDepthKnob>(
			mm2px(Vec(57.24f, 57.f)), module, FmOperator::FM_MOD_PSY_CV_DEPTH_PARAM));

		addParam(createParamCentered<RoundSmallBlackKnob>(
			mm2px(Vec(24.24f, 23.063f)), module, FmOperator::PITCH_PARAM_TRIANGLE));
		addParam(createParamCentered<RoundSmallBlackKnob>(
			mm2px(Vec(24.24f, 38.f)), module, FmOperator::PITCH_PARAM_SINE));
		addParam(createParamCentered<RoundSmallBlackKnob>(
			mm2px(Vec(24.24f, 51.063f)), module, FmOperator::PITCH_PARAM_SAW));
		addParam(createParamCentered<RoundSmallBlackKnob>(
			mm2px(Vec(24.24f, 64.063f)), module, FmOperator::PITCH_PARAM_SQUARE));

		addParam(createParamCentered<PitchBusDepthKnob>(
			mm2px(Vec(45.f, 23.063f)), module, FmOperator::PITCH_BUS_DEPTH_TRIANGLE));
		addParam(createParamCentered<PitchBusDepthKnob>(
			mm2px(Vec(45.f, 38.f)), module, FmOperator::PITCH_BUS_DEPTH_SINE));
		addParam(createParamCentered<PitchBusDepthKnob>(
			mm2px(Vec(45.f, 51.063f)), module, FmOperator::PITCH_BUS_DEPTH_SAW));
		addParam(createParamCentered<PitchBusDepthKnob>(
			mm2px(Vec(45.f, 64.063f)), module, FmOperator::PITCH_BUS_DEPTH_SQUARE));

		addParam(createParamCentered<MySynthTechAlco>(
			mm2px(Vec(40.f, 92.055f)), module, FmOperator::FM_PARAM));
		addParam(createParamCentered<MyPrivateSmallKnob>(
			mm2px(Vec(76.f, 96.4f)), module, FmOperator::FM_AMOUNT_PARAM));

		addParam(createParamCentered<NKKSmall>(
			mm2px(Vec(52.f, 83.55f)), module, FmOperator::FM_MODE_PARAM));
		addParam(createParamCentered<NKKSmall>(
			mm2px(Vec(62.5f, 83.55f)), module, FmOperator::FM_ROUTING_PARAM));

		addParam(createParamCentered<PDKnob>(
			mm2px(Vec(40.f, 80.f)), module, FmOperator::FM_ENV_PARAM));

		addParam(createParamCentered<MyPrivateSmallKnob>(
			mm2px(Vec(89.f, 38.f)), module, FmOperator::SINE_WAVESHAPER_PARAM));
		addParam(createParamCentered<MyPrivateSmallKnob>(
			mm2px(Vec(89.f, 50.f)), module, FmOperator::SAW_WAVESHAPER_PARAM));
		addParam(createParamCentered<CVAMOUNTKnob>(
			mm2px(Vec(107.f, 50.f)), module, FmOperator::SAW_WAVESHAPER_AMOUNT));
		addParam(createParamCentered<MyPrivateSmallKnob>(
			mm2px(Vec(89.24f, 23.63f)), module, FmOperator::TRIANGLE_WAVESHAPER_PARAM));
		addParam(createParamCentered<CVAMOUNTKnob>(
			mm2px(Vec(106.24f, 23.63f)), module, FmOperator::TRIANGLE_WAVESHAPER_AMOUNT));
		addParam(createParamCentered<MyPrivateSmallKnob>(
			mm2px(Vec(89.f, 64.f)), module, FmOperator::SQUARE_WAVESHAPER_PARAM));
		addParam(createParamCentered<CVAMOUNTKnob>(
			mm2px(Vec(106.f, 64.f)), module, FmOperator::SQUARE_WAVESHAPER_AMOUNT));

		addParam(createParamCentered<CKSSThree>(
			mm2px(Vec(81.f, 40.f)), module, FmOperator::CV_MODE_PARAM));

		addChild(createLightCentered<SmallLight<GreenLight>>(
			mm2px(Vec(77.5f, 43.f)), module, FmOperator::CV_MODE_LIGHT_0));
		addChild(createLightCentered<SmallLight<YellowLight>>(
			mm2px(Vec(77.5f, 40.f)), module, FmOperator::CV_MODE_LIGHT_1));
		addChild(createLightCentered<SmallLight<RedLight>>(
			mm2px(Vec(77.5f, 37.f)), module, FmOperator::CV_MODE_LIGHT_2));

		addParam(createParamCentered<MyPrivateSmallRedKnob>(
			mm2px(Vec(94.f, 82.f)), module, FmOperator::RESAMPLE));
		addParam(createParamCentered<MyPrivateSmallKnob>(
			mm2px(Vec(92.f, 94.f)), module, FmOperator::HARMONIC_SUM_PARAM));

		addParam(createParamCentered<PsyKnobSmall>(
			mm2px(Vec(6.f, 84.5f)), module, FmOperator::PSY_MACRO_PARAM));
		addParam(createParamCentered<PsyKnobSmall>(
			mm2px(Vec(6.f, 94.5f)), module, FmOperator::DARK_MACRO_PARAM));
		addParam(createParamCentered<PsyKnobSmall>(
			mm2px(Vec(6.f, 105.5f)), module, FmOperator::MOTION_MACRO_PARAM));
		addParam(createParamCentered<PsyKnobSmall>(
			mm2px(Vec(6.f, 116.f)), module, FmOperator::FERAL_MACRO_PARAM));

		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(15.f, 86.5f)), module, FmOperator::PSY_MACRO_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(15.f, 96.5f)), module, FmOperator::DARK_MACRO_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(15.f, 107.5f)), module, FmOperator::MOTION_MACRO_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(15.f, 118.f)), module, FmOperator::FERAL_MACRO_CV_INPUT));

		addParam(createParamCentered<NKKTiny>(
			mm2px(Vec(21.5f, 107.f)), module, FmOperator::TURBULENCE_MODE_PARAM));

		addParam(createParamCentered<PitchBusModeSwitch>(
			mm2px(Vec(57.24f, 49.5f)), module, FmOperator::PITCH_BUS_MODE_PARAM));

		addParam(createParamCentered<PitchStepModeButton>(
			mm2px(Vec(39.f, 11.f)), module, FmOperator::PITCH_STEP_MODE_PARAM));

		addChild(createLightCentered<SmallLight<YellowLight>>(
			mm2px(Vec(39.f, 7.f)), module, FmOperator::PITCH_STEP_MODE_LIGHT));

		addParam(createParamCentered<RoundBlackKnob>(
			mm2px(Vec(37.f, 108.4f)), module, FmOperator::VOLUME_PARAM_TRIANGLE));
		addParam(createParamCentered<RoundBlackKnob>(
			mm2px(Vec(50.f, 108.713f)), module, FmOperator::VOLUME_PARAM_SINE));
		addParam(createParamCentered<RoundBlackKnob>(
			mm2px(Vec(63.f, 108.4f)), module, FmOperator::VOLUME_PARAM_SAW));
		addParam(createParamCentered<RoundBlackKnob>(
			mm2px(Vec(76.24f, 108.4f)), module, FmOperator::VOLUME_PARAM_SQUARE));

		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(20.24f, 10.f)), module, FmOperator::PITCH_INPUT_ALL));
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(33.24f, 23.f)), module, FmOperator::PITCH_INPUT_TRIANGLE));
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(33.24f, 38.f)), module, FmOperator::PITCH_INPUT_SINE));
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(33.24f, 50.f)), module, FmOperator::PITCH_INPUT_SAW));
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(33.24f, 64.f)), module, FmOperator::PITCH_INPUT_SQUARE));

		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(98.f, 23.f)), module, FmOperator::PSYCHEDELIC_CV_INPUT_TRIANGLE));
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(98.f, 50.f)), module, FmOperator::PSYCHEDELIC_CV_INPUT_SAW));
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(98.f, 64.f)), module, FmOperator::PSYCHEDELIC_CV_INPUT_SQUARE));

		addInput(createInputCentered<CL1362Port>(
			mm2px(Vec(57.24f, 96.4f)), module, FmOperator::FM_INPUT));
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(68.f, 97.5f)), module, FmOperator::FM_AMOUNT_INPUT));

		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(93.f, 102.f)), module, FmOperator::RESAMPLE_INPUT));

		addParam(createParamCentered<MyPrivateSmallKnob>(
			mm2px(Vec(6.f, 23.f)), module, FmOperator::PHASE_MOD_PARAM_TRIANGLE));
		addParam(createParamCentered<MyPrivateSmallKnob>(
			mm2px(Vec(6.f, 38.f)), module, FmOperator::PHASE_MOD_PARAM_SINE));
		addParam(createParamCentered<MyPrivateSmallKnob>(
			mm2px(Vec(6.f, 50.f)), module, FmOperator::PHASE_MOD_PARAM_SAW));
		addParam(createParamCentered<MyPrivateSmallKnob>(
			mm2px(Vec(6.f, 64.f)), module, FmOperator::PHASE_MOD_PARAM_SQUARE));

		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(15.f, 23.f)), module, FmOperator::PHASE_MOD_INPUT_TRIANGLE));
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(15.f, 38.f)), module, FmOperator::PHASE_MOD_INPUT_SINE));
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(15.f, 50.f)), module, FmOperator::PHASE_MOD_INPUT_SAW));
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(15.f, 64.f)), module, FmOperator::PHASE_MOD_INPUT_SQUARE));

		addOutput(createOutputCentered<DarkPJ301MPort>(
			mm2px(Vec(37.f, 118.713f)), module, FmOperator::TRIANGLE_OUTPUT));
		addOutput(createOutputCentered<DarkPJ301MPort>(
			mm2px(Vec(50.f, 118.713f)), module, FmOperator::SINE_OUTPUT));
		addOutput(createOutputCentered<DarkPJ301MPort>(
			mm2px(Vec(63.f, 118.713f)), module, FmOperator::SAW_OUTPUT));
		addOutput(createOutputCentered<DarkPJ301MPort>(
			mm2px(Vec(76.f, 118.713f)), module, FmOperator::SQUARE_OUTPUT));
		addOutput(createOutputCentered<PJ3410Port>(
			mm2px(Vec(105.24f, 102.f)), module, FmOperator::FINAL_OUTPUT));
		addOutput(createOutputCentered<PJ3410Port>(
			mm2px(Vec(105.50f, 118.f)), module, FmOperator::SUM_OUTPUT));

		addChild(createLightCentered<SmallLight<GreenLight>>(
			mm2px(Vec(57.24f, 82.5f)), module, FmOperator::LED_LIGHT));
		addChild(createLightCentered<SmallLight<BlueLight>>(
			mm2px(Vec(57.24f, 85.f)), module, FmOperator::LED_RED_LIGHT));
		addChild(createLightCentered<SmallLight<RedLight>>(
			mm2px(Vec(105.50f, 112.f)), module, FmOperator::FINAL_OUTPUT_LIGHT));

		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(57.24f, 40.263f)), module, FmOperator::PSYCHEDELIC_CV_INPUT_FOR_All));
		addParam(createParamCentered<MyPrivateSmallKnob>(
			mm2px(Vec(57.24f, 32.f)), module, FmOperator::PSYCHEDELIC_CV_KNOB_PARAM));

		addParam(createParamCentered<PDKnob>(
			mm2px(Vec(74.f, 80.f)), module, FmOperator::PHASE_DIST_PARAM));

		addParam(createParamCentered<CrushKnob>(
			mm2px(Vec(102.f, 88.3f)), module, FmOperator::BIT_DEPTH_PARAM));
		addParam(createParamCentered<CrushKnob>(
			mm2px(Vec(109.f, 88.f)), module, FmOperator::SRR_RATE_PARAM));
		addParam(createParamCentered<MyPrivateSmallRedKnob>(
			mm2px(Vec(107.f, 79.f)), module, FmOperator::OUT_DRIVE_PARAM));
	}

	void appendContextMenu(Menu* menu) override {
		ModuleWidget::appendContextMenu(menu);
		FmOperator* m = dynamic_cast<FmOperator*>(module);
		if (!m) return;

		static const std::vector<std::string> ratioLabels =
			{ "1:1", "2:1", "3:1", "4:1", "6:1", "8:1" };

		menu->addChild(new MenuSeparator());

		menu->addChild(createIndexSubmenuItem("FM Source Morph Shape",
			{"Sine", "Folded", "Chebyshev", "Asymmetric", "Sync", "Formant"},
			[m]() {
				const float cur = m->params[FmOperator::SINE_WAVESHAPER_PARAM].getValue();
				const float pos = clamp(cur, 0.f, 1.f) * 5.f;
				return clamp((int)std::round(pos), 0, 5);
			},
			[m](int idx) {
				m->params[FmOperator::SINE_WAVESHAPER_PARAM].setValue(
					clamp(idx, 0, 5) / 5.f);
			}
		));

		menu->addChild(createIndexSubmenuItem("Modulator Ratio", ratioLabels,
			[m]() {
				const float cur = m->params[FmOperator::FM_MOD_RATIO_PARAM].getValue();
				static const float r[] = { 1.f, 2.f, 3.f, 4.f, 6.f, 8.f };
				int best = 1; float bestDiff = 999.f;
				for (int i = 0; i < 6; i++) {
					const float d = std::abs(cur - r[i]);
					if (d < bestDiff) { bestDiff = d; best = i; }
				}
				return best;
			},
			[m](int idx) {
				static const float r[] = { 1.f, 2.f, 3.f, 4.f, 6.f, 8.f };
				m->params[FmOperator::FM_MOD_RATIO_PARAM].setValue(r[clamp(idx, 0, 5)]);
			}
		));

		menu->addChild(createIndexSubmenuItem("Modulator Base Waveform",
			{"Sine (follows FM Src Morph)", "Triangle", "Saw", "Square"},
			[m]() {
				return clamp(
					(int)std::round(m->params[FmOperator::FM_MOD_WAVE_PARAM].getValue()), 0, 3);
			},
			[m](int idx) {
				m->params[FmOperator::FM_MOD_WAVE_PARAM].setValue((float)clamp(idx, 0, 3));
			}
		));

		menu->addChild(createIndexSubmenuItem("Modulator Warmth (non-sine base)",
			{"0%", "25%", "50%", "75%", "100%"},
			[m]() {
				return clamp(
					(int)std::round(m->params[FmOperator::FM_MOD_MORPH_PARAM].getValue() * 4.f), 0, 4);
			},
			[m](int idx) {
				m->params[FmOperator::FM_MOD_MORPH_PARAM].setValue(clamp(idx, 0, 4) * 0.25f);
			}
		));

		menu->addChild(createIndexSubmenuItem("DARK Sub Level",
			{"Off", "Low", "Med", "High"},
			[m]() {
				const float cur = m->params[FmOperator::DARK_SUB_LEVEL_PARAM].getValue();
				static const float lv[] = { 0.f, 0.33f, 0.66f, 1.f };
				int best = 2; float bestDiff = 999.f;
				for (int i = 0; i < 4; i++) {
					const float d = std::abs(cur - lv[i]);
					if (d < bestDiff) { bestDiff = d; best = i; }
				}
				return best;
			},
			[m](int idx) {
				static const float lv[] = { 0.f, 0.33f, 0.66f, 1.f };
				m->params[FmOperator::DARK_SUB_LEVEL_PARAM].setValue(lv[clamp(idx, 0, 3)]);
			}
		));

		// === NEW SUB-OSCILLATOR MENU ITEMS ===
		menu->addChild(new MenuSeparator());

		menu->addChild(createIndexSubmenuItem("Sub Waveform",
			{"Sine", "Triangle", "Saw", "Square", "Fold", "Sync"},
			[m]() {
				return clamp((int)std::round(
					m->params[FmOperator::SUB_WAVE_PARAM].getValue()), 0, 5);
			},
			[m](int idx) {
				m->params[FmOperator::SUB_WAVE_PARAM].setValue((float)clamp(idx, 0, 5));
			}
		));

		menu->addChild(createIndexSubmenuItem("Sub Frequency Mode",
			{"÷2 (Sub-Octave)", "÷3", "÷4", "÷5", "÷7", "Unison",
			 "Fixed Drone (5-80Hz)", "Ratio to Modulator"},
			[m]() {
				return clamp((int)std::round(
					m->params[FmOperator::SUB_FREQ_MODE_PARAM].getValue()), 0, 7);
			},
			[m](int idx) {
				m->params[FmOperator::SUB_FREQ_MODE_PARAM].setValue((float)clamp(idx, 0, 7));
			}
		));

		menu->addChild(createIndexSubmenuItem("Sub FM Route",
			{"Layered Output", "As Modulator", "As Carrier", "Off"},
			[m]() {
				return clamp((int)std::round(
					m->params[FmOperator::SUB_FM_ROUTE_PARAM].getValue()), 0, 3);
			},
			[m](int idx) {
				m->params[FmOperator::SUB_FM_ROUTE_PARAM].setValue((float)clamp(idx, 0, 3));
			}
		));

		menu->addChild(createIndexSubmenuItem("Sub Pitch Offset",
			{"-2 oct", "-1 oct", "0", "+1 oct", "+2 oct"},
			[m]() {
				return clamp((int)std::round(
					m->params[FmOperator::SUB_PITCH_PARAM].getValue()), -2, 2) + 2;
			},
			[m](int idx) {
				m->params[FmOperator::SUB_PITCH_PARAM].setValue(
					(float)(clamp(idx, 0, 4) - 2));
			}
		));

		menu->addChild(createIndexSubmenuItem("Sub Phase Offset",
			{"0°", "45°", "90°", "135°", "180°", "225°", "270°", "315°"},
			[m]() {
				return clamp((int)std::round(
					m->params[FmOperator::SUB_PHASE_PARAM].getValue() * 8.f), 0, 7);
			},
			[m](int idx) {
				m->params[FmOperator::SUB_PHASE_PARAM].setValue(
					clamp(idx, 0, 7) / 8.f);
			}
		));

		menu->addChild(createIndexSubmenuItem("Sub Ring Mod",
			{"Off", "25%", "50%", "75%", "100%"},
			[m]() {
				return clamp((int)std::round(
					m->params[FmOperator::SUB_RING_PARAM].getValue() * 4.f), 0, 4);
			},
			[m](int idx) {
				m->params[FmOperator::SUB_RING_PARAM].setValue(
					clamp(idx, 0, 4) * 0.25f);
			}
		));

		menu->addChild(createIndexSubmenuItem("Sub Drift (Space)",
			{"Off", "25%", "50%", "75%", "100%"},
			[m]() {
				return clamp((int)std::round(
					m->params[FmOperator::SUB_DRIFT_PARAM].getValue() * 4.f), 0, 4);
			},
			[m](int idx) {
				m->params[FmOperator::SUB_DRIFT_PARAM].setValue(
					clamp(idx, 0, 4) * 0.25f);
			}
		));

		menu->addChild(new MenuSeparator());

		menu->addChild(createIndexSubmenuItem("Master Macro",
			{"Off", "25%", "50%", "75%", "100%"},
			[m]() {
				const float cur = m->params[FmOperator::MASTER_MACRO_PARAM].getValue();
				static const float lv[] = { 0.f, 0.25f, 0.50f, 0.75f, 1.f };
				int best = 0; float bestDiff = 999.f;
				for (int i = 0; i < 5; i++) {
					const float d = std::abs(cur - lv[i]);
					if (d < bestDiff) { bestDiff = d; best = i; }
				}
				return best;
			},
			[m](int idx) {
				static const float lv[] = { 0.f, 0.25f, 0.50f, 0.75f, 1.f };
				m->params[FmOperator::MASTER_MACRO_PARAM].setValue(lv[clamp(idx, 0, 4)]);
			}
		));

		menu->addChild(new MenuSeparator());

		menu->addChild(createIndexSubmenuItem("Internal Scale",
			{"0%", "50%", "100%"},
			[m]() {
				const float cur = m->params[FmOperator::FM_INTERNAL_SCALE_PARAM].getValue();
				static const float v[] = { 0.f, 0.5f, 1.f };
				int best = 1; float bestDiff = 999.f;
				for (int i = 0; i < 3; i++) {
					const float d = std::abs(cur - v[i]);
					if (d < bestDiff) { bestDiff = d; best = i; }
				}
				return best;
			},
			[m](int idx) {
				static const float v[] = { 0.f, 0.5f, 1.f };
				m->params[FmOperator::FM_INTERNAL_SCALE_PARAM].setValue(v[clamp(idx, 0, 2)]);
			}
		));

		menu->addChild(createIndexSubmenuItem("Legacy Static",
			{"Off (internal mod)", "On (legacy +5V)"},
			[m]() {
				return m->params[FmOperator::FM_LEGACY_STATIC_PARAM].getValue() > 0.5f ? 1 : 0;
			},
			[m](int idx) {
				m->params[FmOperator::FM_LEGACY_STATIC_PARAM].setValue(idx > 0 ? 1.f : 0.f);
			}
		));

		menu->addChild(createIndexSubmenuItem("Internal Amount",
			{"0%", "25%", "50%", "75%", "100%"},
			[m]() {
				return clamp((int)std::round(
					m->params[FmOperator::FM_INTERNAL_AMT_PARAM].getValue() * 4.f), 0, 4);
			},
			[m](int idx) {
				m->params[FmOperator::FM_INTERNAL_AMT_PARAM].setValue(
					clamp(idx, 0, 4) * 0.25f);
			}
		));

		menu->addChild(new MenuSeparator());
		menu->addChild(createIndexSubmenuItem("Modulator Pitch Range",
			{"±2 oct", "±4 oct", "±6 oct"},
			[m]() {
				return clamp(m->modPitchRangePM.load(std::memory_order_relaxed), 0, 2);
			},
			[m](int idx) {
				m->setModPitchRange(idx);
			}
		));

		menu->addChild(createIndexSubmenuItem("Modulator CV Depth",
			{"0%", "25%", "50%", "75%", "100%"},
			[m]() {
				return clamp((int)std::round(
					m->params[FmOperator::FM_MOD_PSY_CV_DEPTH_PARAM].getValue() * 4.f), 0, 4);
			},
			[m](int idx) {
				m->params[FmOperator::FM_MOD_PSY_CV_DEPTH_PARAM].setValue(
					clamp(idx, 0, 4) * 0.25f);
			}
		));
	}
};

Model* modelFmOperator =
	createModel<FmOperator, FmOperatorWidget>("FmOperator");