#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "tests.h"
#include "ble_ota.h"
#include <BLEDevice.h>
#include <config_manager.h>

#define PANEL_RES_X 32
#define PANEL_RES_Y 32
#define PANEL_CHAIN 1

#define BUTTON_PIN 0
const int LED_PIN = 2;

HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN);
MatrixPanel_I2S_DMA matrix(mxconfig);

TestState currentState = STATE_WHITE;
bool stateNeedsInit = true;

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
int lastButtonState = HIGH;
int buttonState = HIGH;

// Track whether we are currently adjusting colors or switching states
static bool inColorMode = false;

void checkButtonPress() {
    int reading = digitalRead(BUTTON_PIN);
    
    if (reading != lastButtonState) {
        lastDebounceTime = millis();
        lastButtonState = reading;
    }
    
    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (reading != buttonState) {
            buttonState = reading;
            
            if (buttonState == LOW) {
                // Button pressed down
                static unsigned long pressStart = 0;
                pressStart = millis();
                notifyButtonEvent("PRESSED");
            } else {
                // Button released
                notifyButtonEvent("RELEASED");
            }
        }
    }

    // Monitor active press duration while button is held
    static unsigned long lastHoldCheck = 0;
    static bool longPressHandled = false;
    static bool otaUnlockHandled = false;

    if (buttonState == LOW) {
        if (lastHoldCheck == 0) lastHoldCheck = millis();
        unsigned long holdDuration = millis() - lastHoldCheck;

        // Pressing button while on OTA Update Screen activates/unlocks OTA mode
        if (currentState == STATE_OTA_MODE) {
            if (!otaUnlockHandled) {
                otaUnlockHandled = true;
                bool newOtaState = !isOtaEnabled();
                setOtaEnabled(newOtaState);
                notifyButtonEvent(newOtaState ? "OTA:UNLOCKED" : "OTA:LOCKED");
                digitalWrite(LED_PIN, newOtaState ? HIGH : LOW);
            }
            return;
        }

        // Long press (> 600ms): Toggle color mode
        if (holdDuration > 600 && !longPressHandled) {
            longPressHandled = true;
            inColorMode = !inColorMode;

            digitalWrite(LED_PIN, HIGH);
            if (inColorMode) {
                Serial.println("\n>>> ENTERED COLOR CUSTOMIZATION MODE <<<");
                notifyButtonEvent("MODE:COLOR");
            } else {
                Serial.println("\n>>> EXITED COLOR MODE <<<");
                notifyButtonEvent("MODE:NORMAL");
            }
            delay(100);
            digitalWrite(LED_PIN, LOW);
        }


        // Very long hold (> 3000ms): Unlock OTA Mode for security
        if (holdDuration > 3000 && !otaUnlockHandled) {
            otaUnlockHandled = true;
            bool newOtaState = !isOtaEnabled();
            setOtaEnabled(newOtaState);
            notifyButtonEvent(newOtaState ? "OTA:UNLOCKED" : "OTA:LOCKED");

            // Rapid 4x blink for visual confirmation
            for (int i = 0; i < 4; i++) {
                digitalWrite(LED_PIN, HIGH); delay(75);
                digitalWrite(LED_PIN, LOW); delay(75);
            }
        }
    } else {
        // If button was released quickly (< 600ms), process as a short press
        if (lastHoldCheck > 0) {
            unsigned long holdDuration = millis() - lastHoldCheck;
            if (holdDuration < 600 && !longPressHandled) {
                // If on OTA screen, BOOT button is strictly for locking/unlocking OTA. Do NOT cycle test states!
                if (currentState == STATE_OTA_MODE) {
                    lastHoldCheck = 0;
                    return;
                }

                digitalWrite(LED_PIN, HIGH);

                if (inColorMode) {
                    cycleTestColor(matrix);
                    stateNeedsInit = true;
                    notifyButtonEvent("ACTION:COLOR_CYCLE");
                } else {
                    currentState = static_cast<TestState>((currentState + 1) % BUTTON_STATE_COUNT);
                    stateNeedsInit = true;
                    Serial.print("Manual Test State: ");
                    Serial.println(currentState);
                    
                    char buf[32];
                    snprintf(buf, sizeof(buf), "STATE:%d", currentState);
                    notifyButtonEvent(buf);
                }
                
                delay(50);
                digitalWrite(LED_PIN, LOW);
            }
        }
        lastHoldCheck = 0;
        longPressHandled = false;
        otaUnlockHandled = false;
    }
}


void setup() {
    Serial.begin(115200);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    Serial.println("HUB75 Diagnostic Test Initializing...");

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    checkAndRunConfigMode(BUTTON_PIN, LED_PIN);
    loadConfiguration();

    HUB75_I2S_CFG finalConfig(panelWidth, panelHeight, PANEL_CHAIN);

    if (useSlowClock) {
        finalConfig.i2sspeed = HUB75_I2S_CFG::HZ_10M; // Slow down clock to 10MHz for stability
        Serial.println(">>> Cheap Panel Mode Active: Clock slowed to 10MHz <<<");
    }

    matrix = MatrixPanel_I2S_DMA(finalConfig);

    Serial.println("Initializing Matrix Panel...");
    matrix.begin();
    Serial.println("Matrix Panel Initialized.");

    Serial.println("Setting Brightness to 50...");
    matrix.setBrightness8(50);

    Serial.println("Matrix Panel Ready.");
    initColors(matrix);

    setupBLE();

    Serial.println("=========================================");
    Serial.println("      HUB75 DIAGNOSTIC TESTER Ready      ");
    Serial.printf("        Active Panel Size: %dx%d         \n", panelWidth, panelHeight);
    Serial.println("Press BOOT button to cycle through tests.");
    Serial.println("=========================================");

    Serial.println(R"=====(
    ==========================================
          Wiring for HUB75 Matrix Panel:      
    ==========================================
                 +------+
       [R1]  25  | 1  2 |  26  [G1]
       [B1]  27  | 3  4 | GND
       [R2]  14  | 5  6 |  12  [G2]
       [B2]  13  | 7  8 |  18  [E]
        [A]  23  | 9 10 |  19  [B]
        [C]   5  |11 12 |  17  [D]
      [CLK]  16  |13 14 |   4 [LAT]
       [OE]  15  |15 16 | GND
                 +------+
    ==========================================
    )=====");

}

void loop() {
    checkButtonPress();
    runStateEngine(matrix, currentState, stateNeedsInit);

    if (otaPendingReboot) {
        if (millis() - otaRebootTimer > 1000) {
            Serial.println("Rebooting to apply OTA update...");
            
            BLEDevice::deinit(true); // Deinitialize BLE to free resources before reboot
            
            for (int i = 0; i < 5; i++) {
                digitalWrite(LED_PIN, HIGH);
                delay(100); // Wait 100ms
                digitalWrite(LED_PIN, LOW);
                delay(100); // Wait 100ms
            }

            ESP.restart();
        }
    }
}