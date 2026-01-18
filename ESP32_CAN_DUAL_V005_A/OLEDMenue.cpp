// OLEDMenu.cpp
// ===============================================================================
// Implementierung des 3-Tasten OLED-Menüsystems
// ===============================================================================

#include "OLEDMenu.h"
#include "CANopen.h"

// Zustandsvariablen für die Steuerung
ControlSource activeSource = SOURCE_NONE;
unsigned long lastActivityTime = 0;
const unsigned long SOURCE_TIMEOUT = 3000; // 3 Sekunden Timeout
constexpr int VERSION_DISPLAY_TIMEOUT_MS = 2000;

// Zustandsvariablen für Buttons
bool buttonUpPressed = false;
bool buttonDownPressed = false;
bool buttonSelectPressed = false;
unsigned long buttonSelectPressTime = 0;
bool longPressDetected = false;

// Aktuelle Menüposition
MenuID currentMenuID = MENU_MAIN;
int currentMenuIndex = 0;

// Menünavigationshistorie für Zurück-Funktion
#define MAX_MENU_HISTORY 10
MenuID menuHistory[MAX_MENU_HISTORY];
int menuHistoryIndex = 0;

// Eingabewerte
InputValues inputValues;

// Eingabezustand
bool inInputMode = false;
int currentInputValue = 0;
int inputMin = 0;
int inputMax = 127;
int inputStep = 1;
bool showingVersion = false;
unsigned long versionDisplayStart = 0;

// Externe Referenzen zu Variablen aus dem Hauptprogramm
extern DisplayInterface* displayInterface;
extern bool scanning;
extern bool autoBaudrateRequest;
extern bool liveMonitor;
extern bool systemError;
extern uint8_t scanStart;
extern uint8_t scanEnd;
extern int currentBaudrate;
extern bool filterEnabled;
extern const char* getAppVersion();
extern int getDisplayWidth();
extern int getDisplayHeight();

// Vorwärtsdeklarationen externer Funktionen
void scanNodes(int startID, int endID);
bool testSingleNode(int nodeId, int maxAttempts, int timeoutMs);
void changeNodeId(uint8_t from, uint8_t to);
bool updateESP32CANBaudrate(int newBaudrate);
void changeCommunicationSettings(uint8_t targetNodeId, int newBaudrateKbps);
int getBaudrateIndex(int baudrateKbps);
int convertBaudrateToRealBaudrate(int index);
extern void handleSerialCommands();
extern void processCANScanning();
extern void processAutoBaudrate();
extern void processCANMessage();
void showVersionAction();


// Menü-Definitionen
// Hauptmenü-Elemente
MenuItem mainMenuItems[] = {
    {"Scanner", MENU_SCANNER, ACTION_SUBMENU, NULL},
    {"Monitor", MENU_MONITOR, ACTION_SUBMENU, NULL},
    {"Konfiguration", MENU_CONFIG, ACTION_SUBMENU, NULL},
    {"System", MENU_SYSTEM, ACTION_SUBMENU, NULL},
    {"Hilfe", MENU_HELP, ACTION_SUBMENU, NULL}
};

// Scanner-Menü-Elemente
MenuItem scannerMenuItems[] = {
    {"Node-Scan", MENU_SCANNER_NODE, ACTION_SUBMENU, NULL},
    {"Auto-Baudrate", MENU_SCANNER_AUTO, ACTION_SUBMENU, NULL},
    {"Node-Test", MENU_SCANNER_TEST, ACTION_SUBMENU, NULL},
    {"Zurueck", MENU_MAIN, ACTION_BACK, NULL}
};

// Node-Scan-Menü-Elemente
MenuItem nodeScanMenuItems[] = {
    {"Start Scan", MENU_SCANNER_NODE, ACTION_EXECUTE, startNodeScan},
    {"Bereich Start", MENU_INPUT_NODERANGE_START, ACTION_INPUT_START, NULL},
    {"Bereich Ende", MENU_INPUT_NODERANGE_END, ACTION_INPUT_START, NULL},
    {"Zurueck", MENU_SCANNER, ACTION_BACK, NULL}
};

// Auto-Baudrate-Menü-Elemente
MenuItem autoBaudrateMenuItems[] = {
    {"Starten", MENU_SCANNER_AUTO, ACTION_EXECUTE, startAutoBaudrateDetection},
    {"Zurueck", MENU_SCANNER, ACTION_BACK, NULL}
};

// Node-Test-Menü-Elemente
MenuItem nodeTestMenuItems[] = {
    {"Node-ID", MENU_INPUT_NODEFORTEST, ACTION_INPUT_START, NULL},
    {"Test starten", MENU_SCANNER_TEST, ACTION_EXECUTE, startNodeTest},
    {"Zurueck", MENU_SCANNER, ACTION_BACK, NULL}
};

