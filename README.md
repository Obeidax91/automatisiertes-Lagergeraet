# Regal-Picker (X-Achse, Arduino DUE + Python-GUI)

Dieses Projekt fährt eine X-Achse (TMC2209/STEP-DIR) zu Lagerpositionen („Regale“) und nutzt dabei eine Python-GUI (Tkinter) sowie einen Arduino DUE.  
Die GUI sendet **PICK_X <mm>**: Der Arduino führt **Homing → Zielposition → Homing zurück** aus. `HOME_X` referenziert manuell.

## Features
- Stabiler Rampenfahralgorithmus (dt-Limiter, Speed-Clamp)
- **PICK-Sequenz**: HOME → Ziel(mm) → HOME (für manuelle Entnahme in Home-Position)
- **HOME_X**: Referenzfahrt mit Endschalter und 2 mm Pullback
- Klare serielle Statusmeldungen: `HOMING: ...`, `PICK: ...`, `REACHED` (GUI setzt auf „Stillstand“)

## Projektstruktur
PICKER/
│
├── app/
│   ├── assets/
│   │   ├── gui_screenshot.png 
│   │   ├── logo.png                 # GUI-Logo (wird im Header angezeigt)
│   │   ├── gui_screenshot.png       # Screenshot der laufenden GUI
│   │   ├── gui_pick.gif             # Animation: Artikel wird abgeholt
│   │   ├── icon_home.png            # (optional) Home-Button-Symbol
│   │   ├── icon_connect.png         # (optional) Verbinden-Button
│   │   └── placeholder.txt          # Platzhalter, falls Ordner leer bleibt
│   ├── main0.py
│   ├── main1.py
│   ├── main2.py
│   ├── main3.py
│   ├── main4.py
│   ├── README_app.md
│   └── requirements.txt
│
├── arduino/
│   ├── XAxisRamp0
│   ├── XAxisRamp0DUE
│   ├── XAxisRamp1DUE
│   ├── XAxisRamp2DUE
│   ├── XAxisRamp3DUE
│   └── README_arduino.md
│
├── data/
│   ├── mapping.json
│   └── PROTOKOLL_Vorlage.md
│
├── Docs/
│   └── TROUBLESHOOTING.md
│
├── venv/
│
└── README.md




## Hardware
- **Controller:** Arduino DUE (3.3 V-Logik)
- **Treiber:** TMC2209 (STEP/DIR)
- **Endschalter:** an **Pin 8**, **aktiv LOW** (gegen GND), **INPUT_PULLUP** im Code
- **Standard-Parameter:** `STEPS_PER_MM = 80.0`, `DEFAULT_MM_PER_S = 20.0`, `DEFAULT_ACC_MM_S2 = 200.0`
- **Homing:** `HOME_DIR_SIGN = -1` (Richtung „minus“ bis Schalter), `HOME_SPEED_MM_S = 10.0`, `HOME_PULLBACK_MM = 2.0`

## Software
- **Python 3.11+**
- Abhängigkeiten (siehe `app/requirements.txt`): `pyserial`, `Pillow`
- **Serielle Verbindung:** 250000 Baud, Terminator `\n`

## Bedienung (GUI)
1. GUI starten (`python app/mainX.py`).
2. **Port wählen → Verbinden.**
3. **Artikel eingeben → „Hole Artikel“**  
   GUI sendet `PICK_X <mm>` → Arduino: **HOME → Ziel → HOME** → `REACHED`.
4. **HOME manuell:** Button „HOME_X“ (nur Referenzieren + 2 mm zurück).
5. Logfenster zeigt Arduino-Status.

## Serielles Protokoll (Befehle an Arduino)
- `HOME_X` → Referenzieren (mit Pullback), am Ende `REACHED`.
- `PICK_X <mm>` → HOME → Ziel(mm) → HOME → `REACHED`.
- `GOTO_X <mm>` → nur Fahrt (ohne automatisches Homing, primär intern genutzt).

## Sicherheit & Hinweise
- Endschalter muss sicher verdrahtet sein (gegen GND, sauber entprellt).
- Bei Riemen/Spindeln ggf. `STEPS_PER_MM`, `DEFAULT_*` anpassen.
- Bei Nichtbewegung Treiber-Enable/Versorgung prüfen.

## Weiterführend
- Details zur GUI: `app/README_app.md`
- Details zu Arduino: `arduino/README_arduino.md`
- Troubleshooting: `docs/TROUBLESHOOTING.md`
- Mess-/Abnahmeprotokoll: `data/PROTOKOLL_Vorlage.md`

---

## 🖼️ Vorschau

| GUI | Beschreibung |
|-----|---------------|
| ![GUI Übersicht](../assets/gui_screenshot.png) | Hauptfenster der Regal-Picker-Oberfläche mit Dark Theme |
| ![Artikel holen](../assets/gui_pick.gif) | Beispiel: Artikel "M8x20" wird automatisch angefahren |

> Screenshots oder GIFs kannst du in `/app/assets/` ablegen.
> Beispielnamen: `gui_screenshot.png`, `gui_pick.gif`

