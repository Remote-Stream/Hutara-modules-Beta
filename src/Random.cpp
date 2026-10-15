#include "plugin.hpp"
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <string>
using namespace rack;

struct Hutara_Random_CV;

// -------------------------------------------------------------------------
// Custom ParamQuantity classes
// -------------------------------------------------------------------------
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

struct MyHutaraSmallKnob : componentlibrary::RoundSmallBlackKnob {
	MyHutaraSmallKnob() {
		box.size = box.size.mult(0.8f); }
	void draw(const DrawArgs& args) override {
		nvgSave(args.vg);
		nvgScale(args.vg, 0.8f, 0.8f);
		nvgGlobalTint(args.vg, nvgRGBA(0, 180, 220, 255));
		componentlibrary::RoundSmallBlackKnob::draw(args);
		nvgRestore(args.vg); }
};

// -------------------------------------------------------------------------
// BPM Multiplier knob — tuned drag sensitivity for 6 discrete steps
// -------------------------------------------------------------------------
struct HutaraBpmMultKnob : RoundSmallBlackKnob {
	float dragAccumulator = 0.f;
	float startValue = 0.f;
	bool  dragging = false;

	static constexpr float PARAM_MIN = 0.f;
	static constexpr float PARAM_MAX = 5.f;
	static constexpr float PIXELS_PER_STEP = 14.f;

	void onDragStart(const DragStartEvent& e) override {
		if (e.button != GLFW_MOUSE_BUTTON_LEFT) return;
		RoundSmallBlackKnob::onDragStart(e);
		dragging = true;
		ParamQuantity* pq = getParamQuantity();
		startValue = pq ? pq->getValue() : PARAM_MIN;
		dragAccumulator = startValue;
	}

	void onDragMove(const DragMoveEvent& e) override {
		if (e.button != GLFW_MOUSE_BUTTON_LEFT) return;
		float deltaY = -e.mouseDelta.y;
		dragAccumulator += deltaY / PIXELS_PER_STEP;
		if (dragAccumulator < PARAM_MIN) dragAccumulator = PARAM_MIN;
		if (dragAccumulator > PARAM_MAX) dragAccumulator = PARAM_MAX;
		ParamQuantity* pq = getParamQuantity();
		if (pq) pq->setValue(dragAccumulator);
	}

	void onDragEnd(const DragEndEvent& e) override {
		if (e.button != GLFW_MOUSE_BUTTON_LEFT) return;
		dragging = false;
		RoundSmallBlackKnob::onDragEnd(e);
	}

	void onHoverScroll(const HoverScrollEvent& e) override {
		float delta = e.scrollDelta.y;
		if (std::fabs(delta) < 0.001f) return;
		ParamQuantity* pq = getParamQuantity();
		if (pq) {
			float v = pq->getValue() + (delta > 0.f ? 1.f : -1.f);
			pq->setValue(v);
		}
		e.consume(this);
	}
};
// -------------------------------------------------------------------------
// PPQN knob — same tuned behavior as BPM multiplier (5 discrete steps)
// Used only for CLOCK_PPQN_PARAM.
// -------------------------------------------------------------------------
struct HutaraPpqnKnob : RoundSmallBlackKnob {
	float dragAccumulator = 0.f;
	float startValue = 0.f;
	bool  dragging = false;

	static constexpr float PARAM_MIN = 0.f;
	static constexpr float PARAM_MAX = 4.f;   // NUM_PPQN - 1 = 5 - 1 = 4
	static constexpr float PIXELS_PER_STEP = 14.f;

	void onDragStart(const DragStartEvent& e) override {
		if (e.button != GLFW_MOUSE_BUTTON_LEFT) return;
		RoundSmallBlackKnob::onDragStart(e);
		dragging = true;
		ParamQuantity* pq = getParamQuantity();
		startValue = pq ? pq->getValue() : PARAM_MIN;
		dragAccumulator = startValue;
	}

	void onDragMove(const DragMoveEvent& e) override {
		if (e.button != GLFW_MOUSE_BUTTON_LEFT) return;
		float deltaY = -e.mouseDelta.y;
		dragAccumulator += deltaY / PIXELS_PER_STEP;
		if (dragAccumulator < PARAM_MIN) dragAccumulator = PARAM_MIN;
		if (dragAccumulator > PARAM_MAX) dragAccumulator = PARAM_MAX;
		ParamQuantity* pq = getParamQuantity();
		if (pq) pq->setValue(dragAccumulator);
	}

	void onDragEnd(const DragEndEvent& e) override {
		if (e.button != GLFW_MOUSE_BUTTON_LEFT) return;
		dragging = false;
		RoundSmallBlackKnob::onDragEnd(e);
	}

	void onHoverScroll(const HoverScrollEvent& e) override {
		float delta = e.scrollDelta.y;
		if (std::fabs(delta) < 0.001f) return;
		ParamQuantity* pq = getParamQuantity();
		if (pq) {
			float v = pq->getValue() + (delta > 0.f ? 1.f : -1.f);
			pq->setValue(v);
		}
		e.consume(this);
	}
};
struct Hutara_Random_CV : Module {
	static constexpr float DEFAULT_FREQ = 1.0f;
	static constexpr float DEFAULT_GATE_LENGTH = 0.5f;
	static constexpr float CLOCK_ACTIVE_THRESHOLD = 0.1f;
	static constexpr float CLOCK_LOW_THRESHOLD = 0.1f;
	static constexpr float CLOCK_HIGH_THRESHOLD = 1.0f;
	static constexpr float GATE_HIGH_VOLTAGE = 10.0f;
	static constexpr float MIN_CLOCK_INTERVAL = 0.001f;
	static constexpr float MAX_CLOCK_INTERVAL = 2.0f;
	static constexpr float MIN_SMOOTH_TIME = 0.001f;
	static constexpr float SMOOTH_DEADZONE = 0.001f;
	static constexpr int NUM_SHAPES = 16;
	static constexpr int TABLE_N = 256;

	static constexpr int NUM_DIVISIONS = 22;
	static constexpr int NUM_BPM_MULTS = 6;
	static constexpr int NUM_PPQN = 5;   // 0=Auto, 1, 2, 4, 24
	static constexpr float DEFAULT_BPM = 120.f;
	static constexpr float BPM_TIMEOUT_SECONDS = 2.0f;
	static constexpr float BPM_SMOOTHING = 0.1f;

	// Fixed display vertical range (oscilloscope-style)
	static constexpr float SH_DISPLAY_MAX = 10.f;

	enum ParamIds {
		OUTPUT_VOLTAGE_PARAM,
		BIPOLAR_PARAM,
		STRENGTH_PARAM,
		RANDOM_GATE_SPEED_PARAM,
		GATE_LENGTH_PARAM,
		TRIGGER_DIVISION_DIVIDER_PARAM,
		S_H_OFFSET_PARAM,
		GATE_LENGTH_ADJUST_PARAM,
		MODULATION_INPUT_PARAM,
		SMOOTH_RANDOM_PARAM,
		ENHANCED_MODE_PARAM,
		TIME_PARAM,
		SHAPE_PARAM,
		WARP_PARAM,
		FLUX_PARAM,
		LOOP_PARAM,
		RANDOMNESS_PARAM,
		LEVEL_PARAM,
		TRIG_BUTTON_PARAM,
		TIME_SYNC_MODE_PARAM,
		BPM_MULTIPLIER_PARAM,
		CLOCK_PPQN_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ON_INPUT,
		MODULATION_INPUT,
		TIME_INPUT,
		SHAPE_INPUT,
		WARP_INPUT,
		FLUX_INPUT,
		RANDOMNESS_INPUT,
		TRIG_INPUT,
		ENVELOPE_CLOCK_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		GATE_OUTPUT,
		OUTPUT,
		GATE_OUTPUT_INVERTED,
		EOC_OUTPUT,
		ENVELOPE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds { NUM_LIGHTS };

