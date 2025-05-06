// processAutoBaudrate.cpp
// ===============================================================================
// Implementation der automatischen Baudratenerkennung
// ===============================================================================

#include <Arduino.h>
#include "OLEDMenu.h"
#include "CANopen.h"
#include "CANopenClass.h"
#include "CANInterface.h"
#include "DisplayInterface.h"

// Externe Variablen aus Hauptprogramm
extern DisplayInterface* displayInterface;
extern CANInterface* canInterface;
extern bool autoBaudrateRequest;
extern int currentBaudrate;
extern ControlSource activeSource;
extern unsigned long lastActivityTime;
extern CANopen canopen;
extern uint8_t currentCANTransceiverType;

// Externe Funktionen
extern void displayActionScreen(const char* title, const char* message, int timeout);
extern void displayMenu();
extern bool buttonActivity();
extern void handleSerialCommands();
extern void saveSettings();

// Globale Variablen für die Baudratenerkennung
static int currentBaudrateIndex = 0;
static unsigned long lastBaudrateTime = 0;
static int currentAttempt = 0;
static const int maxAttempts = 3;
static const int baudrateTimeout = 500; // ms Timeout pro Baudrate
static bool messageReceived = false;

// Liste der zu testenden Baudraten - sortiert nach Häufigkeit im Feld
static const int baudrates[] = {125, 250, 500, 1000, 100, 50, 20, 10, 800};
static const int numBaudrates = sizeof(baudrates) / sizeof(baudrates[0]);

// Liste bekannter Node-IDs, die zuerst getestet werden sollten
static const int knownNodes[] = {1, 2, 3, 4, 5, 10};
static const int knownNodesCount = sizeof(knownNodes) / sizeof(knownNodes[0]);

// Vorwärtsdeklaration der internen Funktionen
bool initializeForBaudrate(int baudrateKbps);
bool sendTestMessages();
void finalizeBaudrateDetection(bool success);

// Baudratenerkennung durchführen
void processAutoBaudrate() {
    // Initialisierung beim Start der Baudratenerkennung
    if (currentBaudrateIndex == 0 && currentAttempt == 0) {
        Serial.println("[INFO] Starte automatische Baudratenerkennung...");
        displayActionScreen("Auto-Baudrate", "Erkenne Baudrate...", 1000);
        
        // Zeit initialisieren
        lastBaudrateTime = millis();
        messageReceived = false;
    }
    
    // Aktuelle Zeit für Timeout-Berechnung
    unsigned long currentTime = millis();
    
    // Prüfen ob eine CAN-Nachricht empfangen wurde
    if (canInterface != nullptr && canInterface->messageAvailable()) {
        uint32_t rxId;
        uint8_t ext = 0;
        uint8_t len = 0;
        uint8_t buf[8];
        
        if (canInterface->receiveMessage(&rxId, &ext, &len, buf)) {
            // Nachricht empfangen - aktuelle Baudrate ist korrekt!
            Serial.print("[INFO] CAN-Nachricht bei ");
            Serial.print(baudrates[currentBaudrateIndex]);
            Serial.println(" kbps empfangen!");
            
            // ID und Daten ausgeben
            Serial.printf("[INFO] ID: 0x%03X Len: %d Data:", rxId, len);
            for (int i = 0; i < len; i++) {
                Serial.printf(" %02X", buf[i]);
            }
            Serial.println();
            
            // Baudrate gefunden!
            messageReceived = true;
            finalizeBaudrateDetection(true);
            return;
        }
    }
    
    // Prüfen, ob Timeout für die aktuelle Baudrate abgelaufen ist
    if (currentTime - lastBaudrateTime > baudrateTimeout) {
        // Versuche erhöhen
        currentAttempt++;
        
        // Maximale Anzahl Versuche erreicht?
        if (currentAttempt >= maxAttempts) {
            // Zur nächsten Baudrate weitergehen
            currentAttempt = 0;
            currentBaudrateIndex++;
            
            // Ende der Baudraten-Liste erreicht?
            if (currentBaudrateIndex >= numBaudrates) {
                // Keine Baudrate gefunden
                finalizeBaudrateDetection(false);
                return;
            }
            
            // Neue Baudrate initialisieren
            if (!initializeForBaudrate(baudrates[currentBaudrateIndex])) {
                // Wenn die Initialisierung fehlschlägt, zur nächsten Baudrate weitergehen
                currentBaudrateIndex++;
                
                // Ende der Baudraten-Liste erreicht?
                if (currentBaudrateIndex >= numBaudrates) {
                    // Keine Baudrate gefunden
                    finalizeBaudrateDetection(false);
                    return;
                }
            }
        }
        
        // Test-Nachrichten senden
        sendTestMessages();
        
        // Zeit zurücksetzen für nächsten Versuch oder nächste Baudrate
        lastBaudrateTime = currentTime;
    }
}

