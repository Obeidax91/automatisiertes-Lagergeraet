// === X-Achsensteuerung für TMC2209 / Arduino DUE (3.3V logic) ===
// stabile Version mit dt-Limiter, Speed-Clamp, STOP_X und PICK_X-Sequenz
// Autor: Obeida & ChatGPT Engineering Build 2025-11-02

#include <Arduino.h>
#include <math.h>

// --- Pins (UNO-kompatibel) ---
const int PIN_DIR  = 2;
const int PIN_STEP = 3;

// Endschalter (aktiv LOW)
const int PIN_ENDSTOP = 8;

// --- Mechanik ---
const float STEPS_PER_MM = 80.0f;

// --- Standardparameter ---
const float DEFAULT_MM_PER_S  = 20.0f;
const float DEFAULT_ACC_MM_S2 = 200.0f;

// Homing-Parameter
const int   HOME_DIR_SIGN    = -1;        // -1 = Richtung "minus", +1 = Richtung "plus"
const float HOME_SPEED_MM_S  = 10.0f;
const float HOME_PULLBACK_MM = 2.0f;
const unsigned long HOME_TIMEOUT_MS = 15000UL;

// --- Laufzeitvariablen ---
float mm_per_s  = DEFAULT_MM_PER_S;
float acc_mm_s2 = DEFAULT_ACC_MM_S2;

long  current_steps = 0;
long  target_steps  = 0;
float current_sps   = 0.0f;
bool  last_dir_high = false;

unsigned long lastStepMicros   = 0;
unsigned long lastUpdateMicros = 0;

String rxBuf;

// STOP-Logik
volatile bool stop_requested = false;

// =================== Helper ===================
static inline float absf(float x){ return x >= 0 ? x : -x; }
static inline float sqrtf_safe(float x){ return (x <= 0) ? 0.0f : sqrtf(x); }
static inline long  mmToSteps(float mm){ return lround(mm * STEPS_PER_MM); }
static inline float stepsToMM(long st){ return (float)st / STEPS_PER_MM; }
static inline bool  endstopHit(){ return (digitalRead(PIN_ENDSTOP) == LOW); }

void stopMotionImmediate() {
  current_sps  = 0.0f;
  target_steps = current_steps;
  stop_requested = false;
  SerialUSB.println(F("STOPPED"));
}

// *** NEU: schnelle Parser-Routine für STOP in blockierenden Schleifen ***
void pollSerialQuickForStop() {
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

// =================== setup ===================
void setup() {
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_STEP, OUTPUT);
  digitalWrite(PIN_DIR, LOW);
  digitalWrite(PIN_STEP, LOW);
  last_dir_high = false;

  pinMode(PIN_ENDSTOP, INPUT_PULLUP);   // aktiv LOW

  SerialUSB.begin(250000);
  while (!SerialUSB) { ; }

  SerialUSB.println(F("DUE X-Achse bereit (Homing + Fahrt + PICK_X + STOP_X)."));

  unsigned long now = micros();
  lastStepMicros   = now;
  lastUpdateMicros = now;
}

