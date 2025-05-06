#include "MCP2515Interface.h"

MCP2515Interface::MCP2515Interface(uint8_t csPin, uint8_t intPin) 
    : can(new MCP_CAN(csPin)), intPin(intPin) {
    // Constructor initializes MCP_CAN with the given CS pin
}

MCP2515Interface::~MCP2515Interface() {
    // Free resources
    if (can) {
        delete can;
    }
}

// Helper method implementation moved inside the class
uint8_t MCP2515Interface::convertBaudrateToCANSpeed(int baudrateKbps) {
    switch(baudrateKbps) {
        case 1000: return CAN_1000KBPS;
        case 800: return CAN_500KBPS;  // Fallback to 500 kbps
        case 500: return CAN_500KBPS;
        case 250: return CAN_250KBPS;
        case 125: return CAN_125KBPS;
        case 100: return CAN_100KBPS;
        case 50: return CAN_50KBPS;
        case 20: return CAN_20KBPS;
        case 10: return CAN_10KBPS;
        default: return CAN_125KBPS;
    }
}

bool MCP2515Interface::begin(uint32_t baudrate) {
    // Convert baudrate to kbps and then to CAN speed
    uint8_t canSpeed = convertBaudrateToCANSpeed(baudrate / 1000);
    
    // Initialize CAN bus
    if (can->begin(MCP_ANY, canSpeed, MCP_8MHZ) == CAN_OK) {
        can->setMode(MCP_NORMAL);
        return true;
    }
    return false;
}

bool MCP2515Interface::sendMessage(uint32_t id, uint8_t ext, uint8_t len, uint8_t *buf) {
    return can->sendMsgBuf(id, ext, len, buf) == CAN_OK;
}

bool MCP2515Interface::receiveMessage(uint32_t *id, uint8_t *ext, uint8_t *len, uint8_t *buf) {
    if (!messageAvailable()) {
        return false;
    }
    
    byte result = can->readMsgBuf(id, ext, len, buf);
    return result == CAN_OK;
}

bool MCP2515Interface::messageAvailable() {
    return !digitalRead(intPin);  // Check interrupt pin
}

void MCP2515Interface::end() {
    can->setMode(MCP_SLEEP);
}