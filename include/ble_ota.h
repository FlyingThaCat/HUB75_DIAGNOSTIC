#ifndef BLE_OTA_H
#define BLE_OTA_H

#include <Arduino.h>

#define BLE_SERVICE_UUID      "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_OTA_CHAR_UUID     "c8659210-af98-4360-91cc-8e2a10587822"
#define BLE_CMD_CHAR_UUID     "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define BLE_VER_CHAR_UUID     "1a2b3c4d-5e6f-7a8b-9c0d-1e2f3a4b5c6d"
#define BLE_BTN_CHAR_UUID     "5c6d7e8f-9a0b-1c2d-3e4f-5a6b7c8d9e0f"

#define FIRMWARE_VERSION      "1.0.2"

extern bool otaPendingReboot;
extern unsigned long otaRebootTimer;
extern const int LED_PIN;

void setupBLE();
void notifyButtonEvent(const char* event);
void setOtaEnabled(bool enabled);
bool isOtaEnabled();

#endif

