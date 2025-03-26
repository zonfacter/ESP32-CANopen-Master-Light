// === CANopen Node ID Scanner + Live CAN Monitor ===
// Scannt Node-IDs durch SDO-Read an 0x1001:00
// Zeigt laufend alle empfangenen Frames (INT-basiert) mit Dekodierung
// Ergebnisanzeige auf OLED + Serial + Node-ID-Changer

#include <SPI.h>
#include <mcp_can.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "CANopenClass.h"

// Definierte Konstanten
#define OLED_ADDR     0x3C
#define CAN_CS        5
#define CAN_INT       4
#define CAN_BITRATE   CAN_125KBPS
#define CAN_CLOCK     MCP_8MHZ

// Globale Objekte
MCP_CAN CAN(CAN_CS);
Adafruit_SSD1306 display(128, 64, &Wire, -1);
CANopen canopen(CAN_INT);

// Einstellungen für den Node-Scan
int scanStart = 1;
int scanEnd = 10;
bool scanning = false;
bool liveMonitor = false;
bool systemError = false;  // Neuer Fehlerstatus für globale Fehlerbehandlung
unsigned long lastErrorTime = 0;  // Zeitpunkt des letzten Fehlers für Timeout

// Funktionsdeklarationen (wichtig: vor der ersten Verwendung)
void showMessage(const char* msg);
void showStatusMessage(const char* title, const char* message, bool isError = false);
void scanNodes(int startID, int endID);
void changeNodeId(uint8_t from, uint8_t to);

void setup() {
  Serial.begin(115200);
  Wire.begin();
  
  // Display initialisieren
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("CANopen Scanner");
  display.display();

  // CAN-Controller initialisieren
  if (CAN.begin(MCP_ANY, CAN_BITRATE, CAN_CLOCK) == CAN_OK) {
    CAN.setMode(MCP_NORMAL);
    Serial.println("[INFO] CAN bereit");
    showStatusMessage("CANopen Scanner", "CAN-Bus initialisiert\nSystembereit");
  } else {
    Serial.println("[FEHLER] CAN Init fehlgeschlagen");
    showStatusMessage("FEHLER", "CAN-Bus Initialisierung\nfehlgeschlagen!", true);
    // Anstatt einer Endlosschleife verwenden wir einen Fehlerstatus
    liveMonitor = false;
    scanning = false;
    systemError = true;
    lastErrorTime = millis();
  }

  pinMode(CAN_INT, INPUT);

  Serial.println("[INFO] Befehle:");
  Serial.println("  scan         → Node-ID Scan starten");
  Serial.println("  range x y    → Bereich setzen (z. B. 1 10)");
  Serial.println("  monitor on   → Live Monitor aktivieren");
  Serial.println("  monitor off  → Live Monitor deaktivieren");
  Serial.println("  change a b   → Node-ID a → b ändern (SDO)");
  Serial.println("  reset        → System zurücksetzen");
}

// ===================================================================================
// Funktion: changeNodeId
// Beschreibung: Wrapper für die CANopen-Klassen-Methode zum Ändern der Node-ID
// ===================================================================================
void changeNodeId(uint8_t from, uint8_t to) {
  // Live-Monitor aktivieren, um alle CAN-Nachrichten zu sehen
  liveMonitor = true;
  
  // Display-Anzeige aktualisieren
  showStatusMessage("Node-ID Änderung", 
                  String("Von " + String(from) + " auf " + String(to) + "\nStatus: Start...").c_str());
  
  Serial.printf("[CMD] Ändere Node-ID von %d nach %d (via CANopenClass)...\n", from, to);
  
  // Unsere CANopen-Klasse verwenden, um die Node-ID zu ändern
  // Mit verbesserter Fehlerbehandlung
  bool ok = canopen.changeNodeId(from, to, true, 5000);
  
  // Erfolgsstatus anzeigen
  if (!ok) {
    Serial.println("[FEHLER] Änderung fehlgeschlagen!");
    
    // Fehlermeldung mit visuellem Indikator anzeigen
    showStatusMessage("Node-ID Änderung", 
                     String("Von " + String(from) + " auf " + String(to) + 
                           "\nStatus: FEHLER!").c_str(), 
                     true);
    
    // Fehler im System markieren, aber nicht blockieren
    systemError = true;
    lastErrorTime = millis();
  } else {
    // Erfolgsmeldung anzeigen
    showStatusMessage("Node-ID Änderung", 
                     String("Von " + String(from) + " auf " + String(to) + 
                           "\nStatus: ERFOLGREICH").c_str());
    
    Serial.printf("[OK] Node-ID erfolgreich von %d auf %d geändert!\n", from, to);
  }
}

