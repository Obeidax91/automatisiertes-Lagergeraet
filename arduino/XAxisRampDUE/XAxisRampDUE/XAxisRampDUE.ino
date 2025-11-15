// === X-Achsensteuerung – Arduino DUE (Native USB, 3.3V) =====================
// Zweistufiges Homing (FAST→SLOW), Rampenfahrt ohne delay(1), STOP/HOME/PICK
// Obeida & ChatGPT Engineering • 2025-11-09
// ============================================================================

#include <Arduino.h>
#include <math.h>

// ---------------- PINs -------------------------------------------------------
const int PIN_DIR     = 2;
const int PIN_STEP    = 3;
const int PIN_ENDSTOP = 8;   // aktiv LOW

// ---------------- Mechanik / Kalibrierung -----------------------------------
const float STEPS_PER_MM = 35.122f;   // <-- DEIN kalibrierter Wert!

// ---------------- Bewegungs-Parameter ---------------------------------------
const float DEFAULT_MM_PER_S  = 200.0f;   // [mm/s]
const float DEFAULT_ACC_MM_S2 = 2000.0f;  // [mm/s²]
const float RAMP_FACTOR       = 4.0f;     // 1.0 sanft … 6.0 aggressiv

// ---------------- Homing-Parameter ------------------------------------------
const int   HOME_DIR_SIGN     = -1;      // -1 Richtung "minus" zum Schalter
const float HOME_PULLBACK_MM  = 2.0f;    // vom Schalter weg
const unsigned long HOME_TIMEOUT_MS = 0; // 0 = kein Timeout

// Zweistufiges Homing (dein Wunsch)
const float FAST_HOME_MM_S = 200.0f;   // schnelle Annäherung
const float SLOW_HOME_MM_S = 10.0f;   // präzise Phase
const float SLOW_ZONE_MM   = 40.0f;   // ab hier auf "slow" wechseln (4 cm)

// Industrietaugliche Pulsbreiten
const unsigned int PULSE_HIGH_US = 5;
const unsigned int PULSE_LOW_US  = 5;

// ---------------- Laufzeit ---------------------------------------------------
float mm_per_s  = DEFAULT_MM_PER_S;
float acc_mm_s2 = DEFAULT_ACC_MM_S2;

long  current_steps = 0;
long  target_steps  = 0;
float current_sps   = 0.0f;    // steps per second (Vorzeichen = Richtung)
bool  last_dir_high = false;

unsigned long lastStepMicros   = 0;
unsigned long lastUpdateMicros = 0;

String rxBuf;

// STOP / HOME Requests
volatile bool stop_requested = false;
volatile bool force_home_request = false;
bool require_home_after_next_action = false;

// ---------------- Helper -----------------------------------------------------
static inline float absf(float x){ return x >= 0 ? x : -x; }
static inline float sqrtf_safe(float x){ return (x <= 0) ? 0.0f : sqrtf(x); }
static inline long  mmToSteps(float mm){ return lroundf(mm * STEPS_PER_MM); }
static inline float stepsToMM(long st){  return (float)st / STEPS_PER_MM; }
static inline bool  endstopHit(){ return (digitalRead(PIN_ENDSTOP) == LOW); }

static inline void stepPulseOnce(bool dir_high) {
  if (dir_high != last_dir_high) {
    digitalWrite(PIN_DIR, dir_high ? HIGH : LOW);
    last_dir_high = dir_high;
  }
  digitalWrite(PIN_STEP, HIGH);
  delayMicroseconds(PULSE_HIGH_US);
  digitalWrite(PIN_STEP, LOW);
  delayMicroseconds(PULSE_LOW_US);
  current_steps += dir_high ? +1 : -1;
}

void stopMotionImmediate() {
  current_sps  = 0.0f;
  target_steps = current_steps;
  stop_requested = false;
  SerialUSB.println(F("STOPPED"));
}

