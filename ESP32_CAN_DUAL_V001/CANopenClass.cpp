// ===================================================================================
// Datei: CANopenClass.cpp
// Beschreibung:
//   Implementierung der CANopen-Klasse inkl. vollständig dokumentierter
//   Methode zur Änderung der Node-ID eines CANopen-Geräts per SDO-Kommunikation.
// ===================================================================================

#include "CANopenClass.h"
#include <SPI.h>

extern MCP_CAN CAN; // Verwendet MCP_CAN statt MCP2515

// Konstruktor
CANopen::CANopen(uint8_t intPin) {
  _intPin = intPin;
}


// ===================================================================================
// Methode: sendNMTCommand
// Beschreibung: Sendet ein NMT-Kommando (Start, Stop, Reset) an ein Gerät
// ===================================================================================
bool CANopen::sendNMTCommand(uint8_t nodeId, uint8_t command) {
  byte data[2] = { command, nodeId };
  return (CAN.sendMsgBuf(COB_ID_NMT, 0, 2, data) == CAN_OK);
}

// ===================================================================================
// Methode: setPreOperational
// Beschreibung: Versetzt ein Gerät in den Pre-Operational-Modus
// ===================================================================================
bool CANopen::setPreOperational(uint8_t nodeId) {
  return sendNMTCommand(nodeId, NMT_CMD_ENTER_PREOP);
}

// ===================================================================================
// Methode: sendSync
// Beschreibung: Sendet eine SYNC-Nachricht (COB-ID 0x80)
// ===================================================================================
bool CANopen::sendSync() {
  return (CAN.sendMsgBuf(COB_ID_SYNC, 0, 0, nullptr) == CAN_OK);
}

// ===================================================================================
// Methode: readSDO
// Beschreibung: Liest ein Objekt über SDO (SDO Upload Request)
// ===================================================================================
bool CANopen::readSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex, uint32_t &value, uint32_t timeout) {
  byte request[8] = {
    0x40,
    (byte)(index & 0xFF), (byte)(index >> 8),
    subIndex,
    0, 0, 0, 0
  };
  
  // Debug-Ausgabe hinzugefügt
  Serial.print("[DEBUG] Sende SDO Read Request: ");
  for (int i = 0; i < 8; i++) {
    Serial.printf("%02X ", request[i]);
  }
  Serial.println();
  
  CAN.sendMsgBuf(COB_ID_RSDO_BASE + nodeId, 0, 8, request);

  unsigned long start = millis();
  while (millis() - start < timeout) {
    if (!digitalRead(_intPin)) {
      unsigned long id;
      byte len;
      byte buf[8];
      CAN.readMsgBuf(&id, &len, buf);
      
      // Debug-Ausgabe hinzugefügt
      Serial.printf("[DEBUG] Empfangene Antwort von ID 0x%lX: ", id);
      for (int i = 0; i < len; i++) {
        Serial.printf("%02X ", buf[i]);
      }
      Serial.println();
      
      if (id == (COB_ID_TSDO_BASE + nodeId)) {
        if ((buf[0] & 0xE0) == 0x80) {
          // Dies ist ein SDO Abort
          uint32_t abortCode = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
          Serial.printf("[FEHLER] SDO Abort Code: 0x%08X\n", abortCode);
          return false;
        } else if ((buf[0] & 0xE0) != 0x80) {
          value = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
          return true;
        }
      }
    }
  }
  Serial.println("[FEHLER] SDO Timeout bei Leseanfrage");
  return false;
}

