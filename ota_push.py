import asyncio
import os
import subprocess
from bleak import BleakScanner, BleakClient

DEVICE_NAME             = "ESP32_HUB75_DIAGNOSTIC"
SERVICE_UUID            = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
OTA_CHARACTERISTIC_UUID = "c8659210-af98-4360-91cc-8e2a10587822"
CMD_CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"  # Command channel
FIRMWARE_PATH           = ".pio/build/esp32dev/firmware.bin"
CHUNK_SIZE              = 244


# Find pio — works whether run from PlatformIO terminal or plain venv
PIO_PATHS = [
    "pio",
    os.path.expanduser("~/.platformio/penv/bin/pio"),
]

def find_pio():
    import shutil
    for p in PIO_PATHS:
        if shutil.which(p) or os.path.isfile(p):
            return p
    return None



async def find_device():
    """Scan all BLE devices and filter client-side by name or service UUID.
    Avoids CoreBluetooth passive-scan filter timing issues on macOS."""
    print("Scanning all BLE devices (15s)...")

    # return_adv=True gives us both device + advertisement data
    results = await BleakScanner.discover(timeout=15.0, return_adv=True)

    for device, adv in results.values():
        name = (device.name or "").lower()
        uuids = [str(u).lower() for u in adv.service_uuids]

        if "esp32_hub75" in name or SERVICE_UUID.lower() in uuids:
            print(f"  Found: '{device.name or DEVICE_NAME}' [{device.address}]")
            print(f"  Name match: {'esp32_hub75' in name}  |  UUID match: {SERVICE_UUID.lower() in uuids}")
            return device

    # Debug: show what was found
    print(f"  Scanned {len(results)} devices, none matched. Nearby devices:")
    for device, adv in list(results.values())[:8]:
        print(f"    '{device.name or '?'}' [{device.address}]  UUIDs: {[str(u) for u in adv.service_uuids]}")
    return None


async def main():
    # 1. Build
    print("Building firmware via PlatformIO...")
    pio = find_pio()
    if not pio:
        print("Error: 'pio' not found. Install PlatformIO or run from its terminal.")
        return
    try:
        subprocess.run([pio, "run"], check=True)
    except subprocess.CalledProcessError:
        print("Build failed! Aborting OTA update.")
        return
    print("Build successful! Starting OTA push...\n")


    if not os.path.exists(FIRMWARE_PATH):
        print(f"Error: {FIRMWARE_PATH} not found. Build first.")
        return

    # 2. Find device
    device = await find_device()
    if not device:
        print("Device not found. Make sure:")
        print("  1. ESP32 is powered on")
        print("  2. No other app (Flutter) is connected to it")
        print(f"  3. Advertising service UUID: {SERVICE_UUID}")
        return

    name = device.name or DEVICE_NAME
    print(f"Found '{name}' at {device.address}. Connecting...")

    # 3. Connect and flash
    async with BleakClient(device) as client:
        print("Connected.")

        # Step 1: Switch to OTA screen (convenience navigation)
        print("→ Switching to OTA screen (ota_mode)...")
        await client.write_gatt_char(CMD_CHARACTERISTIC_UUID, b"ota_mode", response=True)
        await asyncio.sleep(0.5)

        print("\n⚠️ SECURITY CHECK:")
        print("   1. Look at your ESP32 board / Matrix screen.")
        print("   2. Press the physical BOOT button on the ESP32 to UNLOCK OTA.")
        print("   3. Press ENTER in this terminal once you have pressed the button...\n")
        
        # Pause execution in terminal until user confirms button press
        await asyncio.get_event_loop().run_in_executor(None, input, "Press ENTER after pressing BOOT button on ESP32: ")

        # Step 2: Tell ESP32 how many bytes to expect (with response=True)
        with open(FIRMWARE_PATH, "rb") as f:
            data = f.read()

        size = len(data)
        print(f"→ Starting firmware transfer...")
        print(f"   Firmware: {size} bytes ({size / 1024:.1f} KB)")

        await client.write_gatt_char(OTA_CHARACTERISTIC_UUID, f"START:{size}".encode(), response=True)

        # Step 3: Stream in 512-byte chunks (response=True)
        print(f"   Streaming in {CHUNK_SIZE}-byte chunks...")
        t0 = asyncio.get_event_loop().time()

        for i in range(0, size, CHUNK_SIZE):
            chunk = data[i:i + CHUNK_SIZE]
            await client.write_gatt_char(OTA_CHARACTERISTIC_UUID, chunk, response=True)

            if i % (CHUNK_SIZE * 20) == 0:
                pct = (i / size) * 100
                print(f"   {i}/{size} bytes ({pct:.1f}%)")

        print(f"   {size}/{size} bytes (100.0%)")

        # Step 4: Signal completion
        await client.write_gatt_char(OTA_CHARACTERISTIC_UUID, b"END", response=True)

        elapsed = asyncio.get_event_loop().time() - t0
        print(f"\n✓ OTA Complete! ESP32 is rebooting.")
        print(f"  Transfer time: {elapsed:.1f}s")



if __name__ == "__main__":
    asyncio.run(main())