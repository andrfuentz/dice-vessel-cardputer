# Changelog

All notable changes to DICE\\VESSEL are documented here.

## [1.0.0] - 2026-07-20

### First stable release

- Fast guided roll building plus direct mixed-dice expressions and final modifiers.
- Complete classic set from D2 through percentile D100.
- Normal, Advantage, and Disadvantage modes for single-D20 checks.
- Click and optional motion rolling with cinematic collisions and procedural wooden-box audio.
- Named saved combinations and persistent roll history.
- English and Brazilian Portuguese interfaces with eight persistent accent colors.
- Stability protections for persistent storage, destructive actions, timers, and long sessions.

### Final release-candidate changes

- Added Pink, Red, and Magenta accents.
- Limited Critical and Failure presentation to single-D20 checks, including modifiers, Advantage, and Disadvantage.
- Refreshed public screenshots and documentation for the stable release.

## [0.9.0-rc2] - 2026-07-20

### Added

- Pink, Red, and Magenta persistent accent palettes.

### Changed

- Critical and Failure presentation now applies only to a single-D20 expression, with or without a final modifier.
- Advantage and Disadvantage continue to trigger the event only when the kept D20 is a natural 20 or natural 1.
- Pools, mixed expressions, and non-D20 dice no longer receive system-specific critical/failure treatment.

### Release candidate

- No RNG behavior changed.
- The black background, Amber default, and all rc1 persistence protections remain unchanged.

## [0.9.0-rc1] - 2026-07-20

### Added

- Double-confirmation protection for deleting combinations, clearing history, and restoring settings.
- Restore Defaults action in the System settings tab.
- Global toast overlays that work consistently on every screen.

### Improved

- Saved combinations are cached in RAM instead of reading NVS every animation frame.
- Rapid settings adjustments are consolidated into a single delayed NVS write.
- Roll-history writes are consolidated after the result settles.
- Stored volume, brightness, sensitivity, color, and roll-mode values are bounded defensively when loaded.
- Brightness now reports the minimum safe display level as `0/10` correctly.
- System settings layout was tightened to fit the new recovery action without reducing readability.
- Timer comparisons remain safe across the `millis()` wraparound boundary.

### Release candidate

- No new dice-rule systems are introduced in this build.
- This version is intended for regression, endurance, and cross-model hardware testing before 1.0.

## [0.7.0-beta] - 2026-07-20

### Added

- Named saved combinations with an 18-character on-device name editor.
- Rename action for existing saved combinations.
- Persistent ten-roll history with exact mode-aware rerolling.
- Normal, Advantage, and Disadvantage modes for single-D20 rolls in the guided builder.
- Kept-die highlighting and discarded-die dimming for Advantage and Disadvantage.
- A fourth built-in help page for campaign features.

### Improved

- Saved-combination list now shows both the custom name and compact formula.
- History shows the D20 mode and safely truncates long formulas.
- Critical and failure effects consider only the kept die in Advantage/Disadvantage rolls.

### Compatibility

- Existing numbered combinations migrate automatically and receive a temporary `ROLL N` name.
- Existing settings and the v0.6 dice presentation remain unchanged.

## [0.6.0-beta] - 2026-07-20

### Added

- A true two-die percentile presentation for D100 rolls, with separate tens and units bodies.
- High-contrast result badges that remain upright while each die keeps its final angle.
- Motion trails at higher velocity and extra internal facets for D8, D12, and D20.
- Individual movement profiles for D4, D6, D10, D12, and D20.

### Improved

- Redrawn procedural pixel-art dice with dark bodies, bright outlines, layered faces, and clearer numbers.
- Longer, multi-pulse click-to-roll animation with more varied movement.
- Die weight now influences wall-impact strength and the procedural wooden-box sound.
- Critical/failure banner is drawn once above the dice instead of once per result.

### Compatibility

- Expressions, settings, saved combinations, history behavior, controls, and RNG rules are unchanged.

## [0.5.0-beta] - 2026-07-20

### Added

- Five persistent accent colors: Amber, Terminal Green, Cyan, Violet, and Monochrome.
- A live accent preview in the Visual settings tab.
- `M` as the consistent Menu/Back control while preserving Escape and the legacy backtick shortcut.
- Session-history clearing with Delete.

### Improved

- Centralized interface palette so every screen retains the black DICE\\VESSEL identity.
- Clearer and more consistent command footers across the roller, builder, saved rolls, history, help, About, and charging screens.
- Renamed and reorganized the Display settings tab as Visual, with brightness, accent color, and charging mode together.
- Simplified built-in navigation instructions.

### Compatibility

- Existing settings and saved combinations remain compatible; missing accent settings default to Amber.

## [0.4.0-beta] - 2026-07-18

### Added

- DICE\\VESSEL name, **KEEP ROLLING.** slogan, and startup screen.
- Complete English and Brazilian Portuguese interface with persistent language selection.
- MIT license and GitHub-ready contribution and issue templates.
- Guided four-step roll builder with up to four dice groups and a final modifier.
- Eight persistent saved-combination slots.
- Session history with quick reroll.
- Tabbed settings, 0–10 bars, built-in instructions, About screen, and charging view.
- Critical and failure presentation with dedicated procedural sounds.
- Dedicated metallic D2 sound.

### Improved

- Higher-contrast black and amber presentation.
- Stronger click-to-roll animation and more responsive shake energy.
- Muted procedural wooden-box impacts.
- Readability of values and dice silhouettes.

### Compatibility

- Click-to-roll works on all Cardputer revisions.
- Shake-to-roll activates only when an IMU is available.
- The legacy `dicebox` NVS namespace remains intentionally in use so previous test settings and combinations survive the rename.