// ===================================================================================
// Methode: writeSDO
// Beschreibung: Schreibt ein Objekt über SDO (SDO Download Request)
// ÄNDERUNG: Verbesserte Fehlerbehandlung und Debugausgaben hinzugefügt
// ===================================================================================
bool CANopen::writeSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex, uint32_t value, uint8_t size) {
  byte command = 0x23 | ((4 - size) << 2); // SDO Command Specifier
  byte data[8] = {
    command,
    (byte)(index & 0xFF), (byte)(index >> 8),
    subIndex,
    (byte)(value & 0xFF), (byte)((value >> 8) & 0xFF),
    (byte)((value >> 16) & 0xFF), (byte)((value >> 24) & 0xFF)
  };
  
  // Debug-Ausgabe hinzugefügt
  Serial.print("[DEBUG] Sende SDO Write Request: ");
  for (int i = 0; i < 8; i++) {
    Serial.printf("%02X ", data[i]);
  }
  Serial.println();
  
  // Nachricht senden
  byte result = CAN.sendMsgBuf(COB_ID_RSDO_BASE + nodeId, 0, 8, data);
  if (result != CAN_OK) {
    Serial.printf("[FEHLER] CAN-Nachricht konnte nicht gesendet werden: %d\n", result);
    return false;
  }

  // Auf Antwort warten mit verbesserter Fehlerbehandlung
  unsigned long start = millis();
  while (millis() - start < 1000) {
    if (!digitalRead(_intPin)) {
      unsigned long id;
      byte len;
      byte buf[8];
      CAN.readMsgBuf(&id, &len, buf);
      
      // Debug-Ausgabe hinzugefügt
      Serial.printf("[DEBUG] Empfangene Antwort von ID 0x%lX: ", id);
      for (int i = 0; i < len; i++) {
        Serial.printf("%02X ", buf[i]);
      }
      Serial.println();
      
      if (id == (COB_ID_TSDO_BASE + nodeId)) {
        if ((buf[0] & 0xE0) == 0x80) {
          // Dies ist ein SDO Abort
          uint32_t abortCode = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
          Serial.printf("[FEHLER] SDO Abort Code: 0x%08X\n", abortCode);
          return false;
        } else if ((buf[0] & 0xE0) != 0x80) {
          return true;
        }
      }
    }
  }
  Serial.println("[FEHLER] SDO Timeout bei Schreibanfrage");
  return false;
}

