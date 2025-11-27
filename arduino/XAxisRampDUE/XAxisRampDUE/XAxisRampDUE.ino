// ============================================================================
// Regal-Picker – X+Y-Achsensteuerung für Arduino DUE + RADDS v1.6
// - TMC2209 / Pololu-kompatible Treiber
// - X und Y mit separaten Endschaltern (X_MIN, Y_MIN, aktiv LOW)
// - Befehle von der GUI:
//     HOME_X           → homt X und Y (Ursprung)
//     PICK_X X_mm Y_mm → holt Artikel bei (X,Y), fährt danach zurück nach Home
//     STOP_X           → Not-Stopp (beide Achsen)
// - Serielles Interface über Native USB (SerialUSB, 250000 Baud)
//
// Obeida & ChatGPT • v1.4.x (RADDS, 2-Achs-Version)
// ============================================================================

#include <Arduino.h>
#include <math.h>

// ---------------- PINs RADDS v1.6 -------------------------------------------
// X-Achse
const int PIN_X_DIR     = 23;
const int PIN_X_STEP    = 24;
const int PIN_X_EN      = 26;
const int PIN_X_ENDSTOP = 28;  // X_MIN, aktiv LOW

// Y-Achse
const int PIN_Y_DIR     = 16;
const int PIN_Y_STEP    = 17;
const int PIN_Y_EN      = 22;
const int PIN_Y_ENDSTOP = 30;  // Y_MIN, aktiv LOW

// EN-Signal: bei Pololu/TMC üblich: LOW = Treiber an, HIGH = Treiber aus
const uint8_t DRIVER_ENABLE_LEVEL  = LOW;
const uint8_t DRIVER_DISABLE_LEVEL = HIGH;

// ---------------- Mechanik / Kalibrierung -----------------------------------
// Bei gleicher Mechanik für X und Y kannst du beide identisch setzen.
// Sonst separat kalibrieren.
const float STEPS_PER_MM_X = 35.122f;   // kalibrierter Wert X
const float STEPS_PER_MM_Y = 35.122f;   // kalibrierter Wert Y

// ---------------- Bewegungs-Parameter ---------------------------------------
const float DEFAULT_MM_PER_S_X  = 200.0f;   // [mm/s]
const float DEFAULT_ACC_MM_S2_X = 2000.0f;  // [mm/s²]

const float DEFAULT_MM_PER_S_Y  = 200.0f;   // [mm/s]
const float DEFAULT_ACC_MM_S2_Y = 2000.0f;  // [mm/s²]

const float RAMP_FACTOR = 4.0f;             // 1.0 sanft … 6.0 aggressiv

// ---------------- Homing-Parameter ------------------------------------------
// Koordinatensystem wie mit dir besprochen:
//  Ursprung an der Ecke, X+ nach rechts, Y+ nach oben.
//  Endschalter liegen im negativen Bereich → HOME_DIR_SIGN = -1.
const int HOME_DIR_SIGN_X = -1;   // -1 = Richtung minus zum X-Schalter
const int HOME_DIR_SIGN_Y = -1;   // -1 = Richtung minus zum Y-Schalter

const float HOME_PULLBACK_MM_X  = 2.0f;  // Offset von Schalter weg
const float HOME_PULLBACK_MM_Y  = 2.0f;

const unsigned long HOME_TIMEOUT_MS = 0; // 0 = kein Timeout

const float FAST_HOME_MM_S = 200.0f;  // schnelle Annäherung
const float SLOW_HOME_MM_S = 10.0f;   // langsame Phase nah am Schalter
const float SLOW_ZONE_MM   = 40.0f;   // ab hier auf "slow" (Abstand zur Home-Position)

// Industrietaugliche Pulsbreiten
const unsigned int PULSE_HIGH_US = 5;
const unsigned int PULSE_LOW_US  = 5;

// ---------------- Laufzeitvariablen -----------------------------------------
// X
long  current_steps_x = 0;
float current_sps_x   = 0.0f;    // steps per second
bool  last_dir_high_x = false;

// Y
long  current_steps_y = 0;
float current_sps_y   = 0.0f;
bool  last_dir_high_y = false;

