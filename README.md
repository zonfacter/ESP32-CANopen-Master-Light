# ESP32-CANopen-Master-Light
### Hinweis zur Code-Entstehung

Ein Großteil des Codes in diesem Projekt wurde mithilfe moderner KI-Tools wie ChatGPT (GPT-4.5, OpenAI) und Claude 3 (Anthropic) unter Anleitung des Autors generiert.

Ich (Thomas Hinz) habe die generierten Inhalte geprüft, angepasst und mit meiner Fachkenntnis aus dem Bereich SPS-Programmierung, Automatisierungstechnik und Embedded-Projektleitung erweitert.

Da ich aktuell noch nicht alle Details der C++-Syntax vollständig beherrsche, wurde KI unterstützend eingesetzt, insbesondere bei Strukturierung, Implementierung und Kommentierung des Codes.

Alle Inhalte wurden unter Berücksichtigung der Lizenzbedingungen der KI-Anbieter erstellt und stehen unter der [Apache License 2.0](./LICENSE.txt).

## Übersicht

Der ESP32-CANopen-Master-Light ermöglicht die einfache Integration und Steuerung von CANopen-kompatiblen Geräten in IoT- und Automatisierungsprojekten. Die Kombination aus leistungsstarkem ESP32-Mikrocontroller und dem robusten CANopen-Protokoll bietet eine zuverlässige Lösung für industrielle und Hobby-Anwendungen.

**Aktuelle Version:** V005_A (Januar 2026)

## Review-TODOs (V005_A)
- [x] Menüpunkt „Version“ im OLED-Menü anzeigen
- [x] Serielle Befehlsverarbeitung nicht-blockierend machen (Puffer statt `readStringUntil`)
- [x] Display-Abmessungen zentralisieren (OLED/TFT) und harte Werte reduzieren
- [x] String-Last in der seriellen Eingabe reduzieren


## V001 
# ESP32 CANopen Node-ID-Scanner und Manager

## Übersicht

Dieses Repository enthält eine robuste Implementierung eines CANopen Node-ID-Scanners und Managers für ESP32-Mikrocontroller. Die Software ermöglicht es Benutzern:

1. Ein CANopen-Netzwerk zu scannen, um aktive Knoten zu identifizieren
2. CAN-Bus-Verkehr in Echtzeit zu überwachen
3. Die Node-ID von CANopen-Geräten zu ändern
4. Systemstatus auf einem OLED-Display zu visualisieren

Die Implementierung ist für Arduino-kompatible ESP32-Boards mit CAN-Controllern konzipiert, insbesondere unter Verwendung des MCP2515 CAN-Controllers über SPI.

## Hardware-Anforderungen

- ESP32-Entwicklungsboard
- MCP2515 CAN-Controller-Modul
- SSD1306 OLED-Display (128x64)
- CAN-Transceiver
- CAN-Bus mit CANopen-Geräten

## Verkabelungsdiagramm

```
ESP32           MCP2515         OLED-Display
------------------------------------
GPIO5  ------>  CS
GPIO4  ------>  INT
VSPI MOSI -->  SI
VSPI MISO <--  SO
VSPI CLK  -->  SCK
GND      <-->  GND     <-->  GND
3.3V     -->   VCC     -->   VCC
I2C SDA  ----------------->  SDA
I2C SCL  ----------------->  SCL
```

## Softwarekomponenten

Das Projekt besteht aus folgenden Hauptkomponenten:

1. **ESP32_CAN_DUAL_V001.ino**: Hauptanwendungsdatei mit Benutzeroberfläche und Kommandoverarbeitung
2. **CANopenClass.h/cpp**: CANopen-Protokollimplementierung mit SDO- und NMT-Unterstützung
3. **CANopen.h**: Definitionen für CANopen-Protokollkonstanten (COB-IDs, NMT-Befehle)

## Funktionen

### Node-ID-Scan

Die Scan-Funktion durchsucht einen Bereich von Node-IDs (standardmäßig 1-10, konfigurierbar) und identifiziert aktive CANopen-Geräte. Der Scan funktioniert durch Senden einer SDO-Leseanfrage an das Fehlerregister (Index 0x1001, Subindex 0x00) jedes potenziellen Knotens. Antwortende Knoten werden auf dem OLED-Display und über die serielle Schnittstelle angezeigt.

### Live-Monitor

Der Live-Monitor zeigt alle CAN-Frames in Echtzeit an, einschließlich Dekodierung gängiger CANopen-Nachrichtentypen (NMT, PDO, SDO, Heartbeat). Diese Funktion ist nützlich zur Diagnose und zum Verständnis der Netzwerkkommunikation.

