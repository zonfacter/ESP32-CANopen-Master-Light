// === CANopen Node ID Scanner + Live CAN Monitor ===
// Version V002 - mit Baudratenerkennung und -konfiguration
// Scannt Node-IDs durch SDO-Read an 0x1001:00
// Zeigt laufend alle empfangenen Frames (INT-basiert) mit Dekodierung
// Ergebnisanzeige auf OLED + Serial + Node-ID-Changer + Baudratenwechsel
// === CANopen Node ID Scanner + Live CAN Monitor ===
// Version V002 - mit Baudratenerkennung und -konfiguration

#include <SPI.h>
#include <mcp_can.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include "CANopenClass.h"


// Definierte Konstanten
#define OLED_ADDR     0x3C
#define CAN_CS        5
#define CAN_INT       4
#define CAN_CLOCK     MCP_8MHZ

// Versionsinfo
#define VERSION       "V002"

// Globale Objekte
MCP_CAN CAN(CAN_CS);
Adafruit_SSD1306 display(128, 64, &Wire, -1);
CANopen canopen(CAN_INT);
Preferences preferences;

// Globale Variablen im Hauptprogramm
uint8_t currentNodeId = 127;  // Standardwert
int currentBaudrate = 125;    // Standardwert in kbps
uint8_t scanStart = 1;        // Standardwert
uint8_t scanEnd = 10;         // Standardwert
bool scanning = false;
bool liveMonitor = false;
bool systemError = false;
bool autoBaudrateRequest = false;
unsigned long lastErrorTime = 0;

// Funktionsdeklarationen
void showMessage(const char* msg);
void showStatusMessage(const char* title, const char* message, bool isError = false);
void scanNodes(int startID, int endID);
void changeNodeId(uint8_t from, uint8_t to);
void handleSerialCommands();
void printHelpMenu();
void handleBaudrateCommand(String command);
void handleChangeCommand(String command);
void handleRangeCommand(String command);
bool isValidBaudrate(int baudrate);
void printCurrentSettings();
void systemReset();
bool autoBaudrateDetection();
void processCANMessage();
void decodeCANMessage(unsigned long rxId, uint8_t nodeId, uint16_t baseId, byte* buf, byte len);
void decodeNMTState(byte state);
void decodeNMTCommand(byte* buf, byte len);
void decodeSDOAbortCode(uint32_t abortCode);
bool changeBaudrate(uint8_t nodeId, uint8_t baudrateIndex);
bool updateESP32CANBaudrate(int newBaudrate);
uint8_t getBaudrateIndex(int baudrateKbps);

// Funktionsdeklaration
uint8_t convertBaudrateToCANSpeed(int baudrateKbps);
void changeCommunicationSettings(uint8_t newNodeId, int newBaudrateKbps);

// Implementierung der Preferences-Funktionen
void saveSettings() {
    preferences.begin("canopenscan", false);
    preferences.putUChar("nodeId", currentNodeId);
    preferences.putUInt("baudrate", currentBaudrate);
    preferences.putUChar("scanStart", scanStart);
    preferences.putUChar("scanEnd", scanEnd);
    preferences.end();
    
    Serial.println("[INFO] Einstellungen gespeichert");
}

void loadSettings() {
    preferences.begin("canopenscan", true); // Nur Lesen
    
    currentNodeId = preferences.getUChar("nodeId", 127); // Standardwert 127
    currentBaudrate = preferences.getUInt("baudrate", 125); // Standardwert 125kbps
    scanStart = preferences.getUChar("scanStart", 1); // Standardwert 1
    scanEnd = preferences.getUChar("scanEnd", 10); // Standardwert 10
    
    preferences.end();
    
    Serial.println("[INFO] Einstellungen geladen");
    //printCurrentSettings(); // Stellen Sie sicher, dass diese Funktion existiert
}

void setup() {
    Serial.begin(115200);
    Wire.begin();
    
    // Display initialisieren
    display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("CANopen Scanner V002");
    display.display();

    // Gespeicherte Einstellungen laden
    loadSettings();

    // CAN-Controller mit aktueller Baudrate initialisieren
    if (CAN.begin(MCP_ANY, convertBaudrateToCANSpeed(currentBaudrate), CAN_CLOCK) == CAN_OK) {
        CAN.setMode(MCP_NORMAL);
        Serial.println("[INFO] CAN bereit");
        showStatusMessage("CANopen Scanner", "CAN-Bus initialisiert\nSystembereit");
    } else {
        Serial.println("[FEHLER] CAN Init fehlgeschlagen");
        showStatusMessage("FEHLER", "CAN-Bus Initialisierung\nfehlgeschlagen!", true);
        // Fehlerstatus setzen
        liveMonitor = false;
        scanning = false;
        systemError = true;
        lastErrorTime = millis();
    }

    pinMode(CAN_INT, INPUT);

    // Willkommensnachricht und Hilfe anzeigen
    delay(1000);
    printHelpMenu();
}


