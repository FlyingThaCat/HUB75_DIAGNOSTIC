#include "ble_ota.h"
#include "tests.h" // Needed to modify currentState and custom colors from BLE commands
#include "config_manager.h" // Save hardware parameters directly from BLE commands
#include <Update.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>


size_t otaTotalSize = 0;
size_t otaWrittenSize = 0;

bool otaPendingReboot = false;
unsigned long otaRebootTimer = 0;
static bool g_otaEnabled = false;

static BLECharacteristic *pBtnCharacteristic = nullptr;
static BLEServer *pServerInstance = nullptr;

void setOtaEnabled(bool enabled) {
    g_otaEnabled = enabled;
    digitalWrite(LED_PIN, enabled ? HIGH : LOW);
    Serial.printf(">>> OTA Security Mode: %s <<<\n", enabled ? "ENABLED (Unlocked)" : "DISABLED (Locked)");
}

bool isOtaEnabled() {
    return g_otaEnabled;
}

void notifyButtonEvent(const char* event) {
    if (pBtnCharacteristic != nullptr) {
        pBtnCharacteristic->setValue(event);
        pBtnCharacteristic->notify();
    }
}

// ==========================================
//          OTA FIRMWARE CALLBACKS
// ==========================================
class OtaCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) override {
        if (!g_otaEnabled) {
            Serial.println("BLOCKED: OTA Update attempted while OTA is LOCKED. Press BOOT button while on OTA Screen to Start/Unlock OTA.");
            return;
        }

        uint8_t *data = pCharacteristic->getData();
        size_t length = pCharacteristic->getLength();

        if (length == 0) return;

        if (length > 6 && memcmp(data, "START:", 6) == 0) {
            currentState = STATE_OTA_MODE;
            stateNeedsInit = true;

            String startCmd = String((char*)data).substring(0, length);
            otaTotalSize = startCmd.substring(6).toInt();
            otaWrittenSize = 0;

            Serial.printf("=== BLE OTA Update Started ===\n");
            Serial.printf("Expected File Size: %d bytes\n", otaTotalSize);

            if (!Update.begin(otaTotalSize)) {
                Serial.println("Update.begin() failed");
                Update.printError(Serial);
                return;
            }
            return;
        }


        if (length == 3 && memcmp(data, "END", 3) == 0) {
            if (Update.end(true)) {
                Serial.printf("=== BLE OTA Update Completed ===\n");
                Serial.printf("Total Bytes Written: %d bytes\n", otaWrittenSize);
                Serial.println("Rebooting shortly to apply the update...");
                otaPendingReboot = true;
                otaRebootTimer = millis();
            } else {
                Serial.println("Update.end() failed");
                Update.printError(Serial);
            }
            return;
        }

        if (Update.isRunning()) {
            size_t written = Update.write(data, length);
            otaWrittenSize += written;

            digitalWrite(LED_PIN, !digitalRead(LED_PIN));

            if (otaWrittenSize % 20480 < length || otaWrittenSize >= otaTotalSize) {
                int percentage = (otaWrittenSize * 100) / otaTotalSize;
                Serial.printf("OTA Progress: %d / %d bytes (%d%%)\n", otaWrittenSize, otaTotalSize, percentage);
                
                char progressBuf[32];
                snprintf(progressBuf, sizeof(progressBuf), "PROGRESS:%d", percentage);
                notifyButtonEvent(progressBuf);
            }
        }

    }
};

// ==========================================
//       APP COMMAND & COLOR CALLBACKS
// ==========================================
class CommandCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) override {
        std::string rxValue = pCharacteristic->getValue();
        