// =================== HOMING ===================
bool doHomingSequenceBlocking() {
  stop_requested = false;
  SerialUSB.println(F("HOMING: START"));

  float home_sps = HOME_SPEED_MM_S * STEPS_PER_MM;
  if (home_sps < 1.0f) home_sps = 1.0f;
  unsigned long step_period_us = (unsigned long)(1000000.0f / home_sps);
  if (step_period_us < 1) step_period_us = 1;

  bool dir_to_switch_high = (HOME_DIR_SIGN > 0);
  digitalWrite(PIN_DIR, dir_to_switch_high ? HIGH : LOW);
  last_dir_high = dir_to_switch_high;

  unsigned long t_start = millis();

  // PHASE 1: zum Schalter
  while (!endstopHit()) {
    pollSerialQuickForStop();                 // <<<< NEU: STOP in Homing-Loop erkennen
    if (stop_requested) {
      stopMotionImmediate();
      SerialUSB.println(F("HOMING: ABORTED BY STOP"));
      return false;
    }
    if ((millis() - t_start) > HOME_TIMEOUT_MS) {
      SerialUSB.println(F("HOMING: TIMEOUT (kein Endschalter)"));
      return false;
    }

    digitalWrite(PIN_STEP, HIGH);
    delayMicroseconds(2);
    digitalWrite(PIN_STEP, LOW);

    current_steps += dir_to_switch_high ? +1 : -1;
    delayMicroseconds(step_period_us);
  }

  SerialUSB.println(F("HOMING: TRIGGERED"));

  // Null setzen
  current_steps = 0;
  target_steps  = 0;
  current_sps   = 0.0f;

  // PHASE 2: Pullback
  {
    bool dir_pullback_high = (HOME_DIR_SIGN < 0);
    digitalWrite(PIN_DIR, dir_pullback_high ? HIGH : LOW);
    last_dir_high = dir_pullback_high;

    long pullback_steps = mmToSteps(HOME_PULLBACK_MM);
    for (long i = 0; i < pullback_steps; i++) {
      pollSerialQuickForStop();               // <<<< NEU
      if (stop_requested) {
        stopMotionImmediate();
        SerialUSB.println(F("HOMING: ABORTED BY STOP"));
        return false;
      }

      digitalWrite(PIN_STEP, HIGH);
      delayMicroseconds(2);
      digitalWrite(PIN_STEP, LOW);

      current_steps += dir_pullback_high ? +1 : -1;
      delayMicroseconds(step_period_us);
    }
  }

  target_steps = current_steps;
  current_sps  = 0.0f;

  SerialUSB.print(F("HOMING: DONE, X="));
  SerialUSB.print(stepsToMM(current_steps), 3);
  SerialUSB.println(F(" mm"));
  return true;
}