// Monitor-Menü-Elemente
MenuItem monitorMenuItems[] = {
    {"Live Monitor", MENU_MONITOR, ACTION_EXECUTE, toggleLiveMonitor},
    {"Filter", MENU_MONITOR_FILTER, ACTION_SUBMENU, NULL},
    {"Zurueck", MENU_MAIN, ACTION_BACK, NULL}
};

// Monitor-Filter-Menü-Elemente
MenuItem monitorFilterMenuItems[] = {
    {"ID-Filter", MENU_MONITOR_FILTER, ACTION_EXECUTE, NULL}, // Implementierung fehlt
    {"Node-Filter", MENU_MONITOR_FILTER, ACTION_EXECUTE, NULL}, // Implementierung fehlt
    {"Typ-Filter", MENU_MONITOR_FILTER, ACTION_EXECUTE, NULL}, // Implementierung fehlt
    {"Filter zuruecksetzen", MENU_MONITOR_FILTER, ACTION_EXECUTE, resetFilterAction},
    {"Zurueck", MENU_MONITOR, ACTION_BACK, NULL}
};

// Konfigurations-Menü-Elemente
MenuItem configMenuItems[] = {
    {"Node-ID aendern", MENU_CONFIG_NODE, ACTION_SUBMENU, NULL},
    {"Baudrate", MENU_CONFIG_BAUDRATE, ACTION_SUBMENU, NULL},
    {"System-Profil", MENU_CONFIG_PROFILE, ACTION_SUBMENU, NULL},
    {"Zurueck", MENU_MAIN, ACTION_BACK, NULL}
};

// Node-ID-Ändern-Menü-Elemente
MenuItem nodeIdMenuItems[] = {
    {"Alte ID", MENU_INPUT_OLDNODE, ACTION_INPUT_START, NULL},
    {"Neue ID", MENU_INPUT_NEWNODE, ACTION_INPUT_START, NULL},
    {"Ausfuehren", MENU_CONFIG_NODE, ACTION_EXECUTE, changeNodeIdAction},
    {"Zurueck", MENU_CONFIG, ACTION_BACK, NULL}
};

// Baudrate-Menü-Elemente
MenuItem baudrateMenuItems[] = {
    {"Local Baudrate", MENU_INPUT_BAUDRATE, ACTION_INPUT_START, NULL},
    {"Node Baudrate", MENU_CONFIG_BAUDRATE, ACTION_EXECUTE, changeBaudrateAction},
    {"Zurueck", MENU_CONFIG, ACTION_BACK, NULL}
};

// System-Profil-Menü-Elemente
MenuItem profileMenuItems[] = {
    {"OLED + MCP2515", MENU_CONFIG_PROFILE, ACTION_EXECUTE, NULL}, // Implementierung fehlt
    {"TFT + TJA1051", MENU_CONFIG_PROFILE, ACTION_EXECUTE, NULL}, // Implementierung fehlt
    {"Zurueck", MENU_CONFIG, ACTION_BACK, NULL}
};

// System-Menü-Elemente
MenuItem systemMenuItems[] = {
    {"Info anzeigen", MENU_SYSTEM, ACTION_EXECUTE, NULL}, // Implementierung fehlt
    {"Einst. speichern", MENU_SYSTEM, ACTION_EXECUTE, NULL}, // Implementierung fehlt
    {"Einst. laden", MENU_SYSTEM, ACTION_EXECUTE, NULL}, // Implementierung fehlt
    {"System zuruecksetzen", MENU_SYSTEM, ACTION_EXECUTE, NULL}, // Implementierung fehlt
    {"Zurueck", MENU_MAIN, ACTION_BACK, NULL}
};

// Hilfe-Menü-Elemente
MenuItem helpMenuItems[] = {
    {"Menue-Hilfe", MENU_HELP, ACTION_EXECUTE, NULL}, // Implementierung fehlt
    {"Version", MENU_HELP, ACTION_EXECUTE, showVersionAction},
    {"Zurueck", MENU_MAIN, ACTION_BACK, NULL}
};

