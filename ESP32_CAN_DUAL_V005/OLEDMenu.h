// OLEDMenu.h
// ===============================================================================
// Header-Datei für das 3-Tasten OLED-Menüsystem
// ===============================================================================

#ifndef OLED_MENU_H
#define OLED_MENU_H

#include <Arduino.h>
#include "DisplayInterface.h"

// Button Pins definieren
#define BUTTON_UP      26   // GPIO-Pin für UP-Taste
#define BUTTON_DOWN    27   // GPIO-Pin für DOWN-Taste
#define BUTTON_SELECT  25   // GPIO-Pin für SELECT/BACK-Taste

#define CAN_INT 4  // CAN-Interrupt-Pin

// Button-Parameter
#define DEBOUNCE_TIME    50   // ms Entprellzeit
#define LONG_PRESS_TIME  800  // ms für langes Drücken (zurück)

// Steuerungsquelle
enum ControlSource {
    SOURCE_NONE,
    SOURCE_SERIAL,
    SOURCE_BUTTON,
    SOURCE_AUTO
};

// Menü IDs - jedes Menü hat eine eindeutige ID
enum MenuID {
    MENU_MAIN = 0,
    MENU_SCANNER,
    MENU_SCANNER_NODE,
    MENU_SCANNER_AUTO,
    MENU_SCANNER_TEST,
    MENU_MONITOR,
    MENU_MONITOR_FILTER,
    MENU_CONFIG,
    MENU_CONFIG_NODE,
    MENU_CONFIG_BAUDRATE,
    MENU_CONFIG_PROFILE,
    MENU_SYSTEM,
    MENU_HELP,
    // Untermenüs für Eingabefelder
    MENU_INPUT_NODERANGE_START,
    MENU_INPUT_NODERANGE_END,
    MENU_INPUT_OLDNODE,
    MENU_INPUT_NEWNODE,
    MENU_INPUT_BAUDRATE,
    MENU_INPUT_NODEFORTEST
};

// Status der Menü-Aktion
enum ActionStatus {
    ACTION_NONE,
    ACTION_SUBMENU,
    ACTION_EXECUTE,
    ACTION_BACK,
    ACTION_INPUT_START,
    ACTION_INPUT_CANCEL,
    ACTION_INPUT_SAVE
};

// Struktur für ein Menüelement
struct MenuItem {
    const char* text;     // Anzeigetext
    MenuID targetMenu;    // Zielmenü bei Auswahl (oder aktuelle Menü-ID für Aktionen)
    ActionStatus action;  // Was passiert bei Auswahl
    void (*callback)();   // Optionale Callback-Funktion
};

// Struktur für ein komplettes Menü
struct Menu {
    const char* title;        // Menütitel
    MenuItem* items;          // Array von Menüelementen
    uint8_t itemCount;        // Anzahl der Elemente
    MenuID parentMenu;        // Übergeordnetes Menü für Zurück-Funktion
};

// Struktur für Eingabewerte
struct InputValues {
    int scanStartNode = 1;
    int scanEndNode = 10;
    int oldNodeId = 1;
    int newNodeId = 1;
    int selectedBaudrate = 125;
    int nodeForBaudrate = 1;
    int nodeForTest = 10;
};

// Initialisierung des Menüsystems
void menuInit();

// Button-Verarbeitung
void handleButtons();

// Menü-Anzeige
void displayMenu();

// Hilfsfunktionen
bool buttonActivity();
void handleUpButton();
void handleDownButton();
void handleSelectButton();
void handleBackButton();
void displaySerialModeScreen();
void displayActionScreen(const char* title, const char* message, int timeout);

// Eingabemodus
void startInputMode(MenuID inputType);
void saveInputValue();
void displayInputScreen(MenuID inputType);

// Aktionen
void startNodeScan();
void startAutoBaudrateDetection();
void startNodeTest();
void changeNodeIdAction();
void changeBaudrateAction();
void toggleLiveMonitor();
void resetFilterAction();

// Externe Variablen (definiert in OLEDMenu.cpp)
extern ControlSource activeSource;
extern unsigned long lastActivityTime;
extern const unsigned long SOURCE_TIMEOUT;
extern InputValues inputValues;

#endif // OLED_MENU_H