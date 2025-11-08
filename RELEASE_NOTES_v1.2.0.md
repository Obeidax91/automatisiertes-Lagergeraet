## 🚀 Highlights
- Rewritten motion loop: kein delay(1) mehr → konfigurierte Speed/Acceleration wirken sichtbar.
- RAMP_FACTOR für „sanfter“/„aggressiver“es Beschleunigen.
- GUI: echte Autovervollständigung der Artikelsuche.

## ⚙️ Changes
- Zeitgesteuerte Step-Generierung via micros() (präziser Takt).
- Verbesserte Rampenformel: v_allow = sqrt(RAMP_FACTOR * a * s).
- Dunkles Layout, klarere Log-Ausgaben.

## 🐞 Fixes
- Geschwindigkeitsänderungen hatten zuvor kaum Effekt → behoben.
- GIF/Status Sync beim Homing/Pick stabilisiert.

Getestet: Arduino Due (Native USB), 250 kBaud, TMC2209.