void loop() {
    // Fehlerbehandlung - wenn systemError gesetzt ist, prüfen wir, ob wir zurücksetzen können
    if (systemError) {
        // Prüfen ob genügend Zeit vergangen ist, um einen Reset zu versuchen
        if (millis() - lastErrorTime > 5000) { // 5 Sekunden Timeout
            systemError = false;
            Serial.println("[INFO] System wird nach Fehler fortgesetzt");
            showStatusMessage("System-Status", "Fehler behoben\nSystem fortgesetzt");
        } else {
            delay(100); // Kleine Pause im Fehlerzustand
            return;     // Rest der Loop überspringen
        }
    }

    // Serielle Befehle verarbeiten
    handleSerialCommands();

    // Node-Scan durchführen, wenn angefordert
    if (scanning) {
        scanNodes(scanStart, scanEnd);
        scanning = false;
        // Einstellungen speichern, wenn ein Scan abgeschlossen wurde
        saveSettings();
    }

    // Automatische Baudratenerkennung, wenn angefordert
    if (autoBaudrateRequest) {
        autoBaudrateDetection();
        autoBaudrateRequest = false;
    }

    // Live Monitor für CAN-Frames
    if (liveMonitor && !digitalRead(CAN_INT)) {
        processCANMessage();
    }
}

// ===================================================================================
// Funktion: showMessage
// Beschreibung: Zeigt eine Nachricht auf dem OLED-Display an
// ===================================================================================
void showMessage(const char* msg) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.println(msg);
    display.display();
}

// ===================================================================================
// Funktion: showStatusMessage
// Beschreibung: Zeigt eine Statusmeldung mit Titel und Details auf dem OLED-Display an
// ===================================================================================
void showStatusMessage(const char* title, const char* message, bool isError) {
    display.clearDisplay();
    display.setCursor(0, 0);
    
    // Titel hervorheben
    display.setTextSize(1);
    display.println(title);
    
    // Trennlinie
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    
    // Nachricht anzeigen
    display.setCursor(0, 14);
    display.println(message);
    
    // Bei Fehler einen visuellen Indikator anzeigen
    if (isError) {
        display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
        display.fillRect(0, 54, 128, 10, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setCursor(5, 55);
        display.print("FEHLER!");
        display.setTextColor(SSD1306_WHITE);
    }
    
    display.display();
}

// ===================================================================================
// Funktion: changeNodeId
// Beschreibung: Wrapper für die CANopen-Klassen-Methode zum Ändern der Node-ID
// ===================================================================================
void changeNodeId(uint8_t from, uint8_t to) {
    // Live-Monitor aktivieren, um alle CAN-Nachrichten zu sehen
    liveMonitor = true;
    
    // Display-Anzeige aktualisieren
    showStatusMessage("Node-ID Änderung", 
                    String("Von " + String(from) + " auf " + String(to) + "\nStatus: Start...").c_str());
    
    Serial.printf("[CMD] Ändere Node-ID von %d nach %d (via CANopenClass)...\n", from, to);
    
    // Unsere CANopen-Klasse verwenden, um die Node-ID zu ändern
    bool ok = canopen.changeNodeId(from, to, true, 5000);
    
    // Erfolgsstatus anzeigen
    if (!ok) {
        Serial.println("[FEHLER] Änderung fehlgeschlagen!");
        
        // Fehlermeldung mit visuellem Indikator anzeigen
        showStatusMessage("Node-ID Änderung", 
                        String("Von " + String(from) + " auf " + String(to) + 
                            "\nStatus: FEHLER!").c_str(), 
                        true);
        
        // Fehler im System markieren, aber nicht blockieren
        systemError = true;
        lastErrorTime = millis();
    } else {
        // Erfolgsmeldung anzeigen
        showStatusMessage("Node-ID Änderung", 
                        String("Von " + String(from) + " auf " + String(to) + 
                            "\nStatus: ERFOLGREICH").c_str());
        
        Serial.printf("[OK] Node-ID erfolgreich von %d auf %d geändert!\n", from, to);
        
        // Aktuelle Node-ID aktualisieren
        currentNodeId = to;
        
        // Einstellungen speichern
        saveSettings();
    }
}

// ===================================================================================
// Funktion: scanNodes
// Beschreibung: Scannt einen Bereich von CANopen-Node-IDs und zeigt aktive Knoten an
// ===================================================================================
void scanNodes(int startID, int endID) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Scan...\n");
    display.display();
    
    Serial.printf("[INFO] Starte Node-Scan von %d bis %d bei %d kbps\n", startID, endID, currentBaudrate);
    
    // Zähler für gefundene Nodes
    int foundNodes = 0;
    
    // Bereich prüfen und begrenzen
    if (startID < 1) startID = 1;
    if (endID > 127) endID = 127;
    
    for (int id = startID; id <= endID; id++) {
        // SDO-Leseanfrage an Fehlerregister (0x1001:00) vorbereiten
        byte sdo[8] = { 0x40, 0x01, 0x10, 0x00, 0, 0, 0, 0 };
        
        // Anfrage senden mit Fehlerbehandlung
        byte result = CAN.sendMsgBuf(0x600 + id, 0, 8, sdo);
        if (result != CAN_OK) {
            Serial.printf("[FEHLER] Konnte keine Anfrage an Node %d senden: %d\n", id, result);
            continue; // Mit dem nächsten Node fortfahren
        }
        
        Serial.printf("[SCAN] Frage Node %d ab...\n", id);

        // Auf Antwort warten mit Timeout
        unsigned long start = millis();
        bool responded = false;
        bool errorResponse = false;

        while (millis() - start < 200) { // 200ms Timeout pro Node
            if (!digitalRead(CAN_INT)) {
                unsigned long rxId;
                byte len;
                byte buf[8];
                
                // Antwort lesen mit Fehlerbehandlung
                byte readStatus = CAN.readMsgBuf(&rxId, &len, buf);
                if (readStatus != CAN_OK) {
                    Serial.printf("[FEHLER] Fehler beim Lesen: %d\n", readStatus);
                    break;
                }
                
                // Prüfen, ob es sich um eine Antwort auf unsere SDO-Anfrage handelt
                if (rxId == (0x580 + id)) {
                    // Prüfen, ob es eine Fehlerantwort ist (SDO Abort)
                    if ((buf[0] & 0xE0) == 0x80) {
                        uint32_t abortCode = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
                        Serial.printf("[OK] Node %d hat mit Fehler geantwortet! Abortcode: 0x%08X\n", id, abortCode);
                        display.printf("Node %d: Err\n", id);
                        errorResponse = true;
                    } else {
                        // Reguläre Antwort
                        Serial.printf("[OK] Node %d hat geantwortet! Fehlerreg: 0x%02X\n", id, buf[4]);
                        display.printf("Node %d: OK\n", id);
                        foundNodes++;
                    }
                    
                    display.display();
                    responded = true;
                    break;
                }
            }
        }

        if (!responded) {
            Serial.printf("[--] Node %d keine Antwort\n", id);
        } else if (!errorResponse) {
            foundNodes++;
        }
        
        delay(50); // Kurze Pause zwischen den Anfragen
    }
    
    // Zusammenfassung anzeigen
    display.clearDisplay();
    display.setCursor(0, 0);
    display.printf("Scan abgeschlossen\n");
    display.printf("%d Nodes gefunden\n", foundNodes);
    display.printf("Bereich: %d-%d\n", startID, endID);
    display.printf("Baudrate: %d kbps", currentBaudrate);
    display.display();
    
    Serial.printf("[INFO] Scan abgeschlossen. %d Nodes gefunden bei %d kbps.\n", foundNodes, currentBaudrate);
}

