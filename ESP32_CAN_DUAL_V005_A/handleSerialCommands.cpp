// handleSerialCommands.cpp
// ===============================================================================
// Implementation der Befehlsverarbeitung für serielle Befehle
// ===============================================================================

#include <Arduino.h>
#include "OLEDMenu.h"
#include "CANopen.h"
#include "CANopenClass.h"
#include "DisplayInterface.h"
#include "SystemProfiles.h"
#include <Preferences.h>

// Externe Variablen aus Hauptprogramm
extern ControlSource activeSource;
extern unsigned long lastActivityTime;
extern DisplayInterface* displayInterface;
extern InputValues inputValues;
extern CANopen canopen;
extern uint8_t scanStart;
extern uint8_t scanEnd;
extern bool scanning;
extern bool autoBaudrateRequest;
extern bool liveMonitor;
extern bool filterEnabled;
extern bool systemError;
extern int currentBaudrate;
extern uint8_t currentCANTransceiverType;
extern uint8_t currentDisplayType;
extern Preferences preferences;

// Externe Funktionen
extern void displaySerialModeScreen();
extern void displayActionScreen(const char* title, const char* message, int timeout);
extern void scanNodes(int startID, int endID);
extern bool updateESP32CANBaudrate(int newBaudrate);
extern void changeNodeId(uint8_t from, uint8_t to);
extern bool testSingleNode(int nodeId, int maxAttempts, int timeoutMs);
extern bool isValidBaudrate(int baudrate);
extern void printHelpMenu();
extern void saveSettings();
extern void changeCommunicationSettings(uint8_t targetNodeId, int newBaudrateKbps);
extern void handleModeCommand(String command);
extern void handleTransceiverCommand(String command);
extern void handleMonitorFilterCommand(String command);
extern void printCurrentSettings();
extern void systemReset();

// Hilfsfunktion zum Parsen von Befehlsparametern
bool parseIntParams(String command, int& first, int& second) {
    int firstSpace = command.indexOf(' ');
    if (firstSpace <= 0) return false;
    
    int secondSpace = command.indexOf(' ', firstSpace + 1);
    if (secondSpace <= 0) return false;
    
    first = command.substring(firstSpace + 1, secondSpace).toInt();
    second = command.substring(secondSpace + 1).toInt();
    
    return true;
}

bool readSerialCommand(String& command) {
    static char commandBuffer[128];
    static size_t commandIndex = 0;

    while (Serial.available()) {
        char currentChar = Serial.read();
        if (currentChar == '\n' || currentChar == '\r') {
            if (commandIndex == 0) {
                continue;
            }

            commandBuffer[commandIndex] = '\0';
            commandIndex = 0;
            command = String(commandBuffer);
            command.trim();
            return command.length() > 0;
        }

        if (commandIndex < sizeof(commandBuffer) - 1) {
            commandBuffer[commandIndex++] = currentChar;
        } else {
            commandIndex = 0;
            Serial.println("[FEHLER] Befehl zu lang, Puffer geleert.");
            return false;
        }
    }

    return false;
}

