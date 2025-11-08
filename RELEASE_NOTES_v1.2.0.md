# Regal-Picker v1.2.0 – Release Notes  
📅 Datum: 2025-11-08  
🔧 Plattform: Arduino Due (3.3 V logic) + Python Tkinter GUI  

---

## 🚀 Highlights

- **Deutlich verbesserte Bewegungssteuerung:**  
  Die Step-Generierung wurde vollständig überarbeitet. Der alte 1 ms-Loop-Delay wurde entfernt, wodurch nun die eingestellten Werte für Geschwindigkeit (`DEFAULT_MM_PER_S`) und Beschleunigung (`DEFAULT_ACC_MM_S2`) tatsächlich wirksam sind.  
  → Bewegung jetzt **deutlich dynamischer und präziser**.

- **Neue GUI mit echter Autovervollständigung:**  
  Die Artikelsuch-Combobox ergänzt nun Einträge während der Eingabe (nicht nur per Klick auf den Dropdown-Pfeil).  
  → Komfortablere und schnellere Artikelsuche.

- **Verbesserte Kommunikation zwischen GUI & Firmware:**  
  - Echtzeit-Statusaktualisierung im Logfenster  
  - Homing-, Stop- und Pick-Befehle bleiben vollständig kompatibel  

---

## ⚙️ Änderungen gegenüber v1.1.x

### Arduino-Firmware
- Entfernt: `delay(1)` aus der Bewegungs-Loop (bisherige Limitierung auf ~1 kHz Step-Rate).  
- Neu: zeitgesteuerte Step-Generierung auf Basis von `micros()` → präzise Frequenzregelung.  
- Neu: Parameter  
  ```cpp
  const float RAMP_FACTOR = 4.0f;
