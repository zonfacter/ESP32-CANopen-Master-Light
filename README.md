# ESP32-CANopen-Master-Light
### Hinweis zur Code-Entstehung

Ein Großteil des Codes in diesem Projekt wurde mithilfe moderner KI-Tools wie ChatGPT (GPT-4.5, OpenAI) und Claude 3 (Anthropic) unter Anleitung des Autors generiert.

Ich (Thomas Hinz) habe die generierten Inhalte geprüft, angepasst und mit meiner Fachkenntnis aus dem Bereich SPS-Programmierung, Automatisierungstechnik und Embedded-Projektleitung erweitert.

Da ich aktuell noch nicht alle Details der C++-Syntax vollständig beherrsche, wurde KI unterstützend eingesetzt, insbesondere bei Strukturierung, Implementierung und Kommentierung des Codes.

Alle Inhalte wurden unter Berücksichtigung der Lizenzbedingungen der KI-Anbieter erstellt und stehen unter der [Apache License 2.0](./LICENSE.txt).

## Übersicht

Der ESP32-CANopen-Master-Light ermöglicht die einfache Integration und Steuerung von CANopen-kompatiblen Geräten in IoT- und Automatisierungsprojekten. Die Kombination aus leistungsstarkem ESP32-Mikrocontroller und dem robusten CANopen-Protokoll bietet eine zuverlässige Lösung für industrielle und Hobby-Anwendungen.

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