// Zeitbasis
unsigned long lastStepMicros_x   = 0;
unsigned long lastUpdateMicros_x = 0;
unsigned long lastStepMicros_y   = 0;
unsigned long lastUpdateMicros_y = 0;

// Parser-Buffer
String rxBuf;

// STOP-Flag global für beide Achsen
volatile bool stop_requested = false;

// ---------------- Helper-Funktionen -----------------------------------------
static inline float absf(float x)        { return x >= 0 ? x : -x; }
static inline float sqrtf_safe(float x)  { return (x <= 0) ? 0.0f : sqrtf(x); }

static inline long  mmToStepsX(float mm) { return lroundf(mm * STEPS_PER_MM_X); }
static inline long  mmToStepsY(float mm) { return lroundf(mm * STEPS_PER_MM_Y); }

static inline float stepsToMMX(long st)  { return (float)st / STEPS_PER_MM_X; }
static inline float stepsToMMY(long st)  { return (float)st / STEPS_PER_MM_Y; }

static inline bool endstopHitX()         { return (digitalRead(PIN_X_ENDSTOP) == LOW); }
static inline bool endstopHitY()         { return (digitalRead(PIN_Y_ENDSTOP) == LOW); }

void enableDrivers()
{
  digitalWrite(PIN_X_EN, DRIVER_ENABLE_LEVEL);
  digitalWrite(PIN_Y_EN, DRIVER_ENABLE_LEVEL);
}

void disableDrivers()
{
  digitalWrite(PIN_X_EN, DRIVER_DISABLE_LEVEL);
  digitalWrite(PIN_Y_EN, DRIVER_DISABLE_LEVEL);
}

// Step-Puls für X
static inline void stepPulseX(bool dir_high)
{
  if (dir_high != last_dir_high_x) {
    digitalWrite(PIN_X_DIR, dir_high ? HIGH : LOW);
    last_dir_high_x = dir_high;
  }
  digitalWrite(PIN_X_STEP, HIGH);
  delayMicroseconds(PULSE_HIGH_US);
  digitalWrite(PIN_X_STEP, LOW);
  delayMicroseconds(PULSE_LOW_US);
  current_steps_x += dir_high ? +1 : -1;
}

// Step-Puls für Y
static inline void stepPulseY(bool dir_high)
{
  if (dir_high != last_dir_high_y) {
    digitalWrite(PIN_Y_DIR, dir_high ? HIGH : LOW);
    last_dir_high_y = dir_high;
  }
  digitalWrite(PIN_Y_STEP, HIGH);
  delayMicroseconds(PULSE_HIGH_US);
  digitalWrite(PIN_Y_STEP, LOW);
  delayMicroseconds(PULSE_LOW_US);
  current_steps_y += dir_high ? +1 : -1;
}

void stopMotionImmediate()
{
  current_sps_x = 0.0f;
  current_sps_y = 0.0f;
  stop_requested = false;
  SerialUSB.println(F("STOPPED"));
}

// Parser für STOP während blockierender Loops
void pollSerialQuickForStop()
{
  static String buf;
  while (SerialUSB.available() > 0) {
    char c = (char)SerialUSB.read();
    if (c == '\n' || c == '\r') {
      buf.trim();
      if (buf.length() > 0) {
        if (buf.equalsIgnoreCase("STOP_X")) {
          stop_requested = true;
          SerialUSB.println(F("STOP CMD"));
        }
      }
      buf = "";
    } else {
      buf += c;
      if (buf.length() > 160) buf.remove(0, buf.length() - 160);
    }
  }
}

