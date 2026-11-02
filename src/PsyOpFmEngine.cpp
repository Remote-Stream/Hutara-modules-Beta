#include "PsyOpFmEngine.hpp"
constexpr int FmOperator::CHAIN_PATTERNS[8][4];
constexpr float FmOperator::SHAPE_RMS[14];
// ============================================================
// FmOperator — implementation
// ============================================================

void FmOperator::applyGenreMode(int idx) {
	genreModePM.store(clamp(idx, 0, 3), std::memory_order_relaxed);
	if (idx == 1) {   // Psytrance
		params[SINE_WAVESHAPER_PARAM].setValue(5.f / 13.f);   // SQUELCH
		params[PSY_MACRO_PARAM].setValue(0.50f);
		params[FERAL_MACRO_PARAM].setValue(0.20f);
		params[FM_MODE_PARAM].setValue(0.f);   // EXP
		params[FM_PARAM].setValue(0.40f);
	} else if (idx == 2) {   // Darkpsy
		params[SINE_WAVESHAPER_PARAM].setValue(6.f / 13.f);   // RING-SINE
		params[PSY_MACRO_PARAM].setValue(0.30f);
		params[FERAL_MACRO_PARAM].setValue(0.60f);
		params[FM_MODE_PARAM].setValue(2.f);   // PHASE
		params[FM_PARAM].setValue(0.60f);
	} else if (idx == 3) {   // HiTech
		params[SINE_WAVESHAPER_PARAM].setValue(11.f / 13.f);  // PM-FOLD
		params[PSY_MACRO_PARAM].setValue(0.70f);
		params[FERAL_MACRO_PARAM].setValue(0.90f);
		params[FM_MODE_PARAM].setValue(0.f);   // EXP
		params[FM_PARAM].setValue(0.70f);
	}
}

void FmOperator::setModPitchRange(int rangeIdx) {
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

json_t* FmOperator::dataToJson() {
	json_t* rootJ = json_object();
	json_t* arr = json_array();
	for (int i = 0; i < 4; i++) {
		json_array_append_new(arr, json_integer((json_int_t)chainDepthPM[i].load()));
	}
	json_object_set_new(rootJ, "chainDepthPM", arr);
	json_object_set_new(rootJ, "modPitchRange",
		json_integer((json_int_t)modPitchRangePM.load(std::memory_order_relaxed)));
	json_object_set_new(rootJ, "genreMode",
		json_integer((json_int_t)genreModePM.load(std::memory_order_relaxed)));
	return rootJ;
}

void FmOperator::dataFromJson(json_t* rootJ) {
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
	json_t* genreJ = json_object_get(rootJ, "genreMode");
	if (genreJ) genreModePM.store(
		clamp((int)json_integer_value(genreJ), 0, 3),
		std::memory_order_relaxed);
}

FmOperator::FmOperator() : displayChainPacked(0u) {
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

	configParam(FM_SRC_MORPH_CV_AMOUNT_PARAM, -1.f, 1.f, 0.f,
	            "FM Source Morph CV Amount", "%", 0.f, 100.f);

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
	configInput(GATE_INPUT,                    "Gate Input (VCA & Hard Sync)");
	configInput(FM_SRC_MORPH_CV_INPUT,         "FM Source Morph CV");

	configOutput(SINE_OUTPUT,     "Sine Output");
	configOutput(SAW_OUTPUT,      "Saw Output");
	configOutput(TRIANGLE_OUTPUT, "Triangle Output");
	configOutput(SQUARE_OUTPUT,   "Square Output");
	configOutput(FINAL_OUTPUT,    "Processed Output");
	configOutput(SUM_OUTPUT,      "Clean Oscillator Sum");
	configOutput(SUB_OUTPUT,      "Sub Oscillator Output");
}

void FmOperator::onReset() {
	phaseSine = phaseSaw = phaseTriangle = phaseSquare = 0.f;
	prevPhaseSine = 0.f;
	subPhase = 0.f;
	subPhaseBase = 0.f;
	subPhasePrev = 0.f;
	modPhase = 0.f;
	modPitchSmooth = 0.f;

	gateState = 0.f;
	lastGate = 0.f;
	gateVCA = 1.f;
	gateSmooth = 1.f;

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
	fmSrcMorphCvSmooth = 0.f;

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
		waveBufSub[i]  = 0.f;
	}
	for (int i = 0; i < WAVE_SNAPSHOT_SIZE; i++) {
		snapSine[i] = snapSaw[i] = 0.f;
		snapTri[i]  = snapSquare[i] = 0.f;
		snapSum[i]  = 0.f;
		snapSub[i]  = 0.f;
	}
	waveIdx = 0;
	snapCounter = 0;

	if (APP && APP->engine) {
		updateDcCoeff(APP->engine->getSampleRate());
	}
}

