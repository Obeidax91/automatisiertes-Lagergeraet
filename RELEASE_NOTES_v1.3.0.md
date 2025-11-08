# Regal-Picker v1.3.0 – Release Notes  
📅 Datum: 2025-11-09  
🔧 Plattform: Arduino DUE (3.3 V) + Python Tkinter GUI  

---

## 🎯 Ziel dieses Updates
Dieses Release konzentriert sich auf **verbesserte Bewegungssynchronisierung** zwischen der grafischen Oberfläche und der Firmware.  
Die Bewegung (GIF) zeigt jetzt **exakt den realen Motorzustand** – unabhängig davon, ob die Bewegung durch PICK, HOME oder STOP ausgelöst wird.

---

## 🚀 Neue Funktionen / Verbesserungen

### 🖥️ GUI (Python)
- **GIF-Synchronisierung überarbeitet:**  
  Das Bewegungs-GIF läuft jetzt **bei allen Bewegungen** (HOME, PICK, GOTO)  
  und stoppt **sofort**, sobald der Motor tatsächlich stillsteht.  
  → Erkennt alle Zustände: `REACHED`, `STOPPED`, `STOP CMD`, `HOMING: DONE`, `PICK: DONE`.

- **Konsistente Statusanzeige:**  
  - `"Bewegt sich..."` wird automatisch gesetzt, wenn Bewegung beginnt.  
  - `"Stillstand"` wird sofort angezeigt, wenn das System stoppt.

- **Verbesserte Befehlslogik:**  
  GIF-Start wird beim Senden von Bewegungsbefehlen (PICK_X, HOME_X, GOTO_X)  
  automatisch aktiviert, unabhängig von vorherigem Zustand.  

- **Zentrale Steuerung durch `_gif_on()` und `_gif_off()`**:  
  Bessere Wartbarkeit und klare Trennung zwischen Anzeige- und Bewegungslogik.

---

## ⚙️ Arduino-Firmware
*(keine Änderungen in dieser Version – weiterhin kompatibel mit v1.2.0)*  
- Unterstützt alle Kommandos unverändert (`PICK_X`, `HOME_X`, `STOP_X`, `GOTO_X`).

---

## 🐞 Fehlerbehebungen
- GIF blieb nach „Artikel geholt“ aktiv → **behoben**  
- GIF blieb nach `HOMING: DONE` aktiv → **behoben**  
- Stoppt nun zuverlässig bei allen Endzuständen.  

---

## 🧩 Hinweise
- Kompatibel mit Firmware v1.2.0  
- Empfohlene Baudrate: **250 000 Bd**  
- Testsystem: **Arduino DUE (Native USB Port)** mit **TMC2209** Treiber  

---

## 🔮 Nächste geplante Features (v1.4.0)
- Fortschrittsanzeige (z. B. Balken oder %-Status während Bewegung)  
- Anzeige der zuletzt gefahrenen Regalposition  
- Erweiterung um Y-Achse  

---

© 2025 Obeida A. – Regal-Picker System
