// OLEDDisplay.h
// ===============================================================================
// Konkrete Implementierung des DisplayInterface für OLED-Displays mit SSD1306
// ===============================================================================

#pragma once

#include "DisplayInterface.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#define OLED_ADDR  0x3C   // Typische Adresse für SSD1306-Displays
#define SCREEN_WIDTH 128  // OLED-Display-Breite in Pixeln
#define SCREEN_HEIGHT 64  // OLED-Display-Höhe in Pixeln
#define OLED_RESET    -1  // Reset-Pin (-1 für gemeinsame Nutzung des Arduino-Reset)

class OLEDDisplay : public DisplayInterface {
private:
    // Umbenennung der Display-Variable, um Konflikte zu vermeiden
    Adafruit_SSD1306 oledDisplay;
    bool initialized;
    
public:
    OLEDDisplay() : oledDisplay(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET), 
                    initialized(false) {}
    
    ~OLEDDisplay() {
        // Keine spezielle Bereinigung nötig
    }
    
    bool begin() override {
        if (initialized) return true;
        
        // SSD1306 Display initialisieren
        if(!oledDisplay.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
            Serial.println(F("[FEHLER] SSD1306 Initialisierung fehlgeschlagen"));
            return false;
        }
        
        // Initialisierung abschließen
        oledDisplay.clearDisplay();
        oledDisplay.setTextSize(1);
        oledDisplay.setTextColor(SSD1306_WHITE);
        oledDisplay.setCursor(0, 0);
        oledDisplay.display();
        
        initialized = true;
        return true;
    }
    
    void clear() override {
        oledDisplay.clearDisplay();
        oledDisplay.setCursor(0, 0);
    }
    
    // Korrigierte Implementierung der display-Methode
    void display() override {
        oledDisplay.display();
    }
    
    void setCursor(int16_t x, int16_t y) override {
        oledDisplay.setCursor(x, y);
    }
    
    void setTextSize(uint8_t size) override {
        oledDisplay.setTextSize(size);
    }
    
    void setTextColor(uint16_t color) override {
        oledDisplay.setTextColor(color);
    }
    
    void print(const char* text) override {
        oledDisplay.print(text);
    }
    
    void print(String text) override {
        oledDisplay.print(text);
    }
    
    void println(const char* text) override {
        oledDisplay.println(text);
    }
    
    void println(String text) override {
        oledDisplay.println(text);
    }
    
    void printf(const char* format, ...) override {
        char buf[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        oledDisplay.print(buf);
    }
    // In OLEDDisplay.h ergänzen:
    void print(int value) override {
        oledDisplay.print(value);
    }
    void print(uint8_t value) override {
        oledDisplay.print(value);
    }
    void print(uint32_t value) override {
        oledDisplay.print(value);
    }
    void print(uint32_t value, int base) override {
        oledDisplay.print(value, base);
    }
    void print(int value, int base) override {
        oledDisplay.print(value, base);
    }
    void println(int value) override {
        oledDisplay.println(value);
    }
    void println(uint8_t value) override {
        oledDisplay.println(value);
    }
    void println(uint32_t value) override {
        oledDisplay.println(value);
    }
    void println(uint32_t value, int base) override {
        oledDisplay.println(value, base);
    }
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) override {
        oledDisplay.drawLine(x0, y0, x1, y1, color);
    }
    
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
        oledDisplay.drawRect(x, y, w, h, color);
    }
    
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
        oledDisplay.fillRect(x, y, w, h, color);
    }
};