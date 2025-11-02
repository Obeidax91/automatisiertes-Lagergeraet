# GUI (Tkinter) – Bedienoberfläche

## Zweck
- Artikel auswählen → GUI berechnet X-Position und sendet **PICK_X <mm>**.
- Statusanzeige und Log der seriellen Meldungen.
- Manuelles **HOME_X** verfügbar.

## Kompatibilität
Diese GUI ist **nur mit `XAxisRampDUE`** kompatibel (SerialUSB 250000 Baud, Befehle `PICK_X`, `HOME_X`, `GOTO_X`).

## Start
```bash
cd app
python mainX.py

Abhängigkeiten

requirements.txt

# Python-Abhängigkeiten für Regal-Picker GUI
pyserial>=3.5
Pillow>=10.0.0
tkinter; platform_system == "Windows"
    💡 Tkinter ist bei Windows/Python meist schon vorinstalliert,
    bei Linux ggf. mit sudo apt install python3-tk nachinstallieren.

Wichtige Klassen

    SerialReader(Thread): Puffert eingehende Zeilen bis \n / \r und legt sie in eine Queue.

    PickerGUI:

        Port-Handling: Scannen/Verbinden/Trennen.

        Buttons: „Hole Artikel“ → PICK_X <mm>, „HOME_X“ → HOME_X.

        Log-Fenster: Zeigt << empfangen / >> gesendet.

        Statuswechsel: Auf REACHED setzt die GUI auf „Stillstand“.

Befehlsfluss

    Hole Artikel

        Namenseingabe → Regalnummer → mm-Position (mapping ITEM_TO_REGAL + REGAL_X_MM)

        Sende PICK_X {mm}

        Warte auf REACHED

    HOME_X
    Sende HOME_X → warte auf REACHED.

Anpassungen

    Positionen: REGAL_X_MM

    Artikel → Regal: ITEM_TO_REGAL

    Design: Theme in _apply_dark_theme()

Typische Logs

>> PICK_X 180.0
<< HOMING: START
<< HOMING: TRIGGERED
<< HOMING: DONE, X=2.000 mm
<< GOTO_X -> 14400
<< REACHED
<< HOMING: START
<< HOMING: DONE, X=2.000 mm
<< PICK: DONE
<< REACHED