// ===================================================================================
// Funktion: handleSerialCommands
// Beschreibung: Verarbeitet Befehle, die über die serielle Schnittstelle empfangen werden
// ===================================================================================
void handleSerialCommands() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        
        if (command == "help") {
            printHelpMenu();
        } 
        else if (command == "scan") {
            scanning = true;
            Serial.println("[INFO] Scan wird gestartet...");
        } 
        else if (command.startsWith("range")) {
            handleRangeCommand(command);
        } 
        else if (command == "monitor on") {
            liveMonitor = true;
            Serial.println("[INFO] Live Monitor aktiviert");
        } 
        else if (command == "monitor off") {
            liveMonitor = false;
            Serial.println("[INFO] Live Monitor deaktiviert");
        } 
        else if (command.startsWith("change")) {
            handleChangeCommand(command);
        } 
        else if (command.startsWith("baudrate")) {
            handleBaudrateCommand(command);
        }
        else if (command == "auto") {
            autoBaudrateRequest = true;
            Serial.println("[INFO] Starte automatische Baudratenerkennung...");
        }
        else if (command == "info") {
            printCurrentSettings();
        }
        else if (command == "save") {
            saveSettings();
            Serial.println("[INFO] Einstellungen gespeichert");
        }
        else if (command == "load") {
            loadSettings();
            Serial.println("[INFO] Einstellungen geladen");
        }
        else if (command == "version") {
            Serial.println("\n=== CANopen Scanner und Konfigurator " VERSION " ===");
            Serial.println("© 2023 CANopen Monitor mit Baudratenerkennung");
        }
        else if (command == "reset") {
            systemReset();
        }
        else {
            Serial.println("[FEHLER] Unbekannter Befehl. Geben Sie 'help' ein für eine Liste der verfügbaren Befehle.");
        }
    }
}

// ===================================================================================
// Funktion: printHelpMenu
// Beschreibung: Zeigt eine Hilfe mit allen verfügbaren Befehlen an
// ===================================================================================
void printHelpMenu() {
    Serial.println("\n=== CANopen Scanner und Konfigurator " VERSION " ===");
    Serial.println("Verfügbare Befehle:");
    Serial.println("  help          → Diese Hilfe anzeigen");
    Serial.println("  scan          → Node-ID Scan starten");
    Serial.println("  range x y     → Scan-Bereich setzen (z.B. 1 10)");
    Serial.println("  monitor on    → Live Monitor aktivieren");
    Serial.println("  monitor off   → Live Monitor deaktivieren");
    Serial.println("  change a b    → Node-ID a → b ändern (SDO)");
    Serial.println("  baudrate nodid x    → Baudrate ändern (x in kbps: 10, 20, 50, 100, 125, 250, 500, 800, 1000)");
    Serial.println("  auto          → Automatische Baudratenerkennung starten");
    Serial.println("  info          → Aktuelle Einstellungen anzeigen");
    Serial.println("  save          → Einstellungen speichern");
    Serial.println("  load          → Einstellungen laden");
    Serial.println("  version       → Versionsinfo anzeigen");
    Serial.println("  reset         → System zurücksetzen");
    Serial.println("=======================================");
}

