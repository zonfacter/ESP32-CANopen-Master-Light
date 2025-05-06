# Umfassende Dokumentation für CANopen Scanner mit 3-Tasten OLED-Menüsystem

## Übersicht

Diese Dokumentation beschreibt die Erweiterung eines bestehenden CANopen-Tools für ESP32 um ein 3-Tasten-OLED-Menüsystem. Das System ermöglicht die Navigation durch ein hierarchisches Menü mittels drei Tasten sowie parallele Steuerung über serielle Befehle. Ein besonderer Fokus liegt auf der Koordination verschiedener Eingabequellen, um Konflikte zu vermeiden.

Das Programm bietet Funktionen für Node-Scanning, Baudratenerkennung, LiveMonitoring und CANopen-Konfiguration. Die Implementierung stützt sich auf abstrakte Interfaces für Display und CAN-Controller, wodurch unterschiedliche Hardware-Komponenten unterstützt werden können.

## Menüstruktur

Die Menühierarchie ist wie folgt aufgebaut:

```
1. Hauptmenü
   ├── Scanner
   │   ├── Node-Scan
   │   │   ├── Start Scan
   │   │   ├── Bereich Start
   │   │   ├── Bereich Ende
   │   │   └── Zurück
   │   ├── Auto-Baudrate
   │   │   ├── Starten
   │   │   └── Zurück
   │   ├── Node-Test
   │   │   ├── Node-ID
   │   │   ├── Test starten
   │   │   └── Zurück
   │   └── Zurück
   ├── Monitor
   │   ├── Live Monitor
   │   ├── Filter
   │   │   ├── ID-Filter
   │   │   ├── Node-Filter
   │   │   ├── Typ-Filter
   │   │   ├── Filter zurücksetzen
   │   │   └── Zurück
   │   └── Zurück
   ├── Konfiguration
   │   ├── Node-ID ändern
   │   │   ├── Alte ID
   │   │   ├── Neue ID
   │   │   ├── Ausführen
   │   │   └── Zurück
   │   ├── Baudrate
   │   │   ├── Local Baudrate
   │   │   ├── Node Baudrate
   │   │   └── Zurück
   │   ├── System-Profil
   │   │   ├── OLED + MCP2515
   │   │   ├── TFT + TJA1051
   │   │   └── Zurück
   │   └── Zurück
   ├── System
   │   ├── Info anzeigen
   │   ├── Einst. speichern
   │   ├── Einst. laden
   │   ├── System zurücksetzen
   │   └── Zurück
   └── Hilfe
       ├── Menü-Hilfe
       ├── Version
       └── Zurück
```

## Steuerungskoordination

Ein zentrales Element der Implementierung ist die Koordination zwischen verschiedenen Steuerungsquellen:

1. **Steuerungsquellen-Management**: Ein Enum `ControlSource` definiert die möglichen Steuerungsquellen (NONE, SERIAL, BUTTON, AUTO).
2. **Timeout-Mechanismus**: Nach einer bestimmten Inaktivitätszeit (3 Sekunden) wird die Steuerungsquelle freigegeben.
3. **Prioritätsbasierte Verarbeitung**: Es gibt klare Regeln, welche Steuerungsquelle Vorrang hat, und wie eine untergeordnete Quelle die Kontrolle übernehmen kann.
4. **Visuelle Rückmeldung**: Der aktuelle Steuerungsmodus wird immer auf dem OLED-Display angezeigt.

## Benötigte Dateien

Für die vollständige Implementierung werden folgende Dateien benötigt:

### Abstrakte Interfaces und Basisklassen
1. **DisplayInterface.h**: Abstrakte Basisklasse für verschiedene Display-Typen
2. **DisplayInterface.cpp**: Implementierung der Factory-Methode für Display-Typen
3. **CANInterface.h**: Abstrakte Basisklasse für verschiedene CAN-Controller
4. **CANInterface.cpp**: Implementierung der Factory-Methode für CAN-Controller-Typen
5. **CANopen.h**: Definitionen für CANopen-Protokoll Konstanten und -Funktionen

### Display-Implementierungen
6. **OLEDDisplay.h**: Konkrete Implementierung für SSD1306 OLED-Display
7. **WaveshareDisplay.h**: Konkrete Implementierung für Waveshare ESP32-S3 Touch LCD

### CAN-Controller-Implementierungen
8. **MCP2515Interface.h**: Implementierung für MCP2515 CAN-Controller
9. **MCP2515Interface.cpp**: Implementierung der MCP2515-Funktionen
10. **ESP32CANInterface.h**: Implementierung für ESP32 integrierten CAN-Controller
11. **ESP32CANInterface.cpp**: Implementierung der ESP32-CAN-Funktionen
12. **TJA1051Interface.h**: Implementierung für TJA1051 CAN-Transceiver
13. **TJA1051Interface.cpp**: Implementierung der TJA1051-Funktionen