// Interface für eine bestimmte Baudrate initialisieren
bool initializeForBaudrate(int baudrateKbps) {
    Serial.printf("[INFO] Teste Baudrate: %d kbps\n", baudrateKbps);
    
    // Anzeige aktualisieren
    char message[50];
    sprintf(message, "Teste %d kbps...", baudrateKbps);
    displayActionScreen("Auto-Baudrate", message, 1000);
    
    // CAN-Interface neu initialisieren
    if (canInterface != nullptr) {
        canInterface->end();
        delete canInterface;
        canInterface = nullptr;
    }
    
    // Neues Interface erstellen und initialisieren
    canInterface = CANInterface::createInstance(currentCANTransceiverType);
    if (canInterface == nullptr) {
        Serial.println("[FEHLER] Konnte kein CAN-Interface erstellen");
        return false;
    }
    
    bool success = canInterface->begin(baudrateKbps * 1000);
    if (!success) {
        Serial.printf("[FEHLER] Konnte Interface nicht auf %d kbps initialisieren\n", baudrateKbps);
        return false;
    }
    
    // Kurze Pause nach der Initialisierung
    delay(100);
    
    return true;
}

// Test-Nachrichten senden
bool sendTestMessages() {
    if (canInterface == nullptr) {
        return false;
    }
    
    bool success = false;
    
    // Je nach Versuch verschiedene Nachrichten senden
    switch (currentAttempt) {
        case 0: {
            // Versuch 1: NMT Broadcast (Start Remote Node)
            uint8_t data[2] = {0x01, 0x00};  // Start All Nodes
            success = canInterface->sendMessage(0x000, 0, 2, data);
            
            Serial.println("[INFO] Sende NMT Start All Nodes");
            break;
        }
        
        case 1: {
            // Versuch 2: SDO-Anfragen an bekannte Nodes senden
            for (int i = 0; i < knownNodesCount; i++) {
                uint8_t nodeId = knownNodes[i];
                
                // SDO-Anfrage an Fehlerregister (0x1001:00)
                uint8_t sdo[8] = {0x40, 0x01, 0x10, 0x00, 0, 0, 0, 0};
                success = canInterface->sendMessage(0x600 + nodeId, 0, 8, sdo);
                
                Serial.printf("[INFO] Sende SDO-Anfrage an Node %d\n", nodeId);
                
                delay(50);
            }
            break;
        }
        
        case 2: {
            // Versuch 3: Heartbeat Consumer aktivieren und auf Heartbeats warten
            Serial.println("[INFO] Warte auf Heartbeat-Nachrichten...");
            
            // Wir müssen keine Nachricht senden - warten nur auf Heartbeats
            success = true;
            break;
        }
    }
    
    return success;
}

// Abschluss der Baudratenerkennung
void finalizeBaudrateDetection(bool success) {
    autoBaudrateRequest = false;
    
    if (success) {
        // Baudrate gefunden!
        currentBaudrate = baudrates[currentBaudrateIndex];
        
        // Erfolgsmeldung anzeigen
        char message[50];
        sprintf(message, "Baudrate erkannt:\n%d kbps", currentBaudrate);
        displayActionScreen("Auto-Baudrate", message, 2000);
        
        // Einstellungen speichern
        saveSettings();
        
        Serial.printf("[INFO] Baudrate erkannt: %d kbps\n", currentBaudrate);
    } else {
        // Keine Baudrate gefunden, zurück zu 125 kbps
        currentBaudrate = 125;
        
        // CAN-Interface neu initialisieren mit Standard-Baudrate
        if (canInterface != nullptr) {
            canInterface->end();
            delete canInterface;
            canInterface = nullptr;
        }
        
        canInterface = CANInterface::createInstance(currentCANTransceiverType);
        if (canInterface != nullptr) {
            canInterface->begin(currentBaudrate * 1000);
        }
        
        // Fehlermeldung anzeigen
        displayActionScreen("Auto-Baudrate", "Keine Baudrate\nerkannt!", 2000);
        
        Serial.println("[FEHLER] Keine Baudrate erkannt");
    }
    
    // Variablen zurücksetzen
    currentBaudrateIndex = 0;
    currentAttempt = 0;
    messageReceived = false;
    
    // Zurück zum Menü
    displayMenu();
    activeSource = SOURCE_BUTTON;
    lastActivityTime = millis();
}