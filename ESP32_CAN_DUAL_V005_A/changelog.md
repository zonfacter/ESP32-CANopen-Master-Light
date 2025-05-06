# Changelog für ESP32 CANopen Scanner und Konfigurator

## Version V005 mit Systemkonfigurationsprofilen (April 2025)

### Neue Funktionen
- **Systemprofile für kompatible Hardware-Konfigurationen**:
  - Neuer Befehl `mode x` implementiert zum Wechseln zwischen geprüften Hardwarekonfigurationen
  - Profil 1: OLED (I2C) + MCP2515 (SPI) - für klassische Anwendungen
  - Profil 2: TFT (SPI) + TJA1051 (TWAI) - für moderne ESP32-basierte Anwendungen
  - Verhindert Hardware-Konflikte durch inkompatible Kombinationen

### Verbesserungen
- **Robuste Absturzsicherung**:
  - Implementierung eines automatischen Erkennungssystems für wiederholte Abstürze
  - Fallback in einen sicheren Modus ohne Display nach 3 schnellen Neustarts
  - Kompabilitätsprüfung der gespeicherten Einstellungen beim Start

- **Verbesserte Fehlerbehandlung**:
  - Gründliche Validierung von Display- und CAN-Controllerwechseln
  - Kontrollierter Neustart bei Hardwarekonfigurationsänderungen
  - Sicherere Initialisierung und Bereinigung der Hardware-Interfaces

### Fehlerbehebungen
- Behoben: Abstürze bei inkompatiblen Kombinationen von Display und CAN-Controller
- Behoben: Speicherlecks bei mehrfacher Initialisierung von Hardwarekomponenten
- Behoben: Probleme mit Pin-Konflikten zwischen TFT-Display und MCP2515 CAN-Controller

### Dokumentation
- Aktualisierte Hilfebildschirme mit Erklärungen zu Systemkonfigurationsprofilen
- Deutlichere Warnungen bei Verwendung der direkten Komponentenwechsel-Befehle
- Erweiterte Informationsanzeige mit Details zum aktuell verwendeten Systemprofil

## Version V004 mit Verbesserungen bei der Display-Unterstützung

### Neue Funktionen
- **Umstellung auf Interface-basierte Display-Unterstützung**:
  - Abstrakte DisplayInterface-Klasse für einheitlichen Zugriff auf verschiedene Display-Typen
  - Unterstützung für OLED SSD1306 (I2C) und Waveshare ESP32-S3 Touch LCD (SPI)
  - Dynamischer Wechsel zwischen Display-Typen über den Befehl `transceiver display x`

### Verbesserungen
- **Erweiterte Transceiver-Unterstützung**:
  - Unterstützung für den TJA1051 CAN-Transceiver mit nativer ESP32-TWAI-Schnittstelle
  - Modulare Erweiterbarkeit für zusätzliche CAN-Transceiver
  - Konfiguration des CAN-Transceivers über den Befehl `transceiver can x`

### Fehlerbehebungen
- Behoben: Namenskonflikte in den Display-Klassen
- Behoben: Doppelte Definitionen von Funktionen und Objekten
- Behoben: Probleme bei der Fehlerbehandlung im Display-Code

## Version V003 mit erweiterten Diagnosefunktionen (März 2025)

### Neue Funktionen
- **Neuer Befehl `testnode`**: Ermöglicht intensive Tests einzelner Nodes
  - Format: `testnode nodeID [versuche] [timeout]` (z.B. `testnode 10 5 500`)
  - Testet verschiedene CANopen-Objekte (0x1000, 0x1001, 0x1018, etc.)
  - Wartet zuerst passiv auf Heartbeat/Emergency und versucht dann aktive Kommunikation
  - Zeigt detaillierte Diagnoseinformationen und decodierte Werte

### Verbesserungen
- **Zuverlässigere Baudratenerkennung**:
  - Deutlich verbesserte Scan-Strategie bei der automatischen Baudratenerkennung
  - Priorisierung bekannter Node-IDs für schnellere Erkennung
  - Optimierte Timeouts und mehrere Versuche pro Node
  - Mehr Diagnoseinformationen während des Erkennungsprozesses

- **Korrigierte Kommunikationseinstellungen**:
  - Eindeutige Trennung zwischen Baudrate- und Node-ID-Änderungen
  - Beseitigung von Fehlern bei der Interpretation von Kommandos
  - Verbesserte Fehlermeldungen und Statusanzeigen

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