### CANopen-Funktionalität
14. **CANopenClass.h**: Header für die CANopen-Klasse mit SDO/PDO-Funktionen
15. **CANopenClass.cpp**: Implementierung der CANopen-Klasse

### Menüsystem
16. **OLEDMenu.h**: Header für das Menüsystem mit Definitionen für Menü-IDs, Strukturen und Funktionen
17. **OLEDMenu.cpp**: Implementierung des Menüsystems und Button-Handling

### Systemkonfiguration
18. **SystemProfiles.h**: Definition der Systemkonfigurationsprofile für Display- und CAN-Controller-Kombinationen
19. **User_Setup.h**: Konfigurationsdatei für TFT_eSPI-Bibliothek (für TFT-Displays)

### Hauptprogramm
20. **ESP32_CAN_DUAL_V004.ino**: Hauptprogramm mit `setup()` und `loop()` Funktionen und serieller Befehlsverarbeitung

## Funktionalitäten

### Hardware-Abstraktionsschicht
- **Display-Abstraktion**: Trennung zwischen Anzeigenlogik und spezifischer Display-Hardware
- **CAN-Controller-Abstraktion**: Einheitliche Schnittstelle für verschiedene CAN-Hardware
- **Systemprofile**: Vordefinierte Kombinationen aus Display und CAN-Controller für einfache Konfiguration

### Menüsystem
- Navigation mit 3 Tasten (UP, DOWN, SELECT/BACK)
- Lange Tasten-Drücke für "Zurück"-Funktion
- Einstellbare Parameter mit numerischem Eingabe-Modus
- Scrollbare Menülisten mit Scrollbalken
- Visuelle Rückmeldung der aktuellen Aktion

### CANopen-Funktionen
- **Node-Scanning**: Erkennung aktiver Knoten im Netzwerk mit fortschrittlichem Scan-Algorithmus
- **Baudratenerkennung**: Automatische Erkennung der Bus-Baudrate durch Testen verschiedener Raten
- **Node-Test**: Erweiterte Testfunktion für einzelne Knoten mit anpassbarer Intensität
- **Node-ID-Änderung**: Änderung der ID eines Knotens mittels SDO mit EEPROM-Speicherung
- **Baudrate-Änderung**: Anpassung der Bus-Baudrate für Knoten und Controller
- **Live-Monitor**: Anzeige von CAN-Nachrichten in Echtzeit mit Dekodierung bekannter Nachrichtentypen
- **Nachrichtenfilter**: Filterung von CAN-Nachrichten nach ID, Node oder Nachrichtentyp

### Systemverwaltung
- **Konfigurationsprofile**: Vordefinierte Hardware-Kombinationen für einfachen Wechsel
- **Fehlerbehandlung**: Erkennung von Abstürzen und automatischer Start im sicheren Modus
- **Persistente Einstellungen**: Speichern und Laden von Konfigurationen im nichtflüchtigen Speicher
- **Systemdiagnose**: Anzeige von Systeminformationen und Status

### Serielle Schnittstelle
- Umfassender Befehlssatz für alle Menüfunktionen
- Parallele Nutzung mit OLED-Menü durch Steuerungskoordination
- Hilfe-System zur Anzeige verfügbarer Befehle
- Ausgabe von detaillierten diagnostischen Informationen

## Technische Details

### Hardware-Abhängigkeiten
- **Microcontroller**: ESP32 (getestet mit ESP32 und ESP32-S3)
- **CAN-Controller**: Unterstützt MCP2515 (SPI), ESP32-CAN (integriert) und TJA1051
- **Displays**: Unterstützt SSD1306 OLED (I2C) und Waveshare ESP32-S3 Touch LCD

### Software-Abhängigkeiten
- **Arduino-Framework**: Basis für die gesamte Implementierung
- **SPI-Bibliothek**: Für MCP2515-Ansteuerung
- **Wire-Bibliothek**: Für I2C-Kommunikation (OLED)
- **Adafruit GFX und SSD1306-Bibliothek**: Für OLED-Ansteuerung
- **TFT_eSPI-Bibliothek**: Für Waveshare TFT-Display
- **MCP_CAN-Bibliothek**: Für MCP2515 CAN-Controller
- **esp_twai-Bibliothek**: Für TJA1051 CAN-Controller
- **Preferences-Bibliothek**: Für persistente Einstellungen

