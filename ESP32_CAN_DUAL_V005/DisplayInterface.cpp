// DisplayInterface.cpp
// ===============================================================================
// Implementierung der Factory-Methode für verschiedene Display-Typen
// ===============================================================================

#include "DisplayInterface.h"
#include "OLEDDisplay.h"
#include "WaveshareDisplay.h"

DisplayInterface* DisplayInterface::createInstance(uint8_t displayType) {
    switch (displayType) {
        case DISPLAY_CONTROLLER_OLED_SSD1306:
            return new OLEDDisplay();
            
        case DISPLAY_CONTROLLER_WAVESHARE_ESP32S3_TOUCH_LCD:
            return new WaveshareDisplay();
            
        case DISPLAY_CONTROLLER_NONE:
        default:
            // Bei NONE oder unbekanntem Typ ein Null-Display zurückgeben
            return nullptr;
    }
}