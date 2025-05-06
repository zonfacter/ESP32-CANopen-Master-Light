#define USER_SETUP_INFO "User_Setup"

// Display-Treiber
#define ILI9488_DRIVER
#define TFT_WIDTH  800
#define TFT_HEIGHT 480

// PIN-Konfiguration
#define TFT_MISO 6
#define TFT_MOSI 7
#define TFT_SCLK 8
#define TFT_CS   5    // Chip Select
#define TFT_DC   4    // Data/Command
#define TFT_RST  9    // Reset

#define TOUCH_CS 3    // Touch Controller CS Pin

// SPI-Geschwindigkeit
#define SPI_FREQUENCY  27000000
#define SPI_READ_FREQUENCY 20000000

// Weitere Optionen
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF