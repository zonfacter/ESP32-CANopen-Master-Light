# Changelog für ESP32 CANopen Scanner und Konfigurator

## Version V002 mit Verbesserungen (März 2025)

### Neue Funktionen
- **Neuer Befehl `localbaud`**: Ermöglicht die Änderung der ESP32 CAN-Baudrate ohne CANopen-Kommunikation
  - Format: `localbaud baudrate` (z.B. `localbaud 500`)
  - Ideal für die Anpassung an bestehende CAN-Netzwerke mit fester Baudrate

### Verbesserungen
- **Optimierte Baudratenerkennung**:
  - Verbesserte Erkennung bei höheren Baudraten (insbesondere 500 kbps)
  - Längere Timeouts und mehrere Versuche für zuverlässigere Erkennung
  - Reduzierte Debug-Ausgaben für übersichtlichere Logs
  - Zusätzliche Stabilisierungspausen zwischen Baudratenwechseln

- **Verbesserter Node-Scan**:
  - Priorisierung bekannter Node-IDs für schnellere Erkennung
  - Scanning mit verschiedenen SDO-Objekten (Fehlerregister, Gerätetyp, Identität)
  - Erkennung von Nodes über Heartbeat- und Emergency-Nachrichten
  - Optimierte Timing-Parameter für bessere Zuverlässigkeit

- **CAN-Bus Konfiguration**:
  - Spezielle Konfigurationsparameter für 500 kbps
  - Bessere Fehlerbehandlung bei CAN-Bus Initialisierung
  - Korrekte Erkennung und Handhabung von Standard- und Extended-Frame-IDs

### Fehlerbehebungen
- Behoben: Fehler bei der Node-ID-Änderung, der immer die aktuelle Node-ID statt der angegebenen verwendete
- Behoben: Probleme beim Baudratenwechsel für spezifische Node-IDs
- Behoben: Timing-Probleme bei höheren Baudraten (500 kbps und höher)
- Behoben: Unklare Befehlssyntax im Hilfemenü

### Dokumentation
- Verbesserte Hilfebeschreibungen für alle Befehle
- Klarere Fehlermeldungen mit spezifischeren Hinweisen
- Detailliertere Debug-Ausgaben während der Baudratenerkennung

## Version V001 (Ursprüngliche Version)

- CANopen Node-ID Scanner über SDO-Read an 0x1001:00
- Live-Monitor für alle empfangenen Frames (INT-basiert)
- Dekodierung von CANopen-Nachrichten (PDO, SDO, NMT, HB, EMCY)
- Ergebnisanzeige auf OLED-Display und über Serial
- Node-ID-Änderung über SDO
- Baudratenkonfiguration
- Persistente Einstellungen mittels Preferences API