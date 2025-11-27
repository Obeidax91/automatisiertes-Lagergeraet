# Release Notes – Version 1.4.1  
Dual-Axis Prototype with Fully Functional Homing

---

## Summary

Version 1.4.1 extends the previous single-axis implementation with a complete
and functional Y-axis including homing, motion control and coordinated
dual-axis article retrieval. The system now supports two-dimensional shelf
coordinates and executes PICK operations across X and Y.

---

## New Features

### Dual-Axis Support
- Y-axis added (DIR/STEP/EN mapping for RADDS v1.6)
- Independent homing routine for Y
- Sequential homing: X → Y
- Article coordinates extended to (X, Y)

### Coordinated Article Retrieval
- New command: `PICK x_mm y_mm`
- Deterministic sequence:
  1. Move to X target  
  2. Move to Y target  
  3. Report `REACHED`

### RADDS Compatibility Layer
- Added EN pin handling for both drivers
- Full mapping for X_MIN and Y_MIN endstops

### GUI Enhancements
- Motion GIF also used for Y axis activity
- New protocol: `PICK X_MM Y_MM`
- Mapping updated to 2D coordinates
- Improved RX motion-state detection

---

## Improvements

- Refined STOP logic resets both axes
- Stable detection of movement start/end
- Simplified parsing for dual-value commands
- Homing robustness improved (backoff/ refine)

---

## Known Issues

- Motion is sequential, not simultaneous
- Still a prototype: no hardware watchdogs
- Missing diagonal interpolation (planned)

---

## Upgrade Notes

- Requires updated GUI and Arduino firmware
- Requires updated `mapping.json` with X and Y fields
- Set TMC2209 to 1/16 µstepping for predictable motion
- Requires RADDS power (12 V) + USB connection simultaneously

---

## Files Updated in 1.4.1

- `README.md`
- `README_app.md`
- `README_arduino.md`
- `app/main.py`
- `arduino/XAxisRampDUE.ino`
- `mapping.json`
- `gui_pick.gif` usage unchanged