// ---------------- Setup ------------------------------------------------------
void setup()
{
  // X
  pinMode(PIN_X_DIR, OUTPUT);
  pinMode(PIN_X_STEP, OUTPUT);
  pinMode(PIN_X_EN, OUTPUT);
  digitalWrite(PIN_X_DIR, LOW);
  digitalWrite(PIN_X_STEP, LOW);
  digitalWrite(PIN_X_EN, DRIVER_DISABLE_LEVEL);

  // Y
  pinMode(PIN_Y_DIR, OUTPUT);
  pinMode(PIN_Y_STEP, OUTPUT);
  pinMode(PIN_Y_EN, OUTPUT);
  digitalWrite(PIN_Y_DIR, LOW);
  digitalWrite(PIN_Y_STEP, LOW);
  digitalWrite(PIN_Y_EN, DRIVER_DISABLE_LEVEL);

  // Endstops
  pinMode(PIN_X_ENDSTOP, INPUT_PULLUP);  // aktiv LOW
  pinMode(PIN_Y_ENDSTOP, INPUT_PULLUP);

  SerialUSB.begin(250000);
  while (!SerialUSB) { ; }

  SerialUSB.println(F("DUE X+Y-Achse bereit (PICK_X, HOME_X, STOP_X)."));
  SerialUSB.print(F("STEPS_PER_MM_X=")); SerialUSB.println(STEPS_PER_MM_X, 4);
  SerialUSB.print(F("STEPS_PER_MM_Y=")); SerialUSB.println(STEPS_PER_MM_Y, 4);

  unsigned long now = micros();
  lastStepMicros_x   = now;
  lastUpdateMicros_x = now;
  lastStepMicros_y   = now;
  lastUpdateMicros_y = now;
}

// ---------------- Homing X ---------------------------------------------------
bool homeAxisX()
{
  SerialUSB.println(F("HOMING X: START"));
  stop_requested = false;
  enableDrivers();

  bool dir_to_switch_high = (HOME_DIR_SIGN_X > 0);
  digitalWrite(PIN_X_DIR, dir_to_switch_high ? HIGH : LOW);
  last_dir_high_x = dir_to_switch_high;

  unsigned long t_start = millis();

  // Phase 1: FAST→SLOW zum Schalter
  while (!endstopHitX()) {
    pollSerialQuickForStop();
    if (stop_requested) {
      stopMotionImmediate();
      SerialUSB.println(F("HOMING X: ABORT (STOP)"));
      disableDrivers();
      return false;
    }
    if (HOME_TIMEOUT_MS > 0 && (millis() - t_start) > HOME_TIMEOUT_MS) {
      SerialUSB.println(F("HOMING X: TIMEOUT"));
      disableDrivers();
      return false;
    }

    float mm_to_home = fabsf(stepsToMMX(current_steps_x)); // 0 ist Home
    float mm_s = (mm_to_home <= SLOW_ZONE_MM) ? SLOW_HOME_MM_S : FAST_HOME_MM_S;
    float sps  = mm_s * STEPS_PER_MM_X;
    if (sps < 1.0f) sps = 1.0f;
    unsigned long period_us = (unsigned long)(1000000.0f / sps);
    if (period_us < 1) period_us = 1;

    stepPulseX(dir_to_switch_high);
    unsigned long used = PULSE_HIGH_US + PULSE_LOW_US;
    if (period_us > used) delayMicroseconds(period_us - used);
  }

  SerialUSB.println(F("HOMING X: TRIGGERED"));

  // Referenzpunkt = Schalterkante
  current_steps_x = 0;
  current_sps_x   = 0.0f;

  // Pullback
  bool dir_pullback_high = !dir_to_switch_high;
  digitalWrite(PIN_X_DIR, dir_pullback_high ? HIGH : LOW);
  last_dir_high_x = dir_pullback_high;

  long pullback_steps = mmToStepsX(HOME_PULLBACK_MM_X);
  float pb_sps = SLOW_HOME_MM_S * STEPS_PER_MM_X;
  if (pb_sps < 1.0f) pb_sps = 1.0f;
  unsigned long pb_period_us = (unsigned long)(1000000.0f / pb_sps);
  if (pb_period_us < 1) pb_period_us = 1;

  for (long i = 0; i < pullback_steps; i++) {
    pollSerialQuickForStop();
    if (stop_requested) {
      stopMotionImmediate();
      SerialUSB.println(F("HOMING X: ABORT (STOP PULLBACK)"));
      disableDrivers();
      return false;
    }
    stepPulseX(dir_pullback_high);
    unsigned long used = PULSE_HIGH_US + PULSE_LOW_US;
    if (pb_period_us > used) delayMicroseconds(pb_period_us - used);
  }

  SerialUSB.println(F("HOMING X: DONE"));
  return true;
}

