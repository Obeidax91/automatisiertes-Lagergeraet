# Copilot Instructions for automatisiertes-Lagergeraet

## Project Overview
This repository controls an automated storage device (Regal-Picker) using a Python GUI (Tkinter) and an Arduino DUE. The system moves an X-axis to storage positions, executing homing and pick sequences via serial commands.

## Architecture & Major Components
- **Python GUI (`app/`)**: Tkinter-based interface for user interaction, serial communication, and status logging. Main entry points: `main.py`, `mainX.py`.
- **Arduino Firmware (`arduino/`)**: Handles motor control, homing, and pick logic. Communicates via SerialUSB at 250000 Baud.
- **Data Mapping (`data/mapping.json`)**: Maps item names to storage positions (mm).
- **Assets (`app/assets/`)**: GUI images, icons, and GIFs for user feedback.

## Developer Workflows
- **Run GUI**: `python app/main.py` (or `mainX.py`).
- **Dependencies**: Install from `app/requirements.txt` (`pyserial`, `Pillow`, `tkinter`). Tkinter is preinstalled on Windows; install manually on Linux if needed.
- **Arduino Upload**: Use Arduino IDE to flash `XAxisRampDUE.ino`.
- **Serial Protocol**: GUI sends commands (`PICK_X <mm>`, `HOME_X`, `GOTO_X <mm>`) and waits for status (`REACHED`).
- **Troubleshooting**: See `docs/TROUBLESHOOTING.md` for common issues and hardware checks.

## Project-Specific Patterns & Conventions
- **Serial Communication**: Always use 250000 Baud, terminator `\n`.
- **Status Logging**: GUI logs all sent/received serial messages. Status changes (e.g., `REACHED`) trigger GUI state updates.
- **Homing Sequence**: Always performed before and after pick operations for reliability.
- **Mapping Logic**: Item-to-position mapping is handled via `data/mapping.json` and GUI logic.
- **Hardware Parameters**: Key constants (e.g., `STEPS_PER_MM`, `HOME_DIR_SIGN`) are defined in Arduino code and referenced in documentation.

## Integration Points
- **Python ↔ Arduino**: SerialUSB, 250000 Baud, protocol as above.
- **Data ↔ GUI**: `mapping.json` loaded at runtime for position lookup.

## Examples
- To pick an item: GUI → `PICK_X <mm>` → Arduino → executes homing, moves to position, returns home, sends `REACHED`.
- To home manually: GUI → `HOME_X` → Arduino → homes, sends `REACHED`.

## Key Files
- `app/main.py`, `app/mainX.py`: GUI entry points
- `app/requirements.txt`: Python dependencies
- `arduino/XAxisRampDUE/XAxisRampDUE.ino`: Arduino firmware
- `data/mapping.json`: Item-to-position mapping
- `docs/TROUBLESHOOTING.md`: Troubleshooting guide

---
For unclear workflows or missing conventions, consult the relevant README files or ask for clarification.