	float phase = 0.f;
	float freq = DEFAULT_FREQ;
	float outputVoltage = 0.0f;
	bool sampleNext = false;
	bool bipolarOutput = true;
	bool clockHigh = false;
	float clockTimer = 0.0f;
	float lastClockInterval = 0.25f;
	bool haveClockInterval = false;
	int triggerCounter = 0;
	float currentOutput = 0.0f;
	float previousRandomValue = 0.0f;
	float targetRandomValue = 0.0f;
	bool smoothActive = false;
	bool gateState = false;
	float gateTimer = 0.0f;

	uint32_t rngState = 0x7A31C9D5u;

	float envelopePhase = 1.f;
	bool running = false;
	bool enhancedBipolar = false;

	float drawTable[TABLE_N];
	bool drawMode = false;
	static constexpr int DRAW_UNDO_LEVELS = 15;
	float drawUndoTables[DRAW_UNDO_LEVELS][TABLE_N];
	int drawUndoCount = 0;
	float drawResetTable[TABLE_N];
	bool drawResetAvailable = false;
	bool drawGestureActive = false;

	dsp::SchmittTrigger trigTrigger;
	dsp::BooleanTrigger buttonTrigger;
	dsp::PulseGenerator eocPulse;

	float dispTime = 0.5f;
	float dispShape = 0.f;
	float dispWarp = 0.f;
	float dispFlux = 0.f;
	float dispRandomness = 0.f;
	float dispPhase = 1.f;
	float dispDuration = 1.f;
	bool dispLoop = false;

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
	int   cycleCount = 0;
	int   autoDetectedPpqn = 24;

	// ===== S&H DISPLAY (fixed vertical scale, no auto-zoom) =====
	static constexpr int SH_HISTORY_SIZE = 256;
	static constexpr int SH_DISPLAY_COUNT = 32;
	float shHistory[SH_HISTORY_SIZE] = {};
	int shHistoryWritePos = 0;
	int shHistoryCount = 0;
	float dispShValue = 0.f;
	// Time-based sampling for correct smooth display
	float shSampleTimer = 0.f;

	Hutara_Random_CV() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(OUTPUT_VOLTAGE_PARAM, 0.25f, 10.f, 5.f, "S&H Voltage");
		configParam(BIPOLAR_PARAM, 0.f, 1.f, 1.f, "1 Bipolar/0 Unipolar");
		configParam(STRENGTH_PARAM, 1.f, 10.f, 1.f, "Modulation Strength");
		configParam(RANDOM_GATE_SPEED_PARAM, 1.f, 3.f, 2.f, "Random Gate Speed");
		configParam(GATE_LENGTH_PARAM, 0.01f, 1.0f, DEFAULT_GATE_LENGTH, "Gate Length");
		configParam(GATE_LENGTH_ADJUST_PARAM, 0.0f, 1.0f, 0.5f, "Gate Probability");
		configParam(TRIGGER_DIVISION_DIVIDER_PARAM, 1.f, 3.f, 1.f, "Trigger Division");
		configParam(S_H_OFFSET_PARAM, -5.f, 5.f, 0.f, "S&H Offset");
		configParam(MODULATION_INPUT_PARAM, -1.f, 1.f, 0.f, "Modulation Amount");
		configParam(SMOOTH_RANDOM_PARAM, 0.f, 1.f, 0.f, "Smooth Random");
		configSwitch(ENHANCED_MODE_PARAM, 0.f, 1.f, 0.f, "Mode",
			{"Legacy Random", "enhanced / Enhanced"});

		configParam<HutaraTimeQuantity>(
			TIME_PARAM, 0.f, 1.f, 0.5f, "Envelope Time");

		configParam(SHAPE_PARAM, 0.f, 1.f, 0.0f, "Envelope Shape");
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
			"BPM Multiplier");

		configParam<HutaraPpqnQuantity>(
			CLOCK_PPQN_PARAM, 0.f, static_cast<float>(NUM_PPQN - 1), 0.f,
			"Clock PPQN");

		configOutput(GATE_OUTPUT, "Gate");
		configOutput(OUTPUT, "S&H / Random CV");
		configOutput(GATE_OUTPUT_INVERTED, "Inverted Gate");
		configInput(ON_INPUT, "Clock / Trigger");
		configInput(MODULATION_INPUT, "Modulation CV");
		configInput(TIME_INPUT, "Time CV (10V = full range)");
		configInput(SHAPE_INPUT, "Shape CV (10V = full range)");
		configInput(WARP_INPUT, "Warp CV (5V = full range)");
		configInput(FLUX_INPUT, "Flux CV (10V = full range)");
		configInput(RANDOMNESS_INPUT, "Randomness CV (10V = full range)");
		configInput(TRIG_INPUT, "Enhanced Trigger");
		configInput(ENVELOPE_CLOCK_INPUT, "Envelope Clock (BPM Reference)");
		configOutput(ENVELOPE_OUTPUT, "Right Panel Envelope");
		configOutput(EOC_OUTPUT, "End of Cycle");

		if (auto* q = paramQuantities[TIME_PARAM]) {
			if (auto* hq = dynamic_cast<HutaraTimeQuantity*>(q)) {
				hq->module = this;
			}
		}

		initDrawTable();

