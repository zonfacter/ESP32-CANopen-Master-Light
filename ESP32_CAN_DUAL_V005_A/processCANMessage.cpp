// processCANMessage.cpp
// ===============================================================================
// Implementation der CAN-Nachrichten-Verarbeitung für den Live-Monitor und Scanning
// ===============================================================================

#include <Arduino.h>
#include "OLEDMenu.h"
#include "CANopen.h"
#include "CANInterface.h"
#include "DisplayInterface.h"

// Externe Variablen aus Hauptprogramm
extern DisplayInterface* displayInterface;
extern CANInterface* canInterface;
extern bool liveMonitor;
extern bool scanning;
extern bool filterEnabled;
extern uint32_t filterIdMin;
extern uint32_t filterIdMax;
extern uint8_t filterNodeId;
extern bool filterNodeEnabled;
extern uint8_t filterType;

// Vorwärtsdeklarationen externer Funktionen
extern void nodeFound(uint8_t nodeId);  // In processCANScanning.cpp implementiert

// Hilfsfunktionen für die Dekodierung
void decodeCANMessage(uint32_t rxId, uint8_t nodeId, uint16_t baseId, uint8_t* buf, uint8_t len);
void decodeNMTState(uint8_t state);
void decodeNMTCommand(uint8_t* buf, uint8_t len);
void decodeSDOResponse(uint8_t* buf, uint8_t len);
void decodeSDOAbortCode(uint32_t abortCode);
void displayCANMessage(uint32_t canId, uint8_t* data, uint8_t length);

// CAN-Nachricht empfangen und verarbeiten
void processCANMessage() {
    if (!canInterface->messageAvailable()) {
        return;
    }
    
    uint32_t rxId;
    uint8_t ext = 0;
    uint8_t len = 0;
    uint8_t buf[8];
    
    // Nachricht lesen
    if (!canInterface->receiveMessage(&rxId, &ext, &len, buf)) {
        return;
    }
    
    // Node-ID und Basis-ID (COBID ohne Node-ID) extrahieren
    uint8_t nodeId = rxId & 0x7F;
    uint16_t baseId = rxId & 0x780;
    
    // Filter anwenden
    if (filterEnabled) {
        bool passFilter = true;
        
        // ID-Filter
        if (rxId < filterIdMin || rxId > filterIdMax) {
            passFilter = false;
        }
        
        // Node-Filter
        if (filterNodeEnabled && nodeId != filterNodeId) {
            passFilter = false;
        }
        
        // Typ-Filter (PDO, SDO, EMCY, NMT, Heartbeat)
        if (filterType > 0) {
            switch (filterType) {
                case 1: // PDO Filter
                    if (!(baseId >= 0x180 && baseId <= 0x500)) {
                        passFilter = false;
                    }
                    break;
                case 2: // SDO Filter
                    if (!(baseId == 0x580 || baseId == 0x600)) {
                        passFilter = false;
                    }
                    break;
                case 3: // EMCY Filter
                    if (baseId != 0x080) {
                        passFilter = false;
                    }
                    break;
                case 4: // NMT Filter
                    if (rxId != 0x000) {
                        passFilter = false;
                    }
                    break;
                case 5: // Heartbeat Filter
                    if (baseId != 0x700) {
                        passFilter = false;
                    }
                    break;
            }
        }
        
        // Wenn der Filter nicht bestanden wurde, Nachricht überspringen
        if (!passFilter) {
            return;
        }
    }
    
    // Im Scan-Modus: Prüfen ob es eine Antwort eines gescannten Nodes ist
    if (scanning) {
        // Antworten, die einen Node identifizieren können:
        
        // 1. SDO-Antwort
        if (baseId == 0x580) {
            nodeFound(nodeId);
        }
        
        // 2. Heartbeat
        else if (baseId == 0x700) {
            nodeFound(nodeId);
        }
        
        // 3. Emergency
        else if (baseId == 0x080) {
            nodeFound(nodeId);
        }
        
        // 4. PDO (mit einiger Vorsicht, könnten auch andere sein)
        else if (baseId >= 0x180 && baseId <= 0x480 && baseId % 0x100 == 0x80) {
            nodeFound(nodeId);
        }
    }
    
    // Im LiveMonitor-Modus: Nachricht ausgeben
    if (liveMonitor) {
        // Formatierte Ausgabe im seriellen Monitor
        Serial.printf("[CAN] ID: 0x%03X Len: %d → ", rxId, len);
        for (int i = 0; i < len; i++) {
            Serial.printf("%02X ", buf[i]);
        }
        
        // Bekannte Nachrichtentypen dekodieren und interpretieren
        decodeCANMessage(rxId, nodeId, baseId, buf, len);
        
        Serial.println(); // Zeilenumbruch nach der Dekodierung
        
        // Nachricht auf dem Display anzeigen
        displayCANMessage(rxId, buf, len);
    }
}