void loop() {
  // Fehlerbehandlung - wenn systemError gesetzt ist, prüfen wir, ob wir zurücksetzen können
  if (systemError) {
    // Prüfen ob genügend Zeit vergangen ist, um einen Reset zu versuchen
    if (millis() - lastErrorTime > 5000) { // 5 Sekunden Timeout
      systemError = false;
      Serial.println("[INFO] System wird nach Fehler fortgesetzt");
      showStatusMessage("System-Status", "Fehler behoben\nSystem fortgesetzt");
    } else {
      delay(100); // Kleine Pause im Fehlerzustand
      return;     // Rest der Loop überspringen
    }
  }

  // Überprüfen auf Benutzereingaben über die serielle Schnittstelle
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd == "scan") {
      scanning = true;
      Serial.println("[INFO] Scan wird gestartet...");
    } 
    else if (cmd.startsWith("range")) {
      int idx = cmd.indexOf(' ');
      if (idx > 0) {
        int second = cmd.indexOf(' ', idx + 1);
        if (second > 0) {
          int newStart = cmd.substring(idx + 1, second).toInt();
          int newEnd = cmd.substring(second + 1).toInt();
          
          // Wertebegrenzung und Plausibilitätsprüfung
          if (newStart >= 1 && newStart <= 127 && newEnd >= 1 && newEnd <= 127 && newStart <= newEnd) {
            scanStart = newStart;
            scanEnd = newEnd;
            Serial.printf("[INFO] Bereich gesetzt: %d bis %d\n", scanStart, scanEnd);
          } else {
            Serial.println("[FEHLER] Ungültiger Bereich! Gültige Werte: 1-127");
          }
        } else {
          Serial.println("[FEHLER] Falsche Syntax. Korrekt: range x y");
        }
      } else {
        Serial.println("[FEHLER] Falsche Syntax. Korrekt: range x y");
      }
    } 
    else if (cmd == "monitor on") {
      liveMonitor = true;
      Serial.println("[INFO] Live Monitor aktiviert");
    } 
    else if (cmd == "monitor off") {
      liveMonitor = false;
      Serial.println("[INFO] Live Monitor deaktiviert");
    } 
    else if (cmd.startsWith("change")) {
      int idx = cmd.indexOf(' ');
      if (idx > 0) {
        int second = cmd.indexOf(' ', idx + 1);
        if (second > 0) {
          int oldId = cmd.substring(idx + 1, second).toInt();
          int newId = cmd.substring(second + 1).toInt();
          
          // Wertebegrenzung und Plausibilitätsprüfung
          if (oldId >= 1 && oldId <= 127 && newId >= 1 && newId <= 127) {
            Serial.printf("[INFO] Starte Node-ID Änderung von %d nach %d\n", oldId, newId);
            changeNodeId(oldId, newId);
          } else {
            Serial.println("[FEHLER] Ungültige Node-ID! Gültige Werte: 1-127");
          }
        } else {
          Serial.println("[FEHLER] Falsche Syntax. Korrekt: change old_id new_id");
        }
      } else {
        Serial.println("[FEHLER] Falsche Syntax. Korrekt: change old_id new_id");
      }
    }
    else if (cmd == "reset") {
      // Manuelle Zurücksetzung
      systemError = false;
      liveMonitor = false;
      scanning = false;
      Serial.println("[INFO] System zurückgesetzt");
      showStatusMessage("System-Reset", "Alle Parameter zurückgesetzt\nSystem bereit");
    }
  }

  // Node-Scan durchführen, wenn angefordert
  if (scanning) {
    scanNodes(scanStart, scanEnd);
    scanning = false;
  }

  // Live Monitor für CAN-Frames
  if (liveMonitor && !digitalRead(CAN_INT)) {
    unsigned long rxId;
    byte len;
    byte buf[8];
    
    // Fehlerbehandlung beim Lesen der CAN-Nachricht
    byte status = CAN.readMsgBuf(&rxId, &len, buf);
    if (status != CAN_OK) {
      Serial.printf("[FEHLER] Fehler beim Lesen der CAN-Nachricht: %d\n", status);
      return; // Diese Iteration überspringen
    }

    uint8_t nodeId = rxId & 0x7F;
    uint16_t baseId = rxId & 0x780;

    // Formatierte Ausgabe der CAN-Nachricht
    Serial.printf("[CAN] ID: 0x%03lX Len: %d →", rxId, len);
    for (int i = 0; i < len; i++) {
      Serial.printf(" %02X", buf[i]);
    }

    // Bekannte Nachrichtentypen identifizieren
    if (rxId == 0x000) {
      Serial.print("  [NMT Broadcast]");
    } else if (baseId == 0x180) {
      Serial.printf("  [PDO1 von Node %d]", nodeId);
    } else if (baseId == 0x280) {
      Serial.printf("  [PDO2 von Node %d]", nodeId);
    } else if (baseId == 0x580) {
      Serial.printf("  [SDO Response von Node %d]", nodeId);
      
      // SDO-Antwort interpretieren
      if (len >= 4) {
        byte cs = buf[0] >> 5; // Command Specifier
        if (cs == 4) {
          Serial.print(" (Abort)");
        } else if (cs == 3) {
          Serial.print(" (Upload)");
        } else if (cs == 1) {
          Serial.print(" (Download)");
        }
      }
      
    } else if (baseId == 0x600) {
      Serial.printf("  [SDO Request an Node %d]", nodeId);
    } else if (baseId == 0x700) {
      if (buf[0] == 0x00) {
        Serial.printf("  [Bootup von Node %d]", nodeId);
      } else {
        Serial.printf("  [Heartbeat von Node %d]", nodeId);
      }
    }

    Serial.println();
  }
}

