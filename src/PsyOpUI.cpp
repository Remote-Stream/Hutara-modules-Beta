#include "plugin.hpp"
#include "HutaraWidgets.hpp"
#include "PsyOpFmEngine.hpp"

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
	float chainAnimTime  = 0.f;

	float seenValue[4]   = {0.f, 0.f, 0.f, 0.f};
	bool  seenInit[4]    = {false, false, false, false};

	static constexpr float ANIM_ARROW_SPEED = 1.8f;

	std::shared_ptr<Font> font;

	static constexpr float SIZE_MM  = 21.f;
	static constexpr float UI_SCALE = 1.f;

	static int segParamId(int seg) {
		switch (seg) {
			case 0: return FmOperator::FM_MODE_PARAM;
			case 1: return FmOperator::FM_ROUTING_PARAM;
			case 2: return FmOperator::FM_SYNC_PARAM;
			case 3: return FmOperator::FM_CHAIN_PATTERN_PARAM;
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
			case 0: return nvgRGBA(  0, 255, 245, 255);  // MODE  → Bright Cyan
			case 1: return nvgRGBA(  0, 250, 180, 255);  // ROUTE → Deep Teal
			case 2: return nvgRGBA(100, 255, 255, 255);  // SYNC  → Light Aqua
			case 3: return nvgRGBA(  0, 255, 255, 255);  // CHAIN → Mid Cyan
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
		return R * 0.66f * 0.22f;
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
			const float newStrength = clamp(syncDragStart - syncDragAccumY / 50.f, 0.f, 1.f);
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
			if (std::abs(cur - seenValue[s]) > 0.5f && focusedSeg != s && flashAlpha < 0.05f)
				focusedSeg = s;
			seenValue[s] = cur;
		}
		if (flashAlpha > 0.f) {
			float dt = APP->window->getFrameTime();
			if (dt <= 0.f || !std::isfinite(dt) || dt > 0.1f) dt = 1.f/60.f;
			flashAlpha -= dt * 2.5f;
			if (flashAlpha < 0.f) flashAlpha = 0.f;
		}
		float animDt = APP->window->getFrameTime();
		if (animDt <= 0.f || !std::isfinite(animDt) || animDt > 0.1f) animDt = 1.f/60.f;
		chainAnimTime += animDt;
		const float period = 1.f / ANIM_ARROW_SPEED;
		if (chainAnimTime > period * 1000.f) {
			chainAnimTime = std::fmod(chainAnimTime, period);
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
				NVGcolor glow = col; glow.a = focused ? 0.28f : 0.08f;
				nvgBeginPath(vg); nvgArc(vg, cx, cy, rMid, a0, a1, NVG_CW);
				nvgStrokeWidth(vg, ringThickness + 4.f);
				nvgStrokeColor(vg, glow); nvgStroke(vg);
			}
			{
				NVGcolor c = col; c.a = focused ? 1.f : 0.45f;
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

		if (focusedSeg == 0) {
			const float fmIdx = module->params[FmOperator::FM_PARAM].getValue();
			const float barW = Rinner * 1.15f;
			const float barH = Rinner * 0.10f;
			const float barY = cy + Rinner * 0.50f;
			nvgBeginPath(vg);
			nvgRoundedRect(vg, cx - barW/2, barY, barW, barH, barH/2);
			nvgFillColor(vg, nvgRGBA(0, 0, 0, 160)); nvgFill(vg);
			const float amt = clamp((fmIdx + 1.2f) / 4.43f, 0.f, 1.f);
			nvgBeginPath(vg);
			nvgRoundedRect(vg, cx - barW/2, barY, barW * amt, barH, barH/2);
			nvgFillColor(vg, nvgRGBA(60, 255, 255, 200)); nvgFill(vg);
			nvgStrokeColor(vg, nvgRGBA(60, 255, 255, 90));
			nvgStrokeWidth(vg, 0.4f); nvgStroke(vg);
		}

		nvgFontFaceId(vg, font->handle);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFontSize(vg, Rinner * 0.30f);
		nvgFillColor(vg, nvgRGBA((int)(col.r*220), (int)(col.g*220), (int)(col.b*220), 220));
		nvgText(vg, cx, cy - Rinner * 0.35f, segLabel(focusedSeg), nullptr);
		nvgFontSize(vg, Rinner * 0.55f);
		nvgFillColor(vg, nvgRGBA(220, 255, 230, 240));
		nvgText(vg, cx, cy + Rinner * 0.05f, valStr.c_str(), nullptr);
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
			const Vec end   = p2.minus(dir.mult(nodeR + 0.5f));
			NVGcolor col = nodeCol[mod]; col.a = 0.45f;
			nvgBeginPath(vg); nvgMoveTo(vg, start.x, start.y);
			nvgLineTo(vg, end.x, end.y);
			nvgStrokeColor(vg, col); nvgStrokeWidth(vg, 1.0f); nvgStroke(vg);
		}

		const float arrowSpeed = ANIM_ARROW_SPEED;
		const float arrowSize  = 3.0f;
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
			const Vec end   = p2.minus(dir.mult(nodeR + 0.5f));
			const float arrowLen = std::sqrt(
				(end.x - start.x)*(end.x - start.x) +
				(end.y - start.y)*(end.y - start.y));
			if (arrowLen < 2.f) continue;

			NVGcolor col = nodeCol[mod]; col.a = 1.f;
			const int displayCount = clamp((int)(arrowLen / 15.f) + 2, 2, 4);
			const float basePhase = chainAnimTime * arrowSpeed;

			for (int k = 0; k < displayCount; k++) {
				float t = basePhase + (float)k / (float)displayCount;
				t = t - std::floor(t);
				const Vec arrowPos = Vec(
					start.x + (end.x - start.x) * t,
					start.y + (end.y - start.y) * t);

				const Vec perp = Vec(-dir.y, dir.x);
				const Vec tip   = arrowPos.plus(dir.mult(arrowSize));
				const Vec back1 = arrowPos.plus(perp.mult(arrowSize * 0.6f));
				const Vec back2 = arrowPos.plus(perp.mult(-arrowSize * 0.6f));

				NVGcolor glow = col; glow.a = 0.3f;
				nvgBeginPath(vg);
				nvgMoveTo(vg, tip.x, tip.y);
				nvgLineTo(vg, back1.x, back1.y);
				nvgLineTo(vg, back2.x, back2.y);
				nvgClosePath(vg);
				nvgFillColor(vg, glow); nvgFill(vg);

				nvgBeginPath(vg);
				nvgMoveTo(vg, tip.x, tip.y);
				nvgLineTo(vg, back1.x, back1.y);
				nvgLineTo(vg, back2.x, back2.y);
				nvgClosePath(vg);
				nvgFillColor(vg, col); nvgFill(vg);
			}
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
			nvgFontFaceId(vg, font->handle);
			nvgFontSize(vg, R * 0.16f);
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
			nvgFillColor(vg, nvgRGBA(150, 200, 255, 180));
			nvgText(vg, cx, cy + R * 0.88f, "drag node to set %", nullptr);
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
			case 4: {
				const int sw = module
					? clamp((int)module->params[FmOperator::SUB_WAVE_PARAM].getValue(), 0, 5)
					: 1;
				switch (sw) {
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
						nvgLineTo(vg, 0.f, hh); nvgLineTo(vg, hw*0.5f, -hh);
						nvgLineTo(vg, hw, hh);
						break;
					case 2:
						nvgMoveTo(vg, -hw, hh); nvgLineTo(vg, -hw*0.5f, -hh);
						nvgLineTo(vg, -hw*0.5f, hh); nvgLineTo(vg, 0.f, -hh);
						nvgLineTo(vg, 0.f, hh); nvgLineTo(vg, hw*0.5f, -hh);
						nvgLineTo(vg, hw*0.5f, hh); nvgLineTo(vg, hw, -hh);
						break;
					case 3:
						nvgMoveTo(vg, -hw, -hh); nvgLineTo(vg, -hw*0.5f, -hh);
						nvgLineTo(vg, -hw*0.5f, hh); nvgLineTo(vg, 0.f, hh);
						nvgLineTo(vg, 0.f, -hh); nvgLineTo(vg, hw*0.5f, -hh);
						nvgLineTo(vg, hw*0.5f, hh); nvgLineTo(vg, hw, hh);
						break;
					case 4: {
						const int N = 12;
						for (int i = 0; i <= N; i++) {
							const float t = (float)i / N;
							const float px = -hw + t * w;
							const float py = -std::tanh(2.4f * std::sin(t * 2.f * M_PI)) * hh;
							if (i == 0) nvgMoveTo(vg, px, py); else nvgLineTo(vg, px, py);
						}
						break;
					}
					case 5: {
						const int N = 12;
						for (int i = 0; i <= N; i++) {
							const float t = (float)i / N;
							const float px = -hw + t * w;
							const float ph = t * 2.5f;
							const float py = -std::sin(ph * 2.f * M_PI) * hh;
							if (i == 0) nvgMoveTo(vg, px, py); else nvgLineTo(vg, px, py);
						}
						break;
					}
				}
				break;
			}
		}
		nvgStrokeColor(vg, nvgRGBA(8, 12, 18, 245));
		nvgStrokeWidth(vg, 1.1f);
		nvgLineJoin(vg, NVG_ROUND);
		nvgLineCap(vg, NVG_ROUND);
		nvgStroke(vg);
		nvgRestore(vg);
	}
};

