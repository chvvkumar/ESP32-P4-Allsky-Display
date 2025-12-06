# Manual WiFi Configuration

This guide explains how to manually configure WiFi credentials by editing the source code before compilation. This method is only recommended for advanced users or special deployment scenarios.

**⚠️ NOTE**: For normal use, it's recommended to use the automatic WiFi setup via Access Point mode (see main [README](README.md)).

## Prerequisites

- Arduino IDE or PlatformIO installed
- ESP32-P4 board support configured
- Source code downloaded

## Configuration Steps

### 1. Edit Default WiFi Credentials

Open [`config_storage.cpp`](config_storage.cpp) and locate the `setDefaults()` function:

```cpp
void ConfigStorage::setDefaults() {
    // Set hardcoded defaults from original config.cpp
    // WiFi credentials are intentionally empty - device will start captive portal on first boot
    config.wifiProvisioned = false;
    config.wifiSSID = "";        // ← Edit this line
    config.wifiPassword = "";    // ← Edit this line
    
    config.mqttServer = "192.168.1.250";
    config.mqttPort = 1883;
    config.mqttUser = "";
    config.mqttPassword = "";
    config.mqttClientID = "ESP32_Allsky_Display";
    // ... rest of configuration
}
```

**Replace the empty strings with your WiFi credentials:**

```cpp
config.wifiSSID = "YOUR_WIFI_SSID";        // Your network name
config.wifiPassword = "YOUR_WIFI_PASSWORD"; // Your network password
```

### 2. Configure MQTT Settings (Optional)

While in the same function, you can also pre-configure MQTT settings:

```cpp
config.mqttServer = "192.168.1.250";       // MQTT broker IP or hostname
config.mqttPort = 1883;                    // MQTT port (usually 1883)
config.mqttUser = "";                      // MQTT username (if required)
config.mqttPassword = "";                  // MQTT password (if required)
config.mqttClientID = "ESP32_Allsky_Display"; // Unique client ID
```

### 3. Configure Default Image Sources (Optional)

Open [`config.cpp`](config.cpp) and locate the `DEFAULT_IMAGE_SOURCES` array:

```cpp
const char* DEFAULT_IMAGE_SOURCES[] = {
    "https://i.imgur.com/EsstNmc.jpeg",                   // Default source 1
    "https://i.imgur.com/EtW1eaT.jpeg",                   // Default source 2
    "https://i.imgur.com/k23xBF5.jpeg",                   // Default source 3
    "https://i.imgur.com/BysRDbf.jpeg",                   // Default source 4
    "http://allskypi5.lan/current/resized/image.jpg"      // Default source 5
    // Add more image URLs here as needed (up to MAX_IMAGE_SOURCES = 10)
};
```

**Replace these with your own image URLs:**

```cpp
const char* DEFAULT_IMAGE_SOURCES[] = {
    "http://your-allsky-server.com/resized/image.jpg",
    "http://your-camera2.com/image.jpg",
    // Add up to 10 total sources
};
```

### 4. Compile and Upload

**Initial Upload (USB):**
1. **Arduino IDE**: Click the Upload button (→) in the toolbar
2. **PlatformIO**: Run `pio run --target upload`
3. **PowerShell Script**: `.\compile-and-upload.ps1 -ComPort COM3`

**Subsequent Updates (OTA):**
After initial USB upload, you can update wirelessly:
1. **ElegantOTA**: Navigate to `http://[device-ip]:8080/update` and upload `.bin` file
2. **ArduinoOTA**: Select network port in Arduino IDE and click Upload

See [OTA_GUIDE.md](OTA_GUIDE.md) for detailed OTA update instructions.

### 5. Monitor Serial Output

Open the Serial Monitor (115200 baud) to see:
- WiFi connection status
- Assigned IP address
- Configuration loading status
- System information

### 6. Access Web Interface

Once connected, the device will display its IP address in the serial output. Access the web interface at:

```
http://[device-ip]:8080/
```

From here you can modify all settings without recompiling.

## Important Notes

### First Boot Behavior

- If `config.wifiSSID` is empty, the device will start in Access Point mode
- If `config.wifiSSID` is set, the device will attempt to connect to that network
- After successful configuration via web interface, these hardcoded values are overridden by stored settings

### Resetting to Manual Configuration

To reset the device back to using your hardcoded credentials:

1. Use the Factory Reset button in the web interface, OR
2. Use the reset function in the web API, OR
3. Erase flash memory using `esptool.py erase_flash`

After reset, the device will use the credentials you set in `config_storage.cpp`.

### Security Considerations

**⚠️ WARNING**: Storing WiFi credentials in source code is not recommended for:
- Shared code repositories (credentials will be visible in version control)
- Multi-device deployments with different networks
- Production environments

For these scenarios, use the Access Point mode (default behavior) to configure each device individually.

## Troubleshooting

### Device Won't Connect to WiFi

1. Verify SSID and password are correct (case-sensitive)
2. Check that WiFi network is 2.4GHz (ESP32-P4 may not support 5GHz on all models)
3. Ensure network is in range and operational
4. Monitor serial output for connection errors

### Can't Find IP Address

1. Check your router's DHCP client list
2. Look for device named "ESP32_Allsky_Display" or similar
3. Serial monitor will display the IP address on successful connection
4. Try accessing via mDNS: `http://allskyesp32.lan:8080/` (if supported by your network)

### Need to Reconfigure WiFi

If you need to change WiFi settings after deployment:

**Option 1**: Use the web interface (Network settings page)

**Option 2**: Factory reset and trigger Access Point mode:
1. Access web interface
2. Go to System settings
3. Click "Factory Reset"
4. Device will restart in AP mode for reconfiguration

## Alternative: Access Point Mode (Recommended)

Instead of manual configuration, consider using the automatic Access Point mode:

1. Leave `config.wifiSSID` empty in `config_storage.cpp`
2. Compile and upload firmware
3. Device creates WiFi network: **AllSky-Display-Setup**
4. Connect your phone/computer to this network
5. Captive portal opens automatically
6. Select your WiFi network and enter password
7. Device connects and saves credentials

See main [README](README.md) for detailed instructions on Access Point mode.