### Node-ID-Änderung

Eine der Hauptfunktionen dieses Tools ist die Fähigkeit, die Node-ID eines CANopen-Geräts zu ändern. Dies geschieht in mehreren Schritten:

#### Ablauf der Node-ID-Änderung

1. **Erreichbarkeitsprüfung**: Zuerst wird geprüft, ob der Zielknoten erreichbar ist, indem das Fehlerregister (0x1001:00) gelesen wird.

2. **Pre-Operational-Modus**: Der Knoten wird in den Pre-Operational-Modus versetzt, da viele CANopen-Geräte nur in diesem Zustand Konfigurationsänderungen erlauben.

3. **Schreibfreigabe**: Es wird eine Schreibfreigabe an das Gerät gesendet. Diese besteht aus einem Schreibbefehl an den Index 0x2000, Subindex 0x01 mit dem Wert "nerw" (als ASCII-Hex: 0x6E657277). Dieser Schritt ist oft herstellerspezifisch und dient als Sicherheitsmaßnahme, um versehentliche Änderungen zu verhindern.

   ```cpp
   uint32_t unlockValue = 0x6E657277; // "nerw" als ASCII-Hex
   writeSDO(oldId, 0x2000, 0x01, unlockValue, 4);
   ```

4. **Node-ID-Schreiben**: Nach erfolgreicher Freigabe wird die neue Node-ID an den Index 0x2000, Subindex 0x02 geschrieben. Für maximale Kompatibilität verwenden wir eine 4-Byte-Schreiboperation (obwohl die Node-ID selbst nur 1 Byte groß ist).

   ```cpp
   writeSDO(oldId, 0x2000, 0x02, newId, 4);
   ```

5. **EEPROM-Speicherung**: Um die Änderung dauerhaft zu speichern, wird ein "save"-Befehl (als ASCII-Hex: 0x65766173) an den Index 0x1010, Subindex 0x02 gesendet. Dieser Schritt kann länger dauern als normale SDO-Operationen und benötigt möglicherweise einen größeren Timeout-Wert.

   ```cpp
   uint32_t saveValue = 0x65766173; // "save" als ASCII-Hex
   writeSDO(oldId, 0x1010, 0x02, saveValue, 4);
   ```

6. **Geräte-Reset**: Nach der Konfiguration wird ein NMT-Reset-Befehl gesendet, um das Gerät mit der neuen Node-ID neu zu starten.

   ```cpp
   sendNMTCommand(oldId, NMT_CMD_RESET_NODE);
   ```

7. **Verifikation**: Das System wartet auf einen Heartbeat oder Bootup-Frame von der neuen Node-ID, um zu bestätigen, dass die Änderung erfolgreich war.

#### Wichtige Hinweise zur Node-ID-Änderung

- Die Indizes 0x2000:01, 0x2000:02 für die Schreibfreigabe und Node-ID sind herstellerspezifisch und können je nach Gerät variieren.
- Der Wert "nerw" für die Schreibfreigabe ist ebenfalls herstellerspezifisch.
- Die EEPROM-Speicherung kann bei manchen Geräten fehlschlagen oder länger dauern.
- Es ist wichtig, die korrekte Byte-Reihenfolge für die ASCII-Werte zu verwenden (Little-Endian in diesem Fall).

## Fehlerbehandlung

Das System implementiert eine robuste Fehlerbehandlung:

- **Keine Endlosschleifen**: Bei Fehlern verwendet das System einen Timeout-basierten Ansatz statt Endlosschleifen.
- **Debug-Ausgaben**: Detaillierte Debug-Informationen werden über die serielle Schnittstelle ausgegeben.
- **Visualisierung**: Fehler werden deutlich auf dem OLED-Display angezeigt.
- **Automatische Wiederherstellung**: Nach einem Fehler wird das System nach einem konfigurierbaren Timeout automatisch zurückgesetzt.

## Verwendung

### Serielle Befehle

Die folgenden Befehle können über die serielle Schnittstelle (115200 Baud) gesendet werden:

- `scan` - Startet einen Node-ID-Scan im konfigurierten Bereich
- `range x y` - Setzt den Scanbereich auf Knoten x bis y (z.B. `range 1 127`)
- `monitor on` - Aktiviert den Live-Monitor
- `monitor off` - Deaktiviert den Live-Monitor
- `change a b` - Ändert die Node-ID von a zu b (z.B. `change 8 4`)
- `reset` - Setzt das System zurück

### OLED-Display

Das OLED-Display zeigt Statusinformationen, Scanergebnisse und Fehlermeldungen an.

## Bekannte Einschränkungen