// ============================================================
// SUB WAVE DISPLAY
// ============================================================
struct SubWaveDisplay : TransparentWidget {
	FmOperator* module = nullptr;
	std::shared_ptr<Font> font;

	static constexpr float WIDTH_MM  = 12.f;
	static constexpr float HEIGHT_MM = 4.f;
	static constexpr float BTN_W_MM  = 3.f;

	int hoverBtn = 0;

	SubWaveDisplay() {
		box.size = Vec(mm2px(Vec(WIDTH_MM, 0.f)).x,
		               mm2px(Vec(0.f, HEIGHT_MM)).y);
	}

	float btnWidthPx() const {
		return mm2px(Vec(BTN_W_MM, 0.f)).x;
	}

	void onHover(const event::Hover& e) override {
		TransparentWidget::onHover(e);
		const float bwp = btnWidthPx();
		if (e.pos.x < bwp) hoverBtn = -1;
		else if (e.pos.x > box.size.x - bwp) hoverBtn = 1;
		else hoverBtn = 0;
	}

	void onButton(const event::Button& e) override {
		TransparentWidget::onButton(e);
		if (!module) return;
		if (e.action != GLFW_PRESS || e.button != GLFW_MOUSE_BUTTON_LEFT) return;

		const float bwp = btnWidthPx();
		int dir = 0;
		if (e.pos.x < bwp) dir = 1;                      
		else if (e.pos.x > box.size.x - bwp) dir = -1;    
		if (dir == 0) return;

		const int cur = clamp(
			(int)std::round(module->params[FmOperator::SUB_WAVE_PARAM].getValue()), 0, 5);
		const int next = (cur + dir + 6) % 6;
		if (auto* pq = module->paramQuantities[FmOperator::SUB_WAVE_PARAM]) {
			pq->setValue((float)next);
		}
		e.consume(this);
	}