        if (rxValue.length() > 0) {
            String command = rxValue.c_str();
            command.trim(); 
            
            Serial.print("BLE Command Received: ");
            Serial.println(command);

            // Flash LED to acknowledge command
            digitalWrite(LED_PIN, HIGH);
            
            // 1. Basic & Offline States
            if (command == "white") currentState = STATE_WHITE;
            else if (command == "red") currentState = STATE_RED;
            else if (command == "green") currentState = STATE_GREEN;
            else if (command == "blue") currentState = STATE_BLUE;
            else if (command == "yellow") currentState = STATE_YELLOW;
            else if (command == "cyan") currentState = STATE_CYAN;
            else if (command == "magenta") currentState = STATE_MAGENTA;
            else if (command == "off") currentState = STATE_OFF;
            else if (command == "checkerboard") currentState = STATE_CHECKERBOARD;
            else if (command == "quadrant") currentState = STATE_QUADRANT;
            else if (command == "diagonal") currentState = STATE_DIAGONAL;
            else if (command == "horizontal") currentState = STATE_HORIZONTAL;
            else if (command == "vertical") currentState = STATE_VERTICAL;
            else if (command == "ic_chase") currentState = STATE_IC_CHASE;
            else if (command == "interleaved") currentState = STATE_INTERLEAVED_ROWS;
            
            // 2. Advanced Hardware Isolation Tests
            else if (command == "top_half") currentState = STATE_TOP_HALF;
            else if (command == "bottom_half") currentState = STATE_BOTTOM_HALF;
            else if (command == "addr_a") currentState = STATE_ADDR_A;
            else if (command == "addr_b") currentState = STATE_ADDR_B;
            else if (command == "addr_c") currentState = STATE_ADDR_C;
            else if (command == "addr_d") currentState = STATE_ADDR_D;
            else if (command == "row_sweep") currentState = STATE_ROW_SWEEP;
            else if (command == "col_sweep") currentState = STATE_COLUMN_SWEEP;
            else if (command == "ghosting") currentState = STATE_GHOSTING;
            else if (command == "ota_mode") currentState = STATE_OTA_MODE;

            
            // 3. Interactive Pixel Hunter (Format: "pixel:X,Y")
            else if (command.startsWith("pixel:")) {
                currentState = STATE_PIXEL_HUNTER;
                int commaIndex = command.indexOf(',');
                if (commaIndex > 0) {
                    int x = command.substring(6, commaIndex).toInt();
                    int y = command.substring(commaIndex + 1).toInt();
                    setPixelHunterXY(x, y);
                    Serial.printf("Pixel Hunter Target -> X:%d Y:%d\n", x, y);
                }
            }
            
            // 4. Custom App Color Picker (Format: "color:R,G,B")
            else if (command.startsWith("color:")) {
                int firstComma = command.indexOf(',');
                int secondComma = command.indexOf(',', firstComma + 1);
                if (firstComma > 0 && secondComma > 0) {
                    int r = command.substring(6, firstComma).toInt();
                    int g = command.substring(firstComma + 1, secondComma).toInt();
                    int b = command.substring(secondComma + 1).toInt();
                    
                    setCustomColor(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
                    Serial.printf("App Custom Color Set -> R:%d G:%d B:%d\n", r, g, b);
                }
            }

            // 5. App Hardware Config Update (Format: "cfg:WIDTH,HEIGHT,SLOW_CLOCK")
            else if (command.startsWith("cfg:")) {
                int firstComma = command.indexOf(',');
                int secondComma = command.indexOf(',', firstComma + 1);
                if (firstComma > 0 && secondComma > 0) {
                    int w = command.substring(4, firstComma).toInt();
                    int h = command.substring(firstComma + 1, secondComma).toInt();
                    bool slow = (command.substring(secondComma + 1).toInt() == 1);

                    saveConfiguration(w, h, slow);
                    Serial.printf("App Hardware Config Saved -> %dx%d (Slow Clock: %s). Rebooting...\n", w, h, slow ? "YES" : "NO");

                    otaPendingReboot = true;
                    otaRebootTimer = millis();
                }
            }

            // 6. Force Temporary BLE Disconnect (Format: "disconnect")
            else if (command == "disconnect") {
                Serial.println("App requested temporary BLE disconnect. Disconnecting client...");
                if (pServerInstance != nullptr) {
                    pServerInstance->disconnect(pServerInstance->getConnId());
                }
            }

            // Force screen refresh for the new state/color
            stateNeedsInit = true; 
            
            delay(50);
            digitalWrite(LED_PIN, LOW);
        }
    }
};