// =================== Fahrt (blockierend) ===================
bool driveToPositionBlocking(long goal_steps) {
  stop_requested = false;

  long diff = goal_steps - current_steps;
  bool dir_high = (diff > 0);
  digitalWrite(PIN_DIR, dir_high ? HIGH : LOW);
  last_dir_high = dir_high;

  if ((diff != 0) && ((current_sps > 0) != dir_high)) {
    current_sps = 0.0f;
  }

  target_steps = goal_steps;
  SerialUSB.print(F("GOTO_X -> ")); SerialUSB.println(goal_steps);

  while (true) {
    pollSerialQuickForStop();                 // <<<< NEU: STOP auch in Fahr-Loop
    if (stop_requested) {
      stopMotionImmediate();
      return false;
    }

    unsigned long now = micros();
    float dt = (now - lastUpdateMicros) / 1000000.0f;
    if (dt < 0.0f || dt > 0.1f) dt = 0.0f;
    lastUpdateMicros = now;

    long delta = target_steps - current_steps;

    if (delta == 0) {
      if (current_sps != 0.0f) {
        current_sps = 0.0f;
        SerialUSB.println(F("REACHED"));
      }
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

    float v_allow = sqrtf_safe(2.0f * accel_sps2 * remaining);
    float v_cmd   = (target_sps < v_allow) ? target_sps : v_allow;

    float v_mag  = absf(current_sps);
    float dv_max = (dt > 0 ? accel_sps2 * dt : 0);

    if (v_mag < v_cmd) v_mag = min(v_cmd, v_mag + dv_max);
    else               v_mag = max(v_cmd, v_mag - dv_max);

    if (v_mag < 1.0f) v_mag = 1.0f;
    current_sps = dir_h ? +v_mag : -v_mag;

    if (absf(current_sps) > target_sps * 2.0f) {
      current_sps = dir_h ? target_sps : -target_sps;
    }

    unsigned long period_us = (unsigned long)(1000000.0f / absf(current_sps));
    if (period_us < 1) period_us = 1;

    if (now - lastStepMicros >= period_us) {
      digitalWrite(PIN_STEP, HIGH);
      delayMicroseconds(2);
      digitalWrite(PIN_STEP, LOW);

      current_steps += dir_h ? +1 : -1;
      lastStepMicros = now;
    }

    delay(1);
  }

  return true;
}

// =================== PICK-SEQUENZ ===================
void runPickSequence(float mm_target) {
  SerialUSB.println(F("PICK: START"));

  if (stop_requested || !doHomingSequenceBlocking()) {
    SerialUSB.println(F("PICK: ABORT (HOME FAIL or STOP)"));
    SerialUSB.println(F("REACHED"));
    return;
  }

  if (stop_requested || !driveToPositionBlocking(mmToSteps(mm_target))) {
    SerialUSB.println(F("PICK: ABORT (MOVE FAIL or STOP)"));
    SerialUSB.println(F("REACHED"));
    return;
  }

  if (stop_requested || !doHomingSequenceBlocking()) {
    SerialUSB.println(F("PICK: WARN (zurück kein HOMING)"));
    SerialUSB.println(F("REACHED"));
    return;
  }

  SerialUSB.println(F("PICK: DONE"));
  SerialUSB.println(F("REACHED"));
}

// =================== loop ===================
void loop() {
  // Voll-Parser im Idle (außerhalb der blockierenden Sequenzen)
  while (SerialUSB.available() > 0) {
    char c = (char)SerialUSB.read();
    if (c == '\n' || c == '\r') {
      rxBuf.trim();
      if (rxBuf.length() > 0) {
        String line = rxBuf; rxBuf = "";
        line.trim();

        if (line.equalsIgnoreCase("STOP_X")) {
          stop_requested = true;
          SerialUSB.println(F("STOP CMD"));
        } else if (line.equalsIgnoreCase("HOME_X")) {
          if (doHomingSequenceBlocking()) {
            SerialUSB.println(F("REACHED"));
          } else {
            SerialUSB.println(F("REACHED"));
          }
        } else if (line.startsWith("PICK_X")) {
          int idx = line.indexOf(' ');
          float mm = NAN;
          if (idx >= 0) {
            String tail = line.substring(idx + 1); tail.trim();
            mm = tail.toFloat();
          }
          if (!isnan(mm)) {
            runPickSequence(mm);
          } else {
            SerialUSB.println(F("PICK_X: Zahl fehlt"));
            SerialUSB.println(F("REACHED"));
          }
        } else if (line.startsWith("GOTO_X")) {
          int idx = line.indexOf(' ');
          float mm = NAN;
          if (idx >= 0) {
            String tail = line.substring(idx + 1); tail.trim();
            mm = tail.toFloat();
          }
          if (!isnan(mm)) {
            driveToPositionBlocking(mmToSteps(mm));
            SerialUSB.println(F("REACHED"));
          } else {
            SerialUSB.println(F("GOTO_X: Zahl fehlt"));
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

  // Hintergrund-Rampenupdate (falls Ziel offen ist)
  if (stop_requested) {
    stopMotionImmediate();
    return;
  }

  unsigned long now = micros();
  float dt = (now - lastUpdateMicros) / 1000000.0f;
  if (dt < 0.0f || dt > 0.1f) dt = 0.0f;
  lastUpdateMicros = now;

  long delta = target_steps - current_steps;
  if (delta == 0) {
    if (current_sps != 0.0f) {
      current_sps = 0.0f;
      SerialUSB.println(F("REACHED"));
    }
    return;
  }

  bool dir_h = (delta > 0);
  long remaining = dir_h ? delta : -delta;

  if (dir_h != last_dir_high) {
    digitalWrite(PIN_DIR, dir_h ? HIGH : LOW);
    last_dir_high = dir_h;
  }

  const float target_sps = mm_per_s  * STEPS_PER_MM;
  const float accel_sps2 = acc_mm_s2 * STEPS_PER_MM;

  float v_allow = sqrtf_safe(2.0f * accel_sps2 * remaining);
  float v_cmd   = (target_sps < v_allow) ? target_sps : v_allow;

  float v_mag  = absf(current_sps);
  float dv_max = (dt > 0 ? accel_sps2 * dt : 0);

  if (v_mag < v_cmd) v_mag = min(v_cmd, v_mag + dv_max);
  else               v_mag = max(v_cmd, v_mag - dv_max);

  if (v_mag < 1.0f) v_mag = 1.0f;
  current_sps = dir_h ? +v_mag : -v_mag;

  if (absf(current_sps) > target_sps * 2.0f) {
    current_sps = dir_h ? target_sps : -target_sps;
  }

  unsigned long period_us = (unsigned long)(1000000.0f / absf(current_sps));
  if (period_us < 1) period_us = 1;

  if (now - lastStepMicros >= period_us) {
    digitalWrite(PIN_STEP, HIGH);
    delayMicroseconds(2);
    digitalWrite(PIN_STEP, LOW);

    current_steps += dir_h ? +1 : -1;
    lastStepMicros = now;
  }
}