// Liste aller Menüs
Menu allMenus[] = {
    {"Hauptmenue", mainMenuItems, sizeof(mainMenuItems) / sizeof(MenuItem), MENU_MAIN},
    {"Scanner", scannerMenuItems, sizeof(scannerMenuItems) / sizeof(MenuItem), MENU_MAIN},
    {"Node-Scan", nodeScanMenuItems, sizeof(nodeScanMenuItems) / sizeof(MenuItem), MENU_SCANNER},
    {"Auto-Baudrate", autoBaudrateMenuItems, sizeof(autoBaudrateMenuItems) / sizeof(MenuItem), MENU_SCANNER},
    {"Node-Test", nodeTestMenuItems, sizeof(nodeTestMenuItems) / sizeof(MenuItem), MENU_SCANNER},
    {"Monitor", monitorMenuItems, sizeof(monitorMenuItems) / sizeof(MenuItem), MENU_MAIN},
    {"Monitor-Filter", monitorFilterMenuItems, sizeof(monitorFilterMenuItems) / sizeof(MenuItem), MENU_MONITOR},
    {"Konfiguration", configMenuItems, sizeof(configMenuItems) / sizeof(MenuItem), MENU_MAIN},
    {"Node-ID aendern", nodeIdMenuItems, sizeof(nodeIdMenuItems) / sizeof(MenuItem), MENU_CONFIG},
    {"Baudrate", baudrateMenuItems, sizeof(baudrateMenuItems) / sizeof(MenuItem), MENU_CONFIG},
    {"System-Profil", profileMenuItems, sizeof(profileMenuItems) / sizeof(MenuItem), MENU_CONFIG},
    {"System", systemMenuItems, sizeof(systemMenuItems) / sizeof(MenuItem), MENU_MAIN},
    {"Hilfe", helpMenuItems, sizeof(helpMenuItems) / sizeof(MenuItem), MENU_MAIN}
};

// Hilfsfunktion: Index eines Menüs anhand seiner ID finden
int getMenuIndexByID(MenuID id) {
    for (int i = 0; i < sizeof(allMenus) / sizeof(Menu); i++) {
        if (id == (MenuID)i) {
            return i;
        }
    }
    return 0; // Hauptmenü als Fallback
}

// Initialisierung des Menüsystems
void menuInit() {
    // Pins für Buttons konfigurieren
    pinMode(BUTTON_UP, INPUT_PULLUP);
    pinMode(BUTTON_DOWN, INPUT_PULLUP);
    pinMode(BUTTON_SELECT, INPUT_PULLUP);
    
    // Menü initialisieren
    currentMenuID = MENU_MAIN;
    currentMenuIndex = 0;
    menuHistoryIndex = 0;
    
    // Standardwerte laden
    inputValues.scanStartNode = scanStart;
    inputValues.scanEndNode = scanEnd;
    inputValues.selectedBaudrate = getBaudrateIndex(currentBaudrate);
    
    // Erstes Menü anzeigen
    displayMenu();
}

// UP-Button-Handler
void handleUpButton() {
    if (inInputMode) {
        // Im Eingabemodus: Wert erhöhen
        currentInputValue += inputStep;
        if (currentInputValue > inputMax) {
            currentInputValue = inputMax;
        }
    } else {
        // Im Menümodus: Nach oben navigieren
        if (currentMenuIndex > 0) {
            currentMenuIndex--;
        } else {
            // Am Anfang? Zum Ende springen
            int menuIdx = getMenuIndexByID(currentMenuID);
            currentMenuIndex = allMenus[menuIdx].itemCount - 1;
        }
    }
    displayMenu();
}

// DOWN-Button-Handler
void handleDownButton() {
    if (inInputMode) {
        // Im Eingabemodus: Wert verringern
        currentInputValue -= inputStep;
        if (currentInputValue < inputMin) {
            currentInputValue = inputMin;
        }
    } else {
        // Im Menümodus: Nach unten navigieren
        int menuIdx = getMenuIndexByID(currentMenuID);
        if (currentMenuIndex < allMenus[menuIdx].itemCount - 1) {
            currentMenuIndex++;
        } else {
            // Am Ende? Zum Anfang springen
            currentMenuIndex = 0;
        }
    }
    displayMenu();
}

// SELECT-Button-Handler (kurzes Drücken)
void handleSelectButton() {
    if (inInputMode) {
        // Im Eingabemodus: Wert speichern
        saveInputValue();
        inInputMode = false;
        displayMenu();
        return;
    }
    
    // Aktuelles Menü und ausgewählten Eintrag bestimmen
    int menuIdx = getMenuIndexByID(currentMenuID);
    MenuItem selectedItem = allMenus[menuIdx].items[currentMenuIndex];
    
    // Aktion basierend auf dem ausgewählten Element ausführen
    switch (selectedItem.action) {
        case ACTION_SUBMENU:
            // Untermenü öffnen
            menuHistory[menuHistoryIndex++] = currentMenuID;
            if (menuHistoryIndex >= MAX_MENU_HISTORY) menuHistoryIndex = MAX_MENU_HISTORY - 1;
            currentMenuID = selectedItem.targetMenu;
            currentMenuIndex = 0;
            break;
            
        case ACTION_BACK:
            // Zurück zum übergeordneten Menü
            handleBackButton();
            break;
            
        case ACTION_EXECUTE:
            // Funktion ausführen
            if (selectedItem.callback != NULL) {
                selectedItem.callback();
            }
            break;
            
        case ACTION_INPUT_START:
            // Eingabemodus starten
            startInputMode(selectedItem.targetMenu);
            break;
    }
    
    displayMenu();
}