// Verarbeitung serieller Befehle
void handleSerialCommands() {
    String command;
    if (readSerialCommand(command)) {
        // Serielle Steuerung wird aktiv
        activeSource = SOURCE_SERIAL;
        lastActivityTime = millis();
        
        // Befehl analysieren und ausführen
        if (command.equals("help")) {
            printHelpMenu();
        }
        else if (command.equals("scan")) {
            Serial.printf("[CMD] Starte Node-Scan von %d bis %d...\n", scanStart, scanEnd);
            scanning = true;
            
            // Displayanzeige aktualisieren
            if (displayInterface != nullptr) {
                displaySerialModeScreen();
            }
        }
        else if (command.startsWith("range")) {
            int newStart, newEnd;
            
            // Korrekte Parameter-Extraktion (Index 6 statt 5 + Trim)
            String params = command.substring(6);
            params.trim();
            
            if (parseIntParams(params, newStart, newEnd)) {
                // Erweiterte Plausibilitätsprüfung
                if (newStart >= 1 && newStart <= 127 && 
                    newEnd >= 1 && newEnd <= 127 && 
                    newStart <= newEnd) {
                    
                    scanStart = newStart;
                    scanEnd = newEnd;
                    Serial.printf("[OK] Bereich %d-%d gesetzt\n", scanStart, scanEnd);
                    saveSettings();

                    // Display aktualisieren
                    if (displayInterface != nullptr) {
                        displaySerialModeScreen();}
                    
                } else {
                    Serial.println("[FEHLER] Werte müssen 1-127 sein und Start ≤ Ende");
                }
            } else {
                Serial.println("[FEHLER] Syntax: range <Start> <Ende> (z. B. 'range 1 125')");
            }
        }
        else if (command.startsWith("monitor")) {
            if (command.equals("monitor on")) {
                liveMonitor = true;
                Serial.println("[INFO] Live-Monitor: Ein");
                
                // Displayanzeige aktualisieren
                if (displayInterface != nullptr) {
                    displayActionScreen("Live-Monitor", "CAN-Daten...", 1000);
                }
            }
            else if (command.equals("monitor off")) {
                liveMonitor = false;
                Serial.println("[INFO] Live-Monitor: Aus");
                
                // Displayanzeige aktualisieren
                if (displayInterface != nullptr) {
                    displaySerialModeScreen();
                }
            }
            else if (command.startsWith("monitor filter")) {
                handleMonitorFilterCommand(command.substring(14));
            }
            else {
                Serial.println("[FEHLER] Falsche Syntax. Korrekt: monitor on/off oder monitor filter [parameter]");
            }
        }
        else if (command.startsWith("change")) {
            int oldId, newId;
            
            if (parseIntParams("change " + command.substring(6), oldId, newId)) {
                if (oldId >= 1 && oldId <= 127 && newId >= 1 && newId <= 127) {
                    Serial.printf("[CMD] Ändere Node-ID von %d nach %d...\n", oldId, newId);
                    changeNodeId(oldId, newId);
                } else {
                    Serial.println("[FEHLER] Ungültige Node-ID! Gültige Werte: 1-127");
                }
            } else {
                Serial.println("[FEHLER] Falsche Syntax. Korrekt: change <alte_id> <neue_id>");
            }
        }
        else if (command.startsWith("baudrate")) {
            int nodeId, baudrate;
            
            if (parseIntParams("baudrate " + command.substring(8), nodeId, baudrate)) {
                if (nodeId >= 1 && nodeId <= 127 && isValidBaudrate(baudrate)) {
                    Serial.printf("[CMD] Ändere Baudrate für Node %d auf %d kbps...\n", nodeId, baudrate);
                    changeCommunicationSettings(nodeId, baudrate);
                } else {
                    if (!isValidBaudrate(baudrate)) {
                        Serial.println("[FEHLER] Ungültige Baudrate! Gültige Werte: 10, 20, 50, 100, 125, 250, 500, 800, 1000 kbps");
                    }
                    if (nodeId < 1 || nodeId > 127) {
                        Serial.println("[FEHLER] Ungültige Node-ID! Gültige Werte: 1-127");
                    }
                }
            } else {
                Serial.println("[FEHLER] Falsche Syntax. Korrekt: baudrate <node_id> <baudrate_kbps>");
            }
        }
        else if (command.startsWith("localbaud")) {
            int space = command.indexOf(' ');
            if (space > 0) {
                int baudrate = command.substring(space + 1).toInt();
                if (isValidBaudrate(baudrate)) {
                    Serial.printf("[CMD] Ändere lokale Baudrate auf %d kbps...\n", baudrate);
                    if (updateESP32CANBaudrate(baudrate)) {
                        currentBaudrate = baudrate;
                        saveSettings();
                    }
                } else {
                    Serial.println("[FEHLER] Ungültige Baudrate! Gültige Werte: 10, 20, 50, 100, 125, 250, 500, 800, 1000 kbps");
                }
            } else {
                Serial.println("[FEHLER] Falsche Syntax. Korrekt: localbaud <baudrate_kbps>");
            }
        }
        else if (command.startsWith("testnode")) {
            int space = command.indexOf(' ');
            if (space > 0) {
                String params = command.substring(space + 1);
                int firstSpace = params.indexOf(' ');
                
                int nodeId = 0;
                int attempts = 5;  // Standardwert: 5 Versuche
                int timeout = 500; // Standardwert: 500ms Timeout
                
                if (firstSpace > 0) {
                    // NodeId und weitere Parameter angegeben
                    nodeId = params.substring(0, firstSpace).toInt();
                    
                    String rest = params.substring(firstSpace + 1);
                    int secondSpace = rest.indexOf(' ');
                    
                    if (secondSpace > 0) {
                        // Alle drei Parameter angegeben
                        attempts = rest.substring(0, secondSpace).toInt();
                        timeout = rest.substring(secondSpace + 1).toInt();
                    } else {
                        // Nur NodeId und Attempts angegeben
                        attempts = rest.toInt();
                    }
                } else {
                    // Nur NodeId angegeben
                    nodeId = params.toInt();
                }
                
                if (nodeId >= 1 && nodeId <= 127) {
                    Serial.printf("[CMD] Teste Node %d (%d Versuche, %dms Timeout)...\n", nodeId, attempts, timeout);
                    
                    // Live-Monitor-Status merken und aktivieren
                    bool wasMonitorActive = liveMonitor;
                    liveMonitor = true;
                    
                    bool success = testSingleNode(nodeId, attempts, timeout);
                    
                    // Live-Monitor zurücksetzen
                    liveMonitor = wasMonitorActive;
                    
                    Serial.printf("[INFO] Test %s\n", success ? "erfolgreich" : "fehlgeschlagen");
                } else {
                    Serial.println("[FEHLER] Ungültige Node-ID! Gültige Werte: 1-127");
                }
            } else {
                Serial.println("[FEHLER] Falsche Syntax. Korrekt: testnode <node_id> [versuche] [timeout]");
            }
        }
        else if (command.equals("auto")) {
            Serial.println("[CMD] Starte automatische Baudratenerkennung...");
            autoBaudrateRequest = true;
        }
        else if (command.equals("info")) {
            printCurrentSettings();
        }
        else if (command.equals("save")) {
            Serial.println("[CMD] Speichere Einstellungen...");
            saveSettings();
        }
        else if (command.equals("load")) {
            Serial.println("[CMD] Lade Einstellungen...");
            // Einstellungen werden direkt aus dem Hauptprogramm geladen
        }
        else if (command.equals("version")) {
            Serial.println("[INFO] CANopen Scanner und Konfigurator V005_A");
        }
        else if (command.equals("reset")) {
            Serial.println("[CMD] Setze System zurück...");
            systemReset();
        }
        else if (command.startsWith("mode")) {
            handleModeCommand(command.substring(4));
        }
        else if (command.startsWith("transceiver")) {
            handleTransceiverCommand(command.substring(11));
        }
        else if (command.equals("menu")) {
            // Zur Menüsteuerung wechseln
            Serial.println("[CMD] Wechsle zur Menüsteuerung...");
            
            activeSource = SOURCE_BUTTON;
            lastActivityTime = millis();
            
            if (displayInterface != nullptr) {
                displayMenu();
            }
        }
        else {
            Serial.printf("[FEHLER] Unbekannter Befehl: %s\n", command.c_str());
            Serial.println("Geben Sie 'help' ein für eine Liste der verfügbaren Befehle.");
        }
    }
}
