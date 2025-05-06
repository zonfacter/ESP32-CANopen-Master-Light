// processCANScanning.cpp
// ===============================================================================
// Implementation der CAN-Scanning Funktionalität
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
extern uint8_t scanStart;
extern uint8_t scanEnd;
extern bool scanning;
extern ControlSource activeSource;
extern unsigned long lastActivityTime;
extern CANopen canopen;

// Externe Funktionen
extern void handleSerialCommands();
extern bool buttonActivity();
extern void displayMenu();
extern void displayActionScreen(const char* title, const char* message, int timeout);

// Globale Variablen für den Scan-Prozess
static uint8_t currentNode = 0;
static unsigned long lastScanTime = 0;
static int currentAttempt = 0;
static const int maxAttempts = 3;
static const int nodeTimeout = 100; // ms Timeout pro Node
static int foundNodes = 0;

// Vorwärtsdeklaration der internen Funktionen
void initializeScan();
void processSingleNode();
void finalizeScan();
bool sendCANMessage(uint32_t id, uint8_t ext, uint8_t len, uint8_t *buf);

// CAN-Scan-Prozess
void processCANScanning() {
    // Initialisierung beim Start des Scans
    if (currentNode == 0) {
        initializeScan();
    }
    
    // Aktuelle Zeit für Timeout-Berechnung
    unsigned long currentTime = millis();
    
    // Prüfen, ob Timeout für den aktuellen Node abgelaufen ist
    if (currentTime - lastScanTime > nodeTimeout) {
        processSingleNode();
    }
    
    // Auf Antwort warten wird in der loop() gehandhabt über processCANMessage()
}

// Initialisierung des Scans
void initializeScan() {
    currentNode = scanStart;
    lastScanTime = millis();
    currentAttempt = 0;
    foundNodes = 0;
    
    Serial.print("[SCAN] Starte Node-Scan von ");
    Serial.print(scanStart);
    Serial.print(" bis ");
    Serial.println(scanEnd);
    
    // Display-Anzeige aktualisieren
    char message[50];
    sprintf(message, "Scanne Node %d...", currentNode);
    displayActionScreen("Node-Scan", message, 1000);
}

// Verarbeitung eines einzelnen Knotens
void processSingleNode() {
    // Versuche erhöhen
    currentAttempt++;
    
    // Maximale Anzahl Versuche erreicht?
    if (currentAttempt >= maxAttempts) {
        // Zum nächsten Node weitergehen
        currentAttempt = 0;
        currentNode++;
        
        // Display-Anzeige aktualisieren, wenn wir noch im Bereich sind
        if (currentNode <= scanEnd) {
            char message[50];
            sprintf(message, "Scanne Node %d", currentNode);
            displayActionScreen("Node-Scan", message, 1000);
        }
        
        // Ende des Scan-Bereichs erreicht?
        if (currentNode > scanEnd) {
            finalizeScan();
            return;
        }
    }
    
    // Zeit zurücksetzen für nächsten Versuch oder nächsten Node
    lastScanTime = millis();
    
    // Node nur abfragen, wenn wir noch im Scan-Modus sind
    if (!scanning) {
        return;
    }
    
    // SDO-Leseanfragen für verschiedene wichtige Objekte probieren
    uint16_t objectIndexes[] = {0x1000, 0x1001, 0x1018}; // Gerätetyp, Fehlerregister, Identität
    uint8_t objectIndex = currentAttempt % 3; // Rotiere durch die Objekte
    
    // SDO-Leseanfrage vorbereiten
    uint8_t sdo[8] = {
        0x40, // SDO Read Request
        (uint8_t)(objectIndexes[objectIndex] & 0xFF), // Index Low Byte
        (uint8_t)(objectIndexes[objectIndex] >> 8),  // Index High Byte
        0x00, // Subindex
        0x00, 0x00, 0x00, 0x00 // Reserviert/Ungenutzt
    };
    
    // Request senden
    uint32_t cobId = 0x600 + currentNode; // SDO Transmit = 0x600 + NodeID
    bool success = sendCANMessage(cobId, 0, 8, sdo);
    
    if (!success) {
        Serial.printf("[DEBUG] Sendefehler bei Node %d, Versuch %d\n", currentNode, currentAttempt);
    }
    
    // Alternativ versuchen wir auch, ob der Knoten auf ein NMT Start Commando reagiert
    if (currentAttempt == 0) {
        // NMT Start Remote Node Command
        uint8_t nmt[2] = {0x01, (uint8_t)currentNode};
        sendCANMessage(0x000, 0, 2, nmt); // NMT COB-ID = 0x000
    }
}

// Abschluss des Scans
void finalizeScan() {
    scanning = false;
    currentNode = 0;
    
    Serial.print("[SCAN] Scan abgeschlossen. Gefundene Nodes: ");
    Serial.println(foundNodes);
    
    // Erfolgsmeldung anzeigen
    char message[50];
    sprintf(message, "Scan abgeschlossen\n%d Nodes gefunden", foundNodes);
    displayActionScreen("Node-Scan", message, 2000);
    
    // Nach dem Scan zum Menü zurückkehren
    displayMenu();
    activeSource = SOURCE_BUTTON;
    lastActivityTime = millis();
}

// Hilfsfunktion zum Senden einer CAN-Nachricht
bool sendCANMessage(uint32_t id, uint8_t ext, uint8_t len, uint8_t *buf) {
    if (canInterface == nullptr) {
        Serial.println("[FEHLER] CAN-Interface nicht initialisiert");
        return false;
    }
    
    return canInterface->sendMessage(id, ext, len, buf);
}

// Callback-Funktion für gefundene Nodes (wird aus processCANMessage aufgerufen)
void nodeFound(uint8_t nodeId) {
    // Nur zählen, wenn innerhalb des Scan-Bereichs
    if (scanning && nodeId >= scanStart && nodeId <= scanEnd) {
        foundNodes++;
        
        Serial.print("[SCAN] Node gefunden: ");
        Serial.println(nodeId);
        
        // Display-Anzeige aktualisieren
        char message[50];
        sprintf(message, "Node %d gefunden!", nodeId);
        displayActionScreen("Node-Scan", message, 1000);
        
        // Zum nächsten Node weitergehen, wenn der aktuelle gefunden wurde
        if (nodeId == currentNode) {
            currentAttempt = 0;
            currentNode++;
            
            // Ende des Scan-Bereichs erreicht?
            if (currentNode > scanEnd) {
                finalizeScan();
                return;
            }
            
            // Anzeige für den nächsten Node aktualisieren
            sprintf(message, "Scanne Node %d", currentNode);
            displayActionScreen("Node-Scan", message, 1000);
            
            // Zeit zurücksetzen für den nächsten Node
            lastScanTime = millis();
        }
    }
}