// BACK-Button-Handler (langes Drücken)
void handleBackButton() {
    if (inInputMode) {
        // Im Eingabemodus: Abbrechen und zum Menü zurück
        inInputMode = false;
    } else if (menuHistoryIndex > 0) {
        // Zum vorherigen Menü zurück
        currentMenuID = menuHistory[--menuHistoryIndex];
        currentMenuIndex = 0;
    }
    displayMenu();
}

// Eingabemodus starten
void startInputMode(MenuID inputType) {
    inInputMode = true;
    
    // Eingabewerte basierend auf dem Typ initialisieren
    switch (inputType) {
        case MENU_INPUT_NODERANGE_START:
            currentInputValue = inputValues.scanStartNode;
            inputMin = 1;
            inputMax = 127;
            inputStep = 1;
            break;
            
        case MENU_INPUT_NODERANGE_END:
            currentInputValue = inputValues.scanEndNode;
            inputMin = 1;
            inputMax = 127;
            inputStep = 1;
            break;
            
        case MENU_INPUT_OLDNODE:
            currentInputValue = inputValues.oldNodeId;
            inputMin = 1;
            inputMax = 127;
            inputStep = 1;
            break;
            
        case MENU_INPUT_NEWNODE:
            currentInputValue = inputValues.newNodeId;
            inputMin = 1;
            inputMax = 127;
            inputStep = 1;
            break;
            
        case MENU_INPUT_BAUDRATE:
            currentInputValue = inputValues.selectedBaudrate;
            inputMin = 0;
            inputMax = 8;
            inputStep = 1;
            break;
            
        case MENU_INPUT_NODEFORTEST:
            currentInputValue = inputValues.nodeForTest;
            inputMin = 1;
            inputMax = 127;
            inputStep = 1;
            break;
    }
    
    displayInputScreen(inputType);
}

// Eingabewert speichern
void saveInputValue() {
    switch (currentMenuID) {
        case MENU_INPUT_NODERANGE_START:
            inputValues.scanStartNode = currentInputValue;
            break;
            
        case MENU_INPUT_NODERANGE_END:
            inputValues.scanEndNode = currentInputValue;
            break;
            
        case MENU_INPUT_OLDNODE:
            inputValues.oldNodeId = currentInputValue;
            break;
            
        case MENU_INPUT_NEWNODE:
            inputValues.newNodeId = currentInputValue;
            break;
            
        case MENU_INPUT_BAUDRATE:
            inputValues.selectedBaudrate = currentInputValue;
            break;
            
        case MENU_INPUT_NODEFORTEST:
            inputValues.nodeForTest = currentInputValue;
            break;
    }
}

// Button-Verarbeitung
void handleButtons() {
    static unsigned long lastButtonTime = 0;
    unsigned long currentTime = millis();
    
    // Prüfen, ob eine andere Quelle aktiv ist
    if (activeSource != SOURCE_NONE && activeSource != SOURCE_BUTTON) {
        // Prüfen ob die aktive Quelle abgelaufen ist
        if (millis() - lastActivityTime > SOURCE_TIMEOUT) {
            // Timeout abgelaufen, Button-Steuerung kann übernehmen
            activeSource = SOURCE_NONE;
        } else {
            // Eine andere Quelle ist aktiv, Button-Eingaben ignorieren
            // Aber prüfen, ob ein Button gedrückt wurde (für Übernahmemöglichkeit)
            if (buttonActivity()) {
                // Button wurde gedrückt, Steuerung übernehmen
                activeSource = SOURCE_BUTTON;
                lastActivityTime = millis();
                
                // OLED-Menü wieder anzeigen
                displayMenu();
                
                // Kurze Verzögerung für die Entprellung
                delay(DEBOUNCE_TIME);
                return;
            }
            return;
        }
    }
    
    // Nur alle DEBOUNCE_TIME ms prüfen
    if (currentTime - lastButtonTime < DEBOUNCE_TIME) {
        return;
    }
    
    // UP-Button
    bool upState = digitalRead(BUTTON_UP) == LOW;
    if (upState != buttonUpPressed) {
        buttonUpPressed = upState;
        if (buttonUpPressed) {
            handleUpButton();
            
            // Button-Steuerung wird aktiv
            activeSource = SOURCE_BUTTON;
            lastActivityTime = currentTime;
        }
        lastButtonTime = currentTime;
    }
    
    // DOWN-Button
    bool downState = digitalRead(BUTTON_DOWN) == LOW;
    if (downState != buttonDownPressed) {
        buttonDownPressed = downState;
        if (buttonDownPressed) {
            handleDownButton();
            
            // Button-Steuerung wird aktiv
            activeSource = SOURCE_BUTTON;
            lastActivityTime = currentTime;
        }
        lastButtonTime = currentTime;
    }
    
    // SELECT/BACK-Button
    bool selectState = digitalRead(BUTTON_SELECT) == LOW;
    
    // Wenn der Button gedrückt wird, Zeit speichern
    if (selectState && !buttonSelectPressed) {
        buttonSelectPressTime = currentTime;
        longPressDetected = false;
        buttonSelectPressed = true;
    }
    
    // Wenn Button gehalten wird, prüfen auf langen Druck
    if (selectState && buttonSelectPressed) {
        if (!longPressDetected && (currentTime - buttonSelectPressTime > LONG_PRESS_TIME)) {
            longPressDetected = true;
            handleBackButton();
            
            // Button-Steuerung wird aktiv
            activeSource = SOURCE_BUTTON;
            lastActivityTime = currentTime;
        }
    }
    
    // Wenn Button losgelassen wird
    if (!selectState && buttonSelectPressed) {
        // Wenn kein langer Druck erkannt wurde, als kurzen Druck behandeln
        if (!longPressDetected) {
            handleSelectButton();
            
            // Button-Steuerung wird aktiv
            activeSource = SOURCE_BUTTON;
            lastActivityTime = currentTime;
        }
        buttonSelectPressed = false;
        lastButtonTime = currentTime;
    }
}