	void drawChevron(NVGcontext* vg, float cx, float cy, float size,
	                 bool left, NVGcolor col) {
		const float dir = left ? -1.f : 1.f;
		nvgBeginPath(vg);
		nvgMoveTo(vg, cx + dir * size * 0.45f, cy - size);
		nvgLineTo(vg, cx - dir * size * 0.45f, cy);
		nvgLineTo(vg, cx + dir * size * 0.45f, cy + size);
		nvgStrokeColor(vg, col);
		nvgStrokeWidth(vg, 1.3f);
		nvgLineCap(vg, NVG_ROUND);
		nvgLineJoin(vg, NVG_ROUND);
		nvgStroke(vg);
	}

	void draw(const DrawArgs& args) override {
		if (!module) return;
		if (!font) {
			font = APP->window->loadFont(
				asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		}

		NVGcontext* vg = args.vg;
		const float w = box.size.x;
		const float h = box.size.y;
		const float bwp = btnWidthPx();
		const float pad = 1.f;

		const float waveX0 = bwp + pad;
		const float waveX1 = w - bwp - pad;
		const float waveW  = waveX1 - waveX0;
		const float y0 = pad;
		const float y1 = h - pad;
		const float innerH = y1 - y0;
		const float midY = y0 + innerH * 0.5f;

		const int route = clamp(
			(int)std::round(module->params[FmOperator::SUB_FM_ROUTE_PARAM].getValue()), 0, 3);
		const bool active = (route != FmOperator::SUB_ROUTE_OFF);

		const int curWave = clamp(
			(int)std::round(module->params[FmOperator::SUB_WAVE_PARAM].getValue()), 0, 5);
		static const char* waveNames[] = { "SIN", "TRI", "SAW", "SQR", "FLD", "SYN" };

		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0.5f, 0.5f, w - 1.f, h - 1.f, 1.8f);
		nvgFillColor(vg, nvgRGBA(2, 4, 6, active ? 225 : 145));
		nvgFill(vg);
		nvgStrokeColor(vg, active ? nvgRGBA(60, 255, 120, 130)
		                          : nvgRGBA(80, 80, 80, 90));
		nvgStrokeWidth(vg, 0.7f);
		nvgStroke(vg);

		{
			const float bx = 0.5f, by = 0.5f;
			const float bw = bwp - 0.5f, bh = h - 1.f;
			const bool hovered = (hoverBtn == -1);
			nvgBeginPath(vg);
			nvgRoundedRect(vg, bx, by, bw, bh, 1.6f);
			nvgFillColor(vg, hovered ? nvgRGBA(60, 255, 120, 60)
			                         : nvgRGBA(30, 50, 35, 80));
			nvgFill(vg);
			nvgStrokeColor(vg, active ? nvgRGBA(60, 255, 120, 100)
			                          : nvgRGBA(80, 80, 80, 60));
			nvgStrokeWidth(vg, 0.5f);
			nvgStroke(vg);
			drawChevron(vg, bx + bw * 0.5f, by + bh * 0.5f,
						bh * 0.28f, false,
						hovered ? nvgRGBA(160, 255, 180, 255)
								: nvgRGBA(120, 220, 150, 200));
					}

		{
			const float bx = w - bwp + 0.5f, by = 0.5f;
			const float bw = bwp - 0.5f, bh = h - 1.f;
			const bool hovered = (hoverBtn == 1);
			nvgBeginPath(vg);
			nvgRoundedRect(vg, bx, by, bw, bh, 1.6f);
			nvgFillColor(vg, hovered ? nvgRGBA(60, 255, 120, 60)
			                         : nvgRGBA(30, 50, 35, 80));
			nvgFill(vg);
			nvgStrokeColor(vg, active ? nvgRGBA(60, 255, 120, 100)
			                          : nvgRGBA(80, 80, 80, 60));
			nvgStrokeWidth(vg, 0.5f);
			nvgStroke(vg);
			drawChevron(vg, bx + bw * 0.5f, by + bh * 0.5f,
						bh * 0.28f, true,
						hovered ? nvgRGBA(160, 255, 180, 255)
								: nvgRGBA(120, 220, 150, 200));
		}

		nvgBeginPath(vg);
		nvgRoundedRect(vg, waveX0, y0, waveW, innerH, 1.2f);
		nvgFillColor(vg, nvgRGBA(4, 8, 10, active ? 200 : 120));
		nvgFill(vg);

		nvgBeginPath(vg);
		nvgMoveTo(vg, waveX0 + 1.f, midY);
		nvgLineTo(vg, waveX0 + waveW - 1.f, midY);
		nvgStrokeColor(vg, nvgRGBA(0, 200, 60, 45));
		nvgStrokeWidth(vg, 0.4f);
		nvgStroke(vg);

		if (!active) {
			if (font) {
				nvgFontFaceId(vg, font->handle);
				nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
				nvgFontSize(vg, innerH * 0.35f);
				nvgFillColor(vg, nvgRGBA(120, 120, 120, 180));
				nvgText(vg, waveX0 + waveW * 0.5f, midY, "OFF", nullptr);
			}
			return;
		}

		const int ss = FmOperator::WAVE_SNAPSHOT_SIZE;
		const float* buf = module->snapSub;
		float peak = 0.f;
		for (int i = 0; i < ss; i++) {
			const float a = std::abs(buf[i]);
			if (a > peak) peak = a;
		}
		const float gain = (peak > 0.02f) ? (0.85f / std::max(peak, 0.05f)) : 1.f;
		const float drawX0 = waveX0 + 1.0f;
		const float drawW  = waveW - 2.0f;
		const float halfH  = (innerH - 1.5f) * 0.5f;

		const int N = 80;
		nvgBeginPath(vg);
		for (int i = 0; i < N; i++) {
			const float t = (float)i / (N - 1);
			const int srcIdx = clamp((int)std::round(t * (ss - 1)), 0, ss - 1);
			const int i0 = clamp(srcIdx - 1, 0, ss - 1);
			const int i1 = srcIdx;
			const int i2 = clamp(srcIdx + 1, 0, ss - 1);
			const float v = (buf[i0] + buf[i1] * 2.f + buf[i2]) * 0.25f;
			const float s = clamp(v * gain, -1.f, 1.f);
			const float px = drawX0 + t * drawW;
			const float py = midY - s * halfH;
			if (i == 0) nvgMoveTo(vg, px, py); else nvgLineTo(vg, px, py);
		}
		nvgStrokeColor(vg, nvgRGBA(60, 255, 120, 240));
		nvgStrokeWidth(vg, 1.0f);
		nvgLineJoin(vg, NVG_ROUND);
		nvgLineCap(vg, NVG_ROUND);
		nvgStroke(vg);

		if (font) {
			nvgFontFaceId(vg, font->handle);
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
			nvgFontSize(vg, innerH * 0.22f);
			nvgFillColor(vg, nvgRGBA(180, 255, 200, 200));
			nvgText(vg, waveX0 + waveW * 0.5f, y0 + 1.f, waveNames[curWave], nullptr);
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

	static const std::array<int, 62>& watchedParams() {
		static const std::array<int, 62> watched = {{
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
			FmOperator::SUB_WAVE_PARAM,
			FmOperator::SUB_FREQ_MODE_PARAM,
			FmOperator::SUB_PITCH_PARAM,
			FmOperator::SUB_PHASE_PARAM,
			FmOperator::SUB_FM_ROUTE_PARAM,
			FmOperator::SUB_RING_PARAM,
			FmOperator::SUB_DRIFT_PARAM,
			FmOperator::FM_SRC_MORPH_CV_AMOUNT_PARAM,
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
		const float* buf = module->snapSub;   // ← از snapSum به snapSub تغییر کرد
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
// SVG LOGOS
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
// WIDGET (UI layout)
// ============================================================

struct FmOperatorWidget : ModuleWidget {

	FmOperatorWidget(FmOperator* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/HutaraFm.svg")));

		// Safe mandala background
		if (APP && APP->window) {
			std::shared_ptr<Svg> mandalaSvg =
				APP->window->loadSvg(asset::plugin(pluginInstance, "res/mandala.svg"));
			if (mandalaSvg) {
				SvgLight* svgLight = createWidget<SvgLight>(mm2px(Vec(60.f, 50.f)));
				svgLight->setSvg(mandalaSvg);
				addChild(svgLight);
			} else {
				WARN("Hutara: could not load res/mandala.svg");
			}
		}

		// Hutara logo
		HutaraLogoSvgLight* hutaraLogo = new HutaraLogoSvgLight();
		hutaraLogo->module = module;
		{
			const Vec center = mm2px(Vec(57.24f, 40.263f));
			hutaraLogo->box.pos = center.minus(hutaraLogo->box.size.mult(0.5f));
		}
		addChild(hutaraLogo);

		// FM Amount LED
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

		// Displays
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
		{
			SubWaveDisplay* subDisp = new SubWaveDisplay();
			subDisp->module = module;
			const Vec center = mm2px(Vec(93.f, 112.f));
			subDisp->box.pos = center.minus(subDisp->box.size.mult(0.5f));
			addChild(subDisp);
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
			mm2px(Vec(45.f, 51.f)), module, FmOperator::PITCH_BUS_DEPTH_SAW));
		addParam(createParamCentered<PitchBusDepthKnob>(
			mm2px(Vec(45.f, 63.f)), module, FmOperator::PITCH_BUS_DEPTH_SQUARE));

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

		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(98.f, 38.f)), module, FmOperator::FM_SRC_MORPH_CV_INPUT));
		addParam(createParamCentered<CVAMOUNTKnob>(
			mm2px(Vec(106.24f, 38.f)), module, FmOperator::FM_SRC_MORPH_CV_AMOUNT_PARAM));
		addChild(createLightCentered<SmallLight<GreenLight>>(
			mm2px(Vec(107.f, 32.5f)), module, FmOperator::FM_SRC_MORPH_CV_LIGHT));

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
			mm2px(Vec(14.f, 6.f)), module, FmOperator::PITCH_INPUT_ALL));
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(25.f, 6.f)), module, FmOperator::GATE_INPUT));

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
		addOutput(createOutputCentered<DarkPJ301MPort>(
			mm2px(Vec(93.f, 118.f)), module, FmOperator::SUB_OUTPUT));

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

		menu->addChild(createIndexSubmenuItem("Genre Mode",
			{"Off (manual)", "Psytrance", "Darkpsy", "HiTech"},
			[m]() {
				return clamp(m->genreModePM.load(std::memory_order_relaxed), 0, 3);
			},
			[m](int idx) {
				m->applyGenreMode(idx);
			}
		));

		menu->addChild(new MenuSeparator());

		menu->addChild(createIndexSubmenuItem("FM Source Morph Shape",
			{
				"Sine",
				"Triangle (BL)",
				"Square (BL)",
				"Saw (BL)",
				"Pulse-Variable (HiTech)",
				"Squelch (Psy) *",
				"Ring-Sine (Dark) *",
				"Formant 4:1 (Dark)",
				"Fold (tanh)",
				"Sync 2:1 (Dark/HiTech)",
				"Sync 3:2 (Inharmonic)",
				"PM-Fold (HiTech) *",
				"Chebyshev T5",
				"Noise-Sine (HiTech)"
			},
			[m]() {
				const float cur = m->params[FmOperator::SINE_WAVESHAPER_PARAM].getValue();
				const float pos = clamp(cur, 0.f, 1.f) * 13.f;
				return clamp((int)std::round(pos), 0, 13);
			},
			[m](int idx) {
				m->params[FmOperator::SINE_WAVESHAPER_PARAM].setValue(
					clamp(idx, 0, 13) / 13.f);
			}
		));

		menu->addChild(createIndexSubmenuItem("FM Source Morph CV Amount",
			{"-100%", "-50%", "0%", "+50%", "+100%"},
			[m]() {
				return clamp((int)std::round(
					m->params[FmOperator::FM_SRC_MORPH_CV_AMOUNT_PARAM].getValue() * 2.f), -2, 2) + 2;
			},
			[m](int idx) {
				m->params[FmOperator::FM_SRC_MORPH_CV_AMOUNT_PARAM].setValue(
					(clamp(idx, 0, 4) - 2) * 0.5f);
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

// ============================================================
// Model registration
// ============================================================
Model* modelFmOperator =
	createModel<FmOperator, FmOperatorWidget>("FmOperator");