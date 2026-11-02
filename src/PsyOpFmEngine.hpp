#pragma once

#include "plugin.hpp"
#include "HutaraWidgets.hpp"

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

	enum SubFmRoute {
		SUB_ROUTE_LAYERED   = 0,
		SUB_ROUTE_MODULATOR = 1,
		SUB_ROUTE_CARRIER   = 2,
		SUB_ROUTE_OFF       = 3,
	};

	// === 14 Psy-optimized morph shapes ===
	enum SineMorphShape {
		SM_SINE          = 0,
		SM_TRI_BL        = 1,
		SM_SQUARE_BL     = 2,
		SM_SAW_BL        = 3,
		SM_PULSE_VAR     = 4,
		SM_SQUELCH       = 5,
		SM_RING_SINE     = 6,
		SM_FORMANT4      = 7,
		SM_FOLD          = 8,
		SM_SYNC_2_1      = 9,
		SM_SYNC_3_2      = 10,
		SM_PM_FOLD       = 11,
		SM_CHEB_T5       = 12,
		SM_NOISE_SINE    = 13,
		SM_COUNT         = 14
	};

	static constexpr float SHAPE_RMS[14] = {
		0.707f, 0.577f, 0.707f, 0.577f, 0.500f, 0.550f, 0.500f,
		0.350f, 0.650f, 0.500f, 0.500f, 0.707f, 0.600f, 0.700f,
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
	float subPhaseBase = 0.f;
	float subPhasePrev = 0.f;
	float prevPhaseSine = 0.f;
	float prevRawSine = 0.f;
	float modPhase = 0.f;
	float modPitchSmooth = 0.f;
	bool  lastSyncWrap = false;

	float gateState = 0.f;
	float lastGate = 0.f;
	float gateVCA = 1.f;
	const float gateAttack = 0.003f;
	const float gateRelease = 0.05f;
	float gateSmooth = 1.f;

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
	float fmSrcMorphCvSmooth = 0.f;

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
	float waveBufSub[WAVE_BUF_SIZE]    = {0.f};
	int   waveIdx = 0;

	float snapSine[WAVE_SNAPSHOT_SIZE]   = {0.f};
	float snapSaw[WAVE_SNAPSHOT_SIZE]    = {0.f};
	float snapTri[WAVE_SNAPSHOT_SIZE]    = {0.f};
	float snapSquare[WAVE_SNAPSHOT_SIZE] = {0.f};
	float snapSum[WAVE_SNAPSHOT_SIZE]    = {0.f};
	float snapSub[WAVE_SNAPSHOT_SIZE]    = {0.f};
	int   snapCounter = 0;

	std::atomic<int> genreModePM{ 0 };

	// === PARAM IDS ===
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
		SUB_WAVE_PARAM,
		SUB_FREQ_MODE_PARAM,
		SUB_PITCH_PARAM,
		SUB_PHASE_PARAM,
		SUB_FM_ROUTE_PARAM,
		SUB_RING_PARAM,
		SUB_DRIFT_PARAM,
		FM_SRC_MORPH_CV_AMOUNT_PARAM,
		PARAMS_LEN
	};

	enum InputId {
		PITCH_INPUT_SINE, PITCH_INPUT_SAW, PITCH_INPUT_TRIANGLE, PITCH_INPUT_SQUARE,
		FM_INPUT, FM_AMOUNT_INPUT,
		PSYCHEDELIC_CV_INPUT_SAW, PSYCHEDELIC_CV_INPUT_FOR_All, PSYCHEDELIC_CV_INPUT_TRIANGLE,
		RESAMPLE_INPUT, PITCH_INPUT_ALL, PSYCHEDELIC_CV_INPUT_SQUARE,
		PHASE_MOD_INPUT_SINE, PHASE_MOD_INPUT_SAW, PHASE_MOD_INPUT_TRIANGLE, PHASE_MOD_INPUT_SQUARE,
		PSY_MACRO_CV_INPUT, DARK_MACRO_CV_INPUT, MOTION_MACRO_CV_INPUT, FERAL_MACRO_CV_INPUT,
		GATE_INPUT,
		FM_SRC_MORPH_CV_INPUT,
		INPUTS_LEN
	};

	enum OutputId {
		SINE_OUTPUT, SAW_OUTPUT, TRIANGLE_OUTPUT, SQUARE_OUTPUT,
		FINAL_OUTPUT, SUM_OUTPUT,
		SUB_OUTPUT,
		OUTPUTS_LEN
	};

	enum LightId {
		FINAL_OUTPUT_LIGHT, LED_LIGHT, LED_RED_LIGHT, HEXAGRAM_LIGHT, FM_AMOUNT_LIGHT,
		PITCH_STEP_MODE_LIGHT,
		CV_MODE_LIGHT_0, CV_MODE_LIGHT_1, CV_MODE_LIGHT_2,
		FM_SRC_MORPH_CV_LIGHT,
		LIGHTS_LEN
	};

	// ============================================================
	// Chain depth helpers
	// ============================================================
	float getChainDepth(int modIdx) const {
		if (modIdx < 0 || modIdx > 3) return 1.f;
		return chainDepthPM[modIdx].load(std::memory_order_relaxed) / 1000.f;
	}
	void setChainDepth(int modIdx, float v) {
		if (modIdx < 0 || modIdx > 3) return;
		v = clamp(v, 0.f, 1.f);
		chainDepthPM[modIdx].store((uint32_t)(v * 1000.f + 0.5f), std::memory_order_relaxed);
	}

	void setModPitchRange(int rangeIdx);

	// ============================================================
	// Inline DSP helpers
	// ============================================================

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

	// ============================================================
	// 14 shape morph engine (inline)
	// ============================================================
	static inline float evalFmSourceShape(int idx, float phase,
	                                       float dt, float rawSine,
	                                       float morphFrac = 0.f) {
		const float dtSafe = clampSafe(dt, 1e-6f, 0.45f);
		const float m = clampSafe(morphFrac, 0.f, 1.f);

		switch (idx) {
			case SM_SINE:
				return rawSine;
			case SM_TRI_BL:
				return triangleBL(phase);
			case SM_SQUARE_BL:
				return squareBL(phase, dtSafe);
			case SM_SAW_BL:
				return sawBL(phase, dtSafe);
			case SM_PULSE_VAR: {
				const float duty = 0.15f + 0.35f * m;
				return pulseBL(phase, duty, dtSafe);
			}
			case SM_SQUELCH: {
				const float driven = std::tanh(3.0f * rawSine);
				const float am = 1.0f - 0.35f * (0.5f + 0.5f * std::sin(2.f * PI2 * phase));
				const float squelch = driven * am;
				return clampSafe(rawSine * (1.f - m) + squelch * m, -1.f, 1.f);
			}
			case SM_RING_SINE: {
				const float carrier = rawSine;
				const float modulator = std::sin(3.f * PI2 * phase);
				return clampSafe(carrier * modulator, -1.f, 1.f);
			}
			case SM_FORMANT4: {
				const float am = 0.5f + 0.5f * std::cos(4.f * PI2 * phase);
				return clampSafe(rawSine * am, -1.f, 1.f);
			}
			case SM_FOLD:
				return std::tanh(2.4f * rawSine);
			case SM_SYNC_2_1: {
				constexpr float RATIO = 2.0f;
				const float t = phase * RATIO;
				const float slave = t - std::floor(t);
				const float slaveDt = dtSafe * RATIO;
				const float blep = polyBlep(slave, slaveDt);
				return clampSafe(std::sin(PI2 * slave) * (1.f - 0.5f * blep), -1.f, 1.f);
			}
			case SM_SYNC_3_2: {
				constexpr float RATIO = 1.5f;
				const float t = phase * RATIO;
				const float slave = t - std::floor(t);
				const float slaveDt = dtSafe * RATIO;
				const float blep = polyBlep(slave, slaveDt);
				return clampSafe(std::sin(PI2 * slave) * (1.f - 0.5f * blep), -1.f, 1.f);
			}
			case SM_PM_FOLD: {
				const float pmDepth = 0.4f + 0.8f * m;
				const float pm = pmDepth * std::sin(2.f * PI2 * phase);
				return std::sin(PI2 * (phase + pm));
			}
			case SM_CHEB_T5: {
				const float s = rawSine;
				const float s2 = s * s;
				const float s3 = s2 * s;
				const float s5 = s3 * s2;
				const float t5 = 16.f * s5 - 20.f * s3 + 5.f * s;
				return clampSafe(0.30f * s + 0.85f * t5, -1.f, 1.f);
			}
			case SM_NOISE_SINE: {
				const float n = std::sin(PI2 * wrap(phase * 7.31f + 0.17f))
				              * std::cos(PI2 * wrap(phase * 13.7f + 0.53f));
				const float noiseAmt = 0.05f + 0.20f * m;
				return clampSafe(rawSine * (1.f - noiseAmt) + n * noiseAmt, -1.f, 1.f);
			}
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
			const float v = evalFmSourceShape(maxShape, phase, dt, rawSine, 1.f);
			return clampSafe(v * (0.707f / SHAPE_RMS[maxShape]), -1.2f, 1.2f);
		}
		const float frac = pos - (float)i0;
		const float a = evalFmSourceShape(i0,     phase, dt, rawSine, frac);
		const float b = evalFmSourceShape(i0 + 1, phase, dt, rawSine, frac);

		const float rmsA = 0.707f / SHAPE_RMS[i0];
		const float rmsB = 0.707f / SHAPE_RMS[i0 + 1];
		const float rmsBlend = rmsA + (rmsB - rmsA) * frac;

		const float raw = a + (b - a) * frac;
		const float morphEmph = 0.5f + 0.5f * std::sin((float)M_PI * frac);
		const float emphasized = raw + (raw - rawSine) * (0.10f * morphEmph);

		return clampSafe(emphasized * rmsBlend, -1.2f, 1.2f);
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

	float computeTurbulence(TurbulenceState& t, float internalEnv,
	                        float macro, int mode, float sr, float dt);

	// ============================================================
	// Metadata helpers (implemented in .cpp)
	// ============================================================
	static const char* shortParamName(int paramId);
	std::string shortValueText(int paramId, float value, bool pitchStepMode) const;

	// ============================================================
	// Module interface (implemented in .cpp)
	// ============================================================
	FmOperator();
	void onReset() override;
	void onSampleRateChange() override;
	json_t* dataToJson() override;
	void dataFromJson(json_t* rootJ) override;
	void process(const ProcessArgs& args) override;

	void applyGenreMode(int idx);
	void updateDcCoeff(float sampleRate);
	void updateFinalDcCoeff(float sampleRate);
};