		// Initial S&H display state
		float initVal = 0.f;
		for (int i = 0; i < SH_HISTORY_SIZE; i++) shHistory[i] = initVal;
		dispShValue = initVal;
		shSampleTimer = 0.f;
	}

	static float clampValue(float value, float minValue, float maxValue) {
		return std::min(std::max(value, minValue), maxValue); }

	uint32_t nextRandomU32() {
		rngState ^= rngState << 13;
		rngState ^= rngState >> 17;
		rngState ^= rngState << 5;
		if (rngState == 0u) rngState = 0x6D2B79F5u;
		return rngState; }
	float randomFloat() {
		return nextRandomU32() * (1.f / 4294967296.f) * 2.f - 1.f; }
	float randomUnipolar() {
		return nextRandomU32() * (1.f / 4294967296.f); }

	static float divisionToBeats(int index) {
		static constexpr float beats[NUM_DIVISIONS] = {
			32.0f, 24.0f, 16.0f, 12.0f, 8.0f,
			4.0f, 2.0f,
			1.0f, 2.0f/3.0f, 1.0f/3.0f,
			0.5f, 1.0f/3.0f, 0.75f,
			0.25f, 1.0f/6.0f, 0.375f,
			0.125f, 1.0f/12.0f, 0.1875f,
			0.0625f, 1.0f/24.0f, 0.09375f
		};
		if (index < 0) index = 0;
		if (index >= NUM_DIVISIONS) index = NUM_DIVISIONS - 1;
		return beats[index];
	}

	static const char* divisionToLabel(int index) {
		static const char* labels[NUM_DIVISIONS] = {
			"8/1",  "6/1",  "4/1",  "3/1",  "2/1",
			"1/1",  "1/2",
			"1/4",  "1/4T", "1/4D",
			"1/8",  "1/8T", "1/8D",
			"1/16", "1/16T","1/16D",
			"1/32", "1/32T","1/32D",
			"1/64", "1/64T","1/64D"
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

	// PPQN values: 0=Auto, 1, 2, 4, 24 (standard values only)
	static int ppqnToInt(int idx) {
		static constexpr int values[NUM_PPQN] = { 0, 1, 2, 4, 24 };
		if (idx < 0) idx = 0;
		if (idx >= NUM_PPQN) idx = NUM_PPQN - 1;
		return values[idx];
	}

	static const char* ppqnToLabel(int idx) {
		static const char* labels[NUM_PPQN] = {
			"Auto Detect",
			"1 PPQN",
			"2 PPQN",
			"4 PPQN",
			"24 PPQN (MIDI)"
		};
		if (idx < 0) idx = 0;
		if (idx >= NUM_PPQN) idx = NUM_PPQN - 1;
		return labels[idx];
	}

	static int timeParamToDivisionIndex(float timeParam) {
		float scaled = clampValue(timeParam, 0.f, 1.f) * (NUM_DIVISIONS - 1);
		int idx = static_cast<int>(std::round(scaled));
		if (idx < 0) idx = 0;
		if (idx >= NUM_DIVISIONS) idx = NUM_DIVISIONS - 1;
		return idx;
	}

	void initDrawTable() {
		for (int i = 0; i < TABLE_N; i++) {
			float t = static_cast<float>(i) / static_cast<float>(TABLE_N - 1);
			drawTable[i] = 1.f - std::fabs(2.f * t - 1.f); } }

	void clearDrawHistory() {
		drawUndoCount = 0;
		drawResetAvailable = false;
		drawGestureActive = false;
	}

	void beginDrawGesture() {
		if (drawGestureActive) return;
		if (!drawMode) {
			float shape = clampValue(dispShape, 0.f, 1.f);
			for (int i = 0; i < TABLE_N; ++i) {
				float p = static_cast<float>(i) / static_cast<float>(TABLE_N - 1);
				float shapePosition = shape * static_cast<float>(NUM_SHAPES - 1);
				int shape0 = clampValue(static_cast<int>(shapePosition), 0, NUM_SHAPES - 1);
				int shape1 = std::min(shape0 + 1, NUM_SHAPES - 1);
				float morph = shapePosition - static_cast<float>(shape0);
				float v0 = shapeValue(shape0, p);
				float v1 = shapeValue(shape1, p);
				drawTable[i] = clampValue(v0 + (v1 - v0) * morph, 0.f, 1.f); }
			for (int i = 0; i < TABLE_N; ++i)
				drawResetTable[i] = drawTable[i];
			drawResetAvailable = true;
			drawUndoCount = 0;
			drawMode = true; }
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
				drawUndoTables[0][i] = drawTable[i]; }
		drawGestureActive = true; }

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
		drawMode = true;
		return true; }

	bool resetDraw() {
		if (!drawResetAvailable) return false;
		for (int i = 0; i < TABLE_N; ++i)
			drawTable[i] = drawResetTable[i];
		drawUndoCount = 0;
		drawGestureActive = false;
		drawResetAvailable = false;
		drawMode = false;
		return true; }

	static float bell(float p, float center, float width) {
		float d = (p - center) / width;
		return std::exp(-0.5f * d * d); }

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
			default:
				return clampValue(
					0.5f + 0.35f * std::sin(2.f * M_PI * 1.7f * p + 1.f) +
					0.25f * std::sin(2.f * M_PI * 3.1f * p + 2.f) +
					0.15f * std::sin(2.f * M_PI * 5.3f * p + 4.f), 0.f, 1.f); } }

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
			int shape0 = clampValue(static_cast<int>(shapePosition), 0, NUM_SHAPES - 1);
			int shape1 = std::min(shape0 + 1, NUM_SHAPES - 1);
			float morph = shapePosition - static_cast<float>(shape0);
			float value0 = shapeValue(shape0, warpedPhase);
			float value1 = shapeValue(shape1, warpedPhase);
			value = value0 + (value1 - value0) * morph; }
		float ripple = 0.5f - 0.5f * std::cos(2.f * M_PI * 8.f * warpedPhase);
		value *= 1.f - flux * ripple;
		return clampValue(value, 0.f, 1.f); }

	float randomizedEnvelope(float shape, float warp, float flux,
	                         float randomness, float p) {
		float base = envelopeValue(drawMode ? drawTable : nullptr,
		                           shape, warp, flux, p);
		if (randomness <= 0.f) return base;
		float noise =
			0.50f + 0.25f * std::sin(2.f * M_PI * 1.73f * p + 1.31f) +
			0.15f * std::sin(2.f * M_PI * 3.91f * p + 2.73f) +
			0.10f * std::sin(2.f * M_PI * 7.17f * p + 4.91f);
		noise = clampValue(noise, 0.f, 1.f);
		return clampValue(base * (1.f - randomness) + noise * randomness, 0.f, 1.f); }

	float generateLegacySample() {
		float randomValue = randomFloat() * params[STRENGTH_PARAM].getValue();
		float legacyOutputVoltage = params[OUTPUT_VOLTAGE_PARAM].getValue();
		bool legacyBipolar = params[BIPOLAR_PARAM].getValue() > 0.5f;
		if (legacyBipolar) {
			randomValue *= legacyOutputVoltage;
		} else {
			randomValue = (randomValue + 1.0f) * 0.5f * legacyOutputVoltage;
		}
		randomValue = clampValue(randomValue,
			legacyBipolar ? -legacyOutputVoltage : 0.0f, legacyOutputVoltage);
		randomValue += params[S_H_OFFSET_PARAM].getValue();
		return randomValue; }

	int getLegacyDivider() {
		int divider = static_cast<int>(std::round(params[TRIGGER_DIVISION_DIVIDER_PARAM].getValue()));
		return std::max(1, std::min(divider, 3)); }

	void updateLegacyRandomOutput(const ProcessArgs& args) {
		float smooth = params[SMOOTH_RANDOM_PARAM].getValue();
		if (smooth <= SMOOTH_DEADZONE) {
			currentOutput = targetRandomValue;
			smoothActive = false;
			return; }
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
		currentOutput += diff * coefficient; }

	void processLegacy(const ProcessArgs& args) {
		outputVoltage = params[OUTPUT_VOLTAGE_PARAM].getValue();
		bipolarOutput = params[BIPOLAR_PARAM].getValue() > 0.5f;
		float modulation = 0.f;
		if (inputs[MODULATION_INPUT].isConnected())
			modulation = inputs[MODULATION_INPUT].getVoltage() *
				params[MODULATION_INPUT_PARAM].getValue();
		float onVoltage = inputs[ON_INPUT].isConnected() ? inputs[ON_INPUT].getVoltage() : 0.f;
		if (onVoltage >= 1.0f && !sampleNext) {
			sampleNext = true;
			if (clockTimer > 0.0001f) {
				if (haveClockInterval)
					lastClockInterval = clampValue(clockTimer, MIN_CLOCK_INTERVAL, MAX_CLOCK_INTERVAL);
				else
					haveClockInterval = true; }
			clockTimer = 0.f;
			triggerCounter++;
			if (triggerCounter >= getLegacyDivider()) {
				triggerCounter = 0;
				previousRandomValue = targetRandomValue;
				targetRandomValue = generateLegacySample();
				targetRandomValue += modulation;
				bool bipolar = params[BIPOLAR_PARAM].getValue() > 0.5f;
				targetRandomValue = clampValue(targetRandomValue,
					bipolar ? -outputVoltage : 0.f, outputVoltage);
			}
			float gateLength = clampValue(params[GATE_LENGTH_PARAM].getValue(), 0.01f, 1.f);
			int gateSpeedParam = static_cast<int>(std::round(params[RANDOM_GATE_SPEED_PARAM].getValue()));
			gateSpeedParam = std::max(1, std::min(gateSpeedParam, 3));
			float gateProbability = gateLength;
			if (gateSpeedParam == 1)
				gateProbability = gateLength * 0.25f;
			else if (gateSpeedParam == 2)
				gateProbability = gateLength * 0.125f;
			else
				gateProbability = gateLength * 0.625f;
			gateProbability = clampValue(gateProbability *
				(params[GATE_LENGTH_ADJUST_PARAM].getValue() * 2.f), 0.f, 1.f);
			float randomGate = (randomFloat() < gateProbability) ? GATE_HIGH_VOLTAGE : 0.f;
			outputs[GATE_OUTPUT].setVoltage(randomGate);
			outputs[GATE_OUTPUT_INVERTED].setVoltage(10.f - randomGate); }
		if (onVoltage < 1.0f && sampleNext) {
			sampleNext = false; }
		if (inputs[ON_INPUT].isConnected())
			clockTimer += args.sampleTime;
		else
			clockTimer = 0.f;
		updateLegacyRandomOutput(args);
		outputs[OUTPUT].setVoltage(currentOutput);

		// ===== S&H DISPLAY: time-based sampling =====
		// Sample currentOutput at regular intervals so that:
		//  - if smooth == 0: shows stepped S&H (samples fall on held values)
		//  - if smooth > 0: shows the smoothed curve
		// Sampling interval = clockPeriod / SH_DISPLAY_COUNT
		{
			float clockPeriod = haveClockInterval ? lastClockInterval : 0.25f;
			clockPeriod = clampValue(clockPeriod, MIN_CLOCK_INTERVAL, MAX_CLOCK_INTERVAL);
			float sampleInterval = clockPeriod / static_cast<float>(SH_DISPLAY_COUNT);
			if (sampleInterval < 1e-5f) sampleInterval = 1e-5f;

			shSampleTimer += args.sampleTime;
			if (shSampleTimer >= sampleInterval) {
				shSampleTimer -= sampleInterval;
				// Guard against very long stalls
				if (shSampleTimer > sampleInterval * 4.f) shSampleTimer = 0.f;

				shHistory[shHistoryWritePos] = currentOutput;
				shHistoryWritePos = (shHistoryWritePos + 1) % SH_HISTORY_SIZE;
				if (shHistoryCount < SH_HISTORY_SIZE) shHistoryCount++;
			}
			dispShValue = currentOutput;
		}
	}

	void updateBpmFromClock(const ProcessArgs& args) {
		float onVoltage = inputs[ENVELOPE_CLOCK_INPUT].isConnected()
			? inputs[ENVELOPE_CLOCK_INPUT].getVoltage() : 0.f;

		int ppqnIdx = static_cast<int>(std::round(params[CLOCK_PPQN_PARAM].getValue()));
		int ppqnVal = ppqnToInt(ppqnIdx);
		bool autoMode = (ppqnVal == 0);

		if (onVoltage >= 1.0f && !envelopeClockHigh) {
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
							// big jump: user changed tempo — follow immediately
							measuredBpm = instantBpm;
						}
					}
					float beatPeriod = 60.f / measuredBpm;
					bpmTimeout = std::max(BPM_TIMEOUT_SECONDS, beatPeriod * 4.f);
				}
			}
			envelopeClockTimer = 0.f;
		} else if (onVoltage < 1.0f) {
			envelopeClockHigh = false;
		}

		if (inputs[ENVELOPE_CLOCK_INPUT].isConnected()) {
			envelopeClockTimer += args.sampleTime;
			bpmTimeout -= args.sampleTime;

			// Auto-detect PPQN — restricted to plausible musical BPM range [60, 200]
			if (autoMode && envelopeClockTimer > 0.005f) {
				static const int candidates[] = { 1, 2, 4, 24 };
				float bestScore = 1e9f;
				int bestPpqn = autoDetectedPpqn;
				for (int c : candidates) {
					float spb = envelopeClockTimer * static_cast<float>(c);
					if (spb < 0.001f) continue;
					float bpm = 60.f / spb;
					// Only accept PPQN that puts BPM in a musical range
					if (bpm > 60.f && bpm < 200.f) {
						float score = std::fabs(bpm - 120.f);
						if (score < bestScore) {
							bestScore = score;
							bestPpqn = c;
						}
					}
				}
				if (bestScore < 1e8f) {
					autoDetectedPpqn = bestPpqn;
				}
			}

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

	void processEnhanced(const ProcessArgs& args) {
		updateBpmFromClock(args);

		syncMode = params[TIME_SYNC_MODE_PARAM].getValue() > 0.5f;

		float time = params[TIME_PARAM].getValue();
		if (!syncMode && inputs[TIME_INPUT].isConnected()) {
			time += inputs[TIME_INPUT].getVoltage() / 10.f;
		}
		time = clampValue(time, 0.f, 1.f);

		float shape = params[SHAPE_PARAM].getValue();
		if (inputs[SHAPE_INPUT].isConnected()) {
			shape += inputs[SHAPE_INPUT].getVoltage() / 10.f; }
		shape = clampValue(shape, 0.f, 1.f);

		float warp = params[WARP_PARAM].getValue();
		if (inputs[WARP_INPUT].isConnected()) {
			warp += inputs[WARP_INPUT].getVoltage() / 5.f; }
		warp = clampValue(warp, -1.f, 1.f);

		float flux = params[FLUX_PARAM].getValue();
		if (inputs[FLUX_INPUT].isConnected()) {
			flux += inputs[FLUX_INPUT].getVoltage() / 10.f; }
		flux = clampValue(flux, 0.f, 1.f);

		float randomness = params[RANDOMNESS_PARAM].getValue();
		if (inputs[RANDOMNESS_INPUT].isConnected()) {
			randomness += inputs[RANDOMNESS_INPUT].getVoltage() / 10.f; }
		randomness = clampValue(randomness, 0.f, 1.f);

		float level = params[LEVEL_PARAM].getValue();
		level = clampValue(level, 0.f, 1.f);

		bool loop = params[LOOP_PARAM].getValue() > 0.5f;

		bool externalTrigger = false;
		if (inputs[TRIG_INPUT].isConnected()) {
			externalTrigger = trigTrigger.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 1.f); }
		bool manualTrigger = buttonTrigger.process(params[TRIG_BUTTON_PARAM].getValue() > 0.f);
		bool trigger = externalTrigger || manualTrigger;

		if (trigger) {
			envelopePhase = 0.f;
			running = true; }

		if (loop && !running) {
			if (envelopePhase >= 1.f) {
				envelopePhase = 0.f; }
			running = true; }

		float duration;
		if (syncMode) {
			currentDivisionIndex = timeParamToDivisionIndex(time);
			int multIdx = static_cast<int>(std::round(params[BPM_MULTIPLIER_PARAM].getValue()));
			float beats = divisionToBeats(currentDivisionIndex);
			float mult = multiplierToFactor(multIdx);
			duration = beats * mult * 60.f / measuredBpm;
		} else {
			duration = 0.005f * std::pow(360000.f, time);
			currentDivisionIndex = timeParamToDivisionIndex(time); }

		if (running) {
			envelopePhase += args.sampleTime / duration;
			if (envelopePhase >= 1.f) {
				eocPulse.trigger(1e-3f);
				cycleCount++;
				if (loop) {
					envelopePhase -= std::floor(envelopePhase);
				} else {
					envelopePhase = 1.f;
					running = false; } } }

		float value = randomizedEnvelope(shape, warp, flux, randomness, envelopePhase);
		value *= level;

		bool bipolar = params[BIPOLAR_PARAM].getValue() > 0.5f;
		float output = 0.f;
		if (bipolar) {
			output = value * 10.f - 5.f;
		} else {
			output = value * 10.f; }
		output += params[S_H_OFFSET_PARAM].getValue();
		output = clampValue(output,
			bipolar ? -5.f : 0.f, bipolar ? 5.f : 10.f);

		outputs[ENVELOPE_OUTPUT].setVoltage(output);
		outputs[EOC_OUTPUT].setVoltage(eocPulse.process(args.sampleTime) ? 10.f : 0.f);

		dispTime = time;
		dispShape = shape;
		dispWarp = warp;
		dispFlux = flux;
		dispRandomness = randomness;
		dispPhase = envelopePhase;
		dispDuration = duration;
		dispLoop = loop;
		dispBpm = measuredBpm;
		dispDivisionIndex = currentDivisionIndex;
		dispSyncMode = syncMode;
		dispMultiplierIdx = static_cast<int>(std::round(params[BPM_MULTIPLIER_PARAM].getValue()));
		dispPpqnIdx = static_cast<int>(std::round(params[CLOCK_PPQN_PARAM].getValue()));
	}

	void process(const ProcessArgs& args) override {
		processLegacy(args);
		if (params[ENHANCED_MODE_PARAM].getValue() > 0.5f) {
			processEnhanced(args);
		} else {
			outputs[ENVELOPE_OUTPUT].setVoltage(0.f);
			outputs[EOC_OUTPUT].setVoltage(0.f); } }

	void onReset() override {
		phase = 0.f;
		rngState = 0x7A31C9D5u;
		freq = DEFAULT_FREQ;
		outputVoltage = 0.f;
		sampleNext = false;
		envelopePhase = 1.f;
		running = false;
		drawMode = false;
		eocPulse.reset();
		trigTrigger.reset();
		buttonTrigger.reset();
		clockHigh = false;
		clockTimer = 0.f;
		lastClockInterval = 0.25f;
		haveClockInterval = false;
		triggerCounter = 0;
		currentOutput = 0.f;
		previousRandomValue = 0.f;
		targetRandomValue = 0.f;
		smoothActive = false;
		gateState = false;
		gateTimer = 0.f;
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
		cycleCount = 0;
		autoDetectedPpqn = 24;

		// ===== S&H DISPLAY: reset history =====
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
		outputs[GATE_OUTPUT_INVERTED].setVoltage(10.f);
		initDrawTable();
		clearDrawHistory(); }

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "enhancedMode",
			json_boolean(params[ENHANCED_MODE_PARAM].getValue() > 0.5f));
		json_object_set_new(rootJ, "enhancedBipolar", json_boolean(enhancedBipolar));
		json_object_set_new(rootJ, "drawMode", json_boolean(drawMode));
		json_t* tableJ = json_array();
		for (int i = 0; i < TABLE_N; i++) {
			json_array_append_new(tableJ, json_real(drawTable[i])); }
		json_object_set_new(rootJ, "drawTable", tableJ);
		json_object_set_new(rootJ, "measuredBpm", json_real(measuredBpm));
		json_object_set_new(rootJ, "autoDetectedPpqn", json_integer(autoDetectedPpqn));
		return rootJ; }

	void dataFromJson(json_t* rootJ) override {
		json_t* enhancedJ = json_object_get(rootJ, "enhancedMode");
		if (enhancedJ) {
			bool enhanced = json_boolean_value(enhancedJ);
			params[ENHANCED_MODE_PARAM].setValue(enhanced ? 1.f : 0.f); }
		json_t* bipolarJ = json_object_get(rootJ, "enhancedBipolar");
		if (bipolarJ) {
			enhancedBipolar = json_boolean_value(bipolarJ); }
		json_t* drawModeJ = json_object_get(rootJ, "drawMode");
		if (drawModeJ) {
			drawMode = json_boolean_value(drawModeJ); }
		json_t* tableJ = json_object_get(rootJ, "drawTable");
		if (tableJ) {
			size_t size = json_array_size(tableJ);
			for (int i = 0; i < TABLE_N && i < static_cast<int>(size); i++) {
				json_t* valueJ = json_array_get(tableJ, i);
				if (valueJ) {
					drawTable[i] = clampValue(
						static_cast<float>(json_number_value(valueJ)), 0.f, 1.f); } } }
		json_t* bpmJ = json_object_get(rootJ, "measuredBpm");
		if (bpmJ) {
			float bpm = static_cast<float>(json_number_value(bpmJ));
			if (bpm > 1.f && bpm < 400.f) measuredBpm = bpm;
			dispBpm = measuredBpm; }
		json_t* ppqnJ = json_object_get(rootJ, "autoDetectedPpqn");
		if (ppqnJ) {
			int p = static_cast<int>(json_integer_value(ppqnJ));
			// Only accept standard PPQN values
			if (p == 1 || p == 2 || p == 4 || p == 24) autoDetectedPpqn = p;
		}
		clearDrawHistory(); }
};

