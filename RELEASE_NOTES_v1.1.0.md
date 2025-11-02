## ✨ Neu
- Roter STOP-Button (rechts, prominent) mit sofortiger Reaktion in Fahrt & Homing
- HOME_X reagiert auch während Bewegung (sauberer Stopp → Homing)
- Unendliches Homing-Timeout per Default für lange Achse

## 🛠️ Änderungen
- Blockierende Loops parsen STOP_X/HOME_X ohne Latenz (Quick-Parser)
- Log-Meldungen: "STOP CMD", "STOPPED", "HOMING: …", "REACHED"

## 🧪 Bekannt
- mm ↔ steps Übersetzung wird im nächsten Patch korrigiert.

## 🔧 Upgrade
- Arduino-Sketch neu flashen
- GUI wie gewohnt starten (keine Config-Anpassung nötig)