// ===================================================================================
// Methode: writeSDOWithTimeout
// Beschreibung: Schreibt ein Objekt über SDO mit anpassbarem Timeout
// ===================================================================================
bool CANopen::writeSDOWithTimeout(uint8_t nodeId, uint16_t index, uint8_t subIndex, 
                               uint32_t value, uint8_t size, uint32_t timeout) {
  byte command = 0x23 | ((4 - size) << 2); // SDO Command Specifier
  byte data[8] = {
    command,
    (byte)(index & 0xFF), (byte)(index >> 8),
    subIndex,
    (byte)(value & 0xFF), (byte)((value >> 8) & 0xFF),
    (byte)((value >> 16) & 0xFF), (byte)((value >> 24) & 0xFF)
  };
  
  // Debug-Ausgabe
  Serial.print("[DEBUG] Sende SDO Write Request: ");
  for (int i = 0; i < 8; i++) {
    Serial.printf("%02X ", data[i]);
  }
  Serial.println();
  
  // Nachricht senden
  byte result = CAN.sendMsgBuf(COB_ID_RSDO_BASE + nodeId, 0, 8, data);
  if (result != CAN_OK) {
    Serial.printf("[FEHLER] CAN-Nachricht konnte nicht gesendet werden: %d\n", result);
    return false;
  }

  // Auf Antwort warten mit anpassbarem Timeout
  unsigned long start = millis();
  while (millis() - start < timeout) { // Hier wird der angepasste Timeout verwendet
    if (!digitalRead(_intPin)) {
      unsigned long id;
      byte len;
      byte buf[8];
      CAN.readMsgBuf(&id, &len, buf);
      
      // Debug-Ausgabe
      Serial.printf("[DEBUG] Empfangene Antwort von ID 0x%lX: ", id);
      for (int i = 0; i < len; i++) {
        Serial.printf("%02X ", buf[i]);
      }
      Serial.println();
      
      if (id == (COB_ID_TSDO_BASE + nodeId)) {
        if ((buf[0] & 0xE0) == 0x80) {
          // Dies ist ein SDO Abort
          uint32_t abortCode = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
          Serial.printf("[FEHLER] SDO Abort Code: 0x%08X\n", abortCode);
          return false;
        } else if ((buf[0] & 0xE0) != 0x80) {
          return true;
        }
      }
    }
  }
  Serial.println("[FEHLER] SDO Timeout bei Schreibanfrage");
  return false;
}
// ===================================================================================
// Methode: changeNodeId
// Beschreibung:
//   Ändert die Node-ID eines CANopen-Geräts per SDO-Kommunikation und speichert sie
//   optional dauerhaft im EEPROM. Anschließend wird auf Heartbeat/Bootup der neuen ID gewartet.
// ÄNDERUNG: Verbesserte Fehlerbehandlung und Wartezeiten erhöht
// ===================================================================================
bool CANopen::changeNodeId(uint8_t oldId, uint8_t newId, bool storeInEeprom, uint16_t timeout) {
  Serial.printf("[INFO] Ändere Node-ID von %d auf %d...\n", oldId, newId);

  // Prüfen, ob der Knoten erreichbar ist, bevor wir ihn ändern
  uint32_t errorReg;
  if (!readSDO(oldId, 0x1001, 0x00, errorReg, 1000)) {
    Serial.println("[FEHLER] Knoten ist nicht erreichbar vor der Änderung!");
    return false;
  }
  
  Serial.printf("[INFO] Knoten %d ist erreichbar. Fehlerregister: 0x%02X\n", oldId, errorReg);

  // In Pre-Operational Modus versetzen
  if (!setPreOperational(oldId)) {
    Serial.println("[FEHLER] Konnte Knoten nicht in Pre-Operational versetzen!");
    return false;
  }
  
  Serial.println("[INFO] Knoten in Pre-Operational versetzt.");
  delay(200); // Wartezeit erhöht

  // Schreibfreigabe mit korrekter Wertebelegung
  // ÄNDERUNG: Bei "nerw" handelt es sich um einen magischen Wert, der durch den ASCII-Wert in das richtige Format gebracht werden muss
  uint32_t unlockValue = 0x6E657277; // "nerw" als ASCII-Hex
  
  Serial.printf("[DEBUG] Sende Schreibfreigabe (ASCII 'nerw'): 0x%08X\n", unlockValue);
if (!writeSDO(oldId, 0x2000, 0x01, unlockValue, 4)) {
  Serial.println("[FEHLER] Schreibfreigabe fehlgeschlagen!");
  return false;
}
  
Serial.println("[INFO] Schreibfreigabe erfolgreich.");
delay(500); // Wartezeit erhöht

// Neue Node-ID schreiben
Serial.printf("[DEBUG] Schreibe neue Node-ID: %d\n", newId);
if (!writeSDO(oldId, 0x2000, 0x02, newId, 4)) { // Hier auf 4 Bytes geändert
  Serial.println("[FEHLER] Node-ID schreiben fehlgeschlagen!");
  return false;
}
  
  Serial.println("[INFO] Neue Node-ID geschrieben.");
  delay(500); // Wartezeit erhöht

// Im EEPROM speichern, falls gewünscht
if (storeInEeprom) {
  // ÄNDERUNG: Hier wird "save" als ASCII-Hex-Wert übertragen
  uint32_t saveValue = 0x65766173; // "save" als ASCII-Hex
  
  Serial.printf("[DEBUG] Speichere in EEPROM (ASCII 'save'): 0x%08X\n", saveValue);
  // Verwende einen längeren Timeout (5 Sekunden) für die EEPROM-Speicherung
  if (!writeSDOWithTimeout(oldId, 0x1010, 0x02, saveValue, 4, 5000)) {
    Serial.println("[WARNUNG] EEPROM-Speicherung fehlgeschlagen!");
  } else {
    Serial.println("[INFO] Kommunikation gespeichert (0x1010:02).");
  }
  delay(500); // Wartezeit erhöht
}

  // Gerät neu starten
  Serial.println("[INFO] Sende Reset-Befehl...");
  sendNMTCommand(oldId, NMT_CMD_RESET_NODE);
  Serial.println("[INFO] Reset gesendet. Warte auf neuen Heartbeat...");

  // Auf neuen Heartbeat warten
  unsigned long start = millis();
  while (millis() - start < timeout) {
    if (!digitalRead(_intPin)) {
      unsigned long id;
      byte len;
      byte buf[8];
      CAN.readMsgBuf(&id, &len, buf);
      
      // Debug-Ausgabe für alle empfangenen Nachrichten
      Serial.printf("[DEBUG] Empfangene Nachricht ID 0x%lX: ", id);
      for (int i = 0; i < len; i++) {
        Serial.printf("%02X ", buf[i]);
      }
      Serial.println();
      
      if ((id & 0x780) == COB_ID_HB_BASE && (id & 0x7F) == newId) {
        Serial.printf("[OK] Neue Node-ID %d antwortet.\n", newId);
        return true;
      }
    }
  }

  Serial.println("[FEHLER] Neue Node-ID antwortet nicht.");
  return false;
}
