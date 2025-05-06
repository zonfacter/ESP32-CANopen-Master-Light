// DisplayInterface.h
// ===============================================================================
// Abstrakte Basis-Interface-Klasse für verschiedene Display-Typen
// Ermöglicht die Unterstützung unterschiedlicher Displays mit einer einheitlichen API
// ===============================================================================

#pragma once

#include <Arduino.h>

// Display-Controller-Typen
#define DISPLAY_CONTROLLER_NONE                      0
#define DISPLAY_CONTROLLER_OLED_SSD1306              10
#define DISPLAY_CONTROLLER_WAVESHARE_ESP32S3_TOUCH_LCD   11

class DisplayInterface {
public:
    virtual ~DisplayInterface() {}
    
    // Grundlegende Display-Funktionen
    virtual bool begin() = 0;
    virtual void clear() = 0;
    virtual void display() = 0;
    
    // Textausgabe
    virtual void setCursor(int16_t x, int16_t y) = 0;
    virtual void setTextSize(uint8_t size) = 0;
    virtual void setTextColor(uint16_t color) = 0;
    virtual void print(const char* text) = 0;
    virtual void print(String text) = 0;
    virtual void print(int value) = 0;
    virtual void print(uint8_t value) = 0;
    virtual void print(uint32_t value) = 0;
    virtual void print(uint32_t value, int base) = 0;
    virtual void print(int value, int base) = 0;
    virtual void println(const char* text) = 0;
    virtual void println(String text) = 0;
    virtual void println(int value) = 0;
    virtual void println(uint8_t value) = 0;
    virtual void println(uint32_t value) = 0;
    virtual void println(uint32_t value, int base) = 0;
    virtual void printf(const char* format, ...) = 0;
    
    // Zeichenfunktionen
    virtual void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) = 0;
    virtual void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) = 0;
    virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) = 0;

    // Factory-Methode zum Erstellen der richtigen Display-Instanz
    static DisplayInterface* createInstance(uint8_t displayType);
};