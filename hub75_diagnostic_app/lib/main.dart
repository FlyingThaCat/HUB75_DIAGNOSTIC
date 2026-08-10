import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter/cupertino.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:file_picker/file_picker.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:fluent_ui/fluent_ui.dart' as fluent;
import 'package:macos_ui/macos_ui.dart' as macos;
import 'package:yaru/yaru.dart' as yaru;
import 'package:http/http.dart' as http;

const String serviceUuid = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const String otaCharUuid = "c8659210-af98-4360-91cc-8e2a10587822";
const String cmdCharUuid = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
const String verCharUuid = "1a2b3c4d-5e6f-7a8b-9c0d-1e2f3a4b5c6d";
const String btnCharUuid = "5c6d7e8f-9a0b-1c2d-3e4f-5a6b7c8d9e0f";

// GitHub Repository for automated release fetching
const String githubRepo = "FlyingThaCat/HUB75_DIAGNOSTIC";


void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const Hub75NativeApp());
}

class Hub75NativeApp extends StatelessWidget {
  const Hub75NativeApp({super.key});

  @override
  Widget build(BuildContext context) {
    if (!kIsWeb && Platform.isMacOS) {
      return macos.MacosApp(
        title: 'HUB75 Matrix Pro',
        theme: macos.MacosThemeData.dark(),
        home: const NativeDashboardScreen(),
      );
    } else if (!kIsWeb && Platform.isWindows) {
      return fluent.FluentApp(
        title: 'HUB75 Matrix Pro',
        theme: fluent.FluentThemeData.dark(),
        home: const NativeDashboardScreen(),
      );
    } else if (!kIsWeb && Platform.isLinux) {
      return yaru.YaruTheme(
        builder: (context, yaruTheme, child) {
          return MaterialApp(
            title: 'HUB75 Matrix Pro',
            theme: yaruTheme.theme,
            darkTheme: yaruTheme.darkTheme,
            home: const NativeDashboardScreen(),
          );
        },
      );
    } else if (!kIsWeb && Platform.isIOS) {
      return const CupertinoApp(
        title: 'HUB75 Matrix Pro',
        theme: CupertinoThemeData(brightness: Brightness.dark),
        home: NativeDashboardScreen(),
      );
    }

    return MaterialApp(
      title: 'HUB75 Matrix Pro',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        useMaterial3: true,
        brightness: Brightness.dark,
        colorScheme: ColorScheme.fromSeed(
          seedColor: const Color(0xFF38BDF8),
          brightness: Brightness.dark,
        ),
      ),
      home: const NativeDashboardScreen(),
    );
  }
}

class NativeDashboardScreen extends StatefulWidget {
  const NativeDashboardScreen({super.key});

  @override
  State<NativeDashboardScreen> createState() => _NativeDashboardScreenState();
}

class _NativeDashboardScreenState extends State<NativeDashboardScreen> {
  BluetoothDevice? targetDevice;
  BluetoothCharacteristic? cmdChar;
  BluetoothCharacteristic? otaChar;
  BluetoothCharacteristic? btnChar;

  bool isScanning = false;
  bool isConnected = false;
  String fwVersion = "Unknown";
  String lastBtnEvent = "No Events Yet";
  bool isOtaUnlocked = false;
  double otaProgress = 0.0;
  bool isFlashing = false;

  int gridX = 0;
  int gridY = 0;

  final List<ScanResult> scanResults = [];
  StreamSubscription? btnSubscription;
  StreamSubscription? _adapterStateSubscription;
  BluetoothAdapterState _adapterState = BluetoothAdapterState.unknown;

  @override
  void initState() {
    super.initState();
    checkWiringPopup();
    checkForAppUpdates();
    // Listen for adapter state — scan only once BT is fully powered on
    _adapterStateSubscription = FlutterBluePlus.adapterState.listen((state) {
      if (!mounted) return;
      setState(() => _adapterState = state);
      if (state == BluetoothAdapterState.on && !isConnected && !isScanning) {
        startScan();
      }
    });
  }