// Dekodierung einer CAN-Nachricht
void decodeCANMessage(uint32_t rxId, uint8_t nodeId, uint16_t baseId, uint8_t* buf, uint8_t len) {
    // Bekannte CANopen-Nachrichtentypen identifizieren und interpretieren
    if (rxId == 0x000) {
        Serial.print("  [NMT Broadcast]");
        decodeNMTCommand(buf, len);
    } 
    else if (baseId == 0x080) {
        Serial.printf("  [Emergency von Node %d]", nodeId);
        if (len >= 2) {
            uint16_t errorCode = buf[0] | (buf[1] << 8);
            Serial.printf(" Error: 0x%04X", errorCode);
        }
    } 
    else if (rxId == 0x080) {
        Serial.print("  [SYNC]");
    } 
    else if (rxId == 0x100) {
        Serial.print("  [TIME]");
    } 
    else if (baseId >= 0x180 && baseId <= 0x480 && baseId % 0x100 == 0x80) {
        // TPDOs
        int pdoNumber = (baseId - 0x180) / 0x100 + 1;
        Serial.printf("  [PDO%d von Node %d]", pdoNumber, nodeId);
    } 
    else if (baseId >= 0x200 && baseId <= 0x500 && baseId % 0x100 == 0x00) {
        // RPDOs
        int pdoNumber = (baseId - 0x200) / 0x100 + 1;
        Serial.printf("  [RPDO%d an Node %d]", pdoNumber, nodeId);
    } 
    else if (baseId == 0x580) {
        Serial.printf("  [SDO Response von Node %d]", nodeId);
        decodeSDOResponse(buf, len);
    } 
    else if (baseId == 0x600) {
        Serial.printf("  [SDO Request an Node %d]", nodeId);
    } 
    else if (baseId == 0x700) {
        if (len > 0) {
            if (buf[0] == 0x00) {
                Serial.printf("  [Bootup von Node %d]", nodeId);
            } else {
                Serial.printf("  [Heartbeat von Node %d]", nodeId);
                decodeNMTState(buf[0]);
            }
        }
    } 
    else {
        // Unbekannter CAN-ID-Bereich
        Serial.printf("  [Unbekannte Nachricht]");
    }
}

// NMT-Zustand dekodieren
void decodeNMTState(uint8_t state) {
    switch (state) {
        case 0x00:
            Serial.print(" (Boot-up)");
            break;
        case 0x04:
            Serial.print(" (Stopped)");
            break;
        case 0x05:
            Serial.print(" (Operational)");
            break;
        case 0x7F:
            Serial.print(" (Pre-Operational)");
            break;
        default:
            Serial.printf(" (Unbekannt: 0x%02X)", state);
            break;
    }
}

// NMT-Befehl dekodieren
void decodeNMTCommand(uint8_t* buf, uint8_t len) {
    if (len < 2) return;
    
    uint8_t command = buf[0];
    uint8_t nodeId = buf[1];
    
    switch (command) {
        case NMT_CMD_START_NODE:
            Serial.printf(" Start Node %d", nodeId);
            break;
        case NMT_CMD_STOP_NODE:
            Serial.printf(" Stop Node %d", nodeId);
            break;
        case NMT_CMD_ENTER_PREOP:
            Serial.printf(" EnterPreOperational Node %d", nodeId);
            break;
        case NMT_CMD_RESET_NODE:
            Serial.printf(" Reset Node %d", nodeId);
            break;
        case NMT_CMD_RESET_COMM:
            Serial.printf(" Reset Communication Node %d", nodeId);
            break;
        default:
            Serial.printf(" Unbekannter Befehl 0x%02X für Node %d", command, nodeId);
    }
}

// SDO-Antwort dekodieren
void decodeSDOResponse(uint8_t* buf, uint8_t len) {
    if (len < 4) return;
    
    uint8_t commandSpecifier = buf[0] >> 5; // Bits 5-7
    
    switch (commandSpecifier) {
        case 0: // Segmented upload/download
            Serial.print(" (Segmentiert)");
            break;
        case 1: // Download response
            Serial.print(" (Download OK)");
            break;
        case 2: // Initiating download
            Serial.print(" (Initiiere Download)");
            break;
        case 3: // Upload response
            Serial.print(" (Upload)");
            // Wert extrahieren, wenn expedited transfer
            if (buf[0] & 0x02) { // expedited bit gesetzt
                // Anzahl der nicht verwendeten Bytes
                uint8_t n = (buf[0] >> 2) & 0x03;
                uint8_t dataBytes = 4 - n;
                uint32_t value = 0;
                
                // Wert aus den Datenbytes extrahieren
                for (int i = 0; i < dataBytes; i++) {
                    value |= (uint32_t)buf[4 + i] << (i * 8);
                }
                
                Serial.printf(" Wert: 0x%X", value);
            }
            break;
        case 4: // Abort transfer
            Serial.print(" (Abort)");
            if (len >= 8) {
                uint32_t abortCode = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
                Serial.printf(" Code: 0x%08X", abortCode);
                decodeSDOAbortCode(abortCode);
            }
            break;
        default:
            Serial.printf(" (Unbekannter CS: %d)", commandSpecifier);
    }
}

