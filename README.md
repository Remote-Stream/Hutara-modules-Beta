# Hutara PsyOperator

A multi-operator FM synthesis module for VCV Rack 2, designed for **darkpsy / hitech / psytrance** sound design. PsyOperator combines four phase-locked oscillators, an independent morphing FM modulator, macro-driven chaos/turbulence, and a rich post-processing chain into a single performance-oriented module.

---

## Table of Contents

1. [Overview](#overview)
2. [Features](#features)
3. [Signal Flow](#signal-flow)
4. [Panel Overview](#panel-overview)
5. [Sections](#sections)
   - [Oscillator Section](#1-oscillator-section)
   - [Pitch Section](#2-pitch-section)
   - [FM Section](#3-fm-section)
   - [Modulator Section](#4-modulator-section)
   - [Macro & Turbulence](#5-macro--turbulence)
   - [Post-Processor](#6-post-processor)
   - [Displays](#7-displays)
   - [Outputs](#8-outputs)
6. [FM Source Morph (Deep Dive)](#fm-source-morph-deep-dive)
7. [Context Menu](#context-menu)
8. [Darkpsy / Hitech / Psytrance Tips](#darkpsy--hitech--psytrance-tips)
9. [CV Compatibility](#cv-compatibility)
10. [Changelog](#changelog)
11. [Credits & License](#credits--license)

---

## Overview

PsyOperator is a 16 HP Eurorack-style module built around four oscillators (Sine, Triangle, Saw, Square) that share a unified FM engine. The **Sine** oscillator is special: it is driven by the *FM Source Morph* parameter, which smoothly interpolates through six FM-optimized waveforms (Sine → Folded → Chebyshev → Asymmetric → Sync → Formant).

The module's central modulator can be:
- **Independent** from the oscillators (own pitch, ratio, waveform, morph)
- **Patched externally** via the `FM` input jack
- **Mixed with self-feedback** through the `FERAL` macro

A set of four macros (**PSY**, **DARK**, **MOTION**, **FERAL**) cross-modulate each other and the FM engine, making the module playable with just four knobs.

<!-- SCREENSHOT: Full panel front view -->
![Panel Overview](docs/images/panel-overview.png)

---

## Features

- **4 phase-locked oscillators** with shared pitch bus
- **FM Source Morph** – 6 FM-optimized waveforms for the Sine oscillator (and modulator when its base is Sine)
- **Independent FM modulator** with ratio, pitch, waveform, and morph controls
- **Three FM modes** – Exponential, Linear, Phase
- **Three FM routings** – Global, Self, Chain (with 8 chain patterns)
- **Sync modes** – Off, Soft, Hard, Reverse
- **4 performance macros** – PSY, DARK, MOTION, FERAL + MASTER
- **Turbulence generator** – Lorenz chaos, Envelope follower, or Rhythmic gate
- **Pitch bus** – Bipolar, Quantize, Glide, or Both
- **Post-processing** – Harmonic sum, wavefolder, sample-rate reduction, bit-crush, one-pole dark filter, Feral feedback loop, DARK sub-oscillator
- **Two interactive displays** – Round wave scope + radial FM engine editor

---

## Signal Flow

```text
        ┌──────────────┐    ┌──────────────┐
        │ Internal Mod │───▶│   fmCv       │
        │ (ratio,morph)│    │ (external or │
        └──────────────┘    │  internal)   │
                            └──────┬───────┘
                                   │
Pitch CVs ─┐                       ▼
Pitch knobs├──▶ Pitch Bus ──▶ ┌──────────┐
Pitch Bus ─┘                  │FM Routing│
                              │Mode/Sync │
                              └────┬─────┘
                                   │
                     ┌─────────────┼─────────────┐
                     ▼             ▼             ▼
                 ┌───────┐    ┌───────┐     ┌───────┐
                 │ Sine  │    │  Tri  │ ... │  Saw  │
                 │(Morph)│    │       │     │       │
                 └───┬───┘    └───┬───┘     └───┬───┘
                     │            │             │
                     └───────┬────┴─────────────┘
                             ▼
                   ┌───────────────────┐
                   │ Post-Processor:   │
                   │  harmonics → fold │
                   │  → SRR → crush →  │
                   │  dark LPF → feral │
                   │  feedback → sub   │
                   └─────────┬─────────┘
                             ▼
                       FINAL / SUM
```

<!-- SCREENSHOT: Signal flow diagram (optional) -->
![Signal Flow](docs/images/signal-flow.png)

---

## Panel Overview

The panel is divided into six functional zones:

1. **Left column** – Per-oscillator phase mod + macro CV inputs
2. **Left-center** – Pitch knobs and pitch-bus depths
3. **Center** – Two displays (Wave scope + FM engine) and macro knobs below
4. **Right-center** – FM index, mode, routing, and modulator controls
5. **Right** – Waveshapers, CV modes, and post-processor knobs
6. **Bottom** – Output jacks

<!-- SCREENSHOT: Panel layout with numbered zones -->
![Panel Layout](docs/images/panel-layout.png)

---

## Sections

### 1. Oscillator Section

Four phase-locked oscillators, one per waveform color:

| Oscillator | Color | Shape / Morph Control | Additional |
|---|---|---|---|
| **Sine** | Cyan | `FM SRC MORPH` (knob) | Uses FM Source Morph chain |
| **Triangle** | Green | `TRI SHAPE` + `TRI CV AMT` | `TRI PSY` psychedelic amount |
| **Saw** | Orange | `SAW SHAPE` + `SAW CV AMT` | – |
| **Square** | Pink | `SQR SHAPE` + `SQR CV AMT` | – |

Each oscillator has:
- **Pitch knob** (±4 octaves)
- **Pitch CV input** (per-osc)
- **Pitch Bus Depth knob** – how much the shared shape/pitch CV affects this oscillator
- **Phase Mod knob + CV input** (per-osc)
- **Volume knob** (bottom row)
- **Dedicated output jack** (bottom row)

<!-- SCREENSHOT: Oscillator section close-up -->
![Oscillator Section](docs/images/oscillator-section.png)

#### Sine Oscillator – FM Source Morph

Unlike the other three, the **Sine** oscillator's shape is controlled by the `FM SRC MORPH` knob. This is not a traditional waveshaper — it's a *morph chain* between six distinct FM-optimized waveforms. See the [FM Source Morph](#fm-source-morph-deep-dive) section for details.

#### Triangle, Saw, Square – Waveshaper Amounts

The three waveshaping oscillators expose a classic `SHAPE` knob plus an attenuverting `CV AMOUNT` knob. Each has its own shape CV input (`SAW SHAPE CV`, `TRI SHAPE CV`, `SQR SHAPE CV`), plus a shared `SHAPE/PITCH BUS` CV input.

- **`SHAPE`** – bipolar (-0.5..+1.0) waveshaping depth
- **`CV AMOUNT`** – scales the corresponding shape CV input
- **`TRI PSY`** – additional triangle-only psychedelic shaping

---

### 2. Pitch Section

#### Per-Oscillator Pitch
Each oscillator has:
- **Pitch Knob** – ±4 octaves
- **Pitch CV Jack** – 1V/oct
- **Bus Depth Knob** – how strongly the shared pitch bus drives this oscillator

#### Global Pitch All
The `PITCH ALL` jack sends 1V/oct to all four oscillators simultaneously.

#### Pitch Bus
The shared `SHAPE/PITCH BUS` CV input (`PSYCHEDELIC_CV_INPUT_FOR_All`) feeds:

- **Shape CV** – shapes all four oscillators (see [CV Modes](#cv-modes))
- **Pitch Bus** – can also drive pitch via `PITCH_BUS_MODE`

| Pitch Bus Mode | Behavior |
|---|---|
| **Off** | Bus ignored for pitch |
| **Bipolar** | ±5V → ±4 octaves |
| **Quantize** | Semitone quantization |
| **Glide** | Portamento with motion-dependent time constant |
| **Both** | Quantize + Glide |

#### Pitch Step Mode
Toggle `PITCH MODE` to snap pitch knobs to integer octaves (`OCT`) or leave them continuous (`FREQ`). The yellow LED above the button indicates the active mode.

<!-- SCREENSHOT: Pitch section -->
![Pitch Section](docs/images/pitch-section.png)

---

### 3. FM Section

The FM engine routes modulation from the modulator (internal or external) to the four oscillators.

#### FM Index
The `FM IDX` knob sets the FM depth. It is summed with:
- `FM AMT` CV input × `FM AMT` knob (attenuverted)
- `PSY` macro (0.30 × psy)
- `MOTION` macro × turbulence (0.20)
- `FERAL` macro (0.10)
- Envelope follower × `FM ENV` amount (6×)

The final index is soft-saturated twice (knee at 5, clamp at 6) to keep heavy FM stable.

#### FM Mode
| Mode | Description |
|---|---|
| **EXP** | Exponential FM – classic DX-style |
| **LIN** | Linear FM – keeps pitch more stable |
| **PHASE** | Phase modulation (through-zero) |

#### FM Routing
| Routing | Description |
|---|---|
| **GLOBAL** | Self-modulation from averaged oscillator feedback, scaled by Internal Amount |
| **SELF** | Each oscillator self-FM using its own feedback tap |
| **CHAIN** | Cross-oscillator FM following one of 8 chain patterns |

#### FM Sync
Phase-resets slave oscillators when the master sine wraps.

| Sync Mode | Behavior |
|---|---|
| **Off** | No sync |
| **Soft** | Half-strength sync |
| **Hard** | Full reset to 0.5 phase |
| **Reverse** | Phase inverted |

`SYNC AMT` sets the sync strength (0-100%). The sync strength is scaled up by motion (up to +30%).

#### Chain Patterns A–H
Eight pre-configured modulation routing patterns:

| Pattern | Order |
|---|---|
| A | Tri → Sine → Saw → Sqr |
| B | Sine → Saw → Sqr → Tri |
| C | Saw → Sqr → Tri → Sine |
| D | Sine → Tri → Sqr → Saw |
| E | Sine → Tri → Saw → Sqr |
| F | Saw → Sine → Sqr → Tri |
| G | Tri → Saw → Sine → Sqr |
| H | Sine → Sqr → Saw → Tri |

Edit the chain depth per oscillator via the interactive **FM engine display** (see [Displays](#7-displays)).

<!-- SCREENSHOT: FM section -->
![FM Section](docs/images/fm-section.png)

---

### 4. Modulator Section

The modulator is a dedicated oscillator that drives FM independently of the four audio oscillators.

#### Modulator Pitch
- `MOD PITCH` knob – ±2/4/6 octaves (selectable in context menu)
- `MOD CV` knob – how strongly the shared Shape/Pitch Bus CV affects modulator pitch
- Modulator inherits the Sine oscillator's pitch (further offset by `MOD PITCH`)

#### Modulator Ratio
`MOD RATIO` sets the frequency ratio of modulator to carrier. Context menu presets: 1:1, 2:1, 3:1, 4:1, 6:1, 8:1.

#### Modulator Base Waveform
`MOD WAVE` selects the modulator's starting waveform:

| Base | Behavior |
|---|---|
| **Sine** | **Follows `FM SRC MORPH`** — the modulator morphs exactly like the sine oscillator |
| **Triangle** | Blends toward Folded via `MOD WARMTH` |
| **Saw** | Blends toward Folded via `MOD WARMTH` |
| **Square** | Blends toward Folded via `MOD WARMTH` |

#### Modulator Warmth
When the base is **not** Sine, `MOD WARMTH` blends toward the Folded Sine waveform — adding harmonic richness without changing the base FM character. When the base **is** Sine, this knob has no effect (the modulator follows `FM SRC MORPH` instead).

#### FM Internal Scale / Legacy Static / Internal Amount
- `INT SCALE` – scales internal modulator output before it becomes fmCv
- `LEGACY ST` – when On, ignores internal modulator and uses a fixed +5V (legacy behavior)
- `INT AMT` – adds low-level self-feedback to the FM engine (works with GLOBAL routing)

#### Envelope Follower
`FM ENV` adds envelope-following of the FM CV (either the external CV, or the internal modulator) to the FM index at 6× the follower value. Fast attack (5 ms), useful for percussive FM.

#### Patched FM Input
When a cable is present in the `FM` jack:
- Internal modulator is bypassed
- The external CV drives fmCv directly
- `INT SCALE` / `LEGACY ST` / `MOD WARMTH` are ignored

<!-- SCREENSHOT: Modulator section -->
![Modulator Section](docs/images/modulator-section.png)

---

### 5. Macro & Turbulence

#### The Four Macros
| Macro | Color | Primary Effect |
|---|---|---|
| **PSY** | Green | Shape CV + psychedelic modulation + FM index + morph |
| **DARK** | Blue | Post-processor lowpass + sub-oscillator + slow motion |
| **MOTION** | Yellow | Turbulence speed + FM index modulation + phase jitter |
| **FERAL** | Red | FM self-feedback + drive + feedback loop |

Each macro accepts CV (±5V typical) via its dedicated jack at the left side. The CV is scaled by 0.1 and added to the knob.

#### MASTER Macro
`MASTER` crossfades all four macros upward. At 100%, all four macros reach at least 50% of their range.

#### Cross-Modulation
The macros interact with each other:
- **PSY** boosts FERAL by up to 40%
- **DARK** slows MOTION by up to 30%
- **FERAL** is amplified when **PSY** is high

#### Turbulence
`MOTION` macro drives the turbulence generator. Choose mode with the small toggle:

| Mode | Description |
|---|---|
| **CHAOS** | Lorenz attractor – organic, unpredictable |
| **ENV** | Envelope follower with attack/release shaped by MOTION |
| **RHYTHM** | Tempo-independent LFO gate |

Turbulence feeds into: FM index, waveshaper amount, phase offsets, and the hexagram LED brightness.

<!-- SCREENSHOT: Macro knobs and turbulence -->
![Macros](docs/images/macro-section.png)

---

### 6. Post-Processor

The post-processor is where the module earns its darkpsy/hitech character. It processes the oscillator sum (or an external audio input via `RESAMPLE IN`).

#### Signal Chain
```text
source (internal or external)
    ↓ DC block
    ↓ harmonics (HARMONIC knob + PSY macro)
    ↓ asymmetric drive (FERAL)
    ↓ wavefold (FOLD MIX knob + PSY + FERAL)
    ↓ sample-rate reduction (SRR knob)
    ↓ bit crush (BIT DEPTH knob)
    ↓ dark lowpass filter (DARK macro)
    ↓ feral feedback loop (FERAL macro)
    ↓ + DARK sub-oscillator (SUB LEVEL)
    ↓ OUT DRIVE + tanh + DC block
    ↓ FINAL OUT
```

| Control | Range | Effect |
|---|---|---|
| **FOLD MIX** | 0-80% | Wavefolder blend (`RESAMPLE` parameter) |
| **HARMONIC** | 0-100% | Harmonic enrichment (2nd-5th harmonics) |
| **PHASE DIST** | 0-100% | Phase distortion |
| **BIT DEPTH** | 1-16 bits | Bit crusher depth |
| **SRR** | 1-64× | Sample-rate reduction |
| **OUT DRIVE** | 0-100% | Final saturation |
| **SUB LEVEL** | 0-100% | DARK sub-oscillator (context menu) |

#### Feral Feedback Loop
When `FERAL` is active, a fraction of the processed signal is fed back into the FM engine (via `feralFeedback`), producing self-oscillating mayhem. The loop is safety-tanh'd and bounded to ±0.42.

#### DARK Sub-Oscillator
A half-frequency triangle driven by the same pitch as the sine oscillator. Its level is controlled by `SUB LEVEL` × DARK² — so it becomes prominent only when DARK is pushed.

#### External Audio Input
When a cable is plugged into `RESAMPLE IN`, the post-processor processes that signal instead of the internal oscillator sum. Useful for processing external drums, vocals, or other oscillators through the PsyOperator filter/FX chain.

<!-- SCREENSHOT: Post-processor section -->
![Post-Processor](docs/images/post-processor.png)

---

### 7. Displays

The module has two interactive displays.

#### PsyDisplay (top)
A round oscilloscope-style display showing:
- **Outer ring** – waveform of the four-oscillator sum
- **Inner quadrants** – individual oscillator waveforms (Sine, Square, Tri, Saw)
- **Center** – current parameter value when a knob is adjusted (auto-fades after 3.5s)
- **Status dots** – 8 small LEDs around the edge showing macro/mod state

<!-- SCREENSHOT: PsyDisplay close-up -->
![PsyDisplay](docs/images/psydisplay.png)

#### FM Engine Display (center)
A radial interactive display for editing FM parameters:
- **Four ring segments** – click to cycle FM mode, routing, sync, and chain pattern
- **Center** – shows the focused parameter or the sync strength ring
- **Chain node editor** – drag nodes to set chain depth percentage

<!-- SCREENSHOT: FM engine display with chain editor -->
![FM Engine Display](docs/images/fm-engine-display.png)

**Interactions:**
- **Click a segment** → cycle its value (MODE, ROUTE, SYNC, CHAIN)
- **Right-click a sync ring or chain node** → reset to default (1.0)
- **Drag a chain node vertically** → adjust depth (0-100%)
- **Drag inside the sync circle** → adjust sync strength (release to lock)

The display auto-focuses the segment that corresponds to the most recently adjusted parameter, so editing a knob also updates the display focus.

---

### 8. Outputs

| Jack | Output |
|---|---|
| **SINE** | Sine oscillator (post-morph) |
| **TRI** | Triangle oscillator |
| **SAW** | Saw oscillator |
| **SQUARE** | Square oscillator |
| **SUM** | Clean sum of all four oscillators (soft-tanh to ±5V) |
| **FINAL** | Fully processed output (post-processor chain, ±10V clamp) |

LEDs:
- **FINAL OUT LED** (red) – final output level
- **FM MODE LED** (green) – indicator dims by mode (EXP=dim, LIN=med, PHASE=bright)
- **FM ROUTING LED** (blue) – indicator dims by routing (GLOBAL=dim, SELF=med, CHAIN=bright)
- **CV MODE LEDs** (green/yellow/red) – indicate active CV mode
- **PITCH MODE LED** (yellow) – OCT when lit
- **FM AMT LED** – brightness proportional to patched FM CV × amount
- **HEXAGRAM LED** – driven by pitch-bus activity and turbulence; fades to shape CV when bus is off

---

## FM Source Morph (Deep Dive)

The FM Source Morph is PsyOperator's signature feature. Unlike a traditional waveshaper, it interpolates through **six FM-optimized waveforms** designed for distinct darkpsy/hitech characters.

| # | Name | Formula | Character |
|---|---|---|---|
| 0 | **SINE** | `sin(2πφ)` | Pure sine – classic DX FM |
| 1 | **FOLDED** | `tanh(2.4 · sin)` | Warm, rich lead FM |
| 2 | **CHEBYSHEV** | `0.72·sin + 0.25·sin3θ + 0.10·sin5θ` | Metallic hitech stab |
| 3 | **ASYMMETRIC** | `0.85·sin + 0.28·sin2θ + 0.10·sin4θ` | Dark, gritty, even harmonics |
| 4 | **SYNC** | `sin(2π · frac(2.5 · φ))` | Screaming hitech lead |
| 5 | **FORMANT** | `sinθ · (0.5 + 0.5·cos(8πφ))` | Vocal/bell darkpsy stab |

The knob interpolates linearly between adjacent shapes. The morph amount is also modulated by:
- **Shape CV** (via `SHAPE CV` knob and `SHAPE/PITCH BUS` input)
- **PSY macro** (0.55× with late-curve shaping)
- **Turbulence × Motion** (0.25×)

When the modulator's base waveform is **Sine**, it follows the *same* morph amount — so a single knob morphs both carrier and modulator together, keeping the FM character coherent.

<!-- SCREENSHOT: FM Source Morph chain diagram -->
![FM Source Morph Chain](docs/images/fm-source-morph.png)

<!-- SCREENSHOT: Waveform examples at each morph step -->
![FM Source Morph Waveforms](docs/images/fm-source-morph-waves.png)

### Why These Shapes?

These six waveforms were chosen because:
1. **None duplicate the module's native Tri/Saw/Square**
2. Each produces a **distinct FM sideband structure**
3. They cover the sonic territory of darkpsy/hitech without needing external modules

### Morph Position Guide

| Position | Shape | Sound |
|---|---|---|
| 0% | Sine | Clean sine, DX-style FM |
| 20% | Sine/Fold | Slight warmth, FM-friendly |
| 40% | Fold/Cheb | Metallic, aggressive |
| 60% | Cheb/Asym | Dark, gritty |
| 80% | Asym/Sync | Screaming, chaotic |
| 100% | Formant | Vocal/bell-like |

---

## Context Menu

Right-click the module to access:

| Menu Item | Options |
|---|---|
| **FM Source Morph Shape** | Quick jump: Sine / Folded / Chebyshev / Asymmetric / Sync / Formant |
| **Modulator Ratio** | 1:1, 2:1, 3:1, 4:1, 6:1, 8:1 |
| **Modulator Base Waveform** | Sine (follows Morph) / Triangle / Saw / Square |
| **Modulator Warmth** | 0% / 25% / 50% / 75% / 100% |
| **DARK Sub Level** | Off / Low / Med / High |
| **Master Macro** | Off / 25% / 50% / 75% / 100% |
| **Internal Scale** | 0% / 50% / 100% |
| **Legacy Static** | Off (internal mod) / On (legacy +5V) |
| **Internal Amount** | 0% / 25% / 50% / 75% / 100% |
| **Modulator Pitch Range** | ±2 / ±4 / ±6 octaves |
| **Modulator CV Depth** | 0% / 25% / 50% / 75% / 100% |

---

## CV Modes

The `SHAPE/PITCH BUS` input (`PSYCHEDELIC_CV_INPUT_FOR_All`) supports three CV processing modes, selected by the `CV MODE` toggle:

| Mode | Label | Behavior |
|---|---|---|
| **Bipolar** | BIP | Raw ±5V (passes through unchanged) |
| **Unipolar** | UNI | 0..+5V mapped to 0..+1 (for shape-only sources) |
| **Reverse** | REV | Inverted ±5V (inverts modulation direction) |

Each mode has a corresponding indicator LED (Green/Yellow/Red) above the toggle.

---

## Darkpsy / Hitech / Psytrance Tips

<!-- SCREENSHOT: Example patch screenshot -->
![Example Patch](docs/images/example-patch-darkpsy.png)

### Darkpsy Lead
```text
FM SRC MORPH : 60%  (Asymmetric territory)
FM IDX       : 1.8
FM MODE      : EXP
FM ROUTING   : SELF
PSY          : 35%
DARK         : 25%
FERAL        : 55%
```
Send a fast saw LFO into `FERAL CV IN` for rhythmic aggression.

### Hitech Screamer
```text
FM SRC MORPH : 78%  (Sync territory)
FM MODE      : PHASE
FM ROUTING   : CHAIN (Pattern D)
MOD RATIO    : 6:1
SYNC         : Hard, 90%
PHASE DIST   : 35%
SRR          : 4×
BIT DEPTH    : 10 bits
```

### Psytrance Bass
```text
FM SRC MORPH : 0%   (pure Sine)
FM IDX       : 0.6
FM MODE      : LIN
PITCH ALL    : 1V/oct from sequencer
DARK         : 45%
SUB LEVEL    : High
Waveshapers  : 0 on all
```

### Darkpsy Texture
```text
PSY          : 80%
MOTION       : 90%
TURBULENCE   : CHAOS
FM IDX       : 2.5
FM ROUTING   : GLOBAL
INT AMT      : 40%
FOLD MIX     : 55%
```

### Formant Stab
```text
FM SRC MORPH : 100%  (Formant)
MOD RATIO    : 2:1
MOD WAVE     : Sine (follows Morph)
FM ENV       : 60%
Attack       : Fast, via ADSR → FM AMT IN
```

### Rolling Bass (Glide Bus)
```text
FM SRC MORPH : 5%
PITCH BUS    : Glide
GLIDE VIA    : MOTION knob → SHAPE/PITCH BUS
DARK         : 30%
FERAL        : 20%
```

### Metallic Bell
```text
FM SRC MORPH : 42%  (Chebyshev)
FM IDX       : 2.2
FM MODE      : LINEAR
MOD RATIO    : 3:1
HARMONIC     : 40%
OUT DRIVE    : 35%
```

---

## CV Compatibility

All CV inputs accept **±5V** standard unless otherwise noted:

| Input | Voltage Range |
|---|---|
| Pitch inputs (1V/oct) | 0..+5V typical |
| Pitch All | 0..+5V |
| Shape/Pitch Bus CV | ±5V (mode-dependent) |
| Macro CV inputs | ±5V (scaled by 0.1) |
| FM CV | ±5V |
| FM AMT CV | ±5V × amount |
| Phase Mod CV | ±5V × depth |
| Shape CVs (per-osc) | ±5V × CV AMOUNT |

**Output voltage:**
- Individual outputs: ±5V
- SUM output: ±5V (tanh soft-limited)
- FINAL output: ±10V clamp (typically ±5V)

---

## Changelog

### v2.6.6 (current)
- **Phase 4**: FM Source Morph now applies to the modulator when its base waveform is Sine
- Replaced the previous Tri/Saw/Square/Pulse/Fold morph chain with six FM-optimized shapes:
  **Sine / Folded / Chebyshev / Asymmetric / Sync / Formant**
- Modulator Warmth is now only active for non-Sine modulator bases
- Added `MOD WARMTH` label to distinguish behavior
- Added ±10V output clamp to FINAL OUT

### v2.5.x
- Added `FM MOD MORPH` for modulator warmth
- Independent modulator pitch (±2/4/6 oct)

### v2.0
- Complete rewrite of FM engine
- Added interactive displays
- Added 4-macro system

### v1.0
- Initial release

---

## Credits & License

**Designed and developed by:** Hutara

**Dependencies:**
- [VCV Rack SDK 2.x](https://vcvrack.com/)
- [VCV Component Library](https://github.com/VCVRack/componentlibrary) (widgets)

**Fonts:**
- ShareTechMono-Regular (used in displays)

**License:** [MIT](LICENSE) — see `LICENSE` file

**Special thanks:** VCV Rack community, all beta testers, and everyone who contributed patches and feedback.

---

## Screenshot Checklist

To fully populate this README, add the following screenshots under `docs/images/`:

| Filename | Description |
|---|---|
| `panel-overview.png` | Full panel, well-lit |
| `signal-flow.png` | Signal flow diagram (Mermaid or hand-drawn) |
| `panel-layout.png` | Panel with numbered zones |
| `oscillator-section.png` | Close-up of the four oscillators |
| `pitch-section.png` | Pitch knobs and CV jacks |
| `fm-section.png` | FM index, mode, routing, sync |
| `modulator-section.png` | Modulator pitch, ratio, waveform, warmth |
| `macro-section.png` | Four macros and turbulence |
| `post-processor.png` | Harmonics, fold, SRR, drive |
| `psydisplay.png` | Top round display showing waveforms |
| `fm-engine-display.png` | Center radial display with chain editor |
| `fm-source-morph.png` | The six-shape chain diagram |
| `fm-source-morph-waves.png` | Six waveforms rendered |
| `example-patch-darkpsy.png` | Screenshot of a full darkpsy patch |

---

*Last updated: see Git history.*