// ---------------- Homing Y ---------------------------------------------------
bool homeAxisY()
{
  SerialUSB.println(F("HOMING Y: START"));
  stop_requested = false;
  enableDrivers();

  bool dir_to_switch_high = (HOME_DIR_SIGN_Y > 0);
  digitalWrite(PIN_Y_DIR, dir_to_switch_high ? HIGH : LOW);
  last_dir_high_y = dir_to_switch_high;

  unsigned long t_start = millis();

  while (!endstopHitY()) {
    pollSerialQuickForStop();
    if (stop_requested) {
      stopMotionImmediate();
      SerialUSB.println(F("HOMING Y: ABORT (STOP)"));
      disableDrivers();
      return false;
    }
    if (HOME_TIMEOUT_MS > 0 && (millis() - t_start) > HOME_TIMEOUT_MS) {
      SerialUSB.println(F("HOMING Y: TIMEOUT"));
      disableDrivers();
      return false;
    }

    float mm_to_home = fabsf(stepsToMMY(current_steps_y));
    float mm_s = (mm_to_home <= SLOW_ZONE_MM) ? SLOW_HOME_MM_S : FAST_HOME_MM_S;
    float sps  = mm_s * STEPS_PER_MM_Y;
    if (sps < 1.0f) sps = 1.0f;
    unsigned long period_us = (unsigned long)(1000000.0f / sps);
    if (period_us < 1) period_us = 1;

    stepPulseY(dir_to_switch_high);
    unsigned long used = PULSE_HIGH_US + PULSE_LOW_US;
    if (period_us > used) delayMicroseconds(period_us - used);
  }

  SerialUSB.println(F("HOMING Y: TRIGGERED"));

  current_steps_y = 0;
  current_sps_y   = 0.0f;

  bool dir_pullback_high = !dir_to_switch_high;
  digitalWrite(PIN_Y_DIR, dir_pullback_high ? HIGH : LOW);
  last_dir_high_y = dir_pullback_high;

  long pullback_steps = mmToStepsY(HOME_PULLBACK_MM_Y);
  float pb_sps = SLOW_HOME_MM_S * STEPS_PER_MM_Y;
  if (pb_sps < 1.0f) pb_sps = 1.0f;
  unsigned long pb_period_us = (unsigned long)(1000000.0f / pb_sps);
  if (pb_period_us < 1) pb_period_us = 1;

  for (long i = 0; i < pullback_steps; i++) {
    pollSerialQuickForStop();
    if (stop_requested) {
      stopMotionImmediate();
      SerialUSB.println(F("HOMING Y: ABORT (STOP PULLBACK)"));
      disableDrivers();
      return false;
    }
    stepPulseY(dir_pullback_high);
    unsigned long used = PULSE_HIGH_US + PULSE_LOW_US;
    if (pb_period_us > used) delayMicroseconds(pb_period_us - used);
  }

  SerialUSB.println(F("HOMING Y: DONE"));
  return true;
}

// ---------------- Gemeinsames Homing (HOME_X) -------------------------------
bool homeAllAxes()
{
  SerialUSB.println(F("HOMING: START"));
  bool okX = homeAxisX();
  bool okY = homeAxisY();
  disableDrivers();
  if (okX && okY) {
    SerialUSB.println(F("HOMING: DONE"));
    return true;
  } else {
    SerialUSB.println(F("HOMING: ABORT"));
    return false;
  }
}

