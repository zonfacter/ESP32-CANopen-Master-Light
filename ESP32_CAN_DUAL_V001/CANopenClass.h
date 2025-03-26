// ===================================================================================
// Datei: CANopenClass.h
// Beschreibung:
//   Header-Datei für die CANopen-Klasse zur Verwaltung von Node-ID, SDO, NMT etc.
//   Verwendet MCP_CAN (nicht MCP2515) und ist kompatibel mit der mcp_can Library.
// ===================================================================================

#ifndef CANOPEN_CLASS_H
#define CANOPEN_CLASS_H

#include <Arduino.h>
#include <mcp_can.h>

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

  // NMT-Kommandos
  bool sendNMTCommand(uint8_t nodeId, uint8_t command);
  bool setPreOperational(uint8_t nodeId);
  bool sendSync();

  // SDO-Kommunikation
  bool readSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex, uint32_t &value, uint32_t timeout = 1000);
  bool writeSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex, uint32_t value, uint8_t size);
// In der Klasse CANopen im Header:
bool writeSDOWithTimeout(uint8_t nodeId, uint16_t index, uint8_t subIndex, 
                      uint32_t value, uint8_t size, uint32_t timeout);
  // Node-ID ändern
  bool changeNodeId(uint8_t oldId, uint8_t newId, bool storeInEeprom = true, uint16_t timeout = 5000);
 
  CANopen(uint8_t intPin);  // Neuer Konstruktor mit Interrupt-Pin

private:
  uint8_t _intPin; // Interner Speicher für den Interrupt-Pin

};

#endif