// ---- ParamQuantity out-of-line definitions ------------------------------
inline std::string HutaraTimeQuantity::getDisplayValueString() {
	if (module && module->syncMode) {
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

inline void HutaraTimeQuantity::setValue(float v) {
	if (module && module->syncMode) {
		float scaled = clamp(v, 0.f, 1.f) * (Hutara_Random_CV::NUM_DIVISIONS - 1);
		int idx = static_cast<int>(std::round(scaled));
		if (idx < 0) idx = 0;
		if (idx >= Hutara_Random_CV::NUM_DIVISIONS) idx = Hutara_Random_CV::NUM_DIVISIONS - 1;
		v = static_cast<float>(idx) / static_cast<float>(Hutara_Random_CV::NUM_DIVISIONS - 1);
	}
	ParamQuantity::setValue(v);
}

inline std::string HutaraBpmMultQuantity::getDisplayValueString() {
	int idx = static_cast<int>(std::round(getValue()));
	idx = clamp(idx, 0, Hutara_Random_CV::NUM_BPM_MULTS - 1);
	return std::string(Hutara_Random_CV::multiplierToLabel(idx));
}

inline void HutaraBpmMultQuantity::setValue(float v) {
	int idx = static_cast<int>(std::round(clamp(v, 0.f,
		static_cast<float>(Hutara_Random_CV::NUM_BPM_MULTS - 1))));
	if (idx < 0) idx = 0;
	if (idx >= Hutara_Random_CV::NUM_BPM_MULTS) idx = Hutara_Random_CV::NUM_BPM_MULTS - 1;
	ParamQuantity::setValue(static_cast<float>(idx));
}

inline std::string HutaraPpqnQuantity::getDisplayValueString() {
	int idx = static_cast<int>(std::round(getValue()));
	idx = clamp(idx, 0, Hutara_Random_CV::NUM_PPQN - 1);
	static const char* shortLabels[] = {
		"Auto", "1 PPQN", "2 PPQN", "4 PPQN", "24 PPQN"
	};
	return std::string(shortLabels[idx]);
}

inline void HutaraPpqnQuantity::setValue(float v) {
	int idx = static_cast<int>(std::round(clamp(v, 0.f,
		static_cast<float>(Hutara_Random_CV::NUM_PPQN - 1))));
	if (idx < 0) idx = 0;
	if (idx >= Hutara_Random_CV::NUM_PPQN) idx = Hutara_Random_CV::NUM_PPQN - 1;
	ParamQuantity::setValue(static_cast<float>(idx));
}

// =========================================================================
// Display — v6.6: S&H with time-based sampling + smooth/step rendering
// =========================================================================
struct HutaraRandomDisplay : TransparentWidget {
	Hutara_Random_CV* module = nullptr;
	Vec dragPos;
	Vec mousePos;
	bool hasMousePos = false;
	bool drawing = false;
	bool  snapY = true;
	bool  hoverUndo = false;
	bool  hoverReset = false;

	static NVGcolor colBG()         { return nvgRGB (0x02, 0x08, 0x03); }
	static NVGcolor colGrid()       { return nvgRGBA(0x0a, 0x2a, 0x0a, 0xb0); }
	static NVGcolor colGridSub()    { return nvgRGBA(0x06, 0x1a, 0x06, 0x80); }
	static NVGcolor colCurve()      { return nvgRGBA(0x00, 0xff, 0x41, 0xe6); }
	static NVGcolor colCurveFill()  { return nvgRGBA(0x00, 0xff, 0x41, 0x28); }
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

	// ===== S&H DISPLAY: distinct cyan colors =====
	static NVGcolor colSHCurve()    { return nvgRGBA(0x00, 0xc8, 0xff, 0xf0); }
	static NVGcolor colSHFill()     { return nvgRGBA(0x00, 0xc8, 0xff, 0x20); }
	static NVGcolor colSHDot()      { return nvgRGB (0x60, 0xe0, 0xff); }
	static NVGcolor colSHLabel()    { return nvgRGB (0x00, 0xc8, 0xff); }

	float controlHeight() const { return mm2px(4.5f); }
	float controlWidth()  const { return mm2px(12.0f); }
	float controlGap()    const { return mm2px(1.0f); }
	float controlBottomMargin() const { return mm2px(1.0f); }

	Rect undoRect() const {
		float width = controlWidth();
		float height = controlHeight();
		return Rect(box.size.x - mm2px(1.5f) - width * 2.f - controlGap(),
		            box.size.y - height - controlBottomMargin(),
		            width, height); }
	Rect resetRect() const {
		float width = controlWidth();
		float height = controlHeight();
		return Rect(box.size.x - mm2px(1.5f) - width,
		            box.size.y - height - controlBottomMargin(),
		            width, height); }

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

	// Simple 1:1 mapping — NO zoom, NO pan
	void panelToTable(Vec p, float& t, float& v) const {
		const float mx = marginX(), my = marginY();
		const float cw = box.size.x - 2.f * mx;
		const float ch = box.size.y - 2.f * my;
		float rawT = (cw > 1.f) ? (p.x - mx) / cw : 0.f;
		float rawV = (ch > 1.f) ? 1.f - (p.y - my) / ch : 0.f;
		t = Hutara_Random_CV::clampValue(rawT, 0.f, 1.f);
		v = Hutara_Random_CV::clampValue(rawV, 0.f, 1.f); }

	Vec tableToPanel(float t, float v) const {
		const float mx = marginX(), my = marginY();
		const float cw = box.size.x - 2.f * mx;
		const float ch = box.size.y - 2.f * my;
		float rawT = Hutara_Random_CV::clampValue(t, 0.f, 1.f);
		float rawV = Hutara_Random_CV::clampValue(v, 0.f, 1.f);
		return Vec(mx + rawT * cw, my + (1.f - rawV) * ch); }

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
			return std::round(v / step) * step; };
		if (i0 == i1) {
			const int r = 3;
			float sv = snapV(v0);
			for (int j = i0 - r; j <= i0 + r; ++j) {
				if (j < 0 || j > last) continue;
				float w = 1.f - std::fabs(float(j - i0)) / float(r + 1);
				module->drawTable[j] = (j == i0) ? sv
					: module->drawTable[j] * (1.f - w) + sv * w;
				module->drawTable[j] = Hutara_Random_CV::clampValue(module->drawTable[j], 0.f, 1.f); }
			return; }
		for (int i = i0; i <= i1; ++i) {
			float f = float(i - i0) / float(i1 - i0);
			float v = snapV(v0 + (v1 - v0) * f);
			module->drawTable[i] = Hutara_Random_CV::clampValue(v, 0.f, 1.f); } }

	// NOTE: onHoverScroll deliberately NOT overridden — no zoom/pan.

	void onButton(const ButtonEvent& e) override {
		if (!module || e.action != GLFW_PRESS || e.button != GLFW_MOUSE_BUTTON_LEFT) return;

		if (isInside(undoRect(), e.pos)) {
			e.consume(this);
			drawing = false; hasMousePos = false;
			module->endDrawGesture();
			module->undoDraw();
			return; }
		if (isInside(resetRect(), e.pos)) {
			e.consume(this);
			drawing = false; hasMousePos = false;
			module->endDrawGesture();
			module->resetDraw();
			return; }

		e.consume(this);
		module->beginDrawGesture();
		drawing = module->drawGestureActive;
		if (!drawing) return;
		dragPos = e.pos;
		dragPos.x = Hutara_Random_CV::clampValue(dragPos.x, marginX(), box.size.x - marginX());
		dragPos.y = Hutara_Random_CV::clampValue(dragPos.y, marginY(), box.size.y - marginY());
		mousePos = dragPos;
		hasMousePos = true;
		plotLine(dragPos, dragPos); }

	void onDragMove(const DragMoveEvent& e) override {
		if (!module || e.button != GLFW_MOUSE_BUTTON_LEFT || !drawing || !module->drawGestureActive) return;
		Vec newPos = dragPos.plus(e.mouseDelta);
		newPos.x = Hutara_Random_CV::clampValue(newPos.x, marginX(), box.size.x - marginX());
		newPos.y = Hutara_Random_CV::clampValue(newPos.y, marginY(), box.size.y - marginY());
		plotLine(dragPos, newPos);
		dragPos = newPos;
		mousePos = newPos;
		hasMousePos = true; }

	void onDragEnd(const DragEndEvent& e) override {
		if (e.button != GLFW_MOUSE_BUTTON_LEFT) return;
		drawing = false;
		hasMousePos = false;
		if (module) module->endDrawGesture(); }

	void onHover(const HoverEvent& e) override {
		TransparentWidget::onHover(e);
		hoverUndo  = isInside(undoRect(),  e.pos);
		hoverReset = isInside(resetRect(), e.pos);
		mousePos = e.pos;
		hasMousePos = true; }

	void onLeave(const LeaveEvent& e) override {
		TransparentWidget::onLeave(e);
		hoverUndo = hoverReset = false;
		hasMousePos = false; }

	static std::string formatTime(float seconds) {
		if (seconds < 1.f) return string::f("%.0f ms", seconds * 1000.f);
		if (seconds < 60.f) return string::f("%.2f s", seconds);
		int minutes = static_cast<int>(seconds / 60.f);
		int secs = static_cast<int>(seconds - 60.f * minutes);
		return string::f("%dm %02ds", minutes, secs); }

	static std::string formatDivision(int idx) {
		return std::string(Hutara_Random_CV::divisionToLabel(idx)); }

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;

		float shape = module ? module->dispShape : 0.5f;
		float warp  = module ? module->dispWarp  : 0.f;
		float flux  = module ? module->dispFlux  : 0.f;
		float randomness = module ? module->dispRandomness : 0.f;
		float phase = module ? module->dispPhase : 0.4f;
		float duration = module ? module->dispDuration : 1.f;
		bool  loop  = module ? module->dispLoop : false;
		bool  syncMode = module ? module->dispSyncMode : false;
		float bpm = module ? module->dispBpm : Hutara_Random_CV::DEFAULT_BPM;
		int   divIdx = module ? module->dispDivisionIndex : 2;
		int   multIdx = module ? module->dispMultiplierIdx : 2;

		const float* table = (module && module->drawMode) ? module->drawTable : nullptr;

		float w = box.size.x, h = box.size.y;
		nvgScissor(args.vg, 0, 0, w, h);
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, w, h);
		nvgFillColor(args.vg, colBG());
		nvgFill(args.vg);

		float mx = marginX(), my = marginY();
		float cw = w - 2.f * mx, ch = h - 2.f * my;

		// Main grid lines
		nvgStrokeColor(args.vg, colGrid());
		nvgStrokeWidth(args.vg, mm2px(0.15f));
		for (int i = 0; i <= 4; i++) {
			float x = mx + cw * static_cast<float>(i) / 4.f;
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, x, my);
			nvgLineTo(args.vg, x, my + ch);
			nvgStroke(args.vg); }
		if (snapY) {
			nvgStrokeColor(args.vg, colGridSub());
			for (int i = 1; i < 32; i++) {
				float y = my + ch * (static_cast<float>(i) / 32.f);
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, mx, y);
				nvgLineTo(args.vg, mx + cw, y);
				nvgStroke(args.vg); } }

		// Curve
		static constexpr int N = 128;
		nvgBeginPath(args.vg);
		for (int i = 0; i <= N; i++) {
			float p = static_cast<float>(i) / static_cast<float>(N);
			float v = 0.f;
			if (module) {
				if (drawing && module->drawGestureActive) {
					float x = p * float(Hutara_Random_CV::TABLE_N - 1);
					int k = std::max(0, std::min(Hutara_Random_CV::TABLE_N - 2,
						static_cast<int>(std::floor(x))));
					float f = x - float(k);
					v = module->drawTable[k] + (module->drawTable[k + 1] - module->drawTable[k]) * f;
				} else {
					v = module->randomizedEnvelope(shape, warp, flux, randomness, p); } }
			Vec pt = tableToPanel(p, v);
			if (i == 0) nvgMoveTo(args.vg, pt.x, pt.y);
			else        nvgLineTo(args.vg, pt.x, pt.y); }
		nvgStrokeColor(args.vg, colCurve());
		nvgStrokeWidth(args.vg, mm2px(0.3f));
		nvgStroke(args.vg);
		Vec endPt = tableToPanel(1.f, 0.f);
		nvgLineTo(args.vg, endPt.x, endPt.y);
		Vec startPt = tableToPanel(0.f, 0.f);
		nvgLineTo(args.vg, startPt.x, startPt.y);
		nvgClosePath(args.vg);
		nvgFillColor(args.vg, colCurveFill());
		nvgFill(args.vg);

		// ===== S&H DISPLAY: FIXED vertical scale, NO auto-zoom =====
		// Rendering mode:
		//  - smooth == 0  → step-wise (classic S&H)
		//  - smooth  > 0  → smooth line (interpolated curve)
		if (module) {
			bool bipolar = module->bipolarOutput;
			float shMin = bipolar ? -Hutara_Random_CV::SH_DISPLAY_MAX : 0.f;
			float shMax = Hutara_Random_CV::SH_DISPLAY_MAX;
			float shRange = shMax - shMin;

			auto valueToNorm = [&](float v) -> float {
				float n = (v - shMin) / shRange;
				return Hutara_Random_CV::clampValue(n, 0.f, 1.f);
			};

			// Determine render mode from Smooth knob
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
				int oldestRealIdx =
					(writePos - realCount + Hutara_Random_CV::SH_HISTORY_SIZE * 2)
					% Hutara_Random_CV::SH_HISTORY_SIZE;

				nvgBeginPath(args.vg);
				bool first = true;
				Vec prevPt(0.f, 0.f);

				// Left padding: repeat first real sample
				if (padCount > 0) {
					float v = valueToNorm(module->shHistory[oldestRealIdx]);
					for (int i = 0; i < padCount; ++i) {
						float t = static_cast<float>(i)
							/ static_cast<float>(displayCount - 1);
						Vec pt = tableToPanel(t, v);
						if (first) { nvgMoveTo(args.vg, pt.x, pt.y); first = false; }
						else       { nvgLineTo(args.vg, pt.x, pt.y); }
						prevPt = pt;
					}
				}

				// Real samples
				for (int i = 0; i < realCount; ++i) {
					int idx = (oldestRealIdx + i) % Hutara_Random_CV::SH_HISTORY_SIZE;
					int tIdx = padCount + i;
					float t = static_cast<float>(tIdx)
						/ static_cast<float>(displayCount - 1);
					float v = valueToNorm(module->shHistory[idx]);
					Vec pt = tableToPanel(t, v);
					if (first) { nvgMoveTo(args.vg, pt.x, pt.y); first = false; }
					else {
						if (smoothOn) {
							// Smooth line — interpolate between samples
							nvgLineTo(args.vg, pt.x, pt.y);
						} else {
							// Step-wise S&H — hold then jump
							nvgLineTo(args.vg, pt.x, prevPt.y);
							nvgLineTo(args.vg, pt.x, pt.y);
						}
					}
					prevPt = pt;
				}

				// Extend last sample to right edge
				if (!first) {
					int lastIdx = (writePos - 1 + Hutara_Random_CV::SH_HISTORY_SIZE)
						% Hutara_Random_CV::SH_HISTORY_SIZE;
					float lastV = valueToNorm(module->shHistory[lastIdx]);
					Vec endPtSH = tableToPanel(1.f, lastV);
					nvgLineTo(args.vg, endPtSH.x, endPtSH.y);
				}

				nvgStrokeColor(args.vg, colSHCurve());
				nvgStrokeWidth(args.vg, mm2px(0.35f));
				nvgStroke(args.vg);

				// Dots: only meaningful for step-mode (they mark each held value)
				// In smooth mode we skip them for a cleaner look.
				if (!smoothOn) {
					for (int i = 0; i < realCount; ++i) {
						int idx = (oldestRealIdx + i) % Hutara_Random_CV::SH_HISTORY_SIZE;
						int tIdx = padCount + i;
						float t = static_cast<float>(tIdx)
							/ static_cast<float>(displayCount - 1);
						float v = valueToNorm(module->shHistory[idx]);
						Vec pt = tableToPanel(t, v);
						nvgBeginPath(args.vg);
						nvgCircle(args.vg, pt.x, pt.y, mm2px(0.35f));
						nvgFillColor(args.vg, colSHCurve());
						nvgFill(args.vg);
					}
				}

				// Current value marker at right edge (always visible)
				int lastIdx = (writePos - 1 + Hutara_Random_CV::SH_HISTORY_SIZE)
					% Hutara_Random_CV::SH_HISTORY_SIZE;
				float lastV = valueToNorm(module->shHistory[lastIdx]);
				Vec lastPt = tableToPanel(1.f, lastV);
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, lastPt.x, lastPt.y, mm2px(0.7f));
				nvgFillColor(args.vg, colSHDot());
				nvgFill(args.vg);
			}
		}

		// Playhead
		float pPhase = Hutara_Random_CV::clampValue(phase, 0.f, 1.f);
		float pv = module ? module->randomizedEnvelope(shape, warp, flux, randomness, pPhase) : 0.f;
		Vec ph = tableToPanel(pPhase, pv);
		Vec phTop = tableToPanel(pPhase, 1.f);
		Vec phBot = tableToPanel(pPhase, 0.f);
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

		// Mouse brush indicator
		if (drawing && hasMousePos) {
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, mousePos.x, mousePos.y, mm2px(1.0f));
			nvgFillColor(args.vg, colBrushOut());
			nvgFill(args.vg);
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, mousePos.x, mousePos.y, mm2px(0.45f));
			nvgFillColor(args.vg, colDot());
			nvgFill(args.vg); }

		// Buttons
		auto drawControl = [&](Rect rect, const char* label, bool enabled, bool hover) {
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y, mm2px(0.8f));
			if (enabled)
				nvgFillColor(args.vg, hover ? colBtnBgH() : colBtnBg());
			else
				nvgFillColor(args.vg, colBtnBgD());
			nvgFill(args.vg);
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y, mm2px(0.8f));
			nvgStrokeColor(args.vg,
				enabled ? colBtnBd() : colBtnBdD());
			nvgStrokeWidth(args.vg, mm2px(0.15f));
			nvgStroke(args.vg);
			nvgFontSize(args.vg, mm2px(1.8f));
			nvgFillColor(args.vg,
				enabled ? colText() : colTextDim());
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgText(args.vg, rect.pos.x + rect.size.x * 0.5f,
				rect.pos.y + rect.size.y * 0.5f, label, nullptr); };

		drawControl(undoRect(),  "UNDO",  module && (module->drawUndoCount > 0), hoverUndo);
		drawControl(resetRect(), "RESET", module && module->drawResetAvailable, hoverReset);

		// Text labels
		static std::shared_ptr<Font> font =
			APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (font) {
			nvgFontFaceId(args.vg, font->handle);

			// TOP-LEFT: DRAW / CURVE
			nvgFontSize(args.vg, mm2px(1.7f));
			nvgFillColor(args.vg, colText());
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
			const char* modeTxt = table ? "DRAW" : "CURVE";
			nvgText(args.vg, mx + mm2px(0.5f), my + mm2px(0.5f), modeTxt, nullptr);

			// TOP-RIGHT: PPQN and state
			if (syncMode && module) {
				int ppqnVal;
				if (module->dispPpqnIdx == 0) {
					ppqnVal = module->autoDetectedPpqn;
				} else {
					ppqnVal = Hutara_Random_CV::ppqnToInt(module->dispPpqnIdx);
				}
				std::string stateTxt;
				NVGcolor stateCol = colText();

				bool haveBpm = module->haveBpmMeasurement;
				bool clockConnected = module->inputs[Hutara_Random_CV::ENVELOPE_CLOCK_INPUT].isConnected();

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

			// BOTTOM row
			nvgFontSize(args.vg, mm2px(2.1f));
			nvgFillColor(args.vg, colText());
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
			float textY = h - controlHeight() - controlBottomMargin() - mm2px(0.5f);

			if (syncMode) {
				std::string divTxt = formatDivision(divIdx);
				std::string multTxt = Hutara_Random_CV::multiplierToLabel(multIdx);
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
			} else if (loop) {
				nvgText(args.vg, w - mx - mm2px(0.5f), textY, "LOOP", nullptr);
			}

			// ===== S&H DISPLAY: current value label =====
			if (module) {
				nvgFontSize(args.vg, mm2px(1.5f));
				nvgFillColor(args.vg, colSHLabel());
				nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
				float shLabelY = h - controlHeight() - controlBottomMargin() - mm2px(3.2f);
				std::string shTxt = string::f("S&H %.2fV", module->dispShValue);
				nvgText(args.vg, mx + mm2px(0.5f), shLabelY, shTxt.c_str(), nullptr);
			}
		}

		// Tooltip
		if (!drawing && hasMousePos && font && isInsideDrawArea(mousePos)) {
			float t, v;
			panelToTable(mousePos, t, v);
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, mm2px(1.6f));
			nvgFillColor(args.vg, colText());
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
			std::string txt = string::f("%.2f / %.2f", t, v);
			nvgText(args.vg, mousePos.x + mm2px(1.f), mousePos.y - mm2px(0.5f),
				txt.c_str(), nullptr); }

		nvgResetScissor(args.vg); }
};