- Die Objektverzeichniseinträge für Node-ID-Änderungen (0x2000:01, 0x2000:02) sind herstellerspezifisch.
- Die EEPROM-Speicherung kann bei einigen Geräten fehlschlagen.
- Die maximale Anzahl von Knoten ist auf 127 begrenzt (CANopen-Standard).

## Weiterentwicklung

Dieses Projekt kann als Grundlage für erweiterte CANopen-Tools dienen, wie beispielsweise:

- Vollständiger CANopen-Master mit PDO-Unterstützung
- EDS-Datei-Parser für automatische Gerätekonfiguration
- Erweiterte Diagnose- und Monitoring-Funktionen

---

# ESP32 CANopen Node ID Scanner and Manager

## Overview

This repository contains a robust implementation of a CANopen Node ID Scanner and Manager for ESP32 microcontrollers. The software allows users to:

1. Scan a CANopen network to identify active nodes
2. Monitor CAN bus traffic in real-time
3. Change the Node ID of CANopen devices
4. Visualize system status on an OLED display

The implementation is built for Arduino-compatible ESP32 boards with CAN controllers, specifically using the MCP2515 CAN controller via SPI.

## Hardware Requirements

- ESP32 development board
- MCP2515 CAN controller module
- SSD1306 OLED display (128x64)
- CAN transceiver
- CAN bus with CANopen devices

## Wiring Diagram

```
ESP32           MCP2515         OLED Display
------------------------------------
GPIO5  ------>  CS
GPIO4  ------>  INT
VSPI MOSI -->  SI
VSPI MISO <--  SO
VSPI CLK  -->  SCK
GND      <-->  GND     <-->  GND
3.3V     -->   VCC     -->   VCC
I2C SDA  ----------------->  SDA
I2C SCL  ----------------->  SCL
```

## Software Components

The project consists of the following main components:

1. **ESP32_CAN_DUAL_V001.ino**: Main application file with user interface and command processing
2. **CANopenClass.h/cpp**: CANopen protocol implementation with SDO and NMT support
3. **CANopen.h**: Definitions for CANopen protocol constants (COB-IDs, NMT commands)

## Features

### Node ID Scanning

The scanning function searches through a range of Node IDs (default 1-10, configurable) and identifies active CANopen devices. The scan works by sending an SDO read request to the error register (index 0x1001, subindex 0x00) of each potential node. Responding nodes are displayed on the OLED display and via the serial interface.

### Live Monitor

The live monitor displays all CAN frames in real-time, including decoding of common CANopen message types (NMT, PDO, SDO, Heartbeat). This function is useful for diagnostics and understanding network communication.

### Node ID Changing

One of the main features of this tool is the ability to change the Node ID of a CANopen device. This happens in several steps:

#### Node ID Change Process

1. **Reachability Check**: First, it checks if the target node is reachable by reading the error register (0x1001:00).

2. **Pre-Operational Mode**: The node is set to Pre-Operational mode, as many CANopen devices only allow configuration changes in this state.

3. **Write Unlock**: A write unlock command is sent to the device. This consists of a write command to index 0x2000, subindex 0x01 with the value "nerw" (as ASCII hex: 0x6E657277). This step is often manufacturer-specific and serves as a safety measure to prevent accidental changes.

   ```cpp
   uint32_t unlockValue = 0x6E657277; // "nerw" as ASCII hex
   writeSDO(oldId, 0x2000, 0x01, unlockValue, 4);
   ```

4. **Node ID Writing**: After successful unlocking, the new Node ID is written to index 0x2000, subindex 0x02. For maximum compatibility, we use a 4-byte write operation (although the Node ID itself is only 1 byte).

   ```cpp
   writeSDO(oldId, 0x2000, 0x02, newId, 4);
   ```

5. **EEPROM Storage**: To permanently store the change, a "save" command (as ASCII hex: 0x65766173) is sent to index 0x1010, subindex 0x02. This step may take longer than normal SDO operations and may require a larger timeout value.

   ```cpp
   uint32_t saveValue = 0x65766173; // "save" as ASCII hex
   writeSDO(oldId, 0x1010, 0x02, saveValue, 4);
   ```

6. **Device Reset**: After configuration, an NMT reset command is sent to restart the device with the new Node ID.

   ```cpp
   sendNMTCommand(oldId, NMT_CMD_RESET_NODE);
   ```

7. **Verification**: The system waits for a heartbeat or bootup frame from the new Node ID to confirm that the change was successful.

#### Important Notes on Node ID Changing

