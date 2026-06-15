#include <SPI.h>
#include <mcp_can.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>  // Für OLED (wird weiterhin für Rückwärtskompatibilität benötigt)
#include "OLEDMenu.h"
#include <Preferences.h>
#include <Arduino.h>
#include <TFT_eSPI.h>           // Für Waveshare Display
#include "CANopen.h"
#include "CANopenClass.h"
#include "CANInterface.h"
#include "DisplayInterface.h"   // Neue abstrakte Display-Schnittstelle
#include "OLEDDisplay.h"        // Konkrete Implementierung für OLED
#include "WaveshareDisplay.h"   // Konkrete Implementierung für Waveshare
#include "SystemProfiles.h"

// Versionsinfo
#define VERSION       "V005_A"

// Definierte Konstanten
#define OLED_ADDR     0x3C
#define CAN_CS        5
#define CAN_INT       4
#define CAN_CLOCK     MCP_8MHZ
constexpr int DISPLAY_OLED_WIDTH = 128;
constexpr int DISPLAY_OLED_HEIGHT = 64;
constexpr int DISPLAY_TFT_WIDTH = 800;
constexpr int DISPLAY_TFT_HEIGHT = 480;
constexpr int DISPLAY_OLED_ERROR_BAR_HEIGHT = 10;
constexpr int DISPLAY_TFT_ERROR_BAR_HEIGHT = 30;

// ===================================================================================
// Globale Objekte und Variablen (korrigiert)
// Entfernt doppelte Deklarationen
// ===================================================================================

// Globale Variablen für CAN und Display
uint8_t currentNodeId = 127;  // Standardwert
int currentBaudrate = 125;    // Standardwert in kbps
uint8_t currentCANTransceiverType = CAN_CONTROLLER_MCP2515;
uint8_t currentDisplayType = DISPLAY_CONTROLLER_OLED_SSD1306;
uint8_t scanStart = 1;        // Standardwert
uint8_t scanEnd = 127;         // Standardwert
bool scanning = false;
bool liveMonitor = false;
bool systemError = false;
bool autoBaudrateRequest = false;
unsigned long lastErrorTime = 0;
extern ControlSource activeSource;
extern unsigned long lastActivityTime;

// Globale Variablen für Filter
bool filterEnabled = false;
uint32_t filterIdMin = 0;
uint32_t filterIdMax = 0xFFFFFFFF;
uint8_t filterNodeId = 0;
bool filterNodeEnabled = false;
uint8_t filterType = 0; // 0=All, 1=PDO, 2=SDO, 3=EMCY, 4=NMT, 5=Heartbeat

// Globale Objekte
MCP_CAN CAN(CAN_CS);
Adafruit_SSD1306 display(DISPLAY_OLED_WIDTH, DISPLAY_OLED_HEIGHT, &Wire, -1);  // Alte Implementierung Standard
CANopen canopen(CAN_INT);
Preferences preferences;

// Interface-Objekte (neue Implementierung)
CANInterface* canInterface = nullptr;
DisplayInterface* displayInterface = nullptr;

// Globales TFT-Objekt für Unterstützung alter Code-Teile
// HINWEIS: Dies ist nur ein Übergangsobjekt und sollte durch das Interface ersetzt werden
// Macht noch Probleme wird in einer späteren Version angegangen
TFT_eSPI tft = TFT_eSPI();

// Funktionsdeklarationen
void showMessage(const char* msg);
void showStatusMessage(const char* title, const char* message, bool isError = false);
void scanNodes(int startID, int endID);
bool tryScanNode(int id, bool extendedScan);
void changeNodeId(uint8_t from, uint8_t to);
extern void handleSerialCommands();
void printHelpMenu();
void handleLocalBaudrateCommand(String command);
void handleBaudrateCommand(String command);
void handleChangeCommand(String command);
void handleRangeCommand(String command);
bool isValidBaudrate(int baudrate);
void printCurrentSettings();
void systemReset();
bool autoBaudrateDetection();
bool scanSingleNode(int id, int baudrate, int maxAttempts, int timeoutMs);
extern void processCANMessage();
extern void decodeCANMessage(unsigned long rxId, uint8_t nodeId, uint16_t baseId, byte* buf, byte len);
extern void decodeNMTState(byte state);
extern void decodeNMTCommand(byte* buf, byte len);
extern void decodeSDOAbortCode(uint32_t abortCode);
extern void displayCANMessage(uint32_t canId, uint8_t* data, uint8_t length);
bool changeBaudrate(uint8_t nodeId, uint8_t baudrateIndex);
bool updateESP32CANBaudrate(int newBaudrate);
uint8_t getBaudrateIndex(int baudrateKbps);
void handleTestNodeCommand(String command);
bool testSingleNode(int nodeId, int maxAttempts, int timeoutMs);
const char* getAppVersion();
int getDisplayWidth();
int getDisplayHeight();
int getDisplayErrorBarHeight();

// Funktionsdeklaration
uint8_t convertBaudrateToCANSpeed(int baudrateKbps);
void changeCommunicationSettings(uint8_t newNodeId, int newBaudrateKbps);

// externe Funktionen
extern void menuLoop();
extern void processCANScanning();
extern void processAutoBaudrate();
extern void processCANMessage();

// ===================================================================================
// Funktion: saveSettings (aktualisiert)
// Beschreibung: Speichert alle Einstellungen in den nicht-flüchtigen Speicher
// ===================================================================================
void saveSettings() {
    preferences.begin("canopenscan", false);
    preferences.putUChar("nodeId", currentNodeId);
    preferences.putUInt("baudrate", currentBaudrate);
    preferences.putUChar("canTransceiver", currentCANTransceiverType);
    preferences.putUChar("displayType", currentDisplayType);
    preferences.putUChar("scanStart", scanStart);
    preferences.putUChar("scanEnd", scanEnd);
    preferences.end();
    
    Serial.println("[INFO] Einstellungen gespeichert");
}

// ===================================================================================
// Funktion: loadSettings (aktualisiert mit Absturzsicherung)
// Beschreibung: Lädt alle Einstellungen aus dem nicht-flüchtigen Speicher und
//               implementiert eine Sicherheitsprüfung für wiederholte Abstürze
// ===================================================================================
void loadSettings() {
    preferences.begin("canopenscan", true); // Nur Lesen
    
    // Grundeinstellungen laden
    currentNodeId = preferences.getUChar("nodeId", 127); // Standardwert 127
    currentBaudrate = preferences.getUInt("baudrate", 125); // Standardwert 125kbps
    scanStart = preferences.getUChar("scanStart", 1); // Standardwert 1
    scanEnd = preferences.getUChar("scanEnd", 10); // Standardwert 10
    
    // Crashzähler und Timestamp lesen
    uint8_t crashCount = preferences.getUChar("crashCount", 0);
    unsigned long lastCrashTime = preferences.getULong("lastCrashTime", 0);
    
    // Komponententypen laden
    currentCANTransceiverType = preferences.getUChar("canTransceiver", CAN_CONTROLLER_MCP2515);
    currentDisplayType = preferences.getUChar("displayType", DISPLAY_CONTROLLER_OLED_SSD1306);
    
    // Prüfe die Kompatibilität der geladenen Konfiguration
    if (!isValidComponentCombination(currentDisplayType, currentCANTransceiverType)) {
        // Inkompatible Kombination - setze auf Standardprofil zurück
        Serial.println("[WARNUNG] Inkompatible Komponentenkombination in den Einstellungen gefunden");
        getProfileComponents(SYSTEM_PROFILE_OLED_MCP2515, currentDisplayType, currentCANTransceiverType);
        Serial.printf("[INFO] Zurückgesetzt auf Standardprofil: %s\n", getProfileName(SYSTEM_PROFILE_OLED_MCP2515));
    }
    
    preferences.end();
    
    // Aktuelle Zeit (seit Bootzeit) oder direkt einen hohen Wert für den ersten Start
    unsigned long currentTime = 60000; // Beim ersten Start simulieren wir "lange her"
    
    // Wenn der letzte Crash vor weniger als 10 Sekunden war, erhöhe Zähler
    if (currentTime - lastCrashTime < 10000 && crashCount < 255) {
        crashCount++;
        Serial.printf("[INFO] Schneller Neustart erkannt (%d/3)\n", crashCount);
    } else {
        // Zurücksetzen, wenn es länger her ist
        crashCount = 0;
    }
    
    // Sicherer Modus, wenn zu viele Crashs
    if (crashCount >= 3) {
        // In den sicheren Modus wechseln: Kein Display
        Serial.println("[WARNUNG] Zu viele schnelle Neustarts erkannt, starte im sicheren Modus ohne Display");
        currentDisplayType = DISPLAY_CONTROLLER_NONE;
        crashCount = 0; // Zähler zurücksetzen
    }
    
    // Aktualisierte Werte sofort zurückschreiben 
    preferences.begin("canopenscan", false);
    preferences.putUChar("crashCount", crashCount);
    preferences.putULong("lastCrashTime", currentTime);
    
    // Auch die möglicherweise korrigierten Komponententypen speichern
    preferences.putUChar("canTransceiver", currentCANTransceiverType);
    preferences.putUChar("displayType", currentDisplayType);
    preferences.end();
    
    Serial.println("[INFO] Einstellungen geladen");
}