// ===================================================================================
// Funktion: handleBaudrateCommand
// Beschreibung: Verarbeitet den Befehl zum Ändern der Baudrate
// ===================================================================================
void handleBaudrateCommand(String command) {
    // Format: baudrate nodeID baudrate
    int firstSpace = command.indexOf(' ');
    if (firstSpace > 0) {
        int secondSpace = command.indexOf(' ', firstSpace + 1);
        
        // Prüfen, ob beide Parameter vorhanden sind
        if (secondSpace > 0) {
            // Die NodeID extrahieren
            int targetNodeId = command.substring(firstSpace + 1, secondSpace).toInt();
            
            // Die Baudrate extrahieren
            int baudrate = command.substring(secondSpace + 1).toInt();
            
            // Prüfen, ob die NodeID und Baudrate gültig sind
            if (targetNodeId >= 1 && targetNodeId <= 127 && isValidBaudrate(baudrate)) {
                Serial.printf("[INFO] Starte Baudratenwechsel für Node %d zu %d kbps\n", targetNodeId, baudrate);
                changeCommunicationSettings(targetNodeId, baudrate);
            } else {
                if (!isValidBaudrate(baudrate)) {
                    Serial.println("[FEHLER] Ungültige Baudrate! Gültige Werte: 10, 20, 50, 100, 125, 250, 500, 800, 1000 kbps");
                }
                if (targetNodeId < 1 || targetNodeId > 127) {
                    Serial.println("[FEHLER] Ungültige Node-ID! Gültige Werte: 1-127");
                }
            }
        } else {
            Serial.println("[FEHLER] Falsche Syntax. Korrekt: baudrate nodeID baudrate (z.B. baudrate 7 500)");
        }
    } else {
        Serial.println("[FEHLER] Falsche Syntax. Korrekt: baudrate nodeID baudrate (z.B. baudrate 7 500)");
    }
}
// ===================================================================================
// Funktion: handleChangeCommand
// Beschreibung: Verarbeitet den Befehl zum Ändern der Node-ID
// ===================================================================================
void handleChangeCommand(String command) {
    int idx = command.indexOf(' ');
    if (idx > 0) {
        int second = command.indexOf(' ', idx + 1);
        if (second > 0) {
            int oldId = command.substring(idx + 1, second).toInt();
            int newId = command.substring(second + 1).toInt();
            
            // Wertebegrenzung und Plausibilitätsprüfung
            if (oldId >= 1 && oldId <= 127 && newId >= 1 && newId <= 127) {
                Serial.printf("[INFO] Starte Node-ID Änderung von %d nach %d\n", oldId, newId);
                
                // Korrekte Parameter übergeben: oldId statt currentNodeId
                changeNodeId(oldId, newId);
            } else {
                Serial.println("[FEHLER] Ungültige Node-ID! Gültige Werte: 1-127");
            }
        } else {
            Serial.println("[FEHLER] Falsche Syntax. Korrekt: change alter_id neue_id");
        }
    } else {
        Serial.println("[FEHLER] Falsche Syntax. Korrekt: change alter_id neue_id");
    }
}
// ===================================================================================
// Funktion: handleRangeCommand
// Beschreibung: Verarbeitet den Befehl zum Ändern des Scan-Bereichs
// ===================================================================================
void handleRangeCommand(String command) {
    int idx = command.indexOf(' ');
    if (idx > 0) {
        int second = command.indexOf(' ', idx + 1);
        if (second > 0) {
            int newStart = command.substring(idx + 1, second).toInt();
            int newEnd = command.substring(second + 1).toInt();
            
            // Wertebegrenzung und Plausibilitätsprüfung
            if (newStart >= 1 && newStart <= 127 && newEnd >= 1 && newEnd <= 127 && newStart <= newEnd) {
                scanStart = newStart;
                scanEnd = newEnd;
                Serial.printf("[INFO] Bereich gesetzt: %d bis %d\n", scanStart, scanEnd);
                
                // Einstellungen speichern
                saveSettings();
            } else {
                Serial.println("[FEHLER] Ungültiger Bereich! Gültige Werte: 1-127");
            }
        } else {
            Serial.println("[FEHLER] Falsche Syntax. Korrekt: range x y");
        }
    } else {
        Serial.println("[FEHLER] Falsche Syntax. Korrekt: range x y");
    }
}

// ===================================================================================
// Funktion: isValidBaudrate
// Beschreibung: Prüft, ob eine Baudrate gültig ist
// ===================================================================================
bool isValidBaudrate(int baudrate) {
    switch (baudrate) {
        case 10:
        case 20:
        case 50:
        case 100:
        case 125:
        case 250:
        case 500:
        case 800:
        case 1000:
            return true;
        default:
            return false;
    }
}

// ===================================================================================
// Funktion: printCurrentSettings
// Beschreibung: Zeigt die aktuellen Systemeinstellungen an
// ===================================================================================
void printCurrentSettings() {
    Serial.println("\n=== Aktuelle Systemeinstellungen ===");
    Serial.printf("Node-ID: %d\n", currentNodeId);
    Serial.printf("Baudrate: %d kbps\n", currentBaudrate);
    Serial.printf("Scan-Bereich: %d bis %d\n", scanStart, scanEnd);
    Serial.printf("Live Monitor: %s\n", liveMonitor ? "aktiviert" : "deaktiviert");
    Serial.printf("System-Status: %s\n", systemError ? "Fehler" : "OK");
    Serial.println("=======================================");
}