- The indices 0x2000:01, 0x2000:02 for write unlock and Node ID are manufacturer-specific and may vary by device.
- The value "nerw" for write unlock is also manufacturer-specific.
- EEPROM storage may fail or take longer on some devices.
- It's important to use the correct byte order for ASCII values (Little-Endian in this case).

## Error Handling

The system implements robust error handling:

- **No Infinite Loops**: In case of errors, the system uses a timeout-based approach instead of infinite loops.
- **Debug Outputs**: Detailed debug information is output via the serial interface.
- **Visualization**: Errors are clearly displayed on the OLED display.
- **Automatic Recovery**: After an error, the system automatically resets after a configurable timeout.

## Usage

### Serial Commands

The following commands can be sent via the serial interface (115200 baud):

- `scan` - Starts a Node ID scan in the configured range
- `range x y` - Sets the scan range to nodes x to y (e.g., `range 1 127`)
- `monitor on` - Activates the live monitor
- `monitor off` - Deactivates the live monitor
- `change a b` - Changes the Node ID from a to b (e.g., `change 8 4`)
- `reset` - Resets the system

### OLED Display

The OLED display shows status information, scan results, and error messages.

## Known Limitations

- The object dictionary entries for Node ID changes (0x2000:01, 0x2000:02) are manufacturer-specific.
- EEPROM storage may fail on some devices.
- The maximum number of nodes is limited to 127 (CANopen standard).

## Further Development

This project can serve as a foundation for advanced CANopen tools, such as:

- Full CANopen master with PDO support
- EDS file parser for automatic device configuration
- Advanced diagnostics and monitoring functions




## Features (v0.6)
✅ CANopen Node Scan  
✅ Node ID / Baudrate ändern  
✅ OLED Menü & 3-Tasten Bedienung  
✅ WebSocket-API (JSON)  
✅ Dunkermotor BG45/BG65 Support  
✅ Echtzeit-Statusüberwachung  
✅ Gerätekonfiguration via Webinterface  
✅ Multi-Geräte-Management  
✅ PDO/SDO Kommunikation  

## Hardware-Anforderungen
- ESP32 Development Board
- MCP2515 CAN-Bus-Modul
- 128x64 OLED Display (SSD1306/SH1106)
- 3 Taster für Menübedienung
- Micro-USB-Kabel für Stromversorgung/Programmierung
- Optionaler Gehäuse-3D-Druck verfügbar

## Installation
1. Repository klonen: `git clone https://github.com/Zonfacter/ESP32-CANopen-Master-Light.git`
2. Abhängigkeiten installieren (siehe `platformio.ini` oder `requirements.txt`)
3. Firmware mit PlatformIO oder Arduino IDE kompilieren und auf ESP32 flashen
4. Hardware gemäß Schaltplan verbinden

## Anwendungsbeispiele
- Steuerung und Überwachung von Industriemotoren
- Automatisierungsprojekte mit mehreren CANopen-Geräten
- Retrofit-Lösungen für bestehende CANopen-Netzwerke
- Entwicklung und Prototyping von CANopen-Anwendungen

## Nutzung
```cpp
// Beispiel: Motor über WebSocket API steuern
// POST /api/motor/control
{
  "nodeId": 3,
  "command": "speed",
  "value": 1000,
  "acceleration": 500
}
```

## Roadmap 
Ist erstmal so angedacht, kann sich aber noch verändern:
- [ ] v0.7 – microSD Logging
  - Ereignis- und Datenaufzeichnung auf SD-Karte
  - Exportfunktion für Analysen
  - Konfigurierbares Logging-Level
- [ ] v0.8 – EDS-Dateien laden
  - Unterstützung für Electronic Data Sheets
  - Automatische Geräteerkennung und -konfiguration
  - Erweiterte Geräteparametrisierung
- [ ] v1.0 – Touch-GUI mit LVGL
  - Intuitives Touch-Interface
  - Erweiterte Visualisierungsmöglichkeiten
  - Dashboards und Statistiken
  - Multi-Sprach-Unterstützung

## Mitwirken
Beiträge zum Projekt sind herzlich willkommen! So kannst du helfen:
- Feature-Requests und Bug-Reports über Issues
- Pull-Requests für Verbesserungen oder neue Funktionen
- Dokumentation erweitern oder übersetzen
- Testen mit verschiedenen CANopen-Geräten

## Support
- [Dokumentation](https://github.com/Zonfacter/ESP32-CANopen-Master-Light/wiki)
- [Issues](https://github.com/Zonfacter/ESP32-CANopen-Master-Light/issues)
- [Diskussionen](https://github.com/Zonfacter/ESP32-CANopen-Master-Light/discussions)

## Lizenz
Apache License 2.0 – © 2024 Thomas Hinz (Zonfacter)
