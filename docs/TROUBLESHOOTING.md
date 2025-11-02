---

# 4) `/docs/TROUBLESHOOTING.md` – Fehlersuche & Checks

```markdown
# Troubleshooting – Schnellhilfe

## Keine Bewegung nach Verbinden
- Richtiger COM-Port gewählt?
- Baudrate 250000?
- TMC2209 EN aktiviert? Versorgung 24 V vorhanden?
- STEP/DIR Pins korrekt (DIR=2, STEP=3)?

## Homing findet Endschalter nicht
- Endschalter an **Pin 8**? Gegen **GND** verdrahtet?
- Log zeigt `HOMING: TIMEOUT` → Prüfe Richtung `HOME_DIR_SIGN` (ggf. `-1` ↔ `+1` drehen).
- Mechanischer Weg ausreichend? Schalterposition?

## Fährt, aber „REACHED“ kommt nicht
- `updateMotion()` wird in Loop regelmäßig aufgerufen?
- Zielposition sehr groß → warten (Poll in GUI läuft).

## Unsaubere Referenz
- `HOME_PULLBACK_MM` ggf. auf 3–5 mm erhöhen.
- Mechanisch entprellen / sauberer Endschalterhub.

## GUI meldet „Nicht verbunden“
- Port blockiert? Anderes Programm offen?
- Kabel: Native USB am DUE verwenden.

## Richtungsfehler (fährt vom Schalter weg)
- `HOME_DIR_SIGN` invertieren.
- DIR-Signal am Treiber prüfen / ggf. Motorwicklung tauschen.