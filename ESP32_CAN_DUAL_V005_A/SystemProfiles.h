// SystemProfiles.h
// ===============================================================================
// Definition der Systemkonfigurationsprofile für den CANopen Scanner
// Ermöglicht die Auswahl kompatibler Konfigurationen für Display und CAN-Interface
// ===============================================================================

#pragma once

#include "CANInterface.h"

// Definiere die Systemprofile - feste Kombinationen von Display und CAN-Controller
#define SYSTEM_PROFILE_OLED_MCP2515    1  // Konfiguration mit OLED-Display und MCP2515 CAN-Controller
#define SYSTEM_PROFILE_TFT_TJA1051     2  // Konfiguration mit TFT-Display und TJA1051 CAN-Controller

// Hilfsfunktion, um den Profilnamen zu bekommen
inline const char* getProfileName(uint8_t profileId) {
    switch (profileId) {
        case SYSTEM_PROFILE_OLED_MCP2515:
            return "OLED + MCP2515";
        case SYSTEM_PROFILE_TFT_TJA1051:
            return "TFT + TJA1051";
        default:
            return "Unbekanntes Profil";
    }
}

// Hilfsfunktion, um das aktuelle Profil zu ermitteln
inline uint8_t getCurrentProfile(uint8_t displayType, uint8_t canType) {
    if (displayType == DISPLAY_CONTROLLER_OLED_SSD1306 && 
        canType == CAN_CONTROLLER_MCP2515) {
        return SYSTEM_PROFILE_OLED_MCP2515;
    }
    else if (displayType == DISPLAY_CONTROLLER_WAVESHARE_ESP32S3_TOUCH_LCD && 
             canType == CAN_CONTROLLER_TJA1051) {
        return SYSTEM_PROFILE_TFT_TJA1051;
    }
    // Wenn keine gültige Kombination gefunden wird, Standardprofil zurückgeben
    return SYSTEM_PROFILE_OLED_MCP2515;
}

// Hilfsfunktion, um die zum Profil gehörigen Komponententypen zu ermitteln
inline void getProfileComponents(uint8_t profileId, uint8_t& displayType, uint8_t& canType) {
    switch (profileId) {
        case SYSTEM_PROFILE_OLED_MCP2515:
            displayType = DISPLAY_CONTROLLER_OLED_SSD1306;
            canType = CAN_CONTROLLER_MCP2515;
            break;
        case SYSTEM_PROFILE_TFT_TJA1051:
            displayType = DISPLAY_CONTROLLER_WAVESHARE_ESP32S3_TOUCH_LCD;
            canType = CAN_CONTROLLER_TJA1051;
            break;
        default:
            // Standardwerte für unbekannte Profile
            displayType = DISPLAY_CONTROLLER_OLED_SSD1306;
            canType = CAN_CONTROLLER_MCP2515;
            break;
    }
}