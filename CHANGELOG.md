# Changelog

All notable user-visible changes to the Hutara plugin. Version numbers follow
`plugin.json`. Pre-2.6.0 development history is recorded in the git log (the
in-tree baseline/test-harness records were retired for this release).

## 2.6.0 — Fundamental upgrade (2026-11-07)

This release rebuilds both modules' sound engines. **Patches from the
published 2.0.0 load without error and reconnect correctly, but they no
longer sound identical** — see the breaking changes below.

### Breaking changes

**PsyOperator (FmOperator)**

1. **"RESAMPLE" knob (param 14) re-purposed as "Hard Fold Mix".** In 2.0.0
   this knob set the resampler depth (range 0–0.8, default **0.8**). It now
   blends a wavefolder into the output (0–0.8, default 0). Every 2.0.0 patch
   therefore loads a non-zero wavefold amount. Use the right-click
   **"Reset to 2.6 defaults"** action to put this knob and the FM Source
   Morph knob back at their 2.6 starting points.
2. **"Sine Waveshaper" knob (param 5) is now "FM Source Morph".** It was a
   −0.5…1.5 waveshaper bias; it is now a 0–1 morph across 14 FM source
   shapes. Old saved values outside 0–1 clamp to the edge of the new range.
3. **New FM architecture.** Pitch-CV FM (fixed 32.23 scale) is replaced by
   FM Index routing (Global / Self / Chain) with Exp / Linear / Phase modes
   and an expanded FM Index range (−1.2…3.23).
4. **Unpatched FM input no longer acts as a static +5 V carrier.** It now
   runs a built-in modulator ("FM Internal Scale", default 50 %). To restore
   the 2.0.0 behavior, enable **Legacy Static** in the right-click menu.
5. **Final output chain redesigned.** Hard-fold + harmonic sum + drive/tanh
   + DC blocking replaces the old `5 × (osc sum) × (resample sum) ×
   resample factor` formula. New outputs: SUM (id 5) and SUB (id 6).

**Hutara Random**

6. **GATE_OUTPUT is a new clocked engine.** The 2.0.0 per-trigger
   probabilistic random gate is replaced by a 16-step / 4-bar phrase
   sequencer with width fractions, swing and pattern modes. No mode
   restores the 2.0.0 random-gate character.
7. **GATE_OUTPUT_INVERTED redefined.** In 2.0.0 it rested at +10 V
   (`10 − randomGate`) and dropped to 0 V while a gate fired. It now rests
   at 0 V and emits a pulse at the end of each step (phase ≥ 1 − width).
8. **Enhanced mode.** New instances boot in Enhanced mode: the envelope
   display replaces the gate display and ENVELOPE_OUTPUT / EOC_OUTPUT are
   live. Legacy mode users can switch it off via the "Mode" switch.
9. **Fresh-instance parameter defaults changed.** Strength 5 → 1, Gate
   Width 0.5 → 0.7, and Random Gate Speed moved from the 1–3 speed switch
   to a 0–5 bar-subdivision scale (saved v1 values map 1→1, 2→2, 3→3).

### Fixed

- **2.0.0 patches boot correctly in Legacy mode.** Patches saved by 2.0.0
  have no `enhancedMode` key; previously the configSwitch default silently
  booted them into Enhanced mode, activating the envelope/EOC outputs and
  swapping the display. They now load in the mode they were created in
  (envelope/EOC outputs stay silent until you enable Enhanced yourself).

### Deferred

- **"Classic RNG gate" gate-mode variant** (restoring the 2.0.0
  probabilistic gate behavior through GATE_MODE) is postponed: it would
  modify the running gate engine and could not be re-verified against the
  retired baseline harness within this release cycle.

### Other

- Module name shortened to "PsyOperator"; Library search keeps working via
  the manifest `keywords` field ("psyoperator").
- Unused and backup SVGs removed from the distribution package
  (~2.1 MB smaller download).
- License restored to `GPL-3.0-or-later` (matching the published 2.0.0);
  `changelogUrl` now points to the GitHub releases page.
