#include "CANInterface.h"
#include "MCP2515Interface.h"
#include "ESP32CANInterface.h"
#include "TJA1051Interface.h"

CANInterface* CANInterface::createInstance(uint8_t controllerType) {
    switch (controllerType) {
        case CAN_CONTROLLER_MCP2515:
            return new MCP2515Interface(5, 4);  // CS = 5, INT = 4 (Standard-Werte)
            
        case CAN_CONTROLLER_ESP32CAN:
            return new ESP32CANInterface();
            
        case CAN_CONTROLLER_TJA1051:
            return new TJA1051Interface(255);  // STBY = 26 (kann mit 255 deaktiviert werden)
            
        default:
            return nullptr;
    }
}