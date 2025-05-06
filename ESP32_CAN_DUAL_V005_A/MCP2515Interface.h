#ifndef MCP2515_INTERFACE_H
#define MCP2515_INTERFACE_H

#include "CANInterface.h"
#include <mcp_can.h>

class MCP2515Interface : public CANInterface {
private:
    MCP_CAN *can;
    uint8_t intPin;
    
    // Private method for baudrate conversion
    uint8_t convertBaudrateToCANSpeed(int baudrateKbps);
    
public:
    MCP2515Interface(uint8_t csPin, uint8_t intPin);
    ~MCP2515Interface();
    
    bool begin(uint32_t baudrate) override;
    bool sendMessage(uint32_t id, uint8_t ext, uint8_t len, uint8_t *buf) override;
    bool receiveMessage(uint32_t *id, uint8_t *ext, uint8_t *len, uint8_t *buf) override;
    bool messageAvailable() override;
    void end() override;
};

#endif // MCP2515_INTERFACE_H