// ===================================================================================
// Funktion: showMessage
// Beschreibung: Zeigt eine Nachricht auf dem OLED-Display an
// ÄNDERUNG: Einfache Anzeigefunktion (wird von showStatusMessage verwendet)
// ===================================================================================
void showMessage(const char* msg) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println(msg);
  display.display();
}

// ===================================================================================
// Funktion: showStatusMessage
// Beschreibung: Zeigt eine Statusmeldung mit Titel und Details auf dem OLED-Display an
// ===================================================================================
void showStatusMessage(const char* title, const char* message, bool isError) {
  display.clearDisplay();
  display.setCursor(0, 0);
  
  // Titel hervorheben
  display.setTextSize(1);
  display.println(title);
  
  // Trennlinie
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  
  // Nachricht anzeigen
  display.setCursor(0, 14);
  display.println(message);
  
  // Bei Fehler einen visuellen Indikator anzeigen
  if (isError) {
    display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
    display.fillRect(0, 54, 128, 10, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(5, 55);
    display.print("FEHLER!");
    display.setTextColor(SSD1306_WHITE);
  }
  
  display.display();
}

// ===================================================================================
// Funktion: scanNodes
// Beschreibung: Scannt einen Bereich von CANopen-Node-IDs und zeigt aktive Knoten an
// ÄNDERUNG: Verbesserte Fehlerbehandlung, kein Endlosloop bei Fehler
// ===================================================================================
void scanNodes(int startID, int endID) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Scan...\n");
  display.display();
  
  Serial.printf("[INFO] Starte Node-Scan von %d bis %d\n", startID, endID);
  
  // Zähler für gefundene Nodes
  int foundNodes = 0;
  
  // Bereich prüfen und begrenzen
  if (startID < 1) startID = 1;
  if (endID > 127) endID = 127;
  
  for (int id = startID; id <= endID; id++) {
    // SDO-Leseanfrage an Fehlerregister (0x1001:00) vorbereiten
    byte sdo[8] = { 0x40, 0x01, 0x10, 0x00, 0, 0, 0, 0 };
    
    // Anfrage senden mit Fehlerbehandlung
    byte result = CAN.sendMsgBuf(0x600 + id, 0, 8, sdo);
    if (result != CAN_OK) {
      Serial.printf("[FEHLER] Konnte keine Anfrage an Node %d senden: %d\n", id, result);
      continue; // Mit dem nächsten Node fortfahren
    }
    
    Serial.printf("[SCAN] Frage Node %d...\n", id);

    // Auf Antwort warten mit Timeout
    unsigned long start = millis();
    bool responded = false;
    bool errorResponse = false;

    while (millis() - start < 200) { // 200ms Timeout pro Node
      if (!digitalRead(CAN_INT)) {
        unsigned long rxId;
        byte len;
        byte buf[8];
        
        // Antwort lesen mit Fehlerbehandlung
        byte readStatus = CAN.readMsgBuf(&rxId, &len, buf);
        if (readStatus != CAN_OK) {
          Serial.printf("[FEHLER] Fehler beim Lesen: %d\n", readStatus);
          break;
        }
        
        // Prüfen, ob es sich um eine Antwort auf unsere SDO-Anfrage handelt
        if (rxId == (0x580 + id)) {
          // Prüfen, ob es eine Fehlerantwort ist (SDO Abort)
          if ((buf[0] & 0xE0) == 0x80) {
            uint32_t abortCode = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
            Serial.printf("[OK] Node %d hat mit Fehler geantwortet! Abortcode: 0x%08X\n", id, abortCode);
            display.printf("Node %d: Err\n", id);
            errorResponse = true;
          } else {
            Serial.printf("[OK] Node %d hat geantwortet! Fehlerreg: 0x%02X\n", id, buf[4]);
            display.printf("Node %d: OK\n", id);
            foundNodes++;
          }
          
          display.display();
          responded = true;
          break;
        }
      }
    }

    if (!responded) {
      Serial.printf("[--] Node %d keine Antwort\n", id);
    } else if (!errorResponse) {
      foundNodes++;
    }
    
    delay(50); // Kurze Pause zwischen den Anfragen
  }
  
  // Zusammenfassung anzeigen
  display.clearDisplay();
  display.setCursor(0, 0);
  display.printf("Scan abgeschlossen\n");
  display.printf("%d Nodes gefunden\n", foundNodes);
  display.printf("Bereich: %d-%d", startID, endID);
  display.display();
  
  Serial.printf("[INFO] Scan abgeschlossen. %d Nodes gefunden.\n", foundNodes);
}
