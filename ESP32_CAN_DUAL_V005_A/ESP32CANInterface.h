#ifndef ESP32_CAN_INTERFACE_H
#define ESP32_CAN_INTERFACE_H

#include "CANInterface.h"
// Commented out CAN library include
// #include <CAN.h>

class ESP32CANInterface : public CANInterface {
private:
    bool initialized;
    
public:
    ESP32CANInterface();
    ~ESP32CANInterface();
    
    bool begin(uint32_t baudrate) override;
    bool sendMessage(uint32_t id, uint8_t ext, uint8_t len, uint8_t *buf) override;
    bool receiveMessage(uint32_t *id, uint8_t *ext, uint8_t *len, uint8_t *buf) override;
    bool messageAvailable() override;
    void end() override;
};

#endif // ESP32_CAN_INTERFACE_H