#include "ESP32CANInterface.h"

ESP32CANInterface::ESP32CANInterface() : initialized(false) {
    Serial.println("[INFO] ESP32CANInterface Konstruktor (Dummy)");
}

ESP32CANInterface::~ESP32CANInterface() {
    Serial.println("[INFO] ESP32CANInterface Destruktor (Dummy)");
    end();
}

bool ESP32CANInterface::begin(uint32_t baudrate) {
    Serial.printf("[INFO] ESP32CANInterface begin (Dummy) bei %d bauds\n", baudrate);
    initialized = false;
    return false;
}

bool ESP32CANInterface::sendMessage(uint32_t id, uint8_t ext, uint8_t len, uint8_t *buf) {
    Serial.printf("[INFO] ESP32CANInterface sendMessage (Dummy): ID 0x%lX, Ext: %d, Len: %d\n", id, ext, len);
    if (!initialized) {
        Serial.println("[FEHLER] CAN nicht initialisiert");
        return false;
    }
    return false;
}

bool ESP32CANInterface::receiveMessage(uint32_t *id, uint8_t *ext, uint8_t *len, uint8_t *buf) {
    Serial.println("[INFO] ESP32CANInterface receiveMessage (Dummy)");
    if (!initialized) {
        Serial.println("[FEHLER] CAN nicht initialisiert");
        return false;
    }
    return false;
}

bool ESP32CANInterface::messageAvailable() {
    Serial.println("[INFO] ESP32CANInterface messageAvailable (Dummy)");
    return false;
}

void ESP32CANInterface::end() {
    Serial.println("[INFO] ESP32CANInterface end (Dummy)");
    if (initialized) {
        initialized = false;
    }
}