// Parser für STOP/HOME auch innerhalb blockierender Loops
void pollSerialQuickForStopOrHome() {
  static String buf;
  while (SerialUSB.available() > 0) {
    char c = (char)SerialUSB.read();
    if (c == '\n' || c == '\r') {
      buf.trim();
      if (buf.length() > 0) {
        if (buf.equalsIgnoreCase("STOP_X")) {
          stop_requested = true;
          require_home_after_next_action = true;
          SerialUSB.println(F("STOP CMD"));
        } else if (buf.equalsIgnoreCase("HOME_X")) {
          force_home_request = true;
          stop_requested = true;
          SerialUSB.println(F("HOME CMD"));
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
void setup() {
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_STEP, OUTPUT);
  digitalWrite(PIN_DIR, LOW);
  digitalWrite(PIN_STEP, LOW);

  pinMode(PIN_ENDSTOP, INPUT_PULLUP); // aktiv LOW

  SerialUSB.begin(250000);
  while (!SerialUSB) { ; }

  // Bewegungs-Defaults
  mm_per_s  = DEFAULT_MM_PER_S;
  acc_mm_s2 = DEFAULT_ACC_MM_S2;

  SerialUSB.println(F("DUE X-Achse bereit (PICK_X, HOME_X, GOTO_X, STOP_X)."));
  SerialUSB.print(F("STEPS_PER_MM=")); SerialUSB.println(STEPS_PER_MM, 4);

  unsigned long now = micros();
  lastStepMicros   = now;
  lastUpdateMicros = now;
}

// ---------------- HOMING (zweistufig) ---------------------------------------
bool doHomingSequenceBlocking(bool two_stage = true) {
  stop_requested     = false;
  force_home_request = false;
  SerialUSB.println(F("HOMING: START"));

  // Richtung zum Schalter
  bool dir_to_switch_high = (HOME_DIR_SIGN > 0);
  digitalWrite(PIN_DIR, dir_to_switch_high ? HIGH : LOW);
  last_dir_high = dir_to_switch_high;

  unsigned long t_start = millis();

  // Hauptphase: auf den Schalter zu (FAST→SLOW nahe Home)
  while (!endstopHit()) {
    pollSerialQuickForStopOrHome();
    if (stop_requested) { stopMotionImmediate(); SerialUSB.println(F("HOMING: ABORTED BY STOP")); return false; }
    if (HOME_TIMEOUT_MS > 0 && (millis() - t_start) > HOME_TIMEOUT_MS) {
      SerialUSB.println(F("HOMING: TIMEOUT (kein Endschalter)")); return false;
    }

    // Geschwindigkeit wählen
    float mm_s = SLOW_HOME_MM_S;
    if (two_stage) {
      float mm_to_home = fabsf(stepsToMM(current_steps)); // 0 ist Home
      mm_s = (mm_to_home <= SLOW_ZONE_MM) ? SLOW_HOME_MM_S : FAST_HOME_MM_S;
    }
    float sps = mm_s * STEPS_PER_MM;
    if (sps < 1.0f) sps = 1.0f;
    unsigned long period_us = (unsigned long)(1000000.0f / sps);
    if (period_us < 1) period_us = 1;

    // Step
    stepPulseOnce(dir_to_switch_high);

    // Rest der Periode warten
    unsigned long used = PULSE_HIGH_US + PULSE_LOW_US;
    if (period_us > used) delayMicroseconds(period_us - used);
  }

  SerialUSB.println(F("HOMING: TRIGGERED"));

  // Null setzen
  current_steps = 0;
  target_steps  = 0;
  current_sps   = 0.0f;

  // Pullback (langsam) in Gegenrichtung
  bool dir_pullback_high = (HOME_DIR_SIGN < 0);
  digitalWrite(PIN_DIR, dir_pullback_high ? HIGH : LOW);
  last_dir_high = dir_pullback_high;

  long pullback_steps = mmToSteps(HOME_PULLBACK_MM);
  float pb_sps = SLOW_HOME_MM_S * STEPS_PER_MM;
  if (pb_sps < 1.0f) pb_sps = 1.0f;
  unsigned long pb_period_us = (unsigned long)(1000000.0f / pb_sps);
  if (pb_period_us < 1) pb_period_us = 1;

  for (long i = 0; i < pullback_steps; i++) {
    pollSerialQuickForStopOrHome();
    if (stop_requested) { stopMotionImmediate(); SerialUSB.println(F("HOMING: ABORTED BY STOP")); return false; }
    stepPulseOnce(dir_pullback_high);
    unsigned long used = PULSE_HIGH_US + PULSE_LOW_US;
    if (pb_period_us > used) delayMicroseconds(pb_period_us - used);
  }

  SerialUSB.println(F("HOMING: DONE"));
  require_home_after_next_action = false;
  return true;
}

// ---------------- Bewegung (blockierend, ohne delay(1)) ----------------------
bool driveToPositionBlocking(long goal_steps) {
  stop_requested = false;

  long diff = goal_steps - current_steps;
  bool dir_high = (diff > 0);
  if (dir_high != last_dir_high) {
    digitalWrite(PIN_DIR, dir_high ? HIGH : LOW);
    last_dir_high = dir_high;
  }

  SerialUSB.print(F("GOTO_X -> ")); SerialUSB.println(goal_steps);

  while (true) {
    pollSerialQuickForStopOrHome();
    if (stop_requested) {
      stopMotionImmediate();
      if (force_home_request) { doHomingSequenceBlocking(true); SerialUSB.println(F("REACHED")); force_home_request = false; }
      return false;
    }

    unsigned long now = micros();
    float dt = (now - lastUpdateMicros) / 1000000.0f;
    if (dt < 0.0f || dt > 0.1f) dt = 0.0f;
    lastUpdateMicros = now;

    long delta = goal_steps - current_steps;
    if (delta == 0) {
      if (current_sps != 0.0f) { current_sps = 0.0f; SerialUSB.println(F("REACHED")); }
      break;
    }

    bool dir_h = (delta > 0);
    long remaining = dir_h ? delta : -delta;

    if (dir_h != last_dir_high) {
      digitalWrite(PIN_DIR, dir_h ? HIGH : LOW);
      last_dir_high = dir_h;
    }

    const float target_sps = mm_per_s  * STEPS_PER_MM;
    const float accel_sps2 = acc_mm_s2 * STEPS_PER_MM;

    float v_allow = sqrtf_safe(RAMP_FACTOR * accel_sps2 * remaining);
    float v_cmd   = (target_sps < v_allow) ? target_sps : v_allow;

    float v_mag  = absf(current_sps);
    float dv_max = (dt > 0 ? accel_sps2 * dt : 0.0f);

    if (v_mag < v_cmd) v_mag = (v_mag + dv_max < v_cmd) ? (v_mag + dv_max) : v_cmd;
    else               v_mag = (v_mag - dv_max > v_cmd) ? (v_mag - dv_max) : v_cmd;

    if (v_mag < 1.0f) v_mag = 1.0f;
    current_sps = dir_h ? +v_mag : -v_mag;

    // period_us = 1e6 / |current_sps|
    float cps = absf(current_sps);
    if (cps < 1.0f) cps = 1.0f;
    unsigned long period_us = (unsigned long)(1000000.0f / cps);
    if (period_us < 1) period_us = 1;

    // Zeitgesteuert takten, ggf. nachholen
    while ((micros() - lastStepMicros) >= period_us) {
      stepPulseOnce(dir_h);
      lastStepMicros += period_us;
    }
  }
  return true;
}

// ---------------- PICK-Sequenz -----------------------------------------------
void runPickSequence(float mm_target) {
  SerialUSB.println(F("PICK: START"));

  long tgt_steps = mmToSteps(mm_target);

  if (require_home_after_next_action) {
    if (!driveToPositionBlocking(tgt_steps)) { SerialUSB.println(F("PICK: ABORT (MOVE)")); SerialUSB.println(F("REACHED")); return; }
    if (!doHomingSequenceBlocking(true))     { SerialUSB.println(F("PICK: WARN (HOME FAIL)")); SerialUSB.println(F("REACHED")); return; }
    SerialUSB.println(F("PICK: DONE")); SerialUSB.println(F("REACHED")); return;
  }

  if (!doHomingSequenceBlocking(true))       { SerialUSB.println(F("PICK: ABORT (HOME)")); SerialUSB.println(F("REACHED")); return; }
  if (!driveToPositionBlocking(tgt_steps))   { SerialUSB.println(F("PICK: ABORT (MOVE)")); SerialUSB.println(F("REACHED")); return; }
  if (!doHomingSequenceBlocking(true))       { SerialUSB.println(F("PICK: WARN (BACK HOME)")); SerialUSB.println(F("REACHED")); return; }

  SerialUSB.println(F("PICK: DONE"));
  SerialUSB.println(F("REACHED"));
}

// ---------------- loop() Parser ----------------------------------------------
void loop() {
  while (SerialUSB.available() > 0) {
    char c = (char)SerialUSB.read();
    if (c == '\n' || c == '\r') {
      rxBuf.trim();
      if (rxBuf.length() > 0) {
        String line = rxBuf; rxBuf = "";
        line.trim();

        if (line.equalsIgnoreCase("STOP_X")) {
          stop_requested = true;
          require_home_after_next_action = true;
          SerialUSB.println(F("STOP CMD"));

        } else if (line.equalsIgnoreCase("HOME_X")) {
          doHomingSequenceBlocking(true);   // zweistufiges Homing
          SerialUSB.println(F("REACHED"));

        } else if (line.startsWith("PICK_X")) {
          int idx = line.indexOf(' ');
          float mm = NAN;
          if (idx >= 0) { String tail = line.substring(idx + 1); tail.trim(); mm = tail.toFloat(); }
          if (!isnan(mm)) runPickSequence(mm);
          else { SerialUSB.println(F("PICK_X: Zahl fehlt")); SerialUSB.println(F("REACHED")); }

        } else if (line.startsWith("GOTO_X")) {
          int idx = line.indexOf(' ');
          float mm = NAN;
          if (idx >= 0) { String tail = line.substring(idx + 1); tail.trim(); mm = tail.toFloat(); }
          if (!isnan(mm)) { driveToPositionBlocking(mmToSteps(mm)); SerialUSB.println(F("REACHED")); }
          else { SerialUSB.println(F("GOTO_X: Zahl fehlt")); SerialUSB.println(F("REACHED")); }

        } else {
          SerialUSB.print(F("Unbekannter Befehl: ")); SerialUSB.println(line);
        }
      } else {
        rxBuf = "";
      }
    } else {
      rxBuf += c;
      if (rxBuf.length() > 160) rxBuf.remove(0, rxBuf.length() - 160);
    }
  }

  // (Kein Hintergrundfahren nötig; Fahrfunktionen sind blockierend)
}
