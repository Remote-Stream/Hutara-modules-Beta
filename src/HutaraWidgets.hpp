#pragma once

#include "plugin.hpp"

// ============================================================
// Hutara shared UI widgets
// ============================================================
// These are small visual helpers shared across modules.
// All methods are inline so this is a header-only file.

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
	PsyKnobSmall() { box.size = box.size.mult(0.6f); }
	void draw(const DrawArgs& args) override {
		nvgSave(args.vg);
		nvgScale(args.vg, 0.6f, 0.6f);

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