// SDO-Abort-Code dekodieren
void decodeSDOAbortCode(uint32_t abortCode) {
    switch (abortCode) {
        case 0x05030000:
            Serial.print(" (Toggle-Bit nicht alterniert)");
            break;
        case 0x05040000:
            Serial.print(" (Timeout)");
            break;
        case 0x05040001:
            Serial.print(" (Ungültiger CS)");
            break;
        case 0x05040002:
            Serial.print(" (Ungültige Block-Größe)");
            break;
        case 0x05040003:
            Serial.print(" (Ungültige Sequenznummer)");
            break;
        case 0x05040004:
            Serial.print(" (CRC-Fehler)");
            break;
        case 0x05040005:
            Serial.print(" (Kein Speicher)");
            break;
        case 0x06010000:
            Serial.print(" (Nicht unterstützter Zugriff)");
            break;
        case 0x06010001:
            Serial.print(" (Schreiben nur lesen)");
            break;
        case 0x06010002:
            Serial.print(" (Lesen nur schreiben)");
            break;
        case 0x06020000:
            Serial.print(" (Objekt nicht vorhanden)");
            break;
        case 0x06040041:
            Serial.print(" (Objekt nicht mappbar)");
            break;
        case 0x06040042:
            Serial.print(" (Mapping-Länge überschritten)");
            break;
        case 0x06040043:
            Serial.print(" (Parameter-Inkompatibilität)");
            break;
        case 0x06070010:
            Serial.print(" (Datentyp stimmt nicht)");
            break;
        case 0x06090011:
            Serial.print(" (Subindex nicht vorhanden)");
            break;
        case 0x06090030:
            Serial.print(" (Wert außerhalb Bereich)");
            break;
        case 0x06090031:
            Serial.print(" (Wert zu groß)");
            break;
        case 0x06090032:
            Serial.print(" (Wert zu klein)");
            break;
        case 0x08000000:
            Serial.print(" (Allgemeiner Fehler)");
            break;
        case 0x08000020:
            Serial.print(" (Datentransfer nicht möglich)");
            break;
        case 0x08000021:
            Serial.print(" (Lokaler Kontrollfehler)");
            break;
        case 0x08000022:
            Serial.print(" (Gerätezustand falsch)");
            break;
        default:
            Serial.printf(" (Unbekannter Abortcode)");
    }
}

// CAN-Nachricht auf dem Display anzeigen
void displayCANMessage(uint32_t canId, uint8_t* data, uint8_t length) {
    if (displayInterface == nullptr || !liveMonitor) {
        return;
    }
    
    // Display leeren
    displayInterface->clear();
    
    // Titel anzeigen
    displayInterface->setCursor(0, 0);
    displayInterface->setTextSize(1);
    displayInterface->println("Live-Monitor");
    displayInterface->drawLine(0, 10, 128, 10, 1);
    
    // CAN-ID anzeigen
    displayInterface->setCursor(0, 15);
    displayInterface->print("ID: 0x");
    
    // ID in HEX ausgeben mit führenden Nullen
    char idStr[8];
    sprintf(idStr, "%03X", canId);
    displayInterface->print(idStr);
    
    // Node-ID anzeigen (falls vorhanden)
    uint8_t nodeId = canId & 0x7F;
    uint16_t baseId = canId & 0x780;
    if (baseId >= 0x080 && baseId <= 0x700) {
        // Typischer CANopen Bereich
        displayInterface->printf(" (N:%d)", nodeId);
    }
    
    // Länge anzeigen
    displayInterface->setCursor(0, 25);
    displayInterface->print("Len: ");
    displayInterface->print(length);
    
    // Daten anzeigen (max. 8 Bytes)
    displayInterface->setCursor(0, 35);
    displayInterface->print("Data:");
    
    int x = 30; // X-Position für Datenbytes
    for (int i = 0; i < length && i < 8; i++) {
        // HEX-Ausgabe für jedes Byte
        char hexStr[4];
        sprintf(hexStr, "%02X", data[i]);
        
        // Bei jedem zweiten Byte Zeilenumbruch
        if (i == 4) {
            displayInterface->setCursor(0, 45);
            x = 30;
        }
        
        displayInterface->setCursor(x, (i < 4) ? 35 : 45);
        displayInterface->print(hexStr);
        x += 24; // Abstand zwischen Bytes
    }
    
    // Filteranzeige
    displayInterface->setCursor(0, 55);
    displayInterface->print("Filter: ");
    displayInterface->print(filterEnabled ? "Ein" : "Aus");
    
    // Display aktualisieren
    displayInterface->display();
}