// Prüfen auf Button-Aktivität
bool buttonActivity() {
    return (digitalRead(BUTTON_UP) == LOW || 
            digitalRead(BUTTON_DOWN) == LOW || 
            digitalRead(BUTTON_SELECT) == LOW);
}

// Menü anzeigen
void displayMenu() {
    if (displayInterface == nullptr) {
        return;
    }
    
    if (inInputMode) {
        displayInputScreen(currentMenuID);
        return;
    }
    
    // Aktuelles Menü finden
    int menuIdx = getMenuIndexByID(currentMenuID);
    Menu currentMenu = allMenus[menuIdx];
    
    // Display leeren
    displayInterface->clear();
    
    const int displayWidth = getDisplayWidth();
    const int displayHeight = getDisplayHeight();

    // Menütitel anzeigen
    displayInterface->setCursor(0, 0);
    displayInterface->setTextSize(1);
    displayInterface->println(currentMenu.title);
    
    // Trennlinie
    displayInterface->drawLine(0, 10, displayWidth, 10, 1);
    
    // Sichtbare Menüeinträge bestimmen (max. 5)
    int startItem = max(0, currentMenuIndex - 2);
    int endItem = min(currentMenu.itemCount - 1, startItem + 4);
    
    // Menüeinträge anzeigen
    for (int i = startItem; i <= endItem; i++) {
        displayInterface->setCursor(5, 14 + (i - startItem) * 10);
        
        // Ausgewählten Eintrag markieren
        if (i == currentMenuIndex) {
            displayInterface->fillRect(0, 12 + (i - startItem) * 10, displayWidth, 10, 1);
            displayInterface->setTextColor(0);
        } else {
            displayInterface->setTextColor(1);
        }
        
        displayInterface->print(currentMenu.items[i].text);
        
        // Textfarbe zurücksetzen
        displayInterface->setTextColor(1);
    }
    
    // Scrollbalken anzeigen wenn nötig
    if (currentMenu.itemCount > 5) {
        int menuAreaHeight = displayHeight - 12;
        int barHeight = menuAreaHeight / currentMenu.itemCount;
        int barPos = (menuAreaHeight - barHeight) * currentMenuIndex / (currentMenu.itemCount - 1);
        int barX = displayWidth - 1;
        displayInterface->drawRect(barX, 12, 1, menuAreaHeight, 1);
        displayInterface->fillRect(barX, 12 + barPos, 1, barHeight, 1);
    }
    
    displayInterface->display();
}