// ===================================================================================
// Funktion: isValidComponentCombination
// Beschreibung: Prüft, ob die angegebene Kombination aus Display und CAN-Controller gültig ist
// ===================================================================================
bool isValidComponentCombination(uint8_t displayType, uint8_t canType) {
    // Kein Display ist immer gültig (sicherer Modus)
    if (displayType == DISPLAY_CONTROLLER_NONE) {
        return true;
    }
    
    // Gültige Kombination 1: OLED + MCP2515
    if (displayType == DISPLAY_CONTROLLER_OLED_SSD1306 && 
        canType == CAN_CONTROLLER_MCP2515) {
        return true;
    }
    
    // Gültige Kombination 2: TFT + TJA1051
    if (displayType == DISPLAY_CONTROLLER_WAVESHARE_ESP32S3_TOUCH_LCD && 
        canType == CAN_CONTROLLER_TJA1051) {
        return true;
    }
    
    // Alle anderen Kombinationen sind ungültig
    return false;
}
// ===================================================================================
// Funktion: setup (aktualisiert)
// Beschreibung: Initialisiert das System, lädt Einstellungen und initialisiert CAN und Display
// ===================================================================================
void setup() {
    Serial.begin(115200);
    Wire.begin();
    
    // Gespeicherte Einstellungen laden
    loadSettings();
    
    // Display initialisieren
    if (initializeDisplay()) {
        Serial.println("[INFO] Display bereit");
    } else {
        Serial.println("[WARNUNG] Display nicht verfügbar");
        currentDisplayType = DISPLAY_CONTROLLER_NONE;
    }
    
    // Willkommensbildschirm anzeigen
    showStatusMessage("CANopen Scanner", VERSION "\nSystem wird initialisiert...");
    
    // CAN-Interface basierend auf dem gespeicherten Transceiver-Typ initialisieren
    if (initializeCANInterface()) {
        Serial.println("[INFO] CAN-Interface bereit");
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
    
    // CANopen-Klasse mit dem Interface verknüpfen
    if (canInterface != nullptr) {
        canopen.setCANInterface(canInterface);
    }

    pinMode(BUTTON_UP, INPUT_PULLUP);
    pinMode(BUTTON_DOWN, INPUT_PULLUP);
    pinMode(BUTTON_SELECT, INPUT_PULLUP);
    // Willkommensnachricht und Hilfe anzeigen
    delay(1000);
    printHelpMenu();
    menuInit();
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
    
    // Menü-Verarbeitung (Buttons und Timeout-Handling)
    menuLoop();
    
    // Serielle Befehle verarbeiten
    handleSerialCommands();

    // Node-Scan durchführen, wenn angefordert
    if (scanning) {
        processCANScanning();
        // Hinweis: Das Zurücksetzen von scanning erfolgt in processCANScanning()
    }

    // Automatische Baudratenerkennung, wenn angefordert
    if (autoBaudrateRequest) {
        processAutoBaudrate();
        // Hinweis: Das Zurücksetzen von autoBaudrateRequest erfolgt in processAutoBaudrate()
    }

    // Live Monitor für CAN-Frames
    if (liveMonitor && canInterface && canInterface->messageAvailable()) {
        processCANMessage();
    }
}



// Funktion zur Initialisierung des CAN-Interfaces basierend auf dem aktuellen Transceiver-Typ
bool initializeCANInterface() {
    // Alte Instanz bereinigen, falls vorhanden
    if (canInterface != nullptr) {
        delete canInterface;
        canInterface = nullptr;
    }
    
    // Neue Instanz erstellen
    canInterface = CANInterface::createInstance(currentCANTransceiverType);
    
    if (canInterface == nullptr) {
        Serial.println("[FEHLER] Ungültiger Transceiver-Typ");
        showStatusMessage("FEHLER", "Ungültiger Transceiver-Typ!", true);
        return false;
    }
    
    // Interface mit aktueller Baudrate initialisieren
    if (canInterface->begin(currentBaudrate * 1000)) {  // Umrechnung von kbps in bps
        Serial.printf("[INFO] CAN-Interface (%s) erfolgreich initialisiert bei %d kbps\n", 
                     getTransceiverTypeName(currentCANTransceiverType), 
                     currentBaudrate);
        showStatusMessage("CAN initialisiert", 
                         String("Transceiver: " + String(getTransceiverTypeName(currentCANTransceiverType)) + 
                               "\nBaudrate: " + String(currentBaudrate) + " kbps").c_str());
        return true;
    } else {
        Serial.println("[FEHLER] CAN-Interface Initialisierung fehlgeschlagen");
        showStatusMessage("FEHLER", "CAN-Interface\nInitialisierung fehlgeschlagen!", true);
        return false;
    }
}

// Hilfsfunktion, um den Namen des Transceiver-Typs als String zu erhalten
const char* getTransceiverTypeName(uint8_t transceiverType) {
    switch (transceiverType) {
        case CAN_CONTROLLER_MCP2515:
            return "MCP2515";
        case CAN_CONTROLLER_ESP32CAN:
            return "ESP32CAN";
        case CAN_CONTROLLER_TJA1051:
            return "TJA1051";
        case DISPLAY_CONTROLLER_OLED_SSD1306:
            return "OLED SSD1306";
        case DISPLAY_CONTROLLER_WAVESHARE_ESP32S3_TOUCH_LCD:
            return "Waveshare ESP32-S3 Touch LCD";
        default:
            return "Unbekannt";
    }
}

const char* getAppVersion() {
    return VERSION;
}

int getDisplayWidth() {
    if (currentDisplayType == DISPLAY_CONTROLLER_WAVESHARE_ESP32S3_TOUCH_LCD) {
        return DISPLAY_TFT_WIDTH;
    }
    return DISPLAY_OLED_WIDTH;
}

int getDisplayHeight() {
    if (currentDisplayType == DISPLAY_CONTROLLER_WAVESHARE_ESP32S3_TOUCH_LCD) {
        return DISPLAY_TFT_HEIGHT;
    }
    return DISPLAY_OLED_HEIGHT;
}

int getDisplayErrorBarHeight() {
    if (currentDisplayType == DISPLAY_CONTROLLER_WAVESHARE_ESP32S3_TOUCH_LCD) {
        return DISPLAY_TFT_ERROR_BAR_HEIGHT;
    }
    return DISPLAY_OLED_ERROR_BAR_HEIGHT;
}


// ===================================================================================
// Funktion: initializeDisplay (neue Version mit DisplayInterface)
// Beschreibung: Initialisiert das ausgewählte Display über das Interface
// ===================================================================================
bool initializeDisplay() {
    // Vorsichtsmaßnahme: Freigeben falls bereits initialisiert
    if (displayInterface != nullptr) {
        delete displayInterface;
        displayInterface = nullptr;
    }
    
    // Wenn kein Display verwendet werden soll (sicherer Modus)
    if (currentDisplayType == DISPLAY_CONTROLLER_NONE) {
        Serial.println("[INFO] Kein Display konfiguriert");
        return true;
    }
    
    // Vorsichtsmaßnahme: Falls der Display-Typ ungültig ist, auf NONE zurücksetzen
    if (currentDisplayType != DISPLAY_CONTROLLER_OLED_SSD1306 && 
        currentDisplayType != DISPLAY_CONTROLLER_WAVESHARE_ESP32S3_TOUCH_LCD) {
        Serial.println("[WARNUNG] Ungültiger Display-Typ, setze auf 'Kein Display'");
        currentDisplayType = DISPLAY_CONTROLLER_NONE;
        saveSettings();
        return true;
    }

    // Neues Display-Interface mit Fehlerbehandlung erstellen
    displayInterface = DisplayInterface::createInstance(currentDisplayType);
    
    if (displayInterface == nullptr) {
        Serial.println("[FEHLER] Konnte Display-Interface nicht erstellen!");
        currentDisplayType = DISPLAY_CONTROLLER_NONE;
        saveSettings();
        return false;
    }
    
    // Fehlerbehandlung bei der Initialisierung
    if (!displayInterface->begin()) {
        Serial.println("[FEHLER] Display-Initialisierung fehlgeschlagen!");
        delete displayInterface;
        displayInterface = nullptr;
        currentDisplayType = DISPLAY_CONTROLLER_NONE;
        saveSettings();
        return false;
    }
    
    // Test-Nachricht anzeigen
    displayInterface->clear();
    displayInterface->setCursor(0, 0);
    displayInterface->println("CANopen Scanner " VERSION);
    displayInterface->println("------------------");
    displayInterface->printf("Display: %s", getTransceiverTypeName(currentDisplayType));
    displayInterface->display();
    
    Serial.printf("[INFO] Display %s erfolgreich initialisiert\n", 
                 getTransceiverTypeName(currentDisplayType));
    return true;
}
// ===================================================================================
// Funktion: handleTransceiverCommand (aktualisiert)
// Beschreibung: Verarbeitet Befehle zum Ändern des Transceiver-Typs oder Display-Typs
// ===================================================================================
void handleTransceiverCommand(String command) {
    // Die Funktion erwartet ein Command-Format wie: "can 1" oder "display 11"
    
    int spacePos = command.indexOf(' ');
    
    // Wenn kein Leerzeichen gefunden wurde oder der Befehl leer ist
    if (spacePos <= 0 || command.length() == 0) {
        Serial.println("[FEHLER] Falsche Syntax. Verwendung:");
        Serial.println("  transceiver can <typ>   - Wählt CAN-Controller");
        Serial.println("  transceiver display <typ> - Wählt Display");
        return;
    }
    
    // Unterscheidung zwischen CAN und Display
    String subCommand = command.substring(0, spacePos);
    int newType = command.substring(spacePos + 1).toInt();
    
    if (subCommand == "can") {
        // CAN-Controller-Validierung
        bool isValidCANType = 
            (newType == CAN_CONTROLLER_MCP2515) ||
            (newType == CAN_CONTROLLER_ESP32CAN) ||
            (newType == CAN_CONTROLLER_TJA1051);
        
        if (!isValidCANType) {
            Serial.println("[FEHLER] Ungültiger CAN-Controller-Typ!");
            Serial.println("Gültige Werte sind:");
            Serial.println("  1 = MCP2515");
            Serial.println("  2 = ESP32CAN");
            Serial.println("  3 = TJA1051");
            return;
        }
        
        if (newType == currentCANTransceiverType) {
            Serial.printf("[INFO] CAN-Controller bereits %s (%d)\n", 
                         getTransceiverTypeName(newType), newType);
            return;
        }
        
        currentCANTransceiverType = newType;
        
        if (initializeCANInterface()) {
            Serial.println("[INFO] CAN-Controller erfolgreich gewechselt");
            saveSettings();
        } else {
            Serial.println("[FEHLER] Fehler beim Wechseln des CAN-Controllers");
        }
    }
    else if (subCommand == "display") {
    // Display-Controller-Validierung
    bool isValidDisplayType = 
        (newType == DISPLAY_CONTROLLER_NONE) ||
        (newType == DISPLAY_CONTROLLER_OLED_SSD1306) ||
        (newType == DISPLAY_CONTROLLER_WAVESHARE_ESP32S3_TOUCH_LCD);
    
    if (!isValidDisplayType) {
        Serial.println("[FEHLER] Ungültiger Display-Typ!");
        Serial.println("Gültige Werte sind:");
        Serial.println("  0 = Kein Display");
        Serial.println("  10 = OLED SSD1306");
        Serial.println("  11 = Waveshare ESP32-S3 Touch LCD");
        return;
    }
    
    if (newType == currentDisplayType) {
        Serial.printf("[INFO] Display bereits %s (%d)\n", 
                     getTransceiverTypeName(newType), newType);
        return;
    }
    
    // Neue Konfiguration speichern
    currentDisplayType = newType;
    
    // Setze Crash-Zähler zurück für einen "sauberen" Neustart
    preferences.begin("canopenscan", false);
    preferences.putUChar("crashCount", 0);
    preferences.putULong("lastCrashTime", 0);
    preferences.end();
    
    // Einstellungen speichern
    saveSettings();
    
    // Informiere den Benutzer
    Serial.printf("[INFO] Display-Typ auf %s (%d) geändert.\n", 
                  getTransceiverTypeName(newType), newType);
    Serial.println("[INFO] Führe Neustart durch, um das neue Display zu aktivieren...");
    
    delay(1000); // Gib dem Serial-Buffer Zeit zum Leeren
    
    // System neustarten
    ESP.restart();
  }
}
// ===================================================================================
// Funktion: showMessage (aktualisiert)
// Beschreibung: Zeigt eine Nachricht auf dem Display an
// ===================================================================================
void showMessage(const char* msg) {
    if (displayInterface == nullptr) {
        // Wenn kein Display konfiguriert ist, nur auf Serial ausgeben
        Serial.println(msg);
        return;
    }
    
    displayInterface->clear();
    displayInterface->setCursor(0, 0);
    displayInterface->setTextSize(1);
    displayInterface->println(msg);
    displayInterface->display();
}

// ===================================================================================
// Funktion: showStatusMessage (aktualisiert)
// Beschreibung: Zeigt eine Statusmeldung mit Titel und Details auf dem Display an
// ===================================================================================
void showStatusMessage(const char* title, const char* message, bool isError) {
    if (displayInterface == nullptr) {
        // Wenn kein Display konfiguriert ist, nur auf Serial ausgeben
        Serial.print(title);
        Serial.print(": ");
        Serial.println(message);
        if (isError) {
            Serial.println("[FEHLER]");
        }
        return;
    }
    
    const int displayWidth = getDisplayWidth();
    const int displayHeight = getDisplayHeight();

    displayInterface->clear();
    displayInterface->setCursor(0, 0);
    
    // Titel hervorheben
    displayInterface->setTextSize(1);
    displayInterface->println(title);
    
    // Trennlinie
    if (currentDisplayType == DISPLAY_CONTROLLER_OLED_SSD1306) {
        displayInterface->drawLine(0, 10, displayWidth, 10, 1); // Weiß für OLED
    } else if (currentDisplayType == DISPLAY_CONTROLLER_WAVESHARE_ESP32S3_TOUCH_LCD) {
        displayInterface->drawLine(0, 20, displayWidth, 20, TFT_WHITE); // Weiß für TFT
    }
    
    // Nachricht anzeigen
    if (currentDisplayType == DISPLAY_CONTROLLER_OLED_SSD1306) {
        displayInterface->setCursor(0, 14);
    } else if (currentDisplayType == DISPLAY_CONTROLLER_WAVESHARE_ESP32S3_TOUCH_LCD) {
        displayInterface->setCursor(0, 30);
    }
    displayInterface->println(message);
    
    // Bei Fehler einen visuellen Indikator anzeigen
    if (isError) {
        const int errorBarHeight = getDisplayErrorBarHeight();
        const int errorBarY = displayHeight - errorBarHeight;
        if (currentDisplayType == DISPLAY_CONTROLLER_OLED_SSD1306) {
            // Für OLED-Display
            displayInterface->drawRect(0, 0, displayWidth, displayHeight, 1);
            displayInterface->fillRect(0, errorBarY, displayWidth, errorBarHeight, 1);
            displayInterface->setTextColor(0); // Schwarz für OLED-Inversmodus
            displayInterface->setCursor(5, errorBarY + 1);
            displayInterface->print("FEHLER!");
            displayInterface->setTextColor(1); // Zurück zu Weiß
        } else if (currentDisplayType == DISPLAY_CONTROLLER_WAVESHARE_ESP32S3_TOUCH_LCD) {
            // Für Waveshare-Display
            displayInterface->drawRect(0, 0, displayWidth, displayHeight, TFT_RED);
            displayInterface->fillRect(0, errorBarY, displayWidth, errorBarHeight, TFT_RED);
            displayInterface->setTextColor(TFT_WHITE);
            displayInterface->setCursor(20, errorBarY + 5);
            displayInterface->print("FEHLER!");
        }
    }
    
    displayInterface->display();
}

// Implementiere diese Funktion in deiner .ino
int convertBaudrateToRealBaudrate(int index) {
    int baudrates[] = {10, 20, 50, 100, 125, 250, 500, 800, 1000};
    if (index >= 0 && index < 9) 
        return baudrates[index];
    return 125; // Standardwert
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
// Funktion: scanNodes (aktualisiert mit DisplayInterface)
// Beschreibung: Scannt einen Bereich von CANopen-Node-IDs und zeigt aktive Knoten an
// ===================================================================================
void scanNodes(int startID, int endID) {
    // Verwende das neue DisplayInterface statt des alten display-Objekts
    if (displayInterface != nullptr) {
        displayInterface->clear();
        displayInterface->setCursor(0, 0);
        displayInterface->println("Scan...\n");
        displayInterface->display();
    } else {
        // Fallback auf das alte display-Objekt für Kompatibilität
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("Scan...\n");
        display.display();
    }
    
    Serial.printf("[INFO] Starte Node-Scan von %d bis %d bei %d kbps\n", startID, endID, currentBaudrate);
    
    // Zähler für gefundene Nodes
    int foundNodes = 0;
    
    // Bereich prüfen und begrenzen
    if (startID < 1) startID = 1;
    if (endID > 127) endID = 127;
    
    // Länger warten vor dem Start des Scans, damit der CAN-Bus bereit ist
    delay(200);
    
    // Live-Monitor-Modus während des Scans aktivieren
    bool wasMonitorActive = liveMonitor;
    liveMonitor = true;
    
    // Erst die bekannten Node-IDs scannen (insbesondere Node 10)
    int knownNodes[] = {10, 1, 2, 3, 4, 5};
    int knownNodesCount = sizeof(knownNodes) / sizeof(knownNodes[0]);
    
    for (int i = 0; i < knownNodesCount; i++) {
        int id = knownNodes[i];
        
        // Nur scannen, wenn die ID im angegebenen Bereich liegt
        if (id < startID || id > endID) {
            continue;
        }
        
        Serial.printf("[SCAN] Scanne bekannte Node-ID %d mit erweitertem Timeout...\n", id);
        
        // Verschiedene SDO-Anfragen für diesen Node probieren
        if (tryScanNode(id, true)) {
            foundNodes++;
        }
    }
    
    // Jetzt den Rest des Bereichs scannen
    for (int id = startID; id <= endID; id++) {
        // Bekannte Nodes überspringen, die bereits gescannt wurden
        bool isKnown = false;
        for (int i = 0; i < knownNodesCount; i++) {
            if (id == knownNodes[i]) {
                isKnown = true;
                break;
            }
        }
        
        if (isKnown) {
            continue;
        }
        
        // Normale Nodes mit Standardparametern scannen
        if (tryScanNode(id, false)) {
            foundNodes++;
        }
        
        // Kurze Pause zwischen den Nodes
        delay(20);
    }
    
    // Live-Monitor-Modus zurücksetzen
    liveMonitor = wasMonitorActive;
    
    // Zusammenfassung anzeigen mit dem neuen DisplayInterface
    if (displayInterface != nullptr) {
        displayInterface->clear();
        displayInterface->setCursor(0, 0);
        displayInterface->printf("Scan abgeschlossen\n");
        displayInterface->printf("%d Nodes gefunden\n", foundNodes);
        displayInterface->printf("Bereich: %d-%d\n", startID, endID);
        displayInterface->printf("Baudrate: %d kbps", currentBaudrate);
        displayInterface->display();
    } else {
        // Fallback auf das alte display-Objekt für Kompatibilität
        display.clearDisplay();
        display.setCursor(0, 0);
        display.printf("Scan abgeschlossen\n");
        display.printf("%d Nodes gefunden\n", foundNodes);
        display.printf("Bereich: %d-%d\n", startID, endID);
        display.printf("Baudrate: %d kbps", currentBaudrate);
        display.display();
    }
    
    Serial.printf("[INFO] Scan abgeschlossen. %d Nodes gefunden bei %d kbps.\n", foundNodes, currentBaudrate);
}
// ===================================================================================
// Funktion: tryScanNode (aktualisiert für DisplayInterface)
// Beschreibung: Hilfsfunktion zum Scannen eines einzelnen Nodes mit verschiedenen Methoden
// ===================================================================================
bool tryScanNode(int id, bool extendedScan) {
    bool responded = false;
    bool errorResponse = false;
    
    // Anzahl der Versuche und Timeout-Zeit
    int maxAttempts = extendedScan ? 3 : 1;
    int timeoutMs = extendedScan ? 300 : 200;
    
    Serial.printf("[SCAN] Frage Node %d ab... (%d Versuche, %dms Timeout)\n", id, maxAttempts, timeoutMs);
    
    // Mehrere Versuche mit verschiedenen Objekten
    for (int attempt = 0; attempt < maxAttempts && !responded; attempt++) {
        // Verschiedene SDO-Objekte probieren
        uint16_t objectIndices[] = {0x1001, 0x1000, 0x1018};
        
        for (int objIdx = 0; objIdx < 3 && !responded; objIdx++) {
            // SDO-Leseanfrage vorbereiten (Fehlerstatus, Gerätetyp oder Identität)
            uint8_t sdo[8] = { 0x40, (uint8_t)(objectIndices[objIdx] & 0xFF), (uint8_t)(objectIndices[objIdx] >> 8), 0x00, 0, 0, 0, 0 };
            
            // Anfrage senden mit Fehlerbehandlung
            if (!sendCanMessage(0x600 + id, 0, 8, sdo)) {
                Serial.printf("[FEHLER] Konnte keine Anfrage an Node %d senden\n", id);
                delay(30); // Kurze Pause vor dem nächsten Versuch
                continue;
            }
            
            // Auf Antwort warten mit Timeout
            unsigned long start = millis();
            
            // Erst warten, ob ein Heartbeat oder Emergency auftaucht (passiv)
            while (millis() - start < 50) {
                if (canInterface->messageAvailable()) {
                    uint32_t rxId;
                    uint8_t ext = 0;
                    uint8_t len = 0;
                    uint8_t buf[8];
                    
                    if (!canInterface->receiveMessage(&rxId, &ext, &len, buf)) {
                        continue;
                    }
                    
                    // Heartbeat oder Emergency von diesem Node?
                    if (rxId == (0x700 + id) || rxId == (0x080 + id)) {
                        Serial.printf("[OK] Node %d gefunden über %s!\n", id, 
                                    (rxId == (0x700 + id)) ? "Heartbeat" : "Emergency");
                        
                        // Ergebnis ausgeben mit dem DisplayInterface oder Fallback auf display
                        if (displayInterface != nullptr) {
                            displayInterface->printf("Node %d: OK (HB)\n", id);
                            displayInterface->display();
                        } else {
                            display.printf("Node %d: OK (HB)\n", id);
                            display.display();
                        }
                        
                        responded = true;
                        break;
                    }
                }
            }
            
            // Dann auf die SDO-Antwort warten, wenn noch keine Antwort da ist
            start = millis();
            while (!responded && millis() - start < timeoutMs) {
                if (canInterface->messageAvailable()) {
                    uint32_t rxId;
                    uint8_t ext = 0;
                    uint8_t len = 0;
                    uint8_t buf[8];
                    
                    // Antwort lesen mit Fehlerbehandlung
                    if (!canInterface->receiveMessage(&rxId, &ext, &len, buf)) {
                        continue;
                    }
                    
                    // Prüfen, ob es sich um eine Antwort auf unsere SDO-Anfrage handelt
                    if (rxId == (0x580 + id)) {
                        // Prüfen, ob es eine Fehlerantwort ist (SDO Abort)
                        if ((buf[0] & 0xE0) == 0x80) {
                            uint32_t abortCode = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
                            Serial.printf("[OK] Node %d hat mit Fehler geantwortet! Abortcode: 0x%08X\n", id, abortCode);
                            
                            // Ergebnis ausgeben mit dem DisplayInterface oder Fallback auf display
                            if (displayInterface != nullptr) {
                                displayInterface->printf("Node %d: Err\n", id);
                            } else {
                                display.printf("Node %d: Err\n", id);
                            }
                            
                            errorResponse = true;
                        } else {
                            // Reguläre Antwort
                            Serial.printf("[OK] Node %d hat geantwortet! ObjIdx: 0x%04X\n", id, objectIndices[objIdx]);
                            
                            // Ergebnis ausgeben mit dem DisplayInterface oder Fallback auf display
                            if (displayInterface != nullptr) {
                                displayInterface->printf("Node %d: OK\n", id);
                            } else {
                                display.printf("Node %d: OK\n", id);
                            }
                        }
                        
                        // Display aktualisieren
                        if (displayInterface != nullptr) {
                            displayInterface->display();
                        } else {
                            display.display();
                        }
                        
                        responded = true;
                        break;
                    }
                }
            }
            
            // Pause zwischen verschiedenen Objekten
            if (!responded) {
                delay(20);
            }
        }
        
        // Pause zwischen den Versuchen
        if (!responded && attempt < maxAttempts - 1) {
            delay(50);
        }
    }
    
    if (!responded) {
        Serial.printf("[--] Node %d keine Antwort\n", id);
        return false;
    } else if (errorResponse) {
        // Auch Fehlerantworten zählen als gefundene Nodes
        return true;
    } else {
        return true;
    }
}


// ===================================================================================
// Funktion: handleModeCommand
// Beschreibung: Verarbeitet den Befehl zum Ändern des Systemkonfigurationsprofils
// ===================================================================================
void handleModeCommand(String command) {
    // Format: mode profileId
    int spacePos = command.indexOf(' ');
    if (spacePos <= 0) {
        // Zeige die aktuelle Konfiguration und verfügbaren Profile an
        uint8_t currentProfile = getCurrentProfile(currentDisplayType, currentCANTransceiverType);
        
        Serial.println("[INFO] Aktuelle Systemkonfiguration:");
        Serial.printf("  Profil: %d (%s)\n", currentProfile, getProfileName(currentProfile));
        Serial.printf("  Display: %s (%d)\n", getTransceiverTypeName(currentDisplayType), currentDisplayType);
        Serial.printf("  CAN-Controller: %s (%d)\n", getTransceiverTypeName(currentCANTransceiverType), currentCANTransceiverType);
        
        Serial.println("\n[INFO] Verfügbare Systemprofile:");
        Serial.printf("  %d = %s\n", SYSTEM_PROFILE_OLED_MCP2515, getProfileName(SYSTEM_PROFILE_OLED_MCP2515));
        Serial.printf("  %d = %s\n", SYSTEM_PROFILE_TFT_TJA1051, getProfileName(SYSTEM_PROFILE_TFT_TJA1051));
        
        Serial.println("\n[INFO] Verwendung: mode <profilId>");
        return;
    }
    
    // Wechsle zum angegebenen Profil
    int newProfile = command.substring(spacePos + 1).toInt();
    
    // Validierung des Profils
    if (newProfile != SYSTEM_PROFILE_OLED_MCP2515 && 
        newProfile != SYSTEM_PROFILE_TFT_TJA1051) {
        Serial.println("[FEHLER] Ungültiges Systemprofil!");
        Serial.println("Gültige Profile sind:");
        Serial.printf("  %d = %s\n", SYSTEM_PROFILE_OLED_MCP2515, getProfileName(SYSTEM_PROFILE_OLED_MCP2515));
        Serial.printf("  %d = %s\n", SYSTEM_PROFILE_TFT_TJA1051, getProfileName(SYSTEM_PROFILE_TFT_TJA1051));
        return;
    }
    
    // Aktuelle Konfiguration bestimmen
    uint8_t currentProfile = getCurrentProfile(currentDisplayType, currentCANTransceiverType);
    
    if (newProfile == currentProfile) {
        Serial.printf("[INFO] Systemprofil ist bereits auf %d (%s) eingestellt\n", 
                     newProfile, getProfileName(newProfile));
        return;
    }
    
    // Neue Komponententypen aus dem Profil ermitteln
    uint8_t newDisplayType, newCANType;
    getProfileComponents(newProfile, newDisplayType, newCANType);
    
    // Konfiguration ändern
    currentDisplayType = newDisplayType;
    currentCANTransceiverType = newCANType;
    
    // Crash-Zähler zurücksetzen für einen "sauberen" Neustart
    preferences.begin("canopenscan", false);
    preferences.putUChar("crashCount", 0);
    preferences.putULong("lastCrashTime", 0);
    preferences.end();
    
    // Neue Einstellungen speichern
    saveSettings();
    
    Serial.printf("[INFO] Systemkonfiguration auf Profil %d (%s) geändert\n", 
                 newProfile, getProfileName(newProfile));
    Serial.printf("  Display: %s (%d)\n", getTransceiverTypeName(currentDisplayType), currentDisplayType);
    Serial.printf("  CAN-Controller: %s (%d)\n", getTransceiverTypeName(currentCANTransceiverType), currentCANTransceiverType);
    Serial.println("[INFO] Neustart erforderlich für die Änderungen...");
    
    delay(1000); // Gib dem Serial-Buffer Zeit zum Leeren
    
    // System neustarten
    ESP.restart();
}
// ===================================================================================
// Funktion: printHelpMenu (aktualisiert)
// Beschreibung: Zeigt eine Hilfe mit allen verfügbaren Befehlen an
// ===================================================================================
// ===================================================================================
// Funktion: printHelpMenu (aktualisiert mit neuen Systemkonfigurationsprofilen)
// Beschreibung: Zeigt eine Hilfe mit allen verfügbaren Befehlen an
// ===================================================================================
void printHelpMenu() {
    Serial.println("\n╔═══════════════════════════════════════════════════════════════╗");
    Serial.println("║      CANopen Scanner & Konfigurator " VERSION "                    ║");
    Serial.println("╚═══════════════════════════════════════════════════════════════╝");
    
    Serial.println("\n┌─ Node-Scanning ──────────────────────────────────────────┐");
    Serial.println("│ scan              Node-ID Scan starten                    │");
    Serial.println("│ range <start> <end>  Scan-Bereich setzen (z.B. range 1 10) │");
    Serial.println("│ testnode <id> [attempts] [timeout]                        │");
    Serial.println("│                   Einzelnen Node testen                   │");
    Serial.println("│                   Standard: 5 Versuche, 500ms Timeout     │");
    Serial.println("└──────────────────────────────────────────────────────────┘");
    
    Serial.println("\n┌─ Node-Konfiguration ─────────────────────────────────────┐");
    Serial.println("│ change <old> <new>  Node-ID ändern (z.B. change 8 4)     │");
    Serial.println("│ baudrate <id> <rate>  Node-Baudrate ändern               │");
    Serial.println("│                       Werte: 10, 20, 50, 100, 125,       │");
    Serial.println("│                              250, 500, 800, 1000 kbps    │");
    Serial.println("└──────────────────────────────────────────────────────────┘");
    
    Serial.println("\n┌─ CAN-Bus Monitor ────────────────────────────────────────┐");
    Serial.println("│ monitor on        Live-Monitor aktivieren                │");
    Serial.println("│ monitor off       Live-Monitor deaktivieren              │");
    Serial.println("│ monitor filter <options>  Filter konfigurieren           │");
    Serial.println("└──────────────────────────────────────────────────────────┘");
    
    Serial.println("\n┌─ System-Konfiguration ───────────────────────────────────┐");
    Serial.println("│ localbaud <rate>  Lokale CAN-Baudrate ändern             │");
    Serial.println("│                   (ohne CANopen-Kommunikation)            │");
    Serial.println("│ auto              Automatische Baudratenerkennung         │");
    Serial.println("│ mode [1|2]        Systemprofile anzeigen/wechseln        │");
    Serial.println("│                   1 = OLED + MCP2515                      │");
    Serial.println("│                   2 = TFT + TJA1051                       │");
    Serial.println("│ transceiver       Transceiver-/Display-Befehle anzeigen  │");
    Serial.println("└──────────────────────────────────────────────────────────┘");
    
    Serial.println("\n┌─ Einstellungen & Information ────────────────────────────┐");
    Serial.println("│ info              Aktuelle Einstellungen anzeigen        │");
    Serial.println("│ save              Einstellungen speichern                │");
    Serial.println("│ load              Einstellungen laden                    │");
    Serial.println("│ version           Versionsinfo anzeigen                  │");
    Serial.println("│ reset             System zurücksetzen                    │");
    Serial.println("│ help              Diese Hilfe anzeigen                   │");
    Serial.println("└──────────────────────────────────────────────────────────┘");
    
    Serial.println("\n┌─ Menüsteuerung ──────────────────────────────────────────┐");
    Serial.println("│ menu              Zur Button-Menüsteuerung wechseln      │");
    Serial.println("└──────────────────────────────────────────────────────────┘");
    
    Serial.println("\n► Beispiele:");
    Serial.println("  scan                    ← Scanne Nodes im konfigurierten Bereich");
    Serial.println("  range 1 127             ← Scanne alle möglichen Node-IDs");
    Serial.println("  testnode 3 10 1000      ← Teste Node 3 mit 10 Versuchen à 1000ms");
    Serial.println("  change 8 4              ← Ändere Node-ID von 8 auf 4");
    Serial.println("  baudrate 3 250          ← Setze Node 3 auf 250 kbps");
    Serial.println("  localbaud 500           ← Setze lokale Baudrate auf 500 kbps");
    Serial.println("  monitor on              ← Aktiviere Live CAN-Monitor");
    Serial.println("");
}
// ===================================================================================
// Funktion: handleLocalBaudrateCommand
// Beschreibung: Verarbeitet den Befehl zum Ändern der lokalen ESP32-Baudrate
// ===================================================================================
void handleLocalBaudrateCommand(String command) {
    // Format: localbaud baudrate
    int spacePos = command.indexOf(' ');
    if (spacePos > 0) {
        // Die Baudrate extrahieren
        int baudrate = command.substring(spacePos + 1).toInt();
        
        // Prüfen, ob die Baudrate gültig ist
        if (isValidBaudrate(baudrate)) {
            Serial.printf("[INFO] Ändere lokale ESP32-Baudrate auf %d kbps\n", baudrate);
            
            // Direkt die ESP32-Baudrate umkonfigurieren ohne CANopen-Kommunikation
            if (updateESP32CANBaudrate(baudrate)) {
                // Baudrate erfolgreich geändert
                currentBaudrate = baudrate;
                Serial.printf("[ERFOLG] ESP32 CAN-Bus ist jetzt auf %d kbps eingestellt\n", baudrate);
                
                // Einstellungen speichern
                saveSettings();
                
                // Display aktualisieren
                showStatusMessage("Lokale Baudrate", 
                                 String("Baudrate: " + String(baudrate) + " kbps").c_str());
            }
        } else {
            Serial.println("[FEHLER] Ungültige Baudrate! Gültige Werte: 10, 20, 50, 100, 125, 250, 500, 800, 1000 kbps");
        }
    } else {
        Serial.println("[FEHLER] Falsche Syntax. Korrekt: localbaud baudrate (z.B. localbaud 500)");
    }
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
    // Trimmen und Leerzeichen am Anfang/Ende entfernen
    command.trim();
    
    // Zerlege den Befehl in Tokens mit flexibler Leerzeichen-Behandlung
    int firstSpace = command.indexOf(' ');
    int secondSpace = command.lastIndexOf(' ');

    // Prüfe ob genau zwei Leerzeichen vorhanden sind
    if (firstSpace > 0 && secondSpace > 0 && firstSpace != secondSpace) {
        String strStart = command.substring(firstSpace + 1, secondSpace);
        String strEnd = command.substring(secondSpace + 1);
        
        strStart.trim();
        strEnd.trim();
        
        int newStart = strStart.toInt();
        int newEnd = strEnd.toInt();

        // Debug-Ausgabe zur Fehlersuche
        Serial.printf("DBG: Start: '%s'(%d) End: '%s'(%d)\n", 
                     strStart.c_str(), newStart, strEnd.c_str(), newEnd);

        // Plausibilitätsprüfung
        if (newStart >= 1 && newStart <= 127 && 
            newEnd >= 1 && newEnd <= 127 && 
            newStart <= newEnd) {
            
            scanStart = newStart;
            scanEnd = newEnd;
            Serial.printf("[OK] Bereich %d-%d gesetzt\n", scanStart, scanEnd);
            saveSettings();
        } else {
            Serial.println("[FEHLER] Ungültige Werte (1-127, Start <= Ende)");
        }
    } else {
        Serial.println("[FEHLER] Syntax: range <Start 1-127> <Ende 1-127>");
    }
}
// ===================================================================================
// Funktion: handleTestNodeCommand
// Beschreibung: Verarbeitet den Befehl zum Testen einer einzelnen Node-ID
// ===================================================================================
void handleTestNodeCommand(String command) {
    // Format: testnode nodeID [attempts] [timeout]
    int firstSpace = command.indexOf(' ');
    if (firstSpace > 0) {
        String rest = command.substring(firstSpace + 1);
        rest.trim();
        
        // Parameter extrahieren
        int secondSpace = rest.indexOf(' ');
        int thirdSpace = -1;
        
        int nodeId = 0;
        int attempts = 5;  // Standardwert: 5 Versuche
        int timeout = 500; // Standardwert: 500ms Timeout
        
        if (secondSpace > 0) {
            // NodeID und Versuche angegeben
            nodeId = rest.substring(0, secondSpace).toInt();
            
            String attemptsStr = rest.substring(secondSpace + 1);
            thirdSpace = attemptsStr.indexOf(' ');
            
            if (thirdSpace > 0) {
                // Auch Timeout angegeben
                attempts = attemptsStr.substring(0, thirdSpace).toInt();
                timeout = attemptsStr.substring(thirdSpace + 1).toInt();
            } else {
                // Nur Versuche angegeben
                attempts = attemptsStr.toInt();
            }
        } else {
            // Nur NodeID angegeben
            nodeId = rest.toInt();
        }
        
        // Prüfen, ob NodeID gültig ist
        if (nodeId >= 1 && nodeId <= 127) {
            Serial.printf("[TEST] Starte erweiterten Test für Node %d (%d Versuche, %dms Timeout)\n", 
                          nodeId, attempts, timeout);
            
            // Live-Monitor-Status merken und vorübergehend aktivieren
            bool wasMonitorActive = liveMonitor;
            liveMonitor = true;
            
            bool success = testSingleNode(nodeId, attempts, timeout);
            
            // Live-Monitor zurücksetzen
            liveMonitor = wasMonitorActive;
            
            if (success) {
                Serial.printf("[TEST] Node %d erfolgreich gefunden und kommuniziert!\n", nodeId);
                showStatusMessage("Node Test", String("Node " + String(nodeId) + " gefunden!").c_str());
            } else {
                Serial.printf("[TEST] Node %d konnte nicht erreicht werden.\n", nodeId);
                showStatusMessage("Node Test", String("Node " + String(nodeId) + " nicht gefunden").c_str(), true);
            }
        } else {
            Serial.println("[FEHLER] Ungültige Node-ID! Gültige Werte: 1-127");
        }
    } else {
        Serial.println("[FEHLER] Falsche Syntax. Korrekt: testnode nodeID [versuche] [timeout]");
        Serial.println("         Beispiel: testnode 10    (Standard: 5 Versuche, 500ms Timeout)");
        Serial.println("         Beispiel: testnode 10 10 1000  (10 Versuche, 1000ms Timeout)");
    }
}

// ===================================================================================
// Funktion: testSingleNode
// Beschreibung: Testet eine einzelne Node-ID mit erweiterten Optionen
// ===================================================================================
bool testSingleNode(int nodeId, int maxAttempts, int timeoutMs) {
    bool responded = false;
    
    Serial.printf("[TEST] --- Starte Test für Node %d ---\n", nodeId);
    
    // 1. Erst auf Heartbeats und Emergency-Nachrichten hören
    Serial.println("[TEST] Warte auf Heartbeat oder Emergency...");
    unsigned long startListening = millis();
    while (millis() - startListening < 1000 && !responded) { // 1 Sekunde auf passive Nachrichten warten
        if (!digitalRead(CAN_INT)) {
            unsigned long rxId;
            byte len;
            byte buf[8];
            
            byte readStatus = CAN.readMsgBuf(&rxId, &len, buf);
            if (readStatus != CAN_OK) {
                delay(1);
                continue;
            }
            
            // Zeige alle Nachrichten im Live-Monitor an
            Serial.printf("[CAN] ID: 0x%X Len: %d →", rxId, len);
            for (int i = 0; i < len; i++) {
                Serial.printf(" %02X", buf[i]);
            }
            
            // Heartbeat oder Emergency von diesem Node?
            if (rxId == (0x700 + nodeId) || rxId == (0x080 + nodeId)) {
                Serial.printf("  [%s von Node %d]\n", 
                          (rxId == (0x700 + nodeId)) ? "Heartbeat" : "Emergency");
                responded = true;
                break;
            } else {
                Serial.println();
            }
        }
        delay(1);
    }
    
    // 2. Falls keine passive Nachricht, versuche aktiv verschiedene SDO-Objekte
    if (!responded) {
        Serial.println("[TEST] Kein Heartbeat gefunden, starte aktive SDO-Anfragen...");
        
        // Verschiedene wichtige CANopen-Objekte testen
        uint16_t objectIndices[] = {0x1000, 0x1001, 0x1018, 0x1008, 0x1009, 0x100A, 0x1017};
        const char* objectNames[] = {"Gerätetyp", "Fehlerregister", "Identität", "Herstellername", 
                                    "Hardware-Version", "Software-Version", "Producer Heartbeat"};
        
        for (int attempt = 0; attempt < maxAttempts && !responded; attempt++) {
            Serial.printf("[TEST] Versuch %d von %d...\n", attempt + 1, maxAttempts);
            
            // Verschiedene Objekte durchprobieren
            for (int objIdx = 0; objIdx < 7 && !responded; objIdx++) {
                // SDO-Leseanfrage vorbereiten
                byte sdo[8] = { 0x40, (byte)(objectIndices[objIdx] & 0xFF), 
                               (byte)(objectIndices[objIdx] >> 8), 0x00, 0, 0, 0, 0 };
                
                // Anfrage senden
                Serial.printf("[TEST] Sende SDO-Anfrage für Objekt 0x%04X (%s)...\n", 
                             objectIndices[objIdx], objectNames[objIdx]);
                
                byte result = CAN.sendMsgBuf(0x600 + nodeId, 0, 8, sdo);
                if (result != CAN_OK) {
                    Serial.printf("[TEST] Sendefehler: %d\n", result);
                    delay(50);
                    continue;
                }
                
                // Auf Antwort warten
                unsigned long start = millis();
                while (millis() - start < timeoutMs && !responded) {
                    if (!digitalRead(CAN_INT)) {
                        unsigned long rxId;
                        byte len;
                        byte buf[8];
                        
                        byte readStatus = CAN.readMsgBuf(&rxId, &len, buf);
                        if (readStatus != CAN_OK) {
                            delay(1);
                            continue;
                        }
                        
                        // Alle Nachrichten anzeigen
                        Serial.printf("[CAN] ID: 0x%X Len: %d →", rxId, len);
                        for (int i = 0; i < len; i++) {
                            Serial.printf(" %02X", buf[i]);
                        }
                        Serial.println();
                        
                        // Prüfen, ob es sich um eine Antwort auf unsere SDO-Anfrage handelt
                        if (rxId == (0x580 + nodeId)) {
                            // Prüfen, ob es eine Fehlerantwort ist (SDO Abort)
                            if ((buf[0] & 0xE0) == 0x80) {
                                uint32_t abortCode = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
                                Serial.printf("[TEST] SDO-Abort bei Objekt 0x%04X, Code: 0x%08X\n", 
                                            objectIndices[objIdx], abortCode);
                            } else {
                                Serial.printf("[TEST] Erfolgreich Antwort auf Objekt 0x%04X erhalten!\n", 
                                            objectIndices[objIdx]);
                                
                                // Wert auslesen und anzeigen, wenn es eine Upload-Antwort ist
                                if ((buf[0] & 0xF0) == 0x40) {
                                    uint32_t value = 0;
                                    for (int i = 4; i < 8; i++) {
                                        value |= (uint32_t)buf[i] << ((i - 4) * 8);
                                    }
                                    Serial.printf("[TEST] Wert: 0x%08X (%u)\n", value, value);
                                }
                            }
                            
                            responded = true;
                            break;
                        }
                        
                        // Auch Heartbeat oder Emergency Nachrichten beachten
                        if (rxId == (0x700 + nodeId) || rxId == (0x080 + nodeId)) {
                            Serial.printf("[TEST] %s während SDO-Test empfangen!\n", 
                                     (rxId == (0x700 + nodeId)) ? "Heartbeat" : "Emergency");
                            responded = true;
                            break;
                        }
                    }
                    delay(1);
                }
                
                // Pause zwischen den Objekten
                if (!responded) {
                    delay(30);
                }
            }
            
            // Pause zwischen den Versuchen
            if (!responded && attempt < maxAttempts - 1) {
                delay(100);
            }
        }
    }
    
    // Ergebnis ausgeben
    if (responded) {
        Serial.printf("[TEST] Node %d erfolgreich getestet!\n", nodeId);
        return true;
    } else {
        Serial.printf("[TEST] Node %d konnte nicht erreicht werden.\n", nodeId);
        return false;
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
// Funktion: printCurrentSettings (aktualisiert mit Systemprofilinformationen)
// Beschreibung: Zeigt die aktuellen Systemeinstellungen an
// ===================================================================================
void printCurrentSettings() {
    // Aktuelles Profil bestimmen
    uint8_t currentProfile = getCurrentProfile(currentDisplayType, currentCANTransceiverType);
    
    Serial.println("\n=== Aktuelle Systemeinstellungen ===");
    Serial.printf("Node-ID: %d\n", currentNodeId);
    Serial.printf("Baudrate: %d kbps\n", currentBaudrate);
    Serial.printf("Scan-Bereich: %d bis %d\n", scanStart, scanEnd);
    Serial.printf("System-Profil: %d (%s)\n", currentProfile, getProfileName(currentProfile));
    Serial.printf("Display-Typ: %s (%d)\n", getTransceiverTypeName(currentDisplayType), currentDisplayType);
    Serial.printf("CAN-Controller: %s (%d)\n", getTransceiverTypeName(currentCANTransceiverType), currentCANTransceiverType);
    Serial.printf("Live Monitor: %s\n", liveMonitor ? "aktiviert" : "deaktiviert");
    Serial.printf("System-Status: %s\n", systemError ? "Fehler" : "OK");
    Serial.println("=======================================");
}
// ===================================================================================
// Funktion: systemReset (aktualisiert)
// Beschreibung: Setzt das System auf Standardwerte zurück
// ===================================================================================
void systemReset() {
    // Systemvariablen zurücksetzen
    systemError = false;
    liveMonitor = false;
    scanning = false;
    
    // Zurück zur Standard-Baudrate, wenn nicht bereits eingestellt
    if (currentBaudrate != 125) {
        currentBaudrate = 125;
    }
    
    // Standard-Node-ID wiederherstellen
    currentNodeId = 127;
    
    // Standard-Profil wiederherstellen (OLED + MCP2515)
    uint8_t newDisplayType, newCANType;
    getProfileComponents(SYSTEM_PROFILE_OLED_MCP2515, newDisplayType, newCANType);
    
    currentDisplayType = newDisplayType;
    currentCANTransceiverType = newCANType;
    
    // Crash-Zähler zurücksetzen
    preferences.begin("canopenscan", false);
    preferences.putUChar("crashCount", 0);
    preferences.putULong("lastCrashTime", 0);
    preferences.end();
    
    // Einstellungen speichern
    saveSettings();
    
    // Interfaces neu initialisieren
    if (displayInterface != nullptr) {
        delete displayInterface;
        displayInterface = nullptr;
    }
    
    if (initializeDisplay()) {
        Serial.println("[INFO] Display zurückgesetzt und neu initialisiert");
    }
    
    if (initializeCANInterface()) {
        Serial.println("[INFO] CAN-Interface zurückgesetzt und neu initialisiert");
    }
    
    Serial.println("[INFO] System zurückgesetzt auf Standardwerte");
    showStatusMessage("System-Reset", "Alle Parameter zurückgesetzt\nSystem bereit");
}


// ===================================================================================
// Funktion: handleMonitorFilterCommand
// Beschreibung: Verarbeitet Filter-Befehle für den Live Monitor
// ===================================================================================
void handleMonitorFilterCommand(String command) {
    if (command == "reset") {
        // Alle Filter zurücksetzen
        filterEnabled = false;
        filterIdMin = 0;
        filterIdMax = 0xFFFFFFFF;
        filterNodeEnabled = false;
        filterType = 0;
        Serial.println("[INFO] Alle Monitorfilter zurückgesetzt");
        return;
    }
    
    int spacePos = command.indexOf(' ');
    if (spacePos <= 0) {
        Serial.println("[FEHLER] Ungültiger Filterbefehl. Beispiel: monitor filter id 0x180-0x1FF");
        return;
    }
    
    String filterCommand = command.substring(0, spacePos);
    String filterValue = command.substring(spacePos + 1);
    
    if (filterCommand == "id") {
        // ID-Bereich filtern
        int dashPos = filterValue.indexOf('-');
        if (dashPos > 0) {
            // ID-Bereich (z.B. 0x180-0x1FF)
            String minStr = filterValue.substring(0, dashPos);
            String maxStr = filterValue.substring(dashPos + 1);
            
            // Hex-Strings in Zahlen umwandeln
            uint32_t minId = 0, maxId = 0;
            if (minStr.startsWith("0x")) {
                minId = strtoul(minStr.c_str(), NULL, 16);
            } else {
                minId = minStr.toInt();
            }
            
            if (maxStr.startsWith("0x")) {
                maxId = strtoul(maxStr.c_str(), NULL, 16);
            } else {
                maxId = maxStr.toInt();
            }
            
            if (minId <= maxId) {
                filterIdMin = minId;
                filterIdMax = maxId;
                filterEnabled = true;
                Serial.printf("[INFO] ID-Filter aktiviert: 0x%lX bis 0x%lX\n", filterIdMin, filterIdMax);
            } else {
                Serial.println("[FEHLER] Ungültiger ID-Bereich. Minimum muss kleiner als Maximum sein.");
            }
        } else {
            // Einzelne ID (z.B. 0x180)
            uint32_t singleId = 0;
            if (filterValue.startsWith("0x")) {
                singleId = strtoul(filterValue.c_str(), NULL, 16);
            } else {
                singleId = filterValue.toInt();
            }
            
            filterIdMin = singleId;
            filterIdMax = singleId;
            filterEnabled = true;
            Serial.printf("[INFO] ID-Filter aktiviert: 0x%lX\n", singleId);
        }
    } else if (filterCommand == "node") {
        // Nach Node-ID filtern
        uint8_t nodeId = filterValue.toInt();
        if (nodeId >= 1 && nodeId <= 127) {
            filterNodeId = nodeId;
            filterNodeEnabled = true;
            filterEnabled = true;
            Serial.printf("[INFO] Node-Filter aktiviert: %d\n", nodeId);
        } else {
            Serial.println("[FEHLER] Ungültige Node-ID. Gültige Werte: 1-127");
        }
    } else if (filterCommand == "type") {
        // Nach Typ filtern
        if (filterValue == "pdo") {
            filterType = 1;
            filterEnabled = true;
            Serial.println("[INFO] Typ-Filter aktiviert: PDO");
        } else if (filterValue == "sdo") {
            filterType = 2;
            filterEnabled = true;
            Serial.println("[INFO] Typ-Filter aktiviert: SDO");
        } else if (filterValue == "emcy") {
            filterType = 3;
            filterEnabled = true;
            Serial.println("[INFO] Typ-Filter aktiviert: EMCY");
        } else if (filterValue == "nmt") {
            filterType = 4;
            filterEnabled = true;
            Serial.println("[INFO] Typ-Filter aktiviert: NMT");
        } else if (filterValue == "heartbeat") {
            filterType = 5;
            filterEnabled = true;
            Serial.println("[INFO] Typ-Filter aktiviert: Heartbeat");
        } else {
            Serial.println("[FEHLER] Ungültiger Typ. Gültige Werte: pdo, sdo, emcy, nmt, heartbeat");
        }
    } else {
        Serial.println("[FEHLER] Unbekannter Filterbefehl. Gültige Befehle: id, node, type, reset");
    }
}
// ===================================================================================
// Funktion: sendCanMessage
// Beschreibung: Sendet eine Nachricht über das aktuelle Interface
// ===================================================================================
bool sendCanMessage(uint32_t id, uint8_t ext, uint8_t len, uint8_t *buf) {
    // Sicherstellen, dass das Interface initialisiert ist
    if (canInterface == nullptr) {
        Serial.println("[FEHLER] CAN-Interface nicht initialisiert");
        return false;
    }
    
    // Nachricht über das Interface senden
    return canInterface->sendMessage(id, ext, len, buf);
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
    if (newBaudrate == 500) {
        Serial.println("[INFO] Optimierte Konfiguration für 500 kbps...");
        
        // Bei 500 kbps spezielle Initialisierungsversuche
        // Erster Versuch mit Standardkonfiguration
        if (CAN.begin(MCP_ANY, CAN_500KBPS, CAN_CLOCK) != CAN_OK) {
            Serial.println("[INFO] Standard 500 kbps fehlgeschlagen, versuche alternative Konfiguration...");
            
            // Zweiter Versuch mit anderer Konfiguration
            if (CAN.begin(MCP_STDEXT, CAN_500KBPS, CAN_CLOCK) != CAN_OK) {
                Serial.println("[FEHLER] Alle Versuche für 500 kbps fehlgeschlagen!");
                goto ERROR_RETURN;
            }
        }
        
        // Erfolgreich bei 500 kbps konfiguriert
        CAN.setMode(MCP_NORMAL);
        Serial.println("[INFO] ESP32 CAN-Bus erfolgreich auf 500 kbps umkonfiguriert (optimiert)");
        
        // OLED-Display aktualisieren
        showStatusMessage("Baudrate geändert", 
                        "Neue Baudrate: 500 kbps\n(optimierte Konfiguration)");
        return true;
    } 
    else if (CAN.begin(MCP_ANY, convertBaudrateToCANSpeed(newBaudrate), CAN_CLOCK) == CAN_OK) {
        CAN.setMode(MCP_NORMAL);
        Serial.printf("[INFO] ESP32 CAN-Bus erfolgreich auf %d kbps umkonfiguriert\n", newBaudrate);
        
        // OLED-Display aktualisieren
        showStatusMessage("Baudrate geändert", 
                          String("Neue Baudrate: " + String(newBaudrate) + " kbps").c_str());
        return true;
    } 
    
ERROR_RETURN:
    // Bei Fehler: Zurück zur alten Baudrate
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
    
    // Nur die Baudrate ändern, keine Node-ID-Änderung versuchen
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
    
    Serial.println("--------------------------------------");
    Serial.printf("Aktuelle Einstellungen: NodeID: %d, Baudrate: %d kbps\n", 
                  currentNodeId, currentBaudrate);
    Serial.println("=======================================");
    
    // Zusammenfassende Meldung auf dem Display anzeigen
    String statusMessage = "";
    if (baudrateChanged) {
        statusMessage = "Baudrate geändert";
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
// Funktion: autoBaudrateDetection (aktualisiert für das Interface)
// Beschreibung: Automatische Baudratenerkennung durch Testen verschiedener Baudraten
// ===================================================================================
bool autoBaudrateDetection() {
    // Zu testende Baudraten - wichtigste zuerst
    int baudrates[] = {125, 250, 500, 1000, 100, 50, 20, 10, 800};
    int numBaudrates = sizeof(baudrates) / sizeof(baudrates[0]);
    
    Serial.println("[INFO] Starte automatische Baudratenerkennung...");
    showStatusMessage("Baudratenerkennung", "Suche nach Geräten...");
    
    // Eine Menge bekannter Node-IDs, die wir als erstes testen
    const int knownNodeCount = 5;
    const int knownNodes[knownNodeCount] = {1, 2, 3, 4, 5}; // Häufige Node-IDs
    
    for (int i = 0; i < numBaudrates; i++) {
        Serial.printf("[INFO] Teste Baudrate: %d kbps\n", baudrates[i]);
        
        // CAN-Interface auf diese Baudrate umkonfigurieren
        if (canInterface != nullptr) {
            delete canInterface;
            canInterface = nullptr;
        }
        
        canInterface = CANInterface::createInstance(currentCANTransceiverType);
        if (canInterface == nullptr || !canInterface->begin(baudrates[i] * 1000)) {
            Serial.printf("[FEHLER] Konnte Interface nicht auf %d kbps umkonfigurieren\n", baudrates[i]);
            continue;
        }
        
        // Wichtig: Nach dem Baudratenwechsel etwas warten, damit sich der CAN-Controller stabilisieren kann
        delay(200);
        
        // Scan durchführen
        int foundNodes = 0;
        
        // Erst bekannte Nodes testen
        Serial.println("[INFO] Teste zuerst bekannte Node-IDs...");
        for (int k = 0; k < knownNodeCount; k++) {
            int id = knownNodes[k];
            Serial.printf("[SCAN] Teste bekannte Node-ID: %d\n", id);
            
            // Bei dieser Node mehr Zeit investieren
            bool nodeFound = scanSingleNode(id, baudrates[i], 3, 150); // 3 Versuche, 150ms Timeout
            
            if (nodeFound) {
                foundNodes++;
                Serial.printf("[ERFOLG] Bekannte Node %d gefunden bei %d kbps!\n", id, baudrates[i]);
                break; // Ein Fund ist genug
            }
        }
        
        // Wenn bei bekannten Nodes nichts gefunden wurde, den vollen Scan starten
        if (foundNodes == 0) {
            Serial.println("[INFO] Starte vollständigen Scan...");
            
            // In 10er-Schritten für bessere Übersicht
            for (int idBlock = 1; idBlock <= 127; idBlock += 10) {
                int endBlock = min(idBlock + 9, 127);
                Serial.printf("[INFO] Prüfe Nodes %d bis %d...\n", idBlock, endBlock);
                
                for (int id = idBlock; id <= endBlock; id++) {
                    // Kurze Pause zwischen Nodes
                    delay(10);
                    
                    // Bei 500 kbps mehr Versuche und längeres Timeout
                    int attempts = (baudrates[i] == 500) ? 2 : 1;
                    int timeout = (baudrates[i] == 500) ? 100 : 50;
                    
                    bool nodeFound = scanSingleNode(id, baudrates[i], attempts, timeout);
                    
                    if (nodeFound) {
                        foundNodes++;
                        
                        // Bei 500 kbps schon bei einem Fund erfolgreich
                        if (baudrates[i] == 500 || foundNodes >= 3) {
                            break;
                        }
                    }
                }
                
                // Genug Nodes gefunden?
                if ((baudrates[i] == 500 && foundNodes > 0) || foundNodes >= 3) {
                    break;
                }
                
                // Kurze Pause zwischen Blöcken
                delay(50);
            }
        }
        
        // Wenn Nodes gefunden wurden, haben wir die richtige Baudrate
        if (foundNodes > 0) {
            currentBaudrate = baudrates[i];
            Serial.printf("[ERFOLG] Baudrate erkannt: %d kbps mit %d gefundenen Nodes\n", 
                          currentBaudrate, foundNodes);
            
            // Einstellungen speichern
            saveSettings();
            
            showStatusMessage("Baudrate erkannt", 
                             String("Baudrate: " + String(currentBaudrate) + " kbps\n" + 
                                   String(foundNodes) + " Geräte gefunden").c_str());
            
            return true;
        }
        
        // Längere Pause zwischen Baudraten
        delay(300);
    }
    
    // Keine passende Baudrate gefunden, zurück zur Standard-Baudrate
    Serial.println("[FEHLER] Keine Baudrate mit aktiven Geräten gefunden");
    
    // Interface neu initialisieren mit Standard-Baudrate
    if (canInterface != nullptr) {
        delete canInterface;
        canInterface = nullptr;
    }
    
    canInterface = CANInterface::createInstance(currentCANTransceiverType);
    if (canInterface != nullptr) {
        canInterface->begin(125 * 1000);  // 125 kbps als Standard
    }
    
    currentBaudrate = 125;
    
    showStatusMessage("Fehler", "Keine Baudrate\nmit Geräten gefunden", true);
    
    return false;
}

// ===================================================================================
// Funktion: scanSingleNode (aktualisiert für das Interface)
// Beschreibung: Prüft, ob ein einzelner Node bei einer bestimmten Baudrate erreichbar ist
// ===================================================================================
bool scanSingleNode(int id, int baudrate, int maxAttempts, int timeoutMs) {
    for (int attempt = 0; attempt < maxAttempts; attempt++) {
        // SDO-Leseanfrage an Fehlerregister (0x1001:00) vorbereiten
        uint8_t sdo[8] = { 0x40, 0x01, 0x10, 0x00, 0, 0, 0, 0 };
        
        // Anfrage senden
        if (!sendCanMessage(0x600 + id, 0, 8, sdo)) {
            if (attempt == 0) { // Nur beim ersten Versuch loggen, um das Log nicht zu überfüllen
                Serial.printf("[DEBUG] Sendefehler bei Node %d, Versuch %d\n", id, attempt);
            }
            delay(20); // Kurze Pause vor dem nächsten Versuch
            continue;
        }
        
        // Auf Antwort warten mit Timeout
        unsigned long start = millis();
        while (millis() - start < timeoutMs) {
            if (canInterface->messageAvailable()) {
                uint32_t rxId;
                uint8_t ext = 0;
                uint8_t len = 0;
                uint8_t buf[8];
                
                // Antwort lesen
                if (!canInterface->receiveMessage(&rxId, &ext, &len, buf)) {
                    continue;
                }
                
                // Prüfen, ob es sich um eine Antwort auf unsere SDO-Anfrage handelt
                if (rxId == (0x580 + id)) {
                    Serial.printf("[INFO] Node %d antwortet bei %d kbps! (Versuch %d)\n", 
                                  id, baudrate, attempt+1);
                    return true;
                }
            }
        }
        
        // Pause zwischen Versuchen
        if (attempt < maxAttempts - 1) {
            delay(30);
        }
    }
    
    return false;
}
