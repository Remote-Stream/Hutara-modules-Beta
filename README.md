# Hutara PsyOperator

A multi-operator FM synthesis module for VCV Rack 2, designed for **darkpsy / hitech / psytrance** sound design. PsyOperator combines four phase-locked oscillators, an independent morphing FM modulator, macro-driven chaos/turbulence, and a rich post-processing chain into a single performance-oriented module.

> Formerly part of the [Hutara-modules](https://github.com/hutara/Hutara-modules) collection.

---

## Table of Contents

1. [Overview](#overview)
2. [Features](#features)
3. [Installation](#installation)
4. [Signal Flow](#signal-flow)
5. [Panel Overview](#panel-overview)
6. [Sections](#sections)
   - [Oscillator Section](#1-oscillator-section)
   - [Pitch Section](#2-pitch-section)
   - [FM Section](#3-fm-section)
   - [Modulator Section](#4-modulator-section)
   - [Macro & Turbulence](#5-macro--turbulence)
   - [Post-Processor](#6-post-processor)
   - [Displays](#7-displays)
   - [Outputs](#8-outputs)
7. [FM Source Morph (Deep Dive)](#fm-source-morph-deep-dive)
8. [Context Menu](#context-menu)
9. [Darkpsy / Hitech / Psytrance Tips](#darkpsy--hitech--psytrance-tips)
10. [CV Compatibility](#cv-compatibility)
11. [Changelog](#changelog)
12. [Credits & License](#credits--license)

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

## Installation

```bash
cd ~/Rack2/plugins-<your-username>
git clone <this-repo-url> Hutara-PsyOperator