// ---------------- Bewegung X (Rampenfahrt) ----------------------------------
bool driveXToSteps(long goal_steps)
{
  stop_requested = false;
  enableDrivers();

  long diff = goal_steps - current_steps_x;
  bool dir_high = (diff > 0);
  digitalWrite(PIN_X_DIR, dir_high ? HIGH : LOW);
  last_dir_high_x = dir_high;

  SerialUSB.print(F("GOTO_X -> ")); SerialUSB.println(goal_steps);

  while (true) {
    pollSerialQuickForStop();
    if (stop_requested) {
      stopMotionImmediate();
      disableDrivers();
      return false;
    }

    unsigned long now = micros();
    float dt = (now - lastUpdateMicros_x) / 1000000.0f;
    if (dt < 0.0f || dt > 0.1f) dt = 0.0f;
    lastUpdateMicros_x = now;

    long delta = goal_steps - current_steps_x;
    if (delta == 0) {
      current_sps_x = 0.0f;
      SerialUSB.println(F("REACHED"));
      break;
    }

    bool dir_h = (delta > 0);
    long remaining = dir_h ? delta : -delta;

    if (dir_h != last_dir_high_x) {
      digitalWrite(PIN_X_DIR, dir_h ? HIGH : LOW);
      last_dir_high_x = dir_h;
    }

    const float target_sps = DEFAULT_MM_PER_S_X  * STEPS_PER_MM_X;
    const float accel_sps2 = DEFAULT_ACC_MM_S2_X * STEPS_PER_MM_X;

    float v_allow = sqrtf_safe(RAMP_FACTOR * accel_sps2 * remaining);
    float v_cmd   = (target_sps < v_allow) ? target_sps : v_allow;

    float v_mag  = absf(current_sps_x);
    float dv_max = (dt > 0 ? accel_sps2 * dt : 0.0f);

    if (v_mag < v_cmd) v_mag = (v_mag + dv_max < v_cmd) ? (v_mag + dv_max) : v_cmd;
    else               v_mag = (v_mag - dv_max > v_cmd) ? (v_mag - dv_max) : v_cmd;

    if (v_mag < 1.0f) v_mag = 1.0f;
    current_sps_x = dir_h ? +v_mag : -v_mag;

    float cps = absf(current_sps_x);
    if (cps < 1.0f) cps = 1.0f;
    unsigned long period_us = (unsigned long)(1000000.0f / cps);
    if (period_us < 1) period_us = 1;

    while ((micros() - lastStepMicros_x) >= period_us) {
      stepPulseX(dir_h);
      lastStepMicros_x += period_us;
    }
  }

  disableDrivers();
  return true;
}

// ---------------- Bewegung Y (Rampenfahrt) ----------------------------------
bool driveYToSteps(long goal_steps)
{
  stop_requested = false;
  enableDrivers();

  long diff = goal_steps - current_steps_y;
  bool dir_high = (diff > 0);
  digitalWrite(PIN_Y_DIR, dir_high ? HIGH : LOW);
  last_dir_high_y = dir_high;

  SerialUSB.print(F("GOTO_Y -> ")); SerialUSB.println(goal_steps);

  while (true) {
    pollSerialQuickForStop();
    if (stop_requested) {
      stopMotionImmediate();
      disableDrivers();
      return false;
    }

    unsigned long now = micros();
    float dt = (now - lastUpdateMicros_y) / 1000000.0f;
    if (dt < 0.0f || dt > 0.1f) dt = 0.0f;
    lastUpdateMicros_y = now;

    long delta = goal_steps - current_steps_y;
    if (delta == 0) {
      current_sps_y = 0.0f;
      SerialUSB.println(F("REACHED"));
      break;
    }

    bool dir_h = (delta > 0);
    long remaining = dir_h ? delta : -delta;

    if (dir_h != last_dir_high_y) {
      digitalWrite(PIN_Y_DIR, dir_h ? HIGH : LOW);
      last_dir_high_y = dir_h;
    }

    const float target_sps = DEFAULT_MM_PER_S_Y  * STEPS_PER_MM_Y;
    const float accel_sps2 = DEFAULT_ACC_MM_S2_Y * STEPS_PER_MM_Y;

    float v_allow = sqrtf_safe(RAMP_FACTOR * accel_sps2 * remaining);
    float v_cmd   = (target_sps < v_allow) ? target_sps : v_allow;

    float v_mag  = absf(current_sps_y);
    float dv_max = (dt > 0 ? accel_sps2 * dt : 0.0f);

    if (v_mag < v_cmd) v_mag = (v_mag + dv_max < v_cmd) ? (v_mag + dv_max) : v_cmd;
    else               v_mag = (v_mag - dv_max > v_cmd) ? (v_mag - dv_max) : v_cmd;

    if (v_mag < 1.0f) v_mag = 1.0f;
    current_sps_y = dir_h ? +v_mag : -v_mag;

    float cps = absf(current_sps_y);
    if (cps < 1.0f) cps = 1.0f;
    unsigned long period_us = (unsigned long)(1000000.0f / cps);
    if (period_us < 1) period_us = 1;

    while ((micros() - lastStepMicros_y) >= period_us) {
      stepPulseY(dir_h);
      lastStepMicros_y += period_us;
    }
  }

  disableDrivers();
  return true;
}

