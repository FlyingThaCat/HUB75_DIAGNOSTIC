# ESP32 HUB75 Matrix Controller & Diagnostic Suite

Hardware diagnostic tool, desktop/mobile GUI controller, and wireless BLE OTA updater for HUB75 RGB LED matrix panels using ESP32.

---

## Architecture

- **ESP32 Firmware (`/src`)**: Hardware test patterns, remote panel configuration, and BLE OTA receiver.
- **App (`/hub75_diagnostic_app`)**: Native UI controller (macOS, iOS, Android, Windows, Linux).
- **OTA Script (`ota_push.py`)**: Python BLE firmware updater.
- **CI/CD (`.github/workflows`)**: Automated builds and GitHub pre-releases on push.

---

## HUB75 Pinout (ESP32)

```text
+------------------------------------------+
|   HUB75 16-PIN CONNECTOR (ESP32 GPIO)   |
+------------------------------------------+
| [R1]  GPIO 25 |  1   2 | GPIO 26 [G1]    |
| [B1]  GPIO 27 |  3   4 | GND              |
| [R2]  GPIO 14 |  5   6 | GPIO 12 [G2]    |
| [B2]  GPIO 13 |  7   8 | GND              |
|  [A]  GPIO 23 |  9  10 | GPIO 19 [B]     |
|  [C]  GPIO 5  | 11  12 | GPIO 17 [D]     |
| [CLK] GPIO 16 | 13  14 | GPIO 4  [LAT]   |
|  [OE] GPIO 15 | 15  16 | GND              |
+------------------------------------------+
```

> **Onboard Blue LED (`GPIO 2`)**:
> - **ON**: OTA Security Unlocked.
> - **OFF**: OTA Security Locked.

---

## Security & OTA Authorization

1. Switch matrix to **OTA Screen** (`STATE_OTA_MODE`).
2. Press physical **BOOT button** on ESP32 (LED turns ON).
3. Push firmware via script or app.

---

## Usage

### Wireless Firmware Update (BLE)

```bash
pip install bleak
python ota_push.py
```

### Build & Flash via USB

```bash
# Firmware
pio run -t upload

# Flutter App
cd hub75_diagnostic_app
flutter run
```

---

## CI/CD Pipeline

Pushes to `main` automatically trigger GitHub Actions to compile binaries and create tagged GitHub Releases.
