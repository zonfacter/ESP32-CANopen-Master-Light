// CANInterface.h
#pragma once

#include <Arduino.h>

// Definiere die unterstützten CAN-Controller-Typen
// CAN-Controller-Typen
#define CAN_CONTROLLER_MCP2515     1
#define CAN_CONTROLLER_ESP32CAN    2
#define CAN_CONTROLLER_TJA1051     3

// Display-Controller-Typen
#define DISPLAY_CONTROLLER_NONE            0
#define DISPLAY_CONTROLLER_OLED_SSD1306    10
#define DISPLAY_CONTROLLER_WAVESHARE_ESP32S3_TOUCH_LCD   11


class CANInterface {
public:
    virtual ~CANInterface() {}
    
    // Initialisierung mit Baudrate
    virtual bool begin(uint32_t baudrate) = 0;
    
    // Nachricht senden
    virtual bool sendMessage(uint32_t id, uint8_t ext, uint8_t len, uint8_t *buf) = 0;
    
    // Nachricht empfangen
    virtual bool receiveMessage(uint32_t *id, uint8_t *ext, uint8_t *len, uint8_t *buf) = 0;
    
    // Prüfen, ob Nachrichten verfügbar sind
    virtual bool messageAvailable() = 0;
    
    // CAN-Interface herunterfahren
    virtual void end() = 0;

    // Factory-Methode zum Erstellen der richtigen Interface-Instanz
    static CANInterface* createInstance(uint8_t controllerType);
};