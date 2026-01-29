// ===================================================================================
// Datei: CANopenClass.h (Aktualisiert)
// Beschreibung:
//   Header-Datei für die CANopen-Klasse mit Unterstützung für verschiedene CAN-Interfaces
// ===================================================================================

#ifndef CANOPEN_CLASS_H
#define CANOPEN_CLASS_H

#include <Arduino.h>
#include "CANInterface.h"

// ================================
// Debug-Konfiguration
// ================================
// Setze auf 'true' für detaillierte SDO Debug-Ausgaben, 'false' zum Deaktivieren
#ifndef CANOPEN_DEBUG_SDO
#define CANOPEN_DEBUG_SDO false
#endif

// ================================
// Standard-CANopen COB-IDs (Base)
// ================================
#define COB_ID_SYNC      0x080
#define COB_ID_TIME      0x100
#define COB_ID_EMCY_BASE 0x080
#define COB_ID_TPDO1     0x180
#define COB_ID_RPDO1     0x200
#define COB_ID_TPDO2     0x280
#define COB_ID_RPDO2     0x300
#define COB_ID_TPDO3     0x380
#define COB_ID_RPDO3     0x400
#define COB_ID_TPDO4     0x480
#define COB_ID_RPDO4     0x500
#define COB_ID_TSDO_BASE 0x580
#define COB_ID_RSDO_BASE 0x600
#define COB_ID_HB_BASE   0x700
#define COB_ID_NMT       0x000

// ================================
// NMT-Befehle
// ================================
#define NMT_CMD_START_NODE      0x01
#define NMT_CMD_STOP_NODE       0x02
#define NMT_CMD_ENTER_PREOP     0x80
#define NMT_CMD_RESET_NODE      0x81
#define NMT_CMD_RESET_COMM      0x82

class CANopen {
public:
    CANopen();
    CANopen(uint8_t intPin);  // Alter Konstruktor für Kompatibilität
    CANopen(CANInterface* interface); // Neuer Konstruktor mit Interface

    // NMT-Kommandos
    bool sendNMTCommand(uint8_t nodeId, uint8_t command);
    bool setPreOperational(uint8_t nodeId);
    bool sendSync();

    // SDO-Kommunikation
    bool readSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex, uint32_t &value, uint32_t timeout = 1000);
    bool writeSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex, uint32_t value, uint8_t size);
    bool writeSDOWithTimeout(uint8_t nodeId, uint16_t index, uint8_t subIndex, 
                          uint32_t value, uint8_t size, uint32_t timeout);
                          
    // Node-ID ändern
    bool changeNodeId(uint8_t oldId, uint8_t newId, bool storeInEeprom = true, uint16_t timeout = 5000);
 
    // Interface-Verwaltung
    void setCANInterface(CANInterface* interface);
    CANInterface* getCANInterface() const;
    
    // Debug-Steuerung
    void setDebugMode(bool enabled);
    bool getDebugMode() const;

private:
    uint8_t _intPin; // Interner Speicher für den Interrupt-Pin (für Kompatibilität)
    CANInterface* _interface; // Das zu verwendende CAN-Interface
    bool _debugMode; // Debug-Modus (aktiviert/deaktiviert SDO-Ausgaben)
    
    // Hilfsfunktionen für Fehlerausgabe
    const char* getSDOAbortCodeDescription(uint32_t abortCode);
};

#endif