// Eingabebildschirm anzeigen
void displayInputScreen(MenuID inputType) {
    if (displayInterface == nullptr) {
        return;
    }
    
    const int displayWidth = getDisplayWidth();

    // Display leeren
    displayInterface->clear();
    
    // Titel je nach Eingabetyp
    displayInterface->setCursor(0, 0);
    displayInterface->setTextSize(1);
    
    const char* title = "Eingabe";
    const char* unit = "";
    
    switch (inputType) {
        case MENU_INPUT_NODERANGE_START:
            title = "Scan Start-Node";
            break;
        case MENU_INPUT_NODERANGE_END:
            title = "Scan End-Node";
            break;
        case MENU_INPUT_OLDNODE:
            title = "Alte Node-ID";
            break;
        case MENU_INPUT_NEWNODE:
            title = "Neue Node-ID";
            break;
        case MENU_INPUT_BAUDRATE:
            title = "Baudrate";
            unit = "kbps";
            break;
        case MENU_INPUT_NODEFORTEST:
            title = "Test Node-ID";
            break;
        default:
            break;
    }
    
    displayInterface->println(title);
    displayInterface->drawLine(0, 10, displayWidth, 10, 1);
    
    // Aktuellen Wert anzeigen
    displayInterface->setCursor(20, 25);
    displayInterface->setTextSize(2);
    
    // Bei Baudrate die entsprechende Baudrate anzeigen
    if (inputType == MENU_INPUT_BAUDRATE) {
        int baudrates[] = {10, 20, 50, 100, 125, 250, 500, 800, 1000};
        displayInterface->print(baudrates[currentInputValue]);
    } else {
        displayInterface->print(currentInputValue);
    }
    displayInterface->print(" ");
    displayInterface->print(unit);
    
    // Auf- und Ab-Pfeile anzeigen
    displayInterface->setTextSize(1);
    displayInterface->setCursor(60, 20);
    displayInterface->print("^");
    displayInterface->setCursor(60, 35);
    displayInterface->print("v");
    
    // Hinweise
    displayInterface->setCursor(0, 50);
    displayInterface->println("Kurz: Speichern");
    displayInterface->println("Lang: Abbrechen");
    
    displayInterface->display();
}

// Serieller Modus-Bildschirm anzeigen
void displaySerialModeScreen() {
    if (displayInterface == nullptr) {
        return;
    }
    
    const int displayWidth = getDisplayWidth();

    displayInterface->clear();
    
    displayInterface->setCursor(0, 0);
    displayInterface->setTextSize(1);
    displayInterface->println("Serieller Modus");
    displayInterface->drawLine(0, 10, displayWidth, 10, 1);
    
    displayInterface->setCursor(5, 25);
    displayInterface->println("Steuerung per Terminal");
    displayInterface->setCursor(5, 40);
    displayInterface->println("Taste druecken fuer");
    displayInterface->setCursor(5, 50);
    displayInterface->println("Menue-Steuerung");
    
    displayInterface->display();
}

// Aktionsbildschirm anzeigen
void displayActionScreen(const char* title, const char* message, int timeout) {
    if (displayInterface == nullptr) {
        return;
    }
    
    const int displayWidth = getDisplayWidth();

    // Display leeren
    displayInterface->clear();
    
    // Titel anzeigen
    displayInterface->setCursor(0, 0);
    displayInterface->setTextSize(1);
    displayInterface->println(title);
    displayInterface->drawLine(0, 10, displayWidth, 10, 1);
    
    // Nachricht anzeigen
    displayInterface->setCursor(5, 25);
    displayInterface->println(message);
    
    displayInterface->display();
    
    // Optional: Timeout
    if (timeout > 0) {
        delay(timeout);
    }
}

// Node-Scan starten
void startNodeScan() {
    // Steuerungsquelle auf AUTO setzen
    activeSource = SOURCE_AUTO;
    lastActivityTime = millis();
    
    displayActionScreen("Node-Scan", "Scanne...", 2000);
    
    // Scanbereich setzen
    scanStart = inputValues.scanStartNode;
    scanEnd = inputValues.scanEndNode;
    
    // Scan starten
    scanning = true;
    
    // Warten bis der Scan abgeschlossen ist
    while (scanning) {
        // Auch während des Scans serielle Befehle prüfen
        handleSerialCommands();
        
        // Buttons mit reduzierter Priorität behandeln
        if (buttonActivity()) {
            // Scan abbrechen, wenn ein Button gedrückt wurde
            scanning = false;
            displayMenu();
            activeSource = SOURCE_BUTTON;
            lastActivityTime = millis();
            return;
        }
        
        // Scan-Prozess ausführen
        processCANScanning();
        
        delay(50);
    }
    
    // Nach dem Scan zum Menü zurückkehren
    displayMenu();
    activeSource = SOURCE_BUTTON;
    lastActivityTime = millis();
}

// Auto-Baudrate-Erkennung starten
void startAutoBaudrateDetection() {
    // Steuerungsquelle auf AUTO setzen
    activeSource = SOURCE_AUTO;
    lastActivityTime = millis();
    
    displayActionScreen("Auto-Baudrate", "Erkenne Baudrate...", 2000);
    
    // Baudratenerkennung starten
    autoBaudrateRequest = true;
    
    // Warten bis die Baudratenerkennung abgeschlossen ist
    while (autoBaudrateRequest) {
        // Auch während der Erkennung serielle Befehle prüfen
        handleSerialCommands();
        
        // Buttons mit reduzierter Priorität behandeln
        if (buttonActivity()) {
            // Erkennung abbrechen, wenn ein Button gedrückt wurde
            autoBaudrateRequest = false;
            displayMenu();
            activeSource = SOURCE_BUTTON;
            lastActivityTime = millis();
            return;
        }
        
        // Baudratenerkennung ausführen
        processAutoBaudrate();
        
        delay(50);
    }
    
    // Ergebnis anzeigen
    char message[50];
    sprintf(message, "Baudrate: %d kbps", currentBaudrate);
    displayActionScreen("Auto-Baudrate", message, 2000);
    
    // Nach der Erkennung zum Menü zurückkehren
    displayMenu();
    activeSource = SOURCE_BUTTON;
    lastActivityTime = millis();
}