static OtaCallbacks otaCallbacks;
static CommandCallbacks cmdCallbacks;

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t* param) override {
        Serial.println("BLE Client Connected. Requesting fast connection interval for high-speed OTA...");
        pServer->updateConnParams(param->connect.remote_bda, 6, 12, 0, 200); // 7.5ms - 15ms interval
    }

    void onDisconnect(BLEServer* pServer) override {
        Serial.println("BLE Client Disconnected. Restarting advertising...");
        BLEDevice::startAdvertising();
    }
};

static ServerCallbacks serverCallbacks;


// ==========================================
//             BLE INITIALIZATION
// ==========================================
void setupBLE() {
    Serial.println("Initializing BLE for OTA Update & Control...");

    // Generate unique hardware fingerprint from ESP32 eFuse MAC address
    uint64_t chipid = ESP.getEfuseMac();
    char serialNo[16];
    snprintf(serialNo, sizeof(serialNo), "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);

    char devName[32];
    snprintf(devName, sizeof(devName), "ESP32_HUB75_%04X", (uint16_t)(chipid & 0xFFFF));

    Serial.printf("Device Serial Number / Fingerprint: %s\n", serialNo);
    Serial.printf("Advertising Name: %s\n", devName);

    BLEDevice::init(devName);
    BLEDevice::setMTU(512);
    esp_ble_gap_config_local_icon(0x04C0); // Prevent iOS heart-sensor category bug

    BLEServer *pServer = BLEDevice::createServer();
    pServerInstance = pServer;
    pServer->setCallbacks(&serverCallbacks);

    BLEService *pService = pServer->createService(BLE_SERVICE_UUID);


    // 1. OTA Firmware Upload Characteristic
    BLECharacteristic *pOTACharacteristic = pService->createCharacteristic(
        BLE_OTA_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_WRITE_NR
    );
    pOTACharacteristic->setCallbacks(&otaCallbacks);

    // 2. Remote Test Command Characteristic
    BLECharacteristic *pCmdCharacteristic = pService->createCharacteristic(
        BLE_CMD_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_WRITE_NR
    );
    pCmdCharacteristic->setCallbacks(&cmdCallbacks);

    // 3. Firmware Version & Serial Number Characteristic (Read-Only)
    BLECharacteristic *pVerCharacteristic = pService->createCharacteristic(
        BLE_VER_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ
    );
    char verBuf[64];
    snprintf(verBuf, sizeof(verBuf), "%s|SN:%s", FIRMWARE_VERSION, serialNo);
    pVerCharacteristic->setValue(verBuf);

    // 4. Button Event Notification Characteristic (Notify)
    pBtnCharacteristic = pService->createCharacteristic(
        BLE_BTN_CHAR_UUID,
        BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
    );
    pBtnCharacteristic->addDescriptor(new BLE2902());
    pBtnCharacteristic->setValue("IDLE");

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();

    // Primary advertisement packet (max 31 bytes):
    // Service UUID-128 = 18 bytes — fits cleanly
    BLEAdvertisementData oAdvertisementData;
    oAdvertisementData.setCompleteServices(BLEUUID(BLE_SERVICE_UUID));
    pAdvertising->setAdvertisementData(oAdvertisementData);

    // Scan response packet (max 31 bytes):
    // Device name with unique suffix (e.g. ESP32_HUB75_1650)
    BLEAdvertisementData oScanResponseData;
    oScanResponseData.setName(devName);
    pAdvertising->setScanResponseData(oScanResponseData);

    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);

    BLEDevice::startAdvertising();
    Serial.println("BLE Ready (Advertising 'ESP32_HUB75_DIAGNOSTIC'). Waiting for connections...");
}