# Regal-Picker – Release Notes v1.3.2
📅 Datum: 2025-11-15  
🔧 Plattform: Arduino DUE + Python-Tkinter GUI

---

## 🚀 Überblick

Dieses Release bringt eine weiter verbesserte Bewegungslogik auf der X-Achse, eine realitätsnahe mm-Kalibrierung sowie Feinschliff an GUI und Projektdokumentation. Ziel: Das Verhalten der Achse soll dem „echten“ Regal-Picker noch näher kommen und sich wie eine kleine industrielle Achse anfühlen.

---

## 🧠 Wichtige Änderungen

### 1. Arduino-Firmware (XAxisRampDUE.ino)

- **Kalibrierte Mechanik**
  - `STEPS_PER_MM` auf den gemessenen Wert angepasst (`35.122f`), sodass z. B. 180 mm im Code jetzt möglichst genau 180 mm auf der Achse entsprechen.
- **Vorbereitung/Optimierung Homing**
  - Homing-Routinen auf die neue Kalibrierung abgestimmt.
  - Logging beibehalten (`HOMING: START`, `HOMING: DONE`, `REACHED`, `STOPPED`), damit die GUI den Status exakt auswerten kann.
- **Allgemeine Aufräumarbeiten**
  - Kommentare und Struktur verbessert, um die Firmware besser wartbar zu machen.

### 2. GUI (app/main.py + Assets)

- **Bewegungsanzeige (GIF)**
  - Feinschliff an Start/Stop der Animation in Kombination mit der neuen Firmware.
  - GIF reagiert weiterhin auf `PICK_X`, `HOME_X`, `GOTO_X`, `STOP_X` und die Rückmeldungen `REACHED` / `STOPPED`.
- **Optische Anpassungen**
  - Überarbeitete Screenshots/Icons im `assets`-Ordner zur besseren Dokumentation und Darstellung des Systems.

### 3. Daten & Doku

- **mapping.json**
  - Regal- bzw. Positionsmapping an die neue, kalibrierte Mechanik angepasst.
- **README / TROUBLESHOOTING**
  - Hinweise zur Kalibrierung und zum Umgang mit den Releases ergänzt.
- **GitHub-Konfiguration**
  - `.github/copilot-instructions.md` hinzugefügt/aktualisiert, um das Projekt langfristig besser mit KI-Unterstützung warten zu können.

---

## 🔖 Version

- Tag: **v1.3.2**
- Basis: v1.3.1
- Typ: Patch/Minor (Feinschliff an Mechanik, GUI und Doku)

---

## ✅ Status

Stabiler Stand für weitere Experimente (z. B. zweistufiges Homing, zweite Achse, Umstieg auf industrielle Steuerung).