// ===================================================================================
// Funktion: systemReset
// Beschreibung: Setzt das System auf Standardwerte zurück
// ===================================================================================
void systemReset() {
    // Systemvariablen zurücksetzen
    systemError = false;
    liveMonitor = false;
    scanning = false;
    
    // Zurück zur Standard-Baudrate, wenn nicht bereits eingestellt
    if (currentBaudrate != 125) {
        updateESP32CANBaudrate(125);
        currentBaudrate = 125;
    }
    
    // Standard-Node-ID wiederherstellen
    currentNodeId = 127;
    
    // Einstellungen speichern
    saveSettings();
    
    Serial.println("[INFO] System zurückgesetzt auf Standardwerte");
    showStatusMessage("System-Reset", "Alle Parameter zurückgesetzt\nSystem bereit");
}

// ===================================================================================
// Funktion: processCANMessage
// Beschreibung: Verarbeitet empfangene CAN-Nachrichten
// ===================================================================================
void processCANMessage() {
    unsigned long rxId;
    byte len;
    byte buf[8];
    
    // Fehlerbehandlung beim Lesen der CAN-Nachricht
    byte status = CAN.readMsgBuf(&rxId, &len, buf);
    if (status != CAN_OK) {
        Serial.printf("[FEHLER] Fehler beim Lesen der CAN-Nachricht: %d\n", status);
        return; // Diese Iteration überspringen
    }

    uint8_t nodeId = rxId & 0x7F;
    uint16_t baseId = rxId & 0x780;

    // Formatierte Ausgabe der CAN-Nachricht
    Serial.printf("[CAN] ID: 0x%03lX Len: %d →", rxId, len);
    for (int i = 0; i < len; i++) {
        Serial.printf(" %02X", buf[i]);
    }

    // Bekannte Nachrichtentypen identifizieren und interpretieren
    decodeCANMessage(rxId, nodeId, baseId, buf, len);
    
    Serial.println();
}

// ===================================================================================
// Funktion: decodeCANMessage
// Beschreibung: Dekodiert und interpretiert CAN-Nachrichten nach CANopen-Standard
// ===================================================================================
void decodeCANMessage(unsigned long rxId, uint8_t nodeId, uint16_t baseId, byte* buf, byte len) {
    // Bekannte Nachrichtentypen identifizieren und interpretieren
    if (rxId == 0x000) {
        Serial.print("  [NMT Broadcast]");
        decodeNMTCommand(buf, len);
    } else if (baseId == COB_ID_TPDO1) {
        Serial.printf("  [PDO1 von Node %d]", nodeId);
    } else if (baseId == COB_ID_TPDO2) {
        Serial.printf("  [PDO2 von Node %d]", nodeId);
    } else if (baseId == COB_ID_TPDO3) {
        Serial.printf("  [PDO3 von Node %d]", nodeId);
    } else if (baseId == COB_ID_TPDO4) {
        Serial.printf("  [PDO4 von Node %d]", nodeId);
    } else if (baseId == COB_ID_TSDO_BASE) {
        Serial.printf("  [SDO Response von Node %d]", nodeId);
        
        // SDO-Antwort interpretieren
        if (len >= 4) {
            byte cs = buf[0] >> 5; // Command Specifier
            if (cs == 4) {
                Serial.print(" (Abort)");
                if (len >= 8) {
                    uint32_t abortCode = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
                    Serial.printf(" Code: 0x%08X", abortCode);
                }
            } else if (cs == 3) {
                Serial.print(" (Upload)");
            } else if (cs == 1) {
                Serial.print(" (Download)");
            }
        }
    } else if (baseId == COB_ID_RSDO_BASE) {
        Serial.printf("  [SDO Request an Node %d]", nodeId);
    } else if (baseId == COB_ID_HB_BASE) {
        if (len > 0 && buf[0] == 0x00) {
            Serial.printf("  [Bootup von Node %d]", nodeId);
        } else if (len > 0) {
            Serial.printf("  [Heartbeat von Node %d, Status: %d]", nodeId, buf[0]);
            decodeNMTState(buf[0]);
        }
    } else if (baseId == COB_ID_EMCY_BASE) {
        Serial.printf("  [Emergency von Node %d]", nodeId);
        if (len >= 2) {
            uint16_t errorCode = buf[0] | (buf[1] << 8);
            Serial.printf(" Error: 0x%04X", errorCode);
        }
    } else if (rxId == 0x80) {
        Serial.print("  [SYNC]");
    } else if (rxId == 0x100) {
        Serial.print("  [TIME]");
    }
}

// ===================================================================================
// Funktion: decodeNMTState
// Beschreibung: Dekodiert den NMT-Zustand eines Knotens
// ===================================================================================
void decodeNMTState(byte state) {
    switch(state) {
        case 0:
            Serial.print(" (Initialisierung)");
            break;
        case 4:
            Serial.print(" (Stopped)");
            break;
        case 5:
            Serial.print(" (Operational)");
            break;
        case 127:
            Serial.print(" (Pre-Operational)");
            break;
        default:
            Serial.printf(" (Unbekannt: %d)", state);
            break;
    }
}

