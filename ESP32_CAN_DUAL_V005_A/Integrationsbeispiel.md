# Integrationsanleitung für die Systemprofilaktualisierung

## Übersicht der Änderungen

Diese Aktualisierung führt Systemkonfigurationsprofile ein, um Hardware-Kompatibilitätsprobleme zu beheben und eine robustere Fehlerbehandlung zu implementieren. Die Hauptfunktionen sind:

1. Vordefinierte Systemprofile für kompatible Hardware-Kombinationen
2. Absturzsicherung mit automatischem Fallback in den sicheren Modus
3. Verbesserte Benutzeroberfläche und Hilfeinformationen

## Einzufügende Dateien

1. **SystemProfiles.h**: Neue Header-Datei mit Definitionen und Hilfsfunktionen für Systemprofile
   - Füge diese Datei in dein Arduino-Projekt ein

## Zu aktualisierende Funktionen

1. **In der Hauptdatei (ESP32_CAN_DUAL_V005_A.ino):**
   - Füge den Include für `SystemProfiles.h` hinzu (nach den anderen Includes)
   - Ersetze die `loadSettings`-Funktion mit der aktualisierten Version
   - Ersetze die `printCurrentSettings`-Funktion mit der aktualisierten Version
   - Ersetze die `systemReset`-Funktion mit der aktualisierten Version
   - Füge die neue `isValidComponentCombination`-Funktion hinzu
   - Füge die neue `handleModeCommand`-Funktion hinzu
   - Aktualisiere die `handleSerialCommands`-Funktion, um den neuen `mode`-Befehl zu unterstützen
   - Aktualisiere die `printHelpMenu`-Funktion, um die neuen Befehle anzuzeigen
   - Füge einen Hinweis zur Verwendung von Profilen bei `handleTransceiverCommand` hinzu

2. **In anderen Dateien:**
   - Aktualisiere das `changelog.md` mit den Änderungen für Version V005_A

## Integrationsschritte

1. **SystemProfiles.h hinzufügen**
   - Erstelle eine neue Datei mit dem Namen `SystemProfiles.h` im Projektordner
   - Kopiere den Inhalt aus dem Artefakt `system-profiles-header`

2. **Header-Include hinzufügen**
   - Füge in ESP32_CAN_DUAL_V005_A.ino nach den bestehenden Includes folgende Zeile hinzu:
   ```cpp
   #include "SystemProfiles.h"
   ```

3. **Funktionen aktualisieren**
   - Ersetze die bestehende `loadSettings`-Funktion mit dem Inhalt des Artefakts `crash-protection-update`
   - Ersetze die bestehende `printCurrentSettings`-Funktion mit dem Inhalt des Artefakts `current-settings-update`
   - Ersetze die bestehende `systemReset`-Funktion mit dem Inhalt des Artefakts `system-reset-update`
   - Füge die neue `handleModeCommand`-Funktion aus dem Artefakt `transceiver-mode-command` hinzu
   - Aktualisiere die `handleSerialCommands`-Funktion mit dem Inhalt des Artefakts `handle-commands-update`
   - Aktualisiere die `printHelpMenu`-Funktion mit dem Inhalt des Artefakts `help-menu-update`

4. **Aktualisiere das Changelog**
   - Öffne die Datei `changelog.md`
    - Füge am Anfang (vor Version V005) den Inhalt des Artefakts `changelog-update` ein

## Prüfung nach der Integration

1. **Kompilierung**: Überprüfe, ob der Code ohne Fehler kompiliert

2. **Funktionalitätstests**:
   - Teste den neuen `mode`-Befehl zum Wechseln zwischen Profilen
   - Überprüfe die Absturzsicherung (sollte nach 3 Neustarts in den sicheren Modus wechseln)
   - Teste die Kompatibilitätsprüfung (versuche, inkompatible Kombinationen zu konfigurieren)

3. **Hardware-Tests**:
   - Validiere Profil 1: OLED + MCP2515
   - Validiere Profil 2: TFT + TJA1051
   - Stelle sicher, dass beide Profile ohne Abstürze funktionieren

## Hinweise

- Die ursprünglichen Befehle `transceiver can` und `transceiver display` bleiben aus Kompatibilitätsgründen erhalten, werden jedoch als "nicht empfohlen" markiert.
- Das neue Feature "Systemprofile" wird als bevorzugte Methode zur Konfiguration der Hardware empfohlen.
- Der Crash-Schutz sollte verhindern, dass das System in einem Zustand stecken bleibt, in dem es immer wieder abstürzt.