void FmOperator::onSampleRateChange() {
	updateDcCoeff(APP->engine->getSampleRate());
	prevDarkForCoeff = -1.f;
	srrCounter = 0.f;
	srrHold = 0.f;
}

void FmOperator::updateDcCoeff(float sampleRate) {
	if (sampleRate <= 1.f) return;
	dcCoeff = std::exp(-2.f * M_PI * DC_CUTOFF / sampleRate);
	dcCoeff = clamp(dcCoeff, 0.9f, 0.99999f);
	updateFinalDcCoeff(sampleRate);
}
void FmOperator::updateFinalDcCoeff(float sampleRate) {
	if (sampleRate <= 1.f) return;
	finalDcCoeff = std::exp(-2.f * M_PI * FINAL_DC_CUTOFF / sampleRate);
	finalDcCoeff = clamp(finalDcCoeff, 0.9f, 0.99999f);
	finalDcSr = sampleRate;
}

float FmOperator::computeTurbulence(TurbulenceState& t, float internalEnv,
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

// ============================================================
// shortParamName / shortValueText
// ============================================================

const char* FmOperator::shortParamName(int paramId) {
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
		case FM_SRC_MORPH_CV_AMOUNT_PARAM: return "MORPH CV AMT";
		default:                    return "PARAM";
	}
}


std::string FmOperator::shortValueText(int paramId, float value, bool pitchStepMode) const {
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
				"SINE",  "TRI",   "SQR",   "SAW",   "PULS",
				"SQLCH", "RING",  "FORM",  "FOLD",  "SYNC2",
				"SYNC3", "PMFD",  "CHB5",  "NOIS"
			};
			const float pos = clamp(value, 0.f, 1.f) * 13.f;
			const int idx = clamp((int)std::round(pos), 0, 13);
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
		case FM_SRC_MORPH_CV_AMOUNT_PARAM:
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