// ---------------- PICK-Sequenz (PICK_X X_mm Y_mm) ----------------------------
void runPickSequence(float x_mm, float y_mm)
{
  SerialUSB.println(F("PICK: START"));

  long x_steps = mmToStepsX(x_mm);
  long y_steps = mmToStepsY(y_mm);

  // 1) Immer zuerst referenzieren (beide Achsen)
  if (!homeAllAxes()) {
    SerialUSB.println(F("PICK: ABORT (HOME)"));
    SerialUSB.println(F("REACHED"));
    return;
  }

  // 2) Zu Regalposition fahren: zuerst X, dann Y
  if (!driveXToSteps(x_steps)) {
    SerialUSB.println(F("PICK: ABORT (MOVE X)"));
    SerialUSB.println(F("REACHED"));
    return;
  }
  if (!driveYToSteps(y_steps)) {
    SerialUSB.println(F("PICK: ABORT (MOVE Y)"));
    SerialUSB.println(F("REACHED"));
    return;
  }

  // (hier könnte Greifer etc. kommen)

  // 3) Zurück nach Home: erst Y, dann X
  if (!driveYToSteps(0)) {
    SerialUSB.println(F("PICK: WARN (BACK Y)"));
    SerialUSB.println(F("REACHED"));
    return;
  }
  if (!driveXToSteps(0)) {
    SerialUSB.println(F("PICK: WARN (BACK X)"));
    SerialUSB.println(F("REACHED"));
    return;
  }

  SerialUSB.println(F("PICK: DONE"));
  SerialUSB.println(F("REACHED"));
}

// ---------------- loop(): Befehlsparser -------------------------------------
void loop()
{
  while (SerialUSB.available() > 0) {
    char c = (char)SerialUSB.read();
    if (c == '\n' || c == '\r') {
      rxBuf.trim();
      if (rxBuf.length() > 0) {
        String line = rxBuf;
        rxBuf = "";
        line.trim();

        if (line.equalsIgnoreCase("STOP_X")) {
          stop_requested = true;
          SerialUSB.println(F("STOP CMD"));

        } else if (line.equalsIgnoreCase("HOME_X")) {
          if (homeAllAxes()) {
            SerialUSB.println(F("REACHED"));
          } else {
            SerialUSB.println(F("HOMING: FAILED"));
            SerialUSB.println(F("REACHED"));
          }

        } else if (line.startsWith("PICK_X")) {
          int idx = line.indexOf(' ');
          float x_mm = NAN;
          float y_mm = 0.0f;
          if (idx >= 0) {
            String rest = line.substring(idx + 1);
            rest.trim();
            int idx2 = rest.indexOf(' ');
            if (idx2 >= 0) {
              String sx = rest.substring(0, idx2);
              String sy = rest.substring(idx2 + 1);
              sx.trim(); sy.trim();
              x_mm = sx.toFloat();
              y_mm = sy.toFloat();
            } else {
              // nur X angegeben → Y = 0
              x_mm = rest.toFloat();
              y_mm = 0.0f;
            }
          }
          if (!isnan(x_mm)) {
            runPickSequence(x_mm, y_mm);
          } else {
            SerialUSB.println(F("PICK_X: Zahl(en) fehlen"));
            SerialUSB.println(F("REACHED"));
          }

        } else {
          SerialUSB.print(F("Unbekannter Befehl: "));
          SerialUSB.println(line);
        }
      } else {
        rxBuf = "";
      }
    } else {
      rxBuf += c;
      if (rxBuf.length() > 160) rxBuf.remove(0, rxBuf.length() - 160);
    }
  }

  // keine zusätzlichen Hintergrund-Aktionen nötig
}