// Node-Test starten
void startNodeTest() {
    // Steuerungsquelle auf AUTO setzen
    activeSource = SOURCE_AUTO;
    lastActivityTime = millis();
    
    char message[50];
    sprintf(message, "Teste Node %d...", inputValues.nodeForTest);
    displayActionScreen("Node-Test", message, 2000);
    
    // Test durchführen (3 Versuche, 500ms Timeout)
    bool result = testSingleNode(inputValues.nodeForTest, 3, 500);
    
    // Ergebnis anzeigen
    if (result) {
        sprintf(message, "Node %d: Online", inputValues.nodeForTest);
    } else {
        sprintf(message, "Node %d: Offline", inputValues.nodeForTest);
    }
    displayActionScreen("Node-Test", message, 2000);
    
    // Nach dem Test zum Menü zurückkehren
    displayMenu();
    activeSource = SOURCE_BUTTON;
    lastActivityTime = millis();
}

// Node-ID ändern
void changeNodeIdAction() {
    // Steuerungsquelle auf AUTO setzen
    activeSource = SOURCE_AUTO;
    lastActivityTime = millis();
    
    char message[50];
    sprintf(message, "Aendere ID %d -> %d", inputValues.oldNodeId, inputValues.newNodeId);
    displayActionScreen("Node-ID aendern", message,2000);
    
    // Node-ID ändern
    changeNodeId(inputValues.oldNodeId, inputValues.newNodeId);
    
    // Erfolgsmeldung anzeigen
    sprintf(message, "Node-ID geaendert");
    displayActionScreen("Node-ID aendern", message, 2000);
    
    // Nach der Änderung zum Menü zurückkehren
    displayMenu();
    activeSource = SOURCE_BUTTON;
    lastActivityTime = millis();
}

// Baudrate ändern
void changeBaudrateAction() {
    // Steuerungsquelle auf AUTO setzen
    activeSource = SOURCE_AUTO;
    lastActivityTime = millis();
    
    // Baudrate aus dem Index ermitteln
    int baudrate = convertBaudrateToRealBaudrate(inputValues.selectedBaudrate);
    
    char message[50];
    sprintf(message, "Aendere Baudrate -> %d kbps", baudrate);
    displayActionScreen("Baudrate aendern", message, 2000);
    
    // Lokale Baudrate ändern
    bool success = updateESP32CANBaudrate(baudrate);
    
    // Erfolgsmeldung anzeigen
    if (success) {
        sprintf(message, "Baudrate geaendert");
    } else {
        sprintf(message, "Fehler bei Baudrate");
    }
    displayActionScreen("Baudrate aendern", message, 2000);
    
    // Nach der Änderung zum Menü zurückkehren
    displayMenu();
    activeSource = SOURCE_BUTTON;
    lastActivityTime = millis();
}

// Live-Monitor ein-/ausschalten
void toggleLiveMonitor() {
    // Steuerungsquelle auf AUTO setzen
    activeSource = SOURCE_AUTO;
    lastActivityTime = millis();
    
    // Monitor-Status umschalten
    liveMonitor = !liveMonitor;
    
    char message[50];
    if (liveMonitor) {
        sprintf(message, "Live-Monitor: Ein");
    } else {
        sprintf(message, "Live-Monitor: Aus");
    }
    displayActionScreen("Monitor", message, 1000);
    
    // Wenn Monitor eingeschaltet wurde, Live-Ansicht zeigen
    if (liveMonitor) {
        // Permanente Monitor-Ansicht anzeigen
        displayActionScreen("Live-Monitor", "CAN-Daten...", 1000);
        
        // Monitor läuft im Hintergrund weiter, 
        // die Loop-Funktion kümmert sich um die Anzeige
    } else {
        // Zurück zum Menü
        displayMenu();
        activeSource = SOURCE_BUTTON;
        lastActivityTime = millis();
    }
}

// Filter zurücksetzen
void resetFilterAction() {
    // Filter zurücksetzen
    filterEnabled = false;
    
    // Bestätigung anzeigen
    displayActionScreen("Filter", "Filter zurueckgesetzt", 1000);
    
    // Nach dem Zurücksetzen zum Menü zurückkehren
    displayMenu();
    activeSource = SOURCE_BUTTON;
    lastActivityTime = millis();
}