// ===================================================================================
// Funktion: decodeNMTCommand
// Beschreibung: Dekodiert NMT-Befehle
// ===================================================================================
void decodeNMTCommand(byte* buf, byte len) {
    if (len >= 2) {
        byte command = buf[0];
        byte targetNode = buf[1];
        
        switch(command) {
            case NMT_CMD_START_NODE:
                Serial.printf(" Start Node %d", targetNode);
                break;
            case NMT_CMD_STOP_NODE:
                Serial.printf(" Stop Node %d", targetNode);
                break;
            case NMT_CMD_ENTER_PREOP:
                Serial.printf(" Enter Pre-Operational Node %d", targetNode);
                break;
            case NMT_CMD_RESET_NODE:
                Serial.printf(" Reset Node %d", targetNode);
                break;
            case NMT_CMD_RESET_COMM:
                Serial.printf(" Reset Communication Node %d", targetNode);
                break;
            default:
                Serial.printf(" Unbekannter Befehl: 0x%02X für Node %d", command, targetNode);
                break;
        }
    }
}

// ===================================================================================
// Funktion: decodeSDOAbortCode
// Beschreibung: Hilfsfunktion zur Dekodierung von SDO-Abortcodes
// ===================================================================================
void decodeSDOAbortCode(uint32_t abortCode) {
    switch(abortCode) {
        case 0x05030000:
            Serial.println("[FEHLER] Toggle-Bit nicht alterniert");
            break;
        case 0x05040001:
            Serial.println("[FEHLER] Client/Server-Befehlsspezifizierer ungültig oder unbekannt");
            break;
        case 0x06010000:
            Serial.println("[FEHLER] Zugriff auf ein Objekt nicht unterstützt");
            break;
        case 0x06010001:
            Serial.println("[FEHLER] Schreibversuch auf ein Nur-Lese-Objekt");
            break;
        case 0x06010002:
            Serial.println("[FEHLER] Leseversuch auf ein Nur-Schreib-Objekt");
            break;
        case 0x06020000:
            Serial.println("[FEHLER] Objekt im Objektverzeichnis nicht vorhanden");
            break;
        case 0x06040041:
            Serial.println("[FEHLER] Objekt kann nicht auf diese PDO gemappt werden");
            break;
        case 0x06040042:
            Serial.println("[FEHLER] Die Anzahl und Länge der Objekte würden die PDO-Länge überschreiten");
            break;
        case 0x06040043:
            Serial.println("[FEHLER] Allgemeiner Parameter-Inkompatibilitätsgrund");
            break;
        case 0x06040047:
            Serial.println("[FEHLER] Allgemeiner interner Inkompatibilitätsfehler im Gerät");
            break;
        case 0x06060000:
            Serial.println("[FEHLER] Zugriff wegen Hardwarefehler fehlgeschlagen");
            break;
        case 0x06070010:
            Serial.println("[FEHLER] Datentyp stimmt nicht überein, Länge des Parameters stimmt nicht überein");
            break;
        case 0x06070012:
            Serial.println("[FEHLER] Datentyp stimmt nicht überein, Länge des Parameters zu groß");
            break;
        case 0x06070013:
            Serial.println("[FEHLER] Datentyp stimmt nicht überein, Länge des Parameters zu klein");
            break;
        case 0x06090011:
            Serial.println("[FEHLER] Unterobjekt nicht vorhanden");
            break;
        case 0x06090030:
            Serial.println("[FEHLER] Wert des Parameters außerhalb des erlaubten Bereichs");
            break;
        case 0x06090031:
            Serial.println("[FEHLER] Wert des Parameters zu groß");
            break;
        case 0x06090032:
            Serial.println("[FEHLER] Wert des Parameters zu klein");
            break;
        case 0x08000000:
            Serial.println("[FEHLER] Allgemeiner Fehler");
            break;
        case 0x08000020:
            Serial.println("[FEHLER] Datentransfer oder Speicherung nicht möglich");
            break;
        case 0x08000021:
            Serial.println("[FEHLER] Datentransfer oder Speicherung nicht möglich wegen lokalem Kontrollproblem");
            break;
        case 0x08000022:
            Serial.println("[FEHLER] Datentransfer oder Speicherung nicht möglich wegen aktuellem Gerätezustand");
            break;
        default:
            Serial.printf("[FEHLER] Unbekannter SDO-Abortcode: 0x%08X\n", abortCode);
            break;
    }
}

// ===================================================================================
// Funktion: changeBaudrate
// Beschreibung: Ändert die Baudrate eines Motors
// ===================================================================================
bool changeBaudrate(uint8_t nodeId, uint8_t baudrateIndex) {
    // Schritt 1: Schreiben aktivieren
    uint32_t unlockValue = 0x6E657277; // 'nerw' in ASCII-Hex
    
    Serial.printf("[DEBUG] Sende Schreibfreigabe (ASCII 'nerw'): 0x%08X\n", unlockValue);
    if (!canopen.writeSDO(nodeId, 0x2000, 0x01, unlockValue, 4)) {
        Serial.println("[FEHLER] Schreibfreigabe fehlgeschlagen!");
        return false;
    }
    
    Serial.println("[INFO] Schreibfreigabe erfolgreich.");
    delay(500); // Wartezeit für die Verarbeitung
    
    // Schritt 2: Neue Baudrate setzen
    Serial.printf("[DEBUG] Setze neue Baudrate mit Index: %d\n", baudrateIndex);
    if (!canopen.writeSDO(nodeId, 0x2000, 0x03, baudrateIndex, 4)) { 
        Serial.println("[FEHLER] Baudrate ändern fehlgeschlagen!");
        return false;
    }
    
    Serial.println("[INFO] Neue Baudrate gesetzt.");
    delay(500); // Wartezeit für die Verarbeitung
    
    // Erfolgreiche Ausführung
    Serial.println("[INFO] Baudrate erfolgreich geändert. Motor muss neu gestartet werden.");
    return true;
}

