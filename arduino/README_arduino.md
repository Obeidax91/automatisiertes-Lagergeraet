---

# 3) `/arduino/README_arduino.md` – Arduino/Homing/Pinout

```markdown
# Arduino DUE – XAxisRamp2DUE

## Board/Elektrik
- **Arduino DUE** (3.3 V Logik)
- **TMC2209** STEP/DIR (Enable dauerhaft erlaubt oder extern gesteuert)
- **Endschalter:** **Pin 8**, **aktiv LOW**, `pinMode(INPUT_PULLUP)`

## Pins

DIR = 2
STEP = 3
ENDSTOP = 8 (aktiv LOW)


## Parameter (im Sketch)
```cpp
const float STEPS_PER_MM      = 80.0f;
const float DEFAULT_MM_PER_S  = 20.0f;
const float DEFAULT_ACC_MM_S2 = 200.0f;

// Homing
const int   HOME_DIR_SIGN     = -1;    // − Richtung zum Schalter
const float HOME_SPEED_MM_S   = 10.0f; // langsam
const float HOME_PULLBACK_MM  = 2.0f;  // vom Schalter weg
const unsigned long HOME_TIMEOUT_MS = 15000UL;

Befehle (SerialUSB, 250000 Baud)

    HOME_X – Referenzieren (mit Pullback). Ende: REACHED

    PICK_X <mm> – HOME → Ziel → HOME → REACHED

    GOTO_X <mm> – Nur Fahrt mit Rampenprofil (ohne automatisches Homing)

Funktionsblöcke

    updateMotion() – unverändertes Rampenprofil (Accel/Decel, Speed-Clamp, dt-Limiter)

    doHomingSequenceBlocking() – Blockierende Homing-Sequenz bis Endschalter, Pullback 2 mm

    runPickSequence(mm) – HOMING → GOTO_X → HOMING

    waitUntilMotionDoneBlocking() – Pollt updateMotion() bis Ziel erreicht

Verdrahtung Endschalter

    NC/NO: Im Code erwartet aktiv LOW → bei Betätigung LOW, ansonsten HIGH durch Pullup.

    Entprellung: Mechanisch oder RC (optional), Schrittfrequenz beim Homing ist ohnehin gering.

Hinweise zur Mechanik

    Riemenantrieb (GT2/20T): STEPS_PER_MM ~ 80, Geschw. 20–50 mm/s realistisch

    Spindel (TR8x8): kleinere Geschw./Beschleunigungen wählen

Debug

Serieller Log über Native USB Port (SerialUSB).
Typische Sequenz:

HOMING: START
HOMING: TRIGGERED
HOMING: DONE, X=2.000 mm
GOTO_X -> 14400
REACHED
HOMING: START
HOMING: DONE, X=2.000 mm
PICK: DONE
REACHED