// ============================================================
// process() — main DSP loop
// ============================================================
void FmOperator::process(const ProcessArgs& args) {
	fmopFlushToZero();
	const float sr = args.sampleRate;
	if (sr <= 1.f) return;
	const float dt = 1.f / sr;
	if (sr != finalDcSr) updateDcCoeff(sr);

	int sampleIdx = 0;

	// === GATE PROCESSING (VCA) ===
	const float gateIn = finite(inputs[GATE_INPUT].getVoltage());
	gateState = (gateIn > 1.f) ? 1.f : 0.f;

	if (gateState > 0.5f && lastGate <= 0.5f) {
		phaseSine = 0.f;
		phaseSaw = 0.f;
		phaseTriangle = 0.f;
		phaseSquare = 0.f;
		modPhase = 0.f;
		subPhaseBase = 0.f;
	}
	lastGate = gateState;

	if (inputs[GATE_INPUT].isConnected()) {
		const float target = gateState;
		const float tau = (gateState > 0.5f) ? gateAttack : gateRelease;
		const float coeff = 1.f - std::exp(-dt / tau);
		gateSmooth += (target - gateSmooth) * coeff;
		gateSmooth = clampSafe(gateSmooth, 0.f, 1.f);
		gateSmooth = flushDenorm(gateSmooth);
		gateVCA = gateSmooth;
	} else {
		gateVCA = 1.f;
		gateSmooth = 1.f;
	}

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

	// ============================================================
	// [FIX v5] FM Source Morph CV — BIPOLAR/UNIPOLAR/REVERSE
	// ============================================================
	float sineMorphBase = params[SINE_WAVESHAPER_PARAM].getValue();
	sineMorphBase += shapeCv * 0.30f * shapeKnob;
	sineMorphBase += 0.55f * psy * (1.f - 0.30f * psyLate);
	sineMorphBase += 0.25f * turbulence * motionEffective;
	sineMorphBase = clampSafe(sineMorphBase, 0.f, 1.f);

	const float fmSrcMorphCvRaw =
		finite(inputs[FM_SRC_MORPH_CV_INPUT].getVoltage());

	float fmSrcMorphCvInterpreted = 0.f;
	switch (cvMode) {
		case 1:  // UNIPOLAR
			fmSrcMorphCvInterpreted =
				clampSafe(fmSrcMorphCvRaw, 0.f, 10.f) * 0.2f - 1.f;
			break;
		case 2:  // REVERSE
			fmSrcMorphCvInterpreted =
				-clampSafe(fmSrcMorphCvRaw, -10.f, 10.f) * 0.1f;
			break;
		case 0:
		default:  // BIPOLAR
			fmSrcMorphCvInterpreted =
				clampSafe(fmSrcMorphCvRaw, -10.f, 10.f) * 0.1f;
			break;
	}

	fmSrcMorphCvSmooth = rateIndependentSmooth(
		fmSrcMorphCvSmooth, fmSrcMorphCvInterpreted, dt, 0.008f);
	fmSrcMorphCvSmooth = flushDenorm(finite(fmSrcMorphCvSmooth));

	const float morphCvNorm = clampSafe(fmSrcMorphCvSmooth, -1.f, 1.f);

	const float fmSrcMorphCvAmt = clampSafe(
		params[FM_SRC_MORPH_CV_AMOUNT_PARAM].getValue(), -1.f, 1.f);

	const bool cvConnected = inputs[FM_SRC_MORPH_CV_INPUT].isConnected();
	const float absAmt = std::abs(fmSrcMorphCvAmt);
	float effectiveBase = sineMorphBase;
	if (cvConnected && absAmt > 0.01f) {
		const float headroom = absAmt * 0.5f;
		effectiveBase = clampSafe(sineMorphBase, headroom, 1.f - headroom);
	}

	const float morphOffset = morphCvNorm * fmSrcMorphCvAmt;
	float sineMorphAmt = clampSafe(effectiveBase + morphOffset, 0.f, 1.f);

	const float morphDelta = std::abs(sineMorphAmt - effectiveBase);
	lights[FM_SRC_MORPH_CV_LIGHT].setBrightness(
		clamp(morphDelta * 2.f, 0.f, 1.f));

	// --- Modulator pitch ---
	const float sineKnobEarly = params[PITCH_PARAM_SINE].getValue();
	const float sineKnobEarlyUsed = pitchStepMode
		? std::round(sineKnobEarly) : sineKnobEarly;
	const float sinePitchEarly = clampSafe(
		sineKnobEarlyUsed
		+ inputs[PITCH_INPUT_SINE].getVoltage()
		+ pitchAll
		+ pitchBusOffsetSine, -12.f, 12.f);

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

	// 10.5) SUB AS MODULATOR
	const int subFmRoute = clamp((int)params[SUB_FM_ROUTE_PARAM].getValue(), 0, 3);
	float fmIndexSub = fmIndex;
	if (subFmRoute == SUB_ROUTE_MODULATOR) {
		const float subPrev = std::sin(PI2 * subPhasePrev);
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

	// 13) PHASE ADVANCE
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

	sampleIdx = waveIdx;

	outputs[SINE_OUTPUT    ].setVoltage(clamp(sineOut     * 5.f, -5.f, 5.f) * gateVCA);
	outputs[SAW_OUTPUT     ].setVoltage(clamp(sawOut      * 5.f, -5.f, 5.f) * gateVCA);
	outputs[TRIANGLE_OUTPUT].setVoltage(clamp(triangleOut * 5.f, -5.f, 5.f) * gateVCA);
	outputs[SQUARE_OUTPUT  ].setVoltage(clamp(squareOut   * 5.f, -5.f, 5.f) * gateVCA);

	const float sum = clampSafe(
		sineOut + sawOut + triangleOut + squareOut, -4.f, 4.f);
	waveBufSum[waveIdx] = clamp(sum * 0.35f, -1.f, 1.f);

	waveIdx = (waveIdx + 1) % WAVE_BUF_SIZE;

	outputs[SUM_OUTPUT].setVoltage(SUM_OUTPUT_SCALE * std::tanh(sum * 0.5f) * gateVCA);

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

	// 20) ENHANCED SUB-OSCILLATOR
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

	subPhaseBase = wrap(subPhaseBase + subFreq / sr);
	subPhase = wrap(subPhaseBase + subPhaseOff);

	const float subDt = clampSafe(std::abs(subFreq) / sr, 1e-6f, 0.45f);
	const float subRawSine = std::sin(PI2 * subPhase);
	float subRaw;
	switch (subWave) {
		case 0: subRaw = subRawSine; break;
		case 1: subRaw = triangleBL(subPhase); break;
		case 2: subRaw = sawBL(subPhase, subDt); break;
		case 3: subRaw = squareBL(subPhase, subDt); break;
		case 4: subRaw = std::tanh(2.4f * subRawSine); break;
		case 5: subRaw = evalFmSourceShape(SM_SYNC_2_1, subPhase, subDt, subRawSine, 1.f); break;
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

	float subOutVoltage = 0.f;
	if (subFmRoute != SUB_ROUTE_OFF) {
		const float subDirect = subShaped * subLevelParam;
		subOutVoltage = clamp(subDirect * 5.f, -5.f, 5.f);
	}
	outputs[SUB_OUTPUT].setVoltage(subOutVoltage * gateVCA);

	waveBufSub[sampleIdx] = clamp(subShaped, -1.f, 1.f);

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
			snapSub[i]    = waveBufSub[srcIdx];
		}
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
	outputs[FINAL_OUTPUT].setVoltage(finalVoltage * gateVCA);

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

	subPhasePrev = subPhase;
}