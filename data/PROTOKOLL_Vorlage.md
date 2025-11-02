# Abnahme-/Messprotokoll – Regal-Picker (X-Achse)

**Projekt/Anlage:** __________________________  
**Datum:** __.__.20__  
**Techniker:** _______________________________  
**Controller:** Arduino DUE (SerialUSB 250000)  
**Treiber:** TMC2209 (STEP/DIR)  
**Endschalter:** Pin 8, aktiv LOW

## 1. Sichtprüfung / Verdrahtung
- [ ] Versorgung (24 V) stabil
- [ ] EN aktiv / Treiber OK
- [ ] STEP=3 / DIR=2 / ENDSTOP=8 korrekt
- [ ] Endschalter gegen GND, mechanisch einwandfrei

## 2. Parameter
- STEPS_PER_MM: _____
- DEFAULT_MM_PER_S: _____
- DEFAULT_ACC_MM_S2: _____
- HOME_DIR_SIGN: _____  (−1/ +1)
- HOME_SPEED_MM_S: _____
- HOME_PULLBACK_MM: _____

## 3. Homing-Test
1) Befehl `HOME_X` gesendet  
   Erwartung: `HOMING: START` → `TRIGGERED` → `DONE` → `REACHED`  
   Ergebnis / Bemerkungen: ______________________________________

## 4. PICK-Test
1) Befehl `PICK_X ____` (mm)  
   Erwartung: HOME → GOTO_X → HOME → `REACHED`  
   Ergebnis / Bemerkungen: ______________________________________

## 5. Wiederholgenauigkeit (optional)
- 5× Homing, Abweichung Pullback-Endlage: ___________ mm

## 6. Logs (Auszug)

… füge hier relevante Zeilen aus dem GUI-Log ein …


## 7. Freigabe
- [ ] Anlage in Ordnung
- [ ] Nacharbeit nötig (siehe Bemerkungen)

**Unterschrift:** ______________________