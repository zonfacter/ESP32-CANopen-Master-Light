// TJA1051Interface.h
#ifndef TJA1051_INTERFACE_H
#define TJA1051_INTERFACE_H

#include "CANInterface.h"
#include "driver/twai.h"

// TJA1051Interface.h
#define TJA1051_TX_PIN 18  // Für ESP32-S3-Touch-LCD-4.3B
#define TJA1051_RX_PIN 17  // Für ESP32-S3-Touch-LCD-4.3B

class TJA1051Interface : public CANInterface {
private:
    bool initialized;
    uint8_t stbyPin;

    // TWAI-Konfigurationsstruktur
    twai_general_config_t g_config;
    twai_timing_config_t t_config;
    twai_filter_config_t f_config;

public:
    TJA1051Interface(uint8_t stbyPin = 255);
    ~TJA1051Interface();
    
    bool begin(uint32_t baudrate) override;
    bool sendMessage(uint32_t id, uint8_t ext, uint8_t len, uint8_t *buf) override;
    bool receiveMessage(uint32_t *id, uint8_t *ext, uint8_t *len, uint8_t *buf) override;
    bool messageAvailable() override;
    void end() override;
};

#endif