### CANopen-Protokoll-Implementation
Die Implementierung unterstützt zentrale CANopen-Funktionen:
- **NMT-Befehle**: Start, Stop, Reset
- **SDO-Kommunikation**: Expedited-Upload/Download, Fehlerbehandlung
- **Heartbeat-Überwachung**: Bootup-Erkennung, Status-Dekodierung
- **Emergency-Dekodierung**: Auswertung von Fehlermeldungen

### Steuerungskoordination
Das System verwendet einen ausgefeilten Zustandsmechanismus, um Konflikte zwischen Bedienquellen zu vermeiden:
- Aktive Steuerquelle erhält temporäre Priorität
- Zeitgesteuertes Freigeben der Steuerungsquelle nach Inaktivität
- Explizite Übernahme durch alternative Steuerquelle möglich
- Visuelle Rückmeldung des aktiven Steuerungsmodus

### Robustheitsmerkmale
- **Absturzerkennung**: Zählt schnelle Neustarts, um in einen sicheren Modus zu wechseln
- **Validierung**: Überprüfung der Kompatibilität zwischen Displaytyp und CAN-Controller
- **Fehlerbehebung**: Automatische Wiederherstellung bei fehlgeschlagenen Operationen
- **Timeout-Handling**: Verhindert Blockieren bei Kommunikationsfehlern

## Integration mit bestehendem Code

### Upgradepfad
Die vorhandene Codebasis (`ESP32_CAN_DUAL_V004.ino`) verwendet bereits:
- **Abstraktionsschichten**: `CANInterface` und `DisplayInterface` für Hardwareabstraktion
- **Factory-Pattern**: Zur Erzeugung der konkreten Interface-Implementierungen
- **Persistenz**: Speichern und Laden von Einstellungen über die Preferences-Bibliothek

Die Integration des neuen Menüsystems erfordert:
1. **Einbindung der Menükomponenten**: OLEDMenu.h und OLEDMenu.cpp in das bestehende Projekt
2. **Zusammenführung der Steuerungslogik**: Integration der Steuerungskoordination in die bestehende `loop()`-Funktion
3. **Anpassung der Handler**: Verlagerung der bestehenden Kommandohandler in die Menüaktionen

### Hauptprogramm-Änderungen
- Hinzufügen des Aufrufs von `menuInit()` in `setup()`
- Integration von `handleButtons()` und Button-Event-Handling in die `loop()`-Funktion
- Weiterleitung der seriellen Befehle an `handleSerialCommands()`
- Einbindung der Steuerungsquellenverwaltung (`activeSource`, `lastActivityTime`)

### Serielle Befehle-Erweiterung
Alle bestehenden seriellen Befehle müssen in die neue `handleSerialCommands()`-Funktion integriert werden, wobei die Steuerungsquellenverwaltung berücksichtigt wird:
- Setzen von `activeSource = SOURCE_SERIAL` bei Befehlseingang
- Aktualisieren von `lastActivityTime` bei jeder Befehlsverarbeitung
- Anzeigen des entsprechenden Modi-Bildschirms über `displaySerialModeScreen()`

## Erweiterungsmöglichkeiten

Das System wurde mit Erweiterbarkeit im Blick entworfen:
1. **Alternative Displays**: Durch die Display-Abstraktion können weitere Display-Typen unterstützt werden
2. **Weitere CAN-Controller**: Neue Controller können durch Implementierung des CANInterface ergänzt werden
3. **PDO-Konfiguration**: Implementierung von PDO-Mapping und -Konfiguration
4. **Datalogging**: Aufzeichnung von CAN-Nachrichten auf SD-Karte (via SPI)
5. **Erweiterte Filter**: Komplexere Filterbedingungen für CAN-Nachrichten
6. **LSS-Dienste**: Implementierung des Layer Setting Service für automatische Knotenkonfiguration
7. **Touchscreen-Unterstützung**: Für Waveshare ESP32-S3 Touch LCD
8. **Netzwerkdiagnose**: Erweiterte Analysen des CANopen-Netzwerks (Buslast, Fehlerstatistiken)

## Systemprofile

Das System unterstützt folgende vordefinierte Hardwarekombinationen:

1. **OLED + MCP2515** (Profil 1):
   - SSD1306 OLED-Display via I2C
   - MCP2515 CAN-Controller via SPI
   - 3 Tasten für Menünavigation

2. **TFT + TJA1051** (Profil 2):
   - Waveshare ESP32-S3 Touch LCD
   - TJA1051 CAN-Transceiver mit ESP32-S3 integriertem CAN-Controller
   - 3 Tasten für Menünavigation (auch Touchbedienung möglich)

Diese Dokumentation bietet eine umfassende Grundlage für die Implementierung eines 3-Tasten OLED-Menüsystems für CANopen mit verschiedenen Hardware-Konfigurationen und kann als Referenz für die weitere Entwicklung verwendet werden.