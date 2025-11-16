# Regal-Picker – Release Notes v1.4.0
**Datum:** 2025-11-16  
**Status:** Stable  
**Schwerpunkt:** Kompatibilität mit RADDS v1.6 + TMC2209 (X-Achse)

---

## 🔧 Überblick

Diese Version bringt die **offizielle Unterstützung des RADDS-Shields (v1.6) auf dem Arduino Due** für die X-Achse.  
Der Fokus liegt auf:

- Nutzung der Original-RADDS-Pins für X_DIR, X_STEP, X_EN und X_MIN
- sauberem Ein-/Ausschalten des Treibers über den Enable-Pin
- Beibehaltung des bisherigen Bewegungsverhaltens (Homing, PICK_X, GOTO_X, STOP_X)

Die GUI bleibt gegenüber v1.3.3 unverändert und ist voll kompatibel.

---

## ✅ Änderungen gegenüber v1.3.3

### 1. RADDS-Pinbelegung für X-Achse

Der Arduino-Code verwendet jetzt die RADDS-Standardpins:

- `PIN_DIR  = 23`  → **X_DIR**
- `PIN_STEP = 24`  → **X_STEP**
- `PIN_X_EN = 26`  → **X_EN (Enable, aktiv LOW)**
- `PIN_ENDSTOP = 30` → **X_MIN** (Endschalter, aktiv LOW, mit Pull-Up)

Damit kann ein TMC2209-Treiber im X-Slot des RADDS direkt genutzt werden.

---

### 2. Treiber-Enable / Motorabschaltung

Neu ist die explizite Steuerung des Enable-Pins:

- `motorEnable()` setzt `X_EN` auf **LOW** → Treiber EIN, Motor mit Haltemoment  
- `motorDisable()` setzt `X_EN` auf **HIGH** → Treiber AUS, Motor stromlos

Der Pin wird an folgenden Stellen verwendet:

- **Vor Bewegungen:**  
  - `HOME_X` (Homing)  
  - `PICK_X …` (komplette Pick-Sequenz)  
  - `GOTO_X …`
- **Nach Bewegungen / Sequenzen:**  
  - nach erfolgreichem Homing  
  - nach kompletter PICK-Sequenz (Home → Ziel → zurück Home)  
  - nach `GOTO_X`  
  - nach `STOP_X` (sofortige Abschaltung)

Vorteile:

- geringere Wärmeentwicklung des Motors im Stillstand  
- realistischeres Verhalten wie in industriellen Achssteuerungen

---

### 3. Bewegungslogik (funktional unverändert)

Die bewährten Funktionen aus v1.3.3 bleiben erhalten:

- **Zweistufiges Homing mit Feintuning**
  - schnelle Annäherung (FAST)  
  - langsame Phase nahe Home (SLOW)  
  - Backoff und erneutes, langsames Anfahren des Referenzpunktes  
  - definierter Pullback-Offset nach Home
- **Rampenprofil ohne `delay(1)`**
  - Beschleunigung / Verzögerung über `DEFAULT_MM_PER_S`, `DEFAULT_ACC_MM_S2`  
  - RAMP_FACTOR für Dynamik (1.0 = weich, 4.0 = aggressiver)
- **Kommandos**
  - `HOME_X`
  - `PICK_X <mm>`
  - `GOTO_X <mm>`
  - `STOP_X`

Die GUI nutzt weiterhin dasselbe Protokoll und zeigt Start/Ende der Bewegung mittels GIF-Animation.

---

## 🔌 Hardware-Hinweise für v1.4.0

- **Shield:** RADDS v1.6 auf Arduino Due (Native USB)
- **Treiber:** TMC2209 im X-Slot
- **Microstepping:** DIP-Schalter auf **1/16 Step** (empfohlen für dieses Release)
- **Endschalter:**  
  - X-Endschalter an **X_MIN** anschließen  
  - Endschalter-Elektronik als „aktiv LOW“ (Schalter nach GND), Board-Pull-Ups werden genutzt
- **Versorgung:**
  - RADDS mit 12 V versorgen (z.B. 12 V / 2 A für Tests ausreichend)  
  - Arduino Due zusätzlich via USB mit dem Laptop verbinden (GUI-Kommunikation);  
    die beiden Versorgungen sind dafür ausgelegt.

---

## 🚀 Update-Vorgehen

1. Alte Arduino-Sketch-Version (≤ v1.3.3) durch den **v1.4.0-RADDS-Sketch** ersetzen.  
2. Sketch auf den Arduino Due (Native USB Port) flashen.  
3. Verdrahtung prüfen:
   - Treiber im X-Slot  
   - Motor an X-Klemme  
   - Endschalter an X_MIN  
4. GUI starten, COM-Port wählen, `HOME_X` testen.  
5. Test mit einem Beispielartikel (z.B. „M10 Mutter“) und `PICK_X`.

---

## ⚠️ Bekannte Einschränkungen

- In v1.4.0 ist **nur die X-Achse** über RADDS angebunden.  
  Die Y-Achse und das 2D-Regal-Koordinatensystem werden in einem späteren Release integriert.
- `STEPS_PER_MM` ist für deine aktuelle Mechanik kalibriert, muss aber bei Änderungen an
  Microstepping oder Mechanik neu eingemessen werden.

---
