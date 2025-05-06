// WaveshareDisplay.h
// ===============================================================================
// Konkrete Implementierung des DisplayInterface für Waveshare ESP32-S3 Touch LCD
// ===============================================================================

#pragma once

#include "DisplayInterface.h"
#include <TFT_eSPI.h>

class WaveshareDisplay : public DisplayInterface {
private:
    TFT_eSPI tftDisplay;  // Umbenennung um Konflikte zu vermeiden
    bool initialized;
    uint16_t textColor;
    uint16_t bgColor;
    uint8_t textSize;
    
public:
    WaveshareDisplay() : tftDisplay(), initialized(false), 
                         textColor(TFT_WHITE), bgColor(TFT_BLACK), textSize(1) {}
    
    ~WaveshareDisplay() {
        // Keine spezielle Bereinigung nötig
    }
    
    bool begin() override {
        if (initialized) return true;
        
        // TFT Display initialisieren
        tftDisplay.init();
        tftDisplay.setRotation(1);  // Landscape-Modus
        tftDisplay.fillScreen(TFT_BLACK);
        tftDisplay.setTextColor(TFT_WHITE, TFT_BLACK);
        tftDisplay.setTextSize(1);
        
        initialized = true;
        return true;
    }
    
    void clear() override {
        tftDisplay.fillScreen(bgColor);
        tftDisplay.setCursor(0, 0);
    }
    
    void display() override {
        // TFT_eSPI benötigt keinen expliziten display()-Aufruf
        // Diese Funktion ist nur für die Kompatibilität mit dem Interface
    }
    
    void setCursor(int16_t x, int16_t y) override {
        tftDisplay.setCursor(x, y);
    }
    
    void setTextSize(uint8_t size) override {
        textSize = size;
        tftDisplay.setTextSize(size);
    }
    
    void setTextColor(uint16_t color) override {
        textColor = color;
        tftDisplay.setTextColor(color, bgColor);
    }
    
    void print(const char* text) override {
        tftDisplay.print(text);
    }
    
    void print(String text) override {
        tftDisplay.print(text);
    }
    
    void println(const char* text) override {
        tftDisplay.println(text);
    }
    
    void println(String text) override {
        tftDisplay.println(text);
    }
    
    void printf(const char* format, ...) override {
        char buf[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        tftDisplay.print(buf);
    }
    void print(int value) override {
    tftDisplay.print(value);
    }
    void print(uint8_t value) override {
        tftDisplay.print(value);
    }
    void print(uint32_t value) override {
        tftDisplay.print(value);
    }
    void print(uint32_t value, int base) override {
        tftDisplay.print(value, base);
    }
    void print(int value, int base) override {
        tftDisplay.print(value, base);
    }
    void println(int value) override {
        tftDisplay.println(value);
    }
    void println(uint8_t value) override {
        tftDisplay.println(value);
    }
    void println(uint32_t value) override {
        tftDisplay.println(value);
    }
    void println(uint32_t value, int base) override {
        tftDisplay.println(value, base);
    }
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) override {
        tftDisplay.drawLine(x0, y0, x1, y1, color);
    }
    
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
        tftDisplay.drawRect(x, y, w, h, color);
    }
    
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
        tftDisplay.fillRect(x, y, w, h, color);
    }
};