// ===================================================================================
// Funktion: getBaudrateIndex
// Beschreibung: Liefert den Baudrate-Index für eine gegebene Baudrate in kbps
// ===================================================================================
uint8_t getBaudrateIndex(int baudrateKbps) {
    switch(baudrateKbps) {
        case 1000: return 0; // 1M
        case 800: return 1;  // 800k
        case 500: return 2;  // 500k
        case 250: return 3;  // 250k
        case 125: return 4;  // 125k
        case 100: return 5;  // 100k
        case 50: return 6;   // 50k
        case 20: return 7;   // 20k
        case 10: return 8;   // 10k
        default: return 4;   // Standardwert 125k
    }
}

// ===================================================================================
// Funktion: convertBaudrateToCANSpeed
// Beschreibung: Konvertiert Baudrate in kbps zu CAN_SPEED-Enum für MCP_CAN
// ===================================================================================
uint8_t convertBaudrateToCANSpeed(int baudrateKbps) {
    switch(baudrateKbps) {
        case 1000: return CAN_1000KBPS;
        case 800: return CAN_500KBPS; // Modified: Using CAN_500KBPS instead of undefined CAN_800KBPS
        case 500: return CAN_500KBPS;
        case 250: return CAN_250KBPS;
        case 125: return CAN_125KBPS;
        case 100: return CAN_100KBPS;
        case 50: return CAN_50KBPS;
        case 20: return CAN_20KBPS;
        case 10: return CAN_10KBPS;
        default: return CAN_125KBPS; // Standardwert
    }
}

// ===================================================================================
// Funktion: updateESP32CANBaudrate
// Beschreibung: Aktualisiert die Baudrate des ESP32-CAN-Controllers
// ===================================================================================
bool updateESP32CANBaudrate(int newBaudrate) {
    // CAN.endSPI() existiert nicht - stattdessen setzen wir den CAN-Controller zurück
    
    // Mit neuer Baudrate rekonfigurieren
    if (CAN.begin(MCP_ANY, convertBaudrateToCANSpeed(newBaudrate), CAN_CLOCK) == CAN_OK) {
        CAN.setMode(MCP_NORMAL);
        Serial.printf("[INFO] ESP32 CAN-Bus erfolgreich auf %d kbps umkonfiguriert\n", newBaudrate);
        
        // OLED-Display aktualisieren
        showStatusMessage("Baudrate geändert", 
                          String("Neue Baudrate: " + String(newBaudrate) + " kbps").c_str());
        return true;
    } else {
        Serial.println("[FEHLER] ESP32 CAN-Bus Rekonfiguration fehlgeschlagen!");
        
        // Versuchen, zur alten Baudrate zurückzukehren
        if (CAN.begin(MCP_ANY, convertBaudrateToCANSpeed(currentBaudrate), CAN_CLOCK) == CAN_OK) {
            CAN.setMode(MCP_NORMAL);
            Serial.printf("[INFO] Zurück zur vorherigen Baudrate (%d kbps)\n", currentBaudrate);
        } else {
            Serial.println("[KRITISCH] Kann CAN-Bus nicht zurücksetzen! Neustart erforderlich!");
            systemError = true;
            lastErrorTime = millis();
        }
        return false;
    }
}