// =========================================================================
// Widget
// =========================================================================
struct Hutara_Random_CVWidget : ModuleWidget {
	static constexpr int ADDITIONAL_PANEL_HP = 16;
	Hutara_Random_CVWidget(Hutara_Random_CV* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Hutaraa.svg")));
		const float totalWidth = box.size.x;
		const float addedWidth = static_cast<float>(ADDITIONAL_PANEL_HP) * RACK_GRID_WIDTH;
		const float legacyWidth = std::max(0.f, totalWidth - addedWidth);
		const float rightX = legacyWidth;
		const float rightW = std::max(0.f, totalWidth - rightX);

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0.f)));
		addChild(createWidget<ScrewSilver>(Vec(legacyWidth - 2.f * RACK_GRID_WIDTH, 0.f)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(legacyWidth - 2.f * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(rightX + RACK_GRID_WIDTH, 0.f)));
		addChild(createWidget<ScrewSilver>(Vec(totalWidth - 2.f * RACK_GRID_WIDTH, 0.f)));
		addChild(createWidget<ScrewSilver>(Vec(rightX + RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(totalWidth - 2.f * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Legacy panel controls
		addParam(createParam<Rogan1PWhite>(Vec(45.f, 70.f), module, Hutara_Random_CV::STRENGTH_PARAM));
		addParam(createParam<RoundSmallBlackKnob>(Vec(85.f, 70.f), module, Hutara_Random_CV::SMOOTH_RANDOM_PARAM));
		addParam(createParam<CKSSThree>(Vec(20.f, 330.f), module, Hutara_Random_CV::RANDOM_GATE_SPEED_PARAM));
		addParam(createParam<BefacoTinyKnob>(Vec(47.f, 215.f), module, Hutara_Random_CV::OUTPUT_VOLTAGE_PARAM));
		addParam(createParam<BefacoSwitch>(Vec(10.f, 175.f), module, Hutara_Random_CV::BIPOLAR_PARAM));
		addParam(createParam<RoundSmallBlackKnob>(Vec(47.f, 280.f), module, Hutara_Random_CV::GATE_LENGTH_PARAM));
		addParam(createParam<RoundBlackKnob>(Vec(47.f, 320.f), module, Hutara_Random_CV::TRIGGER_DIVISION_DIVIDER_PARAM));
		addParam(createParam<LEDSliderBlue>(Vec(84.f, 150.f), module, Hutara_Random_CV::S_H_OFFSET_PARAM));
		addParam(createParam<RoundSmallBlackKnob>(Vec(27.f, 300.f), module, Hutara_Random_CV::GATE_LENGTH_ADJUST_PARAM));
		addParam(createParam<MyHutaraSmallKnob>(Vec(65.f, 126.f), module, Hutara_Random_CV::MODULATION_INPUT_PARAM));
		addInput(createInput<PJ301MPort>(Vec(10.f, 55.f), module, Hutara_Random_CV::ON_INPUT));
		addInput(createInput<PJ301MPort>(Vec(85.f, 125.f), module, Hutara_Random_CV::MODULATION_INPUT));
		addOutput(createOutput<PJ3410Port>(Vec(46.f, 175.f), module, Hutara_Random_CV::OUTPUT));
		addOutput(createOutput<PJ3410Port>(Vec(85.f, 290.f), module, Hutara_Random_CV::GATE_OUTPUT));
		addOutput(createOutput<PJ3410Port>(Vec(85.f, 330.f), module, Hutara_Random_CV::GATE_OUTPUT_INVERTED));

		const float c1 = rightX + rightW * 0.20f;
		const float c2 = rightX + rightW * 0.50f;
		const float c3 = rightX + rightW * 0.80f;

		// Row 1: TIME, SHAPE, WARP
		addParam(createParamCentered<RoundBlackKnob>(Vec(c1, 164.f), module, Hutara_Random_CV::TIME_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(c2, 164.f), module, Hutara_Random_CV::SHAPE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(c3, 164.f), module, Hutara_Random_CV::WARP_PARAM));

		// Row 2: Time mode + BPM mult + PPQN
		addParam(createParamCentered<CKSS>(Vec(c1, 200.f), module, Hutara_Random_CV::TIME_SYNC_MODE_PARAM));
		addParam(createParamCentered<HutaraBpmMultKnob>(Vec(c2, 200.f), module, Hutara_Random_CV::BPM_MULTIPLIER_PARAM));
		addParam(createParamCentered<HutaraPpqnKnob>(Vec(c3, 200.f), module, Hutara_Random_CV::CLOCK_PPQN_PARAM));

		// Row 3: FLUX, RANDOMNESS, LEVEL
		addParam(createParamCentered<RoundBlackKnob>(Vec(c1, 240.f), module, Hutara_Random_CV::FLUX_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(c2, 240.f), module, Hutara_Random_CV::RANDOMNESS_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(c3, 240.f), module, Hutara_Random_CV::LEVEL_PARAM));

		// Row 4: ENHANCED, LOOP, TRIG
		addParam(createParamCentered<CKSS>(Vec(c1, 275.f), module, Hutara_Random_CV::ENHANCED_MODE_PARAM));
		addParam(createParamCentered<CKSS>(Vec(c2, 275.f), module, Hutara_Random_CV::LOOP_PARAM));
		addParam(createParamCentered<LEDButton>(Vec(c3, 275.f), module, Hutara_Random_CV::TRIG_BUTTON_PARAM));

		// Row 5: TIME_IN, SHAPE_IN, WARP_IN
		addInput(createInputCentered<PJ301MPort>(Vec(c1, 308.f), module, Hutara_Random_CV::TIME_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(c2, 308.f), module, Hutara_Random_CV::SHAPE_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(c3, 308.f), module, Hutara_Random_CV::WARP_INPUT));

		// Row 6: FLUX_IN, RANDOMNESS_IN, TRIG_IN
		addInput(createInputCentered<PJ301MPort>(Vec(c1, 340.f), module, Hutara_Random_CV::FLUX_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(c2, 340.f), module, Hutara_Random_CV::RANDOMNESS_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(c3, 340.f), module, Hutara_Random_CV::TRIG_INPUT));

		// Row 7: ENVELOPE_CLOCK
		addInput(createInputCentered<PJ301MPort>(Vec(c1 + 30.0f, 155.f), module, Hutara_Random_CV::ENVELOPE_CLOCK_INPUT));

		addOutput(createOutputCentered<PJ3410Port>(Vec(275, 352.f), module, Hutara_Random_CV::ENVELOPE_OUTPUT));
		addOutput(createOutputCentered<PJ3410Port>(Vec(205, 352.f), module, Hutara_Random_CV::EOC_OUTPUT));

		// Display added LAST for top z-order
		auto* display = new HutaraRandomDisplay;
		display->module = module;
		display->box.pos = Vec(rightX + 8.f, 30.f);
		display->box.size = Vec(rightW - 16.f, 105.f);
		addChild(display);
	}

	void appendContextMenu(Menu* menu) override {
		auto* module = getModule<Hutara_Random_CV>();
		if (!module) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Reset Envelope Phase", "", [module]() {
			module->envelopePhase = 1.f;
			module->running = false; }));
		auto* undoItem = createMenuItem("Undo Draw", "", [module]() {
			module->undoDraw(); });
		undoItem->disabled = (module->drawUndoCount <= 0);
		menu->addChild(undoItem);
		auto* resetItem = createMenuItem("Reset Draw", "", [module]() {
			module->resetDraw(); });
		resetItem->disabled = (!module->drawResetAvailable);
		menu->addChild(resetItem);
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Sync Mode: Free", "", [module]() {
			module->params[Hutara_Random_CV::TIME_SYNC_MODE_PARAM].setValue(0.f); }));
		menu->addChild(createMenuItem("Sync Mode: BPM", "", [module]() {
			module->params[Hutara_Random_CV::TIME_SYNC_MODE_PARAM].setValue(1.f); }));
		menu->addChild(createMenuItem("Reset BPM to 120", "", [module]() {
			module->measuredBpm = Hutara_Random_CV::DEFAULT_BPM;
			module->dispBpm = module->measuredBpm; })); }
};

Model* modelHutara_Random_CV =
	createModel<Hutara_Random_CV, Hutara_Random_CVWidget>("Hutara_Random_CV");