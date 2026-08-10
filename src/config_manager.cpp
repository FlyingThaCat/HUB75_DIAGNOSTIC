#include "config_manager.h"

Preferences preferences;
int panelWidth = 32;
int panelHeight = 32;
bool useSlowClock = false;

void loadConfiguration() {
    preferences.begin("matrix_config", true);
    panelWidth = preferences.getInt("width", 32);
    panelHeight = preferences.getInt("height", 32);
    useSlowClock = preferences.getBool("slow_clock", false);
    preferences.end();

    Serial.printf("Loaded Config -> Size: %dx%d | Slow Clock: %s\n", 
                  panelWidth, panelHeight, useSlowClock ? "YES (10MHz)" : "NO (20MHz)");
}

void saveConfiguration(int width, int height, bool slowClock) {
    preferences.begin("matrix_config", false);
    preferences.putInt("width", width);
    preferences.putInt("height", height);
    preferences.putBool("slow_clock", slowClock);
    preferences.end();

    Serial.printf("Saved Config -> Size: %dx%d | Slow Clock: %s\n", 
                  width, height, slowClock ? "YES (10MHz)" : "NO (20MHz)");
}

bool checkAndRunConfigMode(int buttonPin, int ledPin) {
    Serial.println("Checking for Config Mode (Hold BOOT for 2s)...");
    unsigned long startTime = millis();

    while (digitalRead(buttonPin) == LOW) {
        if (millis() - startTime > 2000) {
            Serial.println("\n=== ENTERED CONFIGURATION MODE ===");
            Serial.println("Stage 1: Select Panel Size");

            loadConfiguration();

            // Stage 1 Presets: 
            // 1 = 32x32 
            // 2 = 64x32 
            // 3 = 64x64 
            // 4 = 128x32
            int currentSizePreset = 1;
            if (panelWidth == 64 && panelHeight == 32) currentSizePreset = 2;
            else if (panelWidth == 64 && panelHeight == 64) currentSizePreset = 3;
            else if (panelWidth == 128 && panelHeight == 32) currentSizePreset = 4;

            unsigned long lastBlinkTime = 0;
            bool ledState = false;
            int blinkCount = 0;
            bool targetReached = false;
            bool stage1Done = false;

            // ==========================================
            // STAGE 1: PANEL SIZE LOOP
            // ==========================================
            while (!stage1Done) {
                unsigned long now = millis();

                if (!targetReached && now - lastBlinkTime > 400) {
                    lastBlinkTime = now;
                    ledState = !ledState;
                    digitalWrite(ledPin, ledState);

                    if (!ledState) {
                        blinkCount++;
                        if (blinkCount >= currentSizePreset) {
                            targetReached = true;
                            lastBlinkTime = now + 1400; 
                        }
                    }
                } else if (targetReached && now - lastBlinkTime > 0) {
                    blinkCount = 0;
                    targetReached = false;
                }

                if (digitalRead(buttonPin) == LOW) {
                    delay(50); // Debounce
                    unsigned long pressTime = millis();

                    while (digitalRead(buttonPin) == LOW) {
                        if (millis() - pressTime > 1500) {
                            // LONG HOLD = SAVE SIZE, MOVE TO STAGE 2
                            Serial.println("\nSize confirmed. Moving to Stage 2: Clock Speed");
                            
                            if (currentSizePreset == 1) { panelWidth = 32; panelHeight = 32; }
                            else if (currentSizePreset == 2) { panelWidth = 64; panelHeight = 32; }
                            else if (currentSizePreset == 3) { panelWidth = 64; panelHeight = 64; }
                            else if (currentSizePreset == 4) { panelWidth = 128; panelHeight = 32; }

                            // Visual feedback: double-flash to indicate transition
                            digitalWrite(ledPin, LOW); delay(300);
                            digitalWrite(ledPin, HIGH); delay(150);
                            digitalWrite(ledPin, LOW); delay(150);
                            digitalWrite(ledPin, HIGH); delay(150);
                            digitalWrite(ledPin, LOW); delay(500);

                            stage1Done = true;
                            break;
                        }
                    }

                    if (!stage1Done) {
                        currentSizePreset++;
                        if (currentSizePreset > 4) currentSizePreset = 1;

                        Serial.printf("Size Preset %d selected\n", currentSizePreset);

                        digitalWrite(ledPin, HIGH);
                        delay(200);
                        digitalWrite(ledPin, LOW);
                        delay(400);
                    }
                }
            }

            // ==========================================
            // STAGE 2: CLOCK SPEED LOOP
            // ==========================================
            int currentClockPreset = useSlowClock ? 2 : 1; // 1 = Normal 20MHz, 2 = Slow 10MHz
            lastBlinkTime = 0;
            blinkCount = 0;
            targetReached = false;
            bool stage2Done = false;

            Serial.println("Stage 2: Select Clock Speed (1 blink = Normal 20MHz, 2 blinks = Slow 10MHz)");

            while (!stage2Done) {
                unsigned long now = millis();

                if (!targetReached && now - lastBlinkTime > 400) {
                    lastBlinkTime = now;
                    ledState = !ledState;
                    digitalWrite(ledPin, ledState);

                    if (!ledState) {
                        blinkCount++;
                        if (blinkCount >= currentClockPreset) {
                            targetReached = true;
                            lastBlinkTime = now + 1400; 
                        }
                    }
                } else if (targetReached && now - lastBlinkTime > 0) {
                    blinkCount = 0;
                    targetReached = false;
                }

                if (digitalRead(buttonPin) == LOW) {
                    delay(50); 
                    unsigned long pressTime = millis();

                    while (digitalRead(buttonPin) == LOW) {
                        if (millis() - pressTime > 1500) {
                            // LONG HOLD = SAVE EVERYTHING & EXIT
                            Serial.println("\nSaving final configuration...");
                            useSlowClock = (currentClockPreset == 2);

                            // Properly passes all 3 variables now
                            saveConfiguration(panelWidth, panelHeight, useSlowClock);
                            
                            // Success rapid blinking
                            for(int i = 0; i < 6; i++) {
                                digitalWrite(ledPin, !digitalRead(ledPin));
                                delay(100);
                            }
                            digitalWrite(ledPin, LOW);
                            return true;
                        }
                    }

                    currentClockPreset++;
                    if (currentClockPreset > 2) currentClockPreset = 1;

                    Serial.printf("Clock Preset %d selected (Slow Clock: %s)\n", 
                                  currentClockPreset, currentClockPreset == 2 ? "YES" : "NO");

                    digitalWrite(ledPin, HIGH);
                    delay(200);
                    digitalWrite(ledPin, LOW);
                    delay(400);
                }
            }
        }
    }
    return false;
}