// ===================================================================================
// Funktion: changeCommunicationSettings
// Beschreibung: Umfassende Funktion zum Ändern der Kommunikationseinstellungen
// ===================================================================================
void changeCommunicationSettings(uint8_t targetNodeId, int newBaudrateKbps) {
    uint8_t baudrateIndex = getBaudrateIndex(newBaudrateKbps);
    
    Serial.println("=======================================");
    Serial.printf("Ändere Einstellungen von NodeID: %d, Baudrate: %d kbps\n", 
                  targetNodeId, currentBaudrate);
    Serial.printf("Zu NodeID: %d, Baudrate: %d kbps\n", 
                  targetNodeId, newBaudrateKbps);
    Serial.println("=======================================");
    
    bool baudrateChanged = false;
    bool nodeIdChanged = false;
    
    // Zuerst Baudrate ändern, wenn sie sich unterscheidet
    if (newBaudrateKbps != currentBaudrate) {
        if (changeBaudrate(targetNodeId, baudrateIndex)) {
            // ESP32-Baudrate aktualisieren
            if (updateESP32CANBaudrate(newBaudrateKbps)) {
                baudrateChanged = true;
                currentBaudrate = newBaudrateKbps;
                Serial.println("[ERFOLG] Baudratenänderung abgeschlossen");
                
                // Einstellungen speichern
                saveSettings();
            }
        }
    }
    
    // Jetzt ggf. Node-ID ändern, wenn die Ziel-ID nicht der aktuellen ID entspricht
    // und die angegebene Node ID gleich der aktuell eingestellten ist
    if (targetNodeId == currentNodeId && targetNodeId != newBaudrateKbps) {
        Serial.printf("[INFO] Ändere Node-ID von %d auf %d...\n", targetNodeId, newBaudrateKbps);
        if (canopen.changeNodeId(targetNodeId, newBaudrateKbps, true)) {
            nodeIdChanged = true;
            currentNodeId = newBaudrateKbps;
            Serial.println("[ERFOLG] Node-ID-Änderung abgeschlossen");
            
            // Einstellungen speichern
            saveSettings();
        }
    }
    
    Serial.println("--------------------------------------");
    Serial.printf("Aktuelle Einstellungen: NodeID: %d, Baudrate: %d kbps\n", 
                  currentNodeId, currentBaudrate);
    Serial.println("=======================================");
    
    // Zusammenfassende Meldung auf dem Display anzeigen
    String statusMessage = "";
    if (baudrateChanged && nodeIdChanged) {
        statusMessage = "Baudrate & NodeID geändert";
    } else if (baudrateChanged) {
        statusMessage = "Baudrate geändert";
    } else if (nodeIdChanged) {
        statusMessage = "NodeID geändert";
    } else {
        statusMessage = "Keine Änderung";
    }
    
    // Meldung anzeigen
    String detailMessage = String("NodeID: ") + String(currentNodeId) + 
                           String("\nBaudrate: ") + String(currentBaudrate) + String(" kbps");
    showStatusMessage(statusMessage.c_str(), detailMessage.c_str());
    
    // Wichtiger Hinweis: Der Motor muss neu gestartet werden, damit die Änderungen wirksam werden
    Serial.println("HINWEIS: Schalten Sie den Motor aus und wieder ein, damit die Änderungen wirksam werden!");
}
// ===================================================================================
// Funktion: autoBaudrateDetection
// Beschreibung: Automatische Baudratenerkennung durch Testen verschiedener Baudraten
// ===================================================================================
bool autoBaudrateDetection() {
    // Zu testende Baudraten
    int baudrates[] = {125, 250, 500, 1000, 100, 50, 20, 10, 800};
    int numBaudrates = sizeof(baudrates) / sizeof(baudrates[0]);
    
    Serial.println("[INFO] Starte automatische Baudratenerkennung...");
    showStatusMessage("Baudratenerkennung", "Suche nach Geräten...");
    
    for (int i = 0; i < numBaudrates; i++) {
        Serial.printf("[INFO] Teste Baudrate: %d kbps\n", baudrates[i]);
        
        // ESP32-CAN-Bus auf diese Baudrate umkonfigurieren
        if (!updateESP32CANBaudrate(baudrates[i])) {
            Serial.printf("[FEHLER] Konnte ESP32 nicht auf %d kbps umkonfigurieren\n", baudrates[i]);
            continue;
        }
        
        // Scan durchführen
        int foundNodes = 0;
        
        // Suche nach bekannten Node-IDs im Netzwerk
        for (int id = 1; id <= 127; id++) {
            if (id % 16 == 0) {
                Serial.printf("[INFO] Prüfe Nodes %d bis %d...\n", id-15, id);
            }
            
            // SDO-Leseanfrage an Fehlerregister (0x1001:00) vorbereiten
            byte sdo[8] = { 0x40, 0x01, 0x10, 0x00, 0, 0, 0, 0 };
            
            // Anfrage senden
            CAN.sendMsgBuf(COB_ID_RSDO_BASE + id, 0, 8, sdo);
            
            // Auf Antwort warten mit kurzem Timeout
            unsigned long start = millis();
            while (millis() - start < 50) { // 50ms Timeout
                if (!digitalRead(CAN_INT)) {
                    unsigned long rxId;
                    byte len;
                    byte buf[8];
                    
                    // Antwort lesen
                    CAN.readMsgBuf(&rxId, &len, buf);
                    
                    // Prüfen, ob es sich um eine Antwort auf unsere SDO-Anfrage handelt
                    if (rxId == (COB_ID_TSDO_BASE + id)) {
                        foundNodes++;
                        Serial.printf("[INFO] Node %d gefunden bei %d kbps!\n", id, baudrates[i]);
                        break;
                    }
                }
            }
            
            // Bei mehr als 3 gefundenen Knoten können wir die Suche abbrechen
            if (foundNodes >= 3) {
                break;
            }
        }
        
        // Wenn Nodes gefunden wurden, haben wir die richtige Baudrate
        if (foundNodes > 0) {
            currentBaudrate = baudrates[i];
            Serial.printf("[ERFOLG] Baudrate erkannt: %d kbps mit %d gefundenen Nodes\n", currentBaudrate, foundNodes);
            
            // Einstellungen speichern
            saveSettings();
            
            showStatusMessage("Baudrate erkannt", 
                             String("Baudrate: " + String(currentBaudrate) + " kbps\n" + 
                                   String(foundNodes) + " Geräte gefunden").c_str());
            
            return true;
        }
    }
    
    // Keine passende Baudrate gefunden, zurück zur Standard-Baudrate
    Serial.println("[FEHLER] Keine Baudrate mit aktiven Geräten gefunden");
    updateESP32CANBaudrate(125); // Zurück zur Standard-Baudrate
    currentBaudrate = 125;
    
    showStatusMessage("Fehler", "Keine Baudrate\nmit Geräten gefunden", true);
    
    return false;
}