void showVersionAction() {
    displayActionScreen("Version", getAppVersion(), 0);
    showingVersion = true;
    versionDisplayStart = millis();
}

// Serielle Befehle verarbeiten
/*void handleSerialCommands() {
    if (Serial.available()) {
        // Serielle Steuerung wird aktiv
        activeSource = SOURCE_SERIAL;
        lastActivityTime = millis();
        
        // Bestehender Code zur Verarbeitung serieller Befehle...
        
        // Status im OLED-Display aktualisieren
        if (displayInterface != nullptr) {
            displaySerialModeScreen();
        }
    }
}
*/

/*
// Funktion zur Anzeige von CAN-Nachrichten im Live-Monitor
extern void displayCANMessage(uint32_t canId, uint8_t* data, uint8_t length) {
    if (displayInterface == nullptr || !liveMonitor) {
        return;
    }
    
    // Display leeren
    displayInterface->clear();
    
    // Titel anzeigen
    displayInterface->setCursor(0, 0);
    displayInterface->setTextSize(1);
    displayInterface->println("Live-Monitor");
    displayInterface->drawLine(0, 10, 128, 10, 1);
    
    // CAN-ID anzeigen
    displayInterface->setCursor(0, 15);
    displayInterface->print("ID: 0x");
    displayInterface->println(canId, HEX);
    
    // Länge anzeigen
    displayInterface->setCursor(0, 25);
    displayInterface->print("Laenge: ");
    displayInterface->println(length);
    
    // Daten anzeigen (max. 8 Bytes)
    displayInterface->setCursor(0, 35);
    displayInterface->print("Data: ");
    
    for (int i = 0; i < length && i < 8; i++) {
        displayInterface->print(data[i], HEX);
        displayInterface->print(" ");
    }
    
    displayInterface->setCursor(0, 55);
    displayInterface->print("Filter: ");
    displayInterface->print(filterEnabled ? "An" : "Aus");
    
    displayInterface->display();
}
*/

// Funktion zur Anzeige von Systeminformationen
void displaySystemInfo() {
    if (displayInterface == nullptr) {
        return;
    }
    
    const int displayWidth = getDisplayWidth();

    // Display leeren
    displayInterface->clear();
    
    // Titel anzeigen
    displayInterface->setCursor(0, 0);
    displayInterface->setTextSize(1);
    displayInterface->println("System-Info");
    displayInterface->drawLine(0, 10, displayWidth, 10, 1);
    
    // Informationen anzeigen
    displayInterface->setCursor(0, 15);
    displayInterface->print("Baudrate: ");
    displayInterface->print(String(currentBaudrate));
    displayInterface->println(" kbps");
    
    displayInterface->setCursor(0, 25);
    displayInterface->print("Scan: ");
    displayInterface->print(inputValues.scanStartNode);
    displayInterface->print("-");
    displayInterface->println(inputValues.scanEndNode);
    
    displayInterface->setCursor(0, 35);
    displayInterface->print("Filter: ");
    displayInterface->println(filterEnabled ? "An" : "Aus");
    
    displayInterface->setCursor(0, 45);
    displayInterface->print("Status: ");
    displayInterface->println(systemError ? "Fehler" : "OK");
    
    displayInterface->display();
    
    // Warten auf Tastendruck
    bool waiting = true;
    while (waiting) {
        if (buttonActivity()) {
            waiting = false;
            delay(DEBOUNCE_TIME);
        }
        delay(50);
    }
    
    // Zurück zum Menü
    displayMenu();
}

// Externe Funktionen, die von der Hauptapp aufgerufen werden

// Hauptschleife für die Menüsteuerung
void menuLoop() {
    // Button-Verarbeitung
    handleButtons();
    
    // Timeout für die Steuerungsquelle prüfen
    if (activeSource != SOURCE_NONE && activeSource != SOURCE_AUTO) {
        if (millis() - lastActivityTime > SOURCE_TIMEOUT) {
            // Timeout abgelaufen, keine Quelle aktiv
            activeSource = SOURCE_NONE;
            
            // Falls wir im Button-Modus waren, Menü anzeigen
            if (activeSource == SOURCE_BUTTON) {
                displayMenu();
            }
        }
    }
    
    // Live-Monitor aktualisieren, wenn eine CAN-Nachricht verfügbar ist
    if (liveMonitor && !digitalRead(CAN_INT)) {
        processCANMessage();
    }

    const unsigned long versionDisplayElapsed = millis() - versionDisplayStart;
    if (showingVersion && versionDisplayElapsed >= static_cast<unsigned long>(VERSION_DISPLAY_TIMEOUT_MS)) {
        showingVersion = false;
        displayMenu();
        activeSource = SOURCE_BUTTON;
        lastActivityTime = millis();
    }
}
