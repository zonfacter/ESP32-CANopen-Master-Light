// TJA1051Interface.cpp
#include "TJA1051Interface.h"

TJA1051Interface::TJA1051Interface(uint8_t stbyPin) 
    : initialized(false), stbyPin(stbyPin) {
    
    // Standby-Pin konfigurieren, falls vorhanden
    if (stbyPin != 255) {
        pinMode(stbyPin, OUTPUT);
        digitalWrite(stbyPin, LOW);  // Aktiv-Modus
    }
}

TJA1051Interface::~TJA1051Interface() {
    end();
}

bool TJA1051Interface::begin(uint32_t baudrate) {
    Serial.println("[DEBUG] TJA1051 Initialisierung gestartet");

    // Explizite GPIO-Konfiguration für TX und RX
    gpio_reset_pin((gpio_num_t)18);  // TX Pin
    gpio_reset_pin((gpio_num_t)17);  // RX Pin

    // Generische TWAI-Konfiguration mit expliziten Pins
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)18,   // TX Pin für ESP32-S3-Touch-LCD-4.3B
        (gpio_num_t)17,   // RX Pin für ESP32-S3-Touch-LCD-4.3B
        TWAI_MODE_NORMAL
    );

    // Baudrate-spezifische Timing-Konfiguration
    twai_timing_config_t t_config;
    switch(baudrate / 1000) {
        case 1000:
            t_config = TWAI_TIMING_CONFIG_1MBITS();
            break;
        case 500:
            t_config = TWAI_TIMING_CONFIG_500KBITS();
            break;
        case 250:
            t_config = TWAI_TIMING_CONFIG_250KBITS();
            break;
        case 125:
            t_config = TWAI_TIMING_CONFIG_125KBITS();
            break;
        default:
            Serial.printf("[FEHLER] Nicht unterstützte Baudrate: %lu\n", baudrate);
            return false;
    }

    // Filter-Konfiguration
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // Treiber-Installation
    esp_err_t result = twai_driver_install(&g_config, &t_config, &f_config);
    if (result != ESP_OK) {
        Serial.printf("[FEHLER] TWAI-Treiber-Installation fehlgeschlagen: %s\n", esp_err_to_name(result));
        return false;
    }

    // TWAI-Treiber starten
    result = twai_start();
    if (result != ESP_OK) {
        Serial.printf("[FEHLER] TWAI-Start fehlgeschlagen: %s\n", esp_err_to_name(result));
        twai_driver_uninstall();
        return false;
    }

    Serial.println("[DEBUG] TJA1051 erfolgreich initialisiert");
    initialized = true;
    return true;
}

bool TJA1051Interface::sendMessage(uint32_t id, uint8_t ext, uint8_t len, uint8_t *buf) {
    if (!initialized) return false;

    twai_message_t message;
    message.identifier = id;
    message.extd = ext ? 1 : 0;
    message.data_length_code = len;
    
    memcpy(message.data, buf, len);

    esp_err_t result = twai_transmit(&message, pdMS_TO_TICKS(1000));
    return result == ESP_OK;
}

bool TJA1051Interface::receiveMessage(uint32_t *id, uint8_t *ext, uint8_t *len, uint8_t *buf) {
    if (!initialized) return false;

    twai_message_t message;
    esp_err_t result = twai_receive(&message, pdMS_TO_TICKS(100));
    
    if (result != ESP_OK) return false;

    *id = message.identifier;
    *ext = message.extd;
    *len = message.data_length_code;
    
    memcpy(buf, message.data, *len);
    return true;
}

bool TJA1051Interface::messageAvailable() {
    if (!initialized) return false;

    twai_status_info_t status;
    twai_get_status_info(&status);
    
    return status.msgs_to_rx > 0;
}

void TJA1051Interface::end() {
    if (initialized) {
        twai_stop();
        twai_driver_uninstall();
        initialized = false;
    }

    // Standby-Pin in Standby-Modus setzen, falls vorhanden
    if (stbyPin != 255) {
        digitalWrite(stbyPin, HIGH);
    }
}