# Regal-Picker – Release Notes v1.3.3
📅 Datum: 2025-11-15  
🔧 Plattform: Arduino DUE + Python-Tkinter GUI  

---

## 🚀 Überblick

Dieses Release konzentriert sich auf ein **industrietaugliches, präzises Homing** der X-Achse.  
Die Referenzfahrt wurde in mehrere Phasen unterteilt (Schnellfahrt, Rückzug, Feintuning, Pullback),  
sodass die Home-Position jetzt wesentlich **reproduzierbarer und genauer** ist.

---

## 🧠 Wichtige Änderungen

### 1. Arduino-Firmware (XAxisRampDUE.ino)

- **Mehrstufiges Homing mit Feintuning**
  - Phase 1: Annäherung an den Endschalter mit zweistufiger Geschwindigkeit  
    (FAST_HOME_MM_S / SLOW_HOME_MM_S, abhängig von der Distanz).
  - Phase 2: Kurzer Rückzug, bis der Endschalter sicher frei ist (Entprellung & Mechanik-Entlastung).
  - Phase 3: **Langsame Wiederanfahrt** auf den Schalter – hier wird der **exakte Referenzpunkt** gesetzt.
  - Phase 4: Definierter Pullback (`HOME_PULLBACK_MM`), damit die Achse nicht „im Schalter“ steht.

- **Stabilere Home-Position**
  - `current_steps` wird erst nach der langsamen Präzisionsanfahrt auf den Schalter auf `0` gesetzt.
  - Dadurch ist die Referenzposition weniger abhängig von Schalterhysterese und Geschwindigkeit.

- **Kompatibilität zur GUI erhalten**
  - Log-Meldungen wie  
    `HOMING: START`, `HOMING: DONE`, `STOP CMD`, `STOPPED`, `REACHED`, `PICK: START/DONE`  
    bleiben bestehen, damit die GIF-Steuerung in der GUI weiter korrekt funktioniert.
  - Bewegungsbefehle (`PICK_X`, `HOME_X`, `GOTO_X`, `STOP_X`) bleiben unverändert.

### 2. Verhalten aus Anwendersicht

- Homing nach `HOME_X` oder `PICK_X` fühlt sich jetzt „weicher“ und gleichzeitig **präziser** an.
- Die X-Nullposition wird wiederholgenauer getroffen – wichtig für reale Regalpositionen (180 mm, 240 mm usw.).
- Kein Einfluss auf bestehende Artikel-Logik oder GUI-Bedienung.

---

## 🔖 Version

- Tag: **v1.3.3**
- Basis: v1.3.2  
- Typ: Patch/Feintuning (Verbesserung der Referenzfahrt)

---

## ✅ Status

Empfohlen für alle Tests, bei denen die Positioniergenauigkeit der X-Achse eine Rolle spielt  
(z. B. reale Regalfächer, Wiederholtests, Vorbereitung für industrielle Umsetzung).
