// DisplayConfig.h
// ===============================================================================
// Zentrale Konfigurationsdatei für Display-Konstanten und Menü-Parameter
// ===============================================================================

#ifndef DISPLAY_CONFIG_H
#define DISPLAY_CONFIG_H

// ================================
// OLED Display Konstanten
// ================================
#define OLED_WIDTH           128    // OLED Breite in Pixeln
#define OLED_HEIGHT          64     // OLED Höhe in Pixeln
#define OLED_I2C_ADDR        0x3C   // Standard I2C-Adresse für SSD1306
#define OLED_RESET_PIN       -1     // Reset-Pin (-1 = gemeinsame Nutzung)

// Text-Layout für OLED
#define OLED_TEXT_SIZE_SMALL     1  // Kleine Textgröße
#define OLED_TEXT_SIZE_MEDIUM    2  // Mittlere Textgröße
#define OLED_TEXT_SIZE_LARGE     3  // Große Textgröße
#define OLED_LINE_HEIGHT         10 // Zeilenhöhe in Pixeln (bei Textgröße 1)
#define OLED_CHAR_WIDTH          6  // Zeichenbreite in Pixeln (bei Textgröße 1)
#define OLED_MAX_LINES           6  // Maximale Anzahl Zeilen (64 / 10 = 6.4)
#define OLED_MAX_CHARS_PER_LINE  21 // Maximale Zeichen pro Zeile (128 / 6 = 21.3)

// ================================
// TFT Display Konstanten
// ================================
#define TFT_WIDTH            800    // TFT Breite in Pixeln
#define TFT_HEIGHT           480    // TFT Höhe in Pixeln

// Text-Layout für TFT
#define TFT_TEXT_SIZE_SMALL      2  // Kleine Textgröße
#define TFT_TEXT_SIZE_MEDIUM     3  // Mittlere Textgröße
#define TFT_TEXT_SIZE_LARGE      4  // Große Textgröße
#define TFT_LINE_HEIGHT          24 // Zeilenhöhe in Pixeln
#define TFT_CHAR_WIDTH           12 // Zeichenbreite in Pixeln
#define TFT_MAX_LINES            20 // Maximale Anzahl Zeilen
#define TFT_MAX_CHARS_PER_LINE   66 // Maximale Zeichen pro Zeile

// ================================
// Menü-Konstanten
// ================================
#define MENU_MAX_ITEMS           8  // Maximale Anzahl Menüeinträge pro Seite
#define MENU_TITLE_HEIGHT        12 // Höhe des Menütitels in Pixeln
#define MENU_ITEM_HEIGHT         10 // Höhe eines Menüeintrags in Pixeln
#define MENU_INDENT              4  // Einrückung in Pixeln
#define MENU_CURSOR_CHAR         ">"  // Zeichen für Cursor/Auswahl
#define MENU_MAX_DEPTH           10 // Maximale Menü-Tiefe (für Zurück-Navigation)

// ================================
// Button-Konstanten
// ================================
#define BUTTON_DEBOUNCE_MS       50  // Entprellzeit in Millisekunden
#define BUTTON_LONG_PRESS_MS     800 // Long-Press Zeit in Millisekunden

// ================================
// Timeout-Konstanten
// ================================
#define CONTROL_SOURCE_TIMEOUT_MS  3000  // 3 Sekunden Timeout für Steuerung
#define VERSION_DISPLAY_TIMEOUT_MS 2000  // 2 Sekunden für Version-Anzeige
#define ACTION_SCREEN_TIMEOUT_MS   1000  // 1 Sekunde für Aktions-Bildschirme

// ================================
// Farben (für Display-Kompatibilität)
// ================================
#define COLOR_WHITE          0xFFFF  // Weiß (für alle Display-Typen)
#define COLOR_BLACK          0x0000  // Schwarz (für alle Display-Typen)
#define COLOR_BLUE           0x001F  // Blau (TFT)
#define COLOR_RED            0xF800  // Rot (TFT)
#define COLOR_GREEN          0x07E0  // Grün (TFT)
#define COLOR_YELLOW         0xFFE0  // Gelb (TFT)
#define COLOR_CYAN           0x07FF  // Cyan (TFT)
#define COLOR_MAGENTA        0xF81F  // Magenta (TFT)

// ================================
// Menü-Texte und Labels (Deutsch)
// ================================
// Hauptmenü
#define MENU_LABEL_SCANNER       "Scanner"
#define MENU_LABEL_MONITOR       "Monitor"
#define MENU_LABEL_CONFIG        "Konfiguration"
#define MENU_LABEL_SYSTEM        "System"
#define MENU_LABEL_HELP          "Hilfe"
#define MENU_LABEL_BACK          "Zurueck"
#define MENU_LABEL_VERSION       "Version"

// Scanner-Menü
#define MENU_LABEL_NODE_SCAN     "Node-Scan"
#define MENU_LABEL_AUTO_BAUDRATE "Auto-Baudrate"
#define MENU_LABEL_NODE_TEST     "Node-Test"
#define MENU_LABEL_START_SCAN    "Start Scan"
#define MENU_LABEL_RANGE_START   "Bereich Start"
#define MENU_LABEL_RANGE_END     "Bereich Ende"

// System-Status
#define STATUS_LABEL_IDLE        "Bereit"
#define STATUS_LABEL_SCANNING    "Scannt..."
#define STATUS_LABEL_FOUND       "Gefunden"
#define STATUS_LABEL_TIMEOUT     "Timeout"
#define STATUS_LABEL_ERROR       "Fehler"
#define STATUS_LABEL_SUCCESS     "Erfolg"

// ================================
// Erweiterungspunkte für neue Menüs
// ================================
// Um ein neues Menü hinzuzufügen:
// 1. Definiere eine neue MenuID in OLEDMenu.h (enum MenuID)
// 2. Erstelle ein MenuItem-Array mit den Menüeinträgen
// 3. Füge das Array zur getMenuItems()-Funktion hinzu
// 4. Erstelle eine Action-Funktion für das Menü (falls erforderlich)
// 5. Optional: Füge neue Labels oben zu den Menü-Texten hinzu

// Beispiel für neues Menü:
// MenuItem myNewMenuItems[] = {
//     {MENU_LABEL_MY_ITEM1, MENU_MY_SUBMENU, ACTION_EXECUTE, myFunction1},
//     {MENU_LABEL_MY_ITEM2, MENU_MY_SUBMENU, ACTION_EXECUTE, myFunction2},
//     {MENU_LABEL_BACK, MENU_MAIN, ACTION_BACK, NULL}
// };

#endif // DISPLAY_CONFIG_H