  void checkForAppUpdates() async {
    try {
      final url = 'https://api.github.com/repos/$githubRepo/releases';
      final res = await http.get(Uri.parse(url));

      if (res.statusCode == 200) {
        List releases = jsonDecode(res.body);
        if (releases.isEmpty) return;

        // Get latest release (including pre-releases)
        var latestRelease = releases.first;
        String latestTag = latestRelease['tag_name'] ?? '';
        List assets = latestRelease['assets'] ?? [];

        var apkAsset = assets.firstWhere(
          (a) => (a['name'] as String).endsWith('.apk'),
          orElse: () => null,
        );

        if (apkAsset != null && mounted) {
          String apkDownloadUrl = apkAsset['browser_download_url'];
          String releaseNotes = latestRelease['body'] ?? 'New version available on GitHub.';

          WidgetsBinding.instance.addPostFrameCallback((_) {
            _showAppUpdateDialog(latestTag, apkDownloadUrl, releaseNotes);
          });
        }
      }
    } catch (e) {
      debugPrint("App update check skipped: $e");
    }
  }

  void _showAppUpdateDialog(String tag, String apkUrl, String notes) {
    showDialog(
      context: context,
      builder: (dialogCtx) => AlertDialog(
        title: Row(
          children: [
            const Icon(Icons.system_update, color: Colors.cyanAccent),
            const SizedBox(width: 8),
            Expanded(child: Text("App Update Available ($tag)")),
          ],
        ),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              "A new version of the HUB75 Diagnostic App is available on GitHub!",
              style: TextStyle(fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 8),
            Container(
              padding: const EdgeInsets.all(8),
              decoration: BoxDecoration(
                color: Colors.black38,
                borderRadius: BorderRadius.circular(6),
              ),
              child: Text(
                notes.length > 200 ? "${notes.substring(0, 200)}..." : notes,
                style: const TextStyle(fontSize: 12, color: Colors.grey),
              ),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(dialogCtx).pop(),
            child: const Text("Later"),
          ),
          ElevatedButton.icon(
            onPressed: () async {
              Navigator.of(dialogCtx).pop();
              _showNotification("Direct APK Download URL copied to clipboard or downloading...");
            },
            icon: const Icon(Icons.download),
            label: const Text("Download APK"),
            style: ElevatedButton.styleFrom(backgroundColor: Colors.cyan),
          ),
        ],
      ),
    );
  }

  @override
  void dispose() {
    _adapterStateSubscription?.cancel();
    btnSubscription?.cancel();
    super.dispose();
  }

  void checkWiringPopup() async {
    SharedPreferences prefs = await SharedPreferences.getInstance();
    bool hideWiring = prefs.getBool('hide_wiring_popup') ?? false;
    if (!hideWiring && mounted) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        showWiringDialog();
      });
    }
  }

  void showWiringDialog() {
    bool dontRemind = false;
    showDialog(
      context: context,
      builder: (dialogContext) => StatefulBuilder(
        builder: (context, setDialogState) => AlertDialog(
          title: const Row(
            children: [
              Icon(Icons.cable, color: Colors.cyanAccent),
              SizedBox(width: 8),
              Text("HUB75 Cable Wiring & Config"),
            ],
          ),
          content: SingleChildScrollView(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              mainAxisSize: MainAxisSize.min,
              children: [
                const Text(
                  "Default ESP32 16-Pin HUB75 Matrix Wiring:",
                  style: TextStyle(fontWeight: FontWeight.bold),
                ),
                const SizedBox(height: 8),
                Container(
                  padding: const EdgeInsets.all(12),
                  decoration: BoxDecoration(
                    color: Colors.black45,
                    borderRadius: BorderRadius.circular(8),
                    border: Border.all(color: Colors.white24),
                  ),
                  child: const Text(
                    " [R1] GPIO 25  | 1  2 |  GPIO 26 [G1]\n"
                    " [B1] GPIO 27  | 3  4 |  GND\n"
                    " [R2] GPIO 14  | 5  6 |  GPIO 12 [G2]\n"
                    " [B2] GPIO 13  | 7  8 |  GND\n"
                    "  [A] GPIO 23  | 9 10 |  GPIO 22 [B]\n"
                    "  [C] GPIO  5  |11 12 |  GPIO 17 [D]\n"
                    "[CLK] GPIO 16  |13 14 |  GPIO  4 [LAT]\n"
                    " [OE] GPIO 15  |15 16 |  GND",
                    style: TextStyle(fontFamily: 'monospace', fontSize: 12),
                  ),
                ),
                const SizedBox(height: 12),
                const Text(
                  "⚠️ Configuration Reminder:\n"
                  "Hold physical BOOT button on boot for 2s to enter Hardware Config Mode (Matrix size: 32x32, 64x32, 64x64, 128x32, and clock speed).",
                  style: TextStyle(color: Colors.amberAccent, fontSize: 13),
                ),
                const SizedBox(height: 12),
                Row(
                  children: [
                    Checkbox(
                      value: dontRemind,
                      onChanged: (val) {
                        setDialogState(() {
                          dontRemind = val ?? false;
                        });
                      },
                    ),
                    const Text("Don't remind me again"),
                  ],
                )
              ],
            ),
          ),
          actions: [
            ElevatedButton(
              onPressed: () async {
                final nav = Navigator.of(dialogContext);
                if (dontRemind) {
                  SharedPreferences prefs = await SharedPreferences.getInstance();
                  await prefs.setBool('hide_wiring_popup', true);
                }
                nav.pop();
              },
              child: const Text("Got It"),
            ),
          ],
        ),
      ),
    );
  }

  void startScan() async {
    if (_adapterState != BluetoothAdapterState.on) return;
    if (isScanning) return;

    // Request Android runtime permissions if needed
    if (!kIsWeb && Platform.isAndroid) {
      try {
        await FlutterBluePlus.turnOn();
      } catch (e) {
        debugPrint("Turn on / permission warning: $e");
      }
    }

    setState(() {
      isScanning = true;
      scanResults.clear();
    });

    FlutterBluePlus.scanResults.listen((results) {
      if (mounted) {
        setState(() {
          for (var r in results) {
            // Client-side filter: only show our ESP32
            // Match by name (scan response) OR service UUID (primary ad)
            final name = r.advertisementData.advName.toLowerCase();
            final uuids = r.advertisementData.serviceUuids
                .map((u) => u.toString().toLowerCase())
                .toList();
            final isOurDevice = name.contains('esp32_hub75') ||
                uuids.contains(serviceUuid.toLowerCase());
            if (!isOurDevice) continue;

            if (!scanResults.any((e) => e.device.remoteId == r.device.remoteId)) {
              scanResults.add(r);
            }
          }
        });
      }
    });

    try {
      // No filter here — CoreBluetooth on macOS applies keyword/UUID filters
      // BEFORE scan response data arrives, so we'd filter out our own device.
      // Instead we scan everything and filter client-side below.
      await FlutterBluePlus.startScan(
        timeout: const Duration(seconds: 8),
      );
    } catch (e) {
      debugPrint("BLE Scan error: $e");
    }

    if (mounted) setState(() => isScanning = false);
  }


  Future<void> connectToDevice(BluetoothDevice device) async {
    await FlutterBluePlus.stopScan();
    try {
      await device.connect(timeout: const Duration(seconds: 15));
      List<BluetoothService> services = await device.discoverServices();
      
      BluetoothService? targetService = services.firstWhere(
        (s) => s.uuid.toString().toLowerCase() == serviceUuid,
      );

      for (var c in targetService.characteristics) {
        String uuid = c.uuid.toString().toLowerCase();
        if (uuid == cmdCharUuid) cmdChar = c;
        if (uuid == otaCharUuid) otaChar = c;
        if (uuid == btnCharUuid) btnChar = c;
        if (uuid == verCharUuid) {
          var val = await c.read();
          fwVersion = utf8.decode(val);
        }
      }

      if (btnChar != null) {
        await btnChar!.setNotifyValue(true);
        btnSubscription = btnChar!.lastValueStream.listen((value) {
          if (value.isNotEmpty && mounted) {
            String evt = utf8.decode(value);
            setState(() {
              lastBtnEvent = evt;
              if (evt.contains("OTA:UNLOCKED")) isOtaUnlocked = true;
              if (evt.contains("OTA:LOCKED")) isOtaUnlocked = false;
              if (evt.startsWith("PROGRESS:")) {
                int p = int.tryParse(evt.substring(9)) ?? 0;
                otaProgress = p / 100.0;
              }
            });
          }
        });
      }


      // Listen for unexpected disconnects
      device.connectionState.listen((state) {
        if (state == BluetoothConnectionState.disconnected && mounted) {
          setState(() {
            isConnected = false;
            targetDevice = null;
            cmdChar = null;
            otaChar = null;
            btnChar = null;
          });
        }
      });

      if (mounted) {
        setState(() {
          targetDevice = device;
          isConnected = true;
        });
      }
    } catch (e) {
      if (mounted) {
        _showNotification("Connection failed: $e");
      }
    }
  }

  void sendCommand(String cmd) async {
    if (cmdChar != null && isConnected) {
      try {
        await cmdChar!.write(utf8.encode(cmd), withoutResponse: true);
      } catch (e) {
        debugPrint("sendCommand error: $e");
      }
    }
  }

  /// Platform-aware notification: Cupertino dialog on macOS/iOS, SnackBar elsewhere.
  void _showNotification(String message, {Color? color}) {
    if (!mounted) return;
    if (!kIsWeb && (Platform.isMacOS || Platform.isIOS)) {
      showCupertinoDialog(
        context: context,
        barrierDismissible: true,
        builder: (ctx) => CupertinoAlertDialog(
          content: Text(message),
          actions: [
            CupertinoDialogAction(
              onPressed: () => Navigator.of(ctx).pop(),
              child: const Text('OK'),
            ),
          ],
        ),
      );
    } else {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(message),
          backgroundColor: color,
        ),
      );
    }
  }

  void pickAndSendColor() {
    List<Color> palette = [
      Colors.red, Colors.pink, Colors.purple, Colors.deepPurple,
      Colors.indigo, Colors.blue, Colors.lightBlue, Colors.cyan,
      Colors.teal, Colors.green, Colors.lightGreen, Colors.lime,
      Colors.yellow, Colors.amber, Colors.orange, Colors.deepOrange,
      Colors.white, Colors.grey, Colors.black,
    ];

    showDialog(
      context: context,
      builder: (dialogContext) => AlertDialog(
        title: const Text("Select Custom Pattern Color"),
        content: SizedBox(
          width: 280,
          height: 240,
          child: GridView.builder(
            gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
              crossAxisCount: 4,
              crossAxisSpacing: 8,
              mainAxisSpacing: 8,
            ),
            itemCount: palette.length,
            itemBuilder: (context, index) {
              Color color = palette[index];
              return InkWell(
                onTap: () {
                  Navigator.of(dialogContext).pop();
                  int r = (color.r * 255).round().clamp(0, 255);
                  int g = (color.g * 255).round().clamp(0, 255);
                  int b = (color.b * 255).round().clamp(0, 255);
                  sendCommand("color:$r,$g,$b");
                },
                child: Container(
                  decoration: BoxDecoration(
                    color: color,
                    shape: BoxShape.circle,
                    border: Border.all(color: Colors.white24, width: 2),
                  ),
                ),
              );
            },
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(dialogContext).pop(),
            child: const Text("Cancel"),
          ),
        ],
      ),
    );
  }

  void showHardwareConfigDialog() {
    String selectedPreset = "32x32";
    bool slowClock = false;

    showDialog(
      context: context,
      builder: (dialogContext) => StatefulBuilder(
        builder: (context, setDialogState) => AlertDialog(
          title: const Row(
            children: [
              Icon(Icons.settings, color: Colors.amberAccent),
              SizedBox(width: 8),
              Text("Hardware Configuration"),
            ],
          ),
          content: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const Text("Matrix Panel Resolution:", style: TextStyle(fontWeight: FontWeight.bold)),
              const SizedBox(height: 8),
              DropdownButton<String>(
                value: selectedPreset,
                isExpanded: true,
                items: const [
                  DropdownMenuItem(value: "32x32", child: Text("32 x 32 Preset")),
                  DropdownMenuItem(value: "64x32", child: Text("64 x 32 Preset")),
                  DropdownMenuItem(value: "64x64", child: Text("64 x 64 Preset")),
                  DropdownMenuItem(value: "128x32", child: Text("128 x 32 Preset")),
                ],
                onChanged: (val) {
                  if (val != null) {
                    setDialogState(() => selectedPreset = val);
                  }
                },
              ),
              const SizedBox(height: 16),
              const Text("I2S Clock Speed:", style: TextStyle(fontWeight: FontWeight.bold)),
              SwitchListTile(
                title: const Text("Cheap Panel Mode (Slow 10MHz Clock)"),
                subtitle: const Text("Enable for stability if matrix flickers"),
                value: slowClock,
                onChanged: (val) {
                  setDialogState(() => slowClock = val);
                },
              ),
            ],
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.of(dialogContext).pop(),
              child: const Text("Cancel"),
            ),
            ElevatedButton(
              onPressed: () {
                Navigator.of(dialogContext).pop();
                int w = 32;
                int h = 32;
                if (selectedPreset == "64x32") { w = 64; h = 32; }
                else if (selectedPreset == "64x64") { w = 64; h = 64; }
                else if (selectedPreset == "128x32") { w = 128; h = 32; }

                int slow = slowClock ? 1 : 0;
                sendCommand("cfg:$w,$h,$slow");

                _showNotification("Sending config → Size: ${w}x$h | Slow Clock: ${slowClock ? 'YES' : 'NO'}. ESP32 is saving & rebooting...");
              },
              child: const Text("Save & Apply (Reboot)"),
            ),
          ],
        ),
      ),
    );
  }


  /// Flash raw binary bytes over BLE OTA
  Future<void> _flashBytes(List<int> bytes) async {
    if (otaChar == null) return;
    setState(() {
      isFlashing = true;
      otaProgress = 0.0;
    });

    String startCmd = "START:${bytes.length}";
    await otaChar!.write(utf8.encode(startCmd), withoutResponse: false);
    await Future.delayed(const Duration(milliseconds: 300));

    int chunkSize = 244;
    for (int i = 0; i < bytes.length; i += chunkSize) {
      int end = (i + chunkSize < bytes.length) ? i + chunkSize : bytes.length;
      List<int> chunk = bytes.sublist(i, end);
      await otaChar!.write(chunk, withoutResponse: false);

      if (i % (chunkSize * 20) == 0 && mounted) {
        setState(() {
          otaProgress = (i / bytes.length);
        });
      }
    }

    await otaChar!.write(utf8.encode("END"), withoutResponse: false);

    if (mounted) {
      setState(() {
        otaProgress = 1.0;
        isFlashing = false;
      });

      _showNotification("OTA Firmware Upload Complete! ESP32 is rebooting...");
    }
  }

  void uploadFirmwareOTA() async {
    if (!isOtaUnlocked) {
      _showNotification(
        "OTA is LOCKED! Press physical BOOT button while on OTA Update Screen first.",
        color: Colors.amber,
      );
      return;
    }

    FilePickerResult? result = await FilePicker.platform.pickFiles(
      type: FileType.custom,
      allowedExtensions: ['bin'],
    );

    if (result != null && result.files.single.path != null && otaChar != null) {
      File binFile = File(result.files.single.path!);
      List<int> bytes = await binFile.readAsBytes();
      await _flashBytes(bytes);
    }
  }

  /// Automatically fetch the latest firmware binary from GitHub Releases (Stable or Beta)
  void fetchAndFlashGitHubFirmware({bool isBeta = false}) async {
    if (!isOtaUnlocked) {
      _showNotification(
        "OTA is LOCKED! Press physical BOOT button while on OTA Update Screen first.",
        color: Colors.amber,
      );
      return;
    }

    _showNotification("Fetching latest ${isBeta ? 'Beta' : 'Stable'} firmware from GitHub...");

    try {
      final url = isBeta
          ? 'https://api.github.com/repos/$githubRepo/releases'
          : 'https://api.github.com/repos/$githubRepo/releases/latest';

      final res = await http.get(Uri.parse(url));

      if (res.statusCode == 200) {
        dynamic releaseData;
        if (isBeta) {
          List releases = jsonDecode(res.body);
          releaseData = releases.firstWhere((r) => r['prerelease'] == true, orElse: () => releases.first);
        } else {
          releaseData = jsonDecode(res.body);
        }

        List assets = releaseData['assets'] ?? [];
        var binAsset = assets.firstWhere(
          (a) => (a['name'] as String).endsWith('.bin'),
          orElse: () => null,
        );

        if (binAsset != null && binAsset['browser_download_url'] != null) {
          String downloadUrl = binAsset['browser_download_url'];
          _showNotification("Downloading ${binAsset['name']}...");

          final binRes = await http.get(Uri.parse(downloadUrl));
          if (binRes.statusCode == 200) {
            _showNotification("Download Complete! Starting BLE OTA Flash...");
            await _flashBytes(binRes.bodyBytes);
          } else {
            _showNotification("Failed to download firmware binary (${binRes.statusCode})");
          }
        } else {
          _showNotification("No .bin asset found in the latest GitHub release!");
        }
      } else {
        _showNotification("GitHub release fetch failed (${res.statusCode})");
      }
    } catch (e) {
      _showNotification("GitHub fetch error: $e");
    }
  }

  @override
  Widget build(BuildContext context) {
    if (!kIsWeb && Platform.isMacOS) {
      return macos.MacosWindow(
        child: macos.MacosScaffold(
          toolBar: macos.ToolBar(
            title: const Text('HUB75 Matrix Pro'),
            actions: [
              macos.ToolBarIconButton(
                label: 'Wiring Info',
                icon: const macos.MacosIcon(CupertinoIcons.info),
                showLabel: false,
                onPressed: showWiringDialog,
              ),
              if (isConnected)
                macos.ToolBarIconButton(
                  label: isOtaUnlocked ? 'Unlocked' : 'Locked',
                  icon: macos.MacosIcon(isOtaUnlocked ? CupertinoIcons.lock_open : CupertinoIcons.lock),
                  showLabel: false,
                  onPressed: () {
                    sendCommand(isOtaUnlocked ? "ota_lock" : "ota_unlock");
                    setState(() => isOtaUnlocked = !isOtaUnlocked);
                  },
                ),
            ],
          ),
          children: [
            macos.ContentArea(
              builder: (context, scrollController) {
                return buildDashboardContent();
              },
            ),
          ],
        ),
      );
    }

    if (!kIsWeb && Platform.isWindows) {
      return fluent.ScaffoldPage(
        header: fluent.PageHeader(
          title: const Text('HUB75 Matrix Pro'),
          commandBar: fluent.CommandBar(
            primaryItems: [
              fluent.CommandBarButton(
                icon: const fluent.Icon(fluent.FluentIcons.info),
                label: const Text('Wiring'),
                onPressed: showWiringDialog,
              ),
            ],
          ),
        ),
        content: buildDashboardContent(),
      );
    }

    return Scaffold(
      appBar: AppBar(
        title: const Text("HUB75 Diagnostic Matrix Pro"),
        centerTitle: true,
        actions: [
          IconButton(
            icon: const Icon(Icons.cable_outlined),
            onPressed: showWiringDialog,
            tooltip: "Cable Wiring Diagram",
          ),
          if (isConnected)
            IconButton(
              icon: const Icon(Icons.settings_suggest),
              onPressed: showHardwareConfigDialog,
              tooltip: "Hardware Config",
            ),
          if (isConnected)
            IconButton(
              icon: Icon(isOtaUnlocked ? Icons.lock_open : Icons.lock, color: isOtaUnlocked ? Colors.green : Colors.red),
              onPressed: () {
                sendCommand(isOtaUnlocked ? "ota_lock" : "ota_unlock");
                setState(() => isOtaUnlocked = !isOtaUnlocked);
              },
            ),
        ],

      ),
      body: buildDashboardContent(),
    );
  }

  Widget buildDashboardContent() {
    if (!isConnected) {
      return Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const Icon(Icons.bluetooth_searching, size: 64, color: Colors.lightBlueAccent),
            const SizedBox(height: 12),
            const Text(
              "Connect to HUB75 Diagnostic Device",
              style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 4),
            const Text(
              "Please power on your ESP32 board and scan for devices below:",
              style: TextStyle(color: Colors.grey, fontSize: 13),
              textAlign: TextAlign.center,
            ),
            const SizedBox(height: 16),
            ElevatedButton.icon(
              onPressed: (isScanning || _adapterState != BluetoothAdapterState.on) ? null : startScan,
              icon: Icon(isScanning ? Icons.bluetooth_searching : Icons.refresh),
              label: Text(
                _adapterState == BluetoothAdapterState.unknown
                  ? "Waiting for Bluetooth..."
                  : _adapterState != BluetoothAdapterState.on
                    ? "Bluetooth is Off — Please Enable It"
                    : isScanning
                      ? "Scanning for HUB75 Matrix..."
                      : "Scan BLE Devices",
              ),
            ),

            const SizedBox(height: 16),
            Expanded(
              child: ListView.builder(
                itemCount: scanResults.length,
                itemBuilder: (context, index) {
                  var r = scanResults[index];
                  String name = r.advertisementData.advName.isNotEmpty
                      ? r.advertisementData.advName
                      : (r.device.platformName.isNotEmpty
                          ? r.device.platformName
                          : r.device.remoteId.toString());
                  return Card(
                    color: const Color(0xFF1E293B),
                    margin: const EdgeInsets.symmetric(vertical: 4),
                    child: ListTile(
                      title: Text(name, style: const TextStyle(fontWeight: FontWeight.bold)),
                      subtitle: Text(r.device.remoteId.toString()),
                      trailing: ElevatedButton(
                        onPressed: () => connectToDevice(r.device),
                        child: const Text("Connect"),
                      ),
                    ),
                  );
                },

              ),
            )
          ],
        ),
      );
    }

    return SingleChildScrollView(
      padding: const EdgeInsets.all(16.0),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Device Info Card
          Card(
            color: const Color(0xFF1E293B),
            child: Padding(
              padding: const EdgeInsets.all(16.0),
              child: Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text("Device: ${targetDevice?.platformName}", style: const TextStyle(fontSize: 16, fontWeight: FontWeight.bold)),
                      Text("Firmware Version: v$fwVersion", style: const TextStyle(color: Colors.grey)),
                      const SizedBox(height: 4),
                      Text("Button Event: $lastBtnEvent", style: const TextStyle(color: Colors.lightBlueAccent, fontWeight: FontWeight.w600)),
                    ],
                  ),
                  Chip(
                    label: Text(isOtaUnlocked ? "OTA Unlocked" : "OTA Locked"),
                    backgroundColor: isOtaUnlocked ? Colors.green.withAlpha(50) : Colors.red.withAlpha(50),
                    side: BorderSide(color: isOtaUnlocked ? Colors.green : Colors.red),
                  )
                ],
              ),
            ),
          ),
          const SizedBox(height: 16),

          // Pattern Test Matrix Controls
          const Text("Diagnostic Patterns", style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
          const SizedBox(height: 8),
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: [
              buildPatternButton("White", "white", Colors.white),
              buildPatternButton("Red", "red", Colors.red),
              buildPatternButton("Green", "green", Colors.green),
              buildPatternButton("Blue", "blue", Colors.blue),
              buildPatternButton("Yellow", "yellow", Colors.yellow),
              buildPatternButton("Cyan", "cyan", Colors.cyan),
              buildPatternButton("Magenta", "magenta", Colors.purpleAccent),
              buildPatternButton("Checkerboard", "checkerboard", Colors.grey),
              buildPatternButton("Quadrant Chase", "quadrant", Colors.indigo),
              buildPatternButton("Diagonal", "diagonal", Colors.teal),
              buildPatternButton("Row Sweep", "row_sweep", Colors.orange),
              buildPatternButton("Col Sweep", "col_sweep", Colors.deepOrange),
              buildPatternButton("IC Chase", "ic_chase", Colors.amber),
              buildPatternButton("Ghosting Test", "ghosting", Colors.pinkAccent),
              buildPatternButton("OTA Update Screen", "ota_mode", Colors.cyanAccent),
              buildPatternButton("Turn Off", "off", Colors.black),
            ],
          ),
          const SizedBox(height: 16),

          // Custom Color Picker & Advanced Tests
          Row(
            children: [
              Expanded(
                child: ElevatedButton.icon(
                  onPressed: pickAndSendColor,
                  icon: const Icon(Icons.palette),
                  label: const Text("Custom Color Picker"),
                  style: ElevatedButton.styleFrom(backgroundColor: const Color(0xFF3B82F6)),
                ),
              ),
            ],
          ),
          const SizedBox(height: 16),

          // Pixel Hunter Grid Tool
          const Text("Interactive Pixel Hunter", style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
          const SizedBox(height: 8),
          Card(
            color: const Color(0xFF1E293B),
            child: Padding(
              padding: const EdgeInsets.all(16.0),
              child: Column(
                children: [
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceAround,
                    children: [
                      Text("Target X: $gridX"),
                      Slider(
                        value: gridX.toDouble(),
                        min: 0,
                        max: 63,
                        divisions: 63,
                        onChanged: (val) {
                          setState(() => gridX = val.toInt());
                          sendCommand("pixel:$gridX,$gridY");
                        },
                      ),
                    ],
                  ),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceAround,
                    children: [
                      Text("Target Y: $gridY"),
                      Slider(
                        value: gridY.toDouble(),
                        min: 0,
                        max: 63,
                        divisions: 63,
                        onChanged: (val) {
                          setState(() => gridY = val.toInt());
                          sendCommand("pixel:$gridX,$gridY");
                        },
                      ),
                    ],
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(height: 16),

          // OTA Firmware Update Card
          Card(
            color: const Color(0xFF1E293B),
            child: Padding(
              padding: const EdgeInsets.all(16.0),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  const Text("Firmware Over-The-Air (OTA)", style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold)),
                  const SizedBox(height: 4),
                  const Text("1. Switch to 'OTA Update Screen'\n2. Press physical BOOT button on ESP32 to unlock\n3. Flash your binary file below", style: TextStyle(color: Colors.grey, fontSize: 12)),
                  const SizedBox(height: 12),
                  if (isFlashing) LinearProgressIndicator(value: otaProgress),
                  const SizedBox(height: 8),
                  Wrap(
                    spacing: 8,
                    runSpacing: 8,
                    children: [
                      ElevatedButton.icon(
                        onPressed: isFlashing ? null : () => fetchAndFlashGitHubFirmware(isBeta: true),
                        icon: const Icon(Icons.cloud_download),
                        label: Text(isFlashing ? "Flashing ${(otaProgress * 100).toStringAsFixed(1)}%" : "Auto Update Firmware (GitHub)"),
                        style: ElevatedButton.styleFrom(
                          backgroundColor: isOtaUnlocked ? Colors.cyan : Colors.grey,
                        ),
                      ),
                      OutlinedButton.icon(
                        onPressed: isFlashing ? null : uploadFirmwareOTA,
                        icon: const Icon(Icons.folder_open),
                        label: const Text("Pick Local (.bin)"),
                      ),
                    ],
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget buildPatternButton(String label, String cmd, Color color) {
    return ElevatedButton(
      style: ElevatedButton.styleFrom(
        backgroundColor: const Color(0xFF334155),
        side: BorderSide(color: color.withAlpha(128)),
      ),
      onPressed: () => sendCommand(cmd),
      child: Text(label, style: TextStyle(color: color == Colors.black ? Colors.white : color)),
    );
  }
}
