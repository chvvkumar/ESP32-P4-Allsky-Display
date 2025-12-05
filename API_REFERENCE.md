# API Reference

The device exposes a REST API for programmatic control at `http://[device-ip]:8080/api/`. All API endpoints accept POST requests with form-encoded or JSON data.

## Configuration Management

### Save Configuration
```bash
POST /api/save
```
**Parameters:**
- `wifi_ssid` - WiFi network name
- `wifi_password` - WiFi password
- `mqtt_server` - MQTT broker IP/hostname
- `mqtt_port` - MQTT broker port (default: 1883)
- `mqtt_user` - MQTT username (optional)
- `mqtt_password` - MQTT password (optional)
- `mqtt_client_id` - MQTT client identifier
- `ha_device_name` - Home Assistant device name
- `ha_discovery_prefix` - HA discovery prefix (default: homeassistant)
- `ha_state_topic` - HA state topic
- `ha_sensor_update_interval` - Sensor update interval in seconds
- `image_url` - Legacy single image URL
- `default_brightness` - Display brightness (0-255)
- `default_scale_x` - X-axis scale factor
- `default_scale_y` - Y-axis scale factor
- `default_offset_x` - X-axis offset in pixels
- `default_offset_y` - Y-axis offset in pixels
- `default_rotation` - Rotation angle in degrees
- `cycle_interval` - Time between images in seconds
- `update_interval` - Image refresh interval in minutes

**Example:**
```bash
curl -X POST http://192.168.1.100:8080/api/save \
  -d "default_brightness=200" \
  -d "cycle_interval=30"
```

## Image Source Management

### Add Image Source
```bash
POST /api/add-source
```
**Parameters:**
- `url` - Image URL to add

**Example:**
```bash
curl -X POST http://192.168.1.100:8080/api/add-source \
  -d "url=https://example.com/image.jpg"
```

### Remove Image Source
```bash
POST /api/remove-source
```
**Parameters:**
- `index` - Index of image to remove (0-based)

**Example:**
```bash
curl -X POST http://192.168.1.100:8080/api/remove-source \
  -d "index=2"
```

### Update Image Source
```bash
POST /api/update-source
```
**Parameters:**
- `index` - Index of image to update (0-based)
- `url` - New image URL

**Example:**
```bash
curl -X POST http://192.168.1.100:8080/api/update-source \
  -d "index=0" \
  -d "url=https://example.com/new-image.jpg"
```

### Clear All Image Sources
```bash
POST /api/clear-sources
```
**Example:**
```bash
curl -X POST http://192.168.1.100:8080/api/clear-sources
```

## Image Navigation

### Next Image
```bash
POST /api/next-image
```
Manually advance to the next image in the cycle.

**Example:**
```bash
curl -X POST http://192.168.1.100:8080/api/next-image
```

### Previous Image
```bash
POST /api/previous-image
```
Go back to the previous image.

**Example:**
```bash
curl -X POST http://192.168.1.100:8080/api/previous-image
```

## Image Transformations

### Update Image Transform
```bash
POST /api/update-transform
```
**Parameters:**
- `index` - Image index (0-based)
- `scale_x` - X-axis scale factor
- `scale_y` - Y-axis scale factor
- `offset_x` - X-axis offset in pixels
- `offset_y` - Y-axis offset in pixels
- `rotation` - Rotation angle in degrees

**Example:**
```bash
curl -X POST http://192.168.1.100:8080/api/update-transform \
  -d "index=0" \
  -d "scale_x=1.2" \
  -d "scale_y=1.2" \
  -d "offset_x=50" \
  -d "offset_y=-30" \
  -d "rotation=0"
```

### Copy Defaults to Image
```bash
POST /api/copy-defaults
```
Copy default transformation settings to a specific image.

**Parameters:**
- `index` - Target image index (0-based)

**Example:**
```bash
curl -X POST http://192.168.1.100:8080/api/copy-defaults \
  -d "index=1"
```

### Apply Transform
```bash
POST /api/apply-transform
```
Immediately apply transformation changes and refresh the display.

**Example:**
```bash
curl -X POST http://192.168.1.100:8080/api/apply-transform
```

## System Control

### Restart Device
```bash
POST /api/restart
```
Reboot the ESP32 device.

**Example:**
```bash
curl -X POST http://192.168.1.100:8080/api/restart
```

### Factory Reset
```bash
POST /api/factory-reset
```
Reset all settings to factory defaults and reboot.

**Example:**
```bash
curl -X POST http://192.168.1.100:8080/api/factory-reset
```

## Device Information

### Get Device Info
```bash
GET /api/device-info
```
Retrieve comprehensive device information including hardware specifications, memory usage, flash metrics, network status, and system health.

**Response includes:**
- **device** - Chip model, revision, CPU cores/frequency, SDK version
- **flash** - Size, speed, sketch size, usage percentage, free space, MD5 hash
- **memory** - Heap and PSRAM size, free/used amounts, minimum free values
- **network** - WiFi status, SSID, IP address, MAC, RSSI, gateway, DNS
- **mqtt** - Connection status, server, port, client ID, HA discovery status
- **display** - Brightness level, auto mode status, resolution
- **imageCycling** - Enabled status, current index, total sources, intervals
- **system** - Uptime, health status, temperature

**Example:**
```bash
curl http://192.168.1.100:8080/api/device-info
```

**Sample Response:**
```json
{
  "device": {
    "name": "ESP32-P4 AllSky Display",
    "chipModel": "ESP32-P4",
    "chipRevision": 0,
    "cpuCores": 2,
    "cpuFreqMHz": 400,
    "sdkVersion": "5.5",
    "arduinoVersion": "10812"
  },
  "flash": {
    "size": 16777216,
    "speed": 80000000,
    "sketchSize": 1316020,
    "sketchUsedPercent": 7,
    "freeSketchSpace": 15461196,
    "sketchMD5": "abc123..."
  },
  "memory": {
    "heapSize": 327680,
    "freeHeap": 273580,
    "heapUsedPercent": 16,
    "psramSize": 8388608,
    "freePsram": 8200000
  },
  "network": {
    "connected": true,
    "ssid": "YourNetwork",
    "ipAddress": "192.168.1.100",
    "rssi": -45
  }
}
```

## API Response Format

All API endpoints return JSON responses:

**Success Response:**
```json
{
  "status": "success",
  "message": "Operation completed successfully"
}
```

**Error Response:**
```json
{
  "status": "error",
  "message": "Error description"
}
```

## Integration Examples

### Python Script
```python
import requests
import json

# Device IP
device_ip = "192.168.1.100"
base_url = f"http://{device_ip}:8080/api"

# Get device information
info = requests.get(f"{base_url}/device-info").json()
print(f"Device: {info['device']['chipModel']}")
print(f"Flash used: {info['flash']['sketchUsedPercent']}%")
print(f"Free heap: {info['memory']['freeHeap']:,} bytes")
print(f"Free PSRAM: {info['memory']['freePsram']:,} bytes")
print(f"Uptime: {info['system']['uptime'] / 1000:.0f} seconds")

# Set brightness
requests.post(f"{base_url}/save", data={"default_brightness": 200})

# Add new image source
requests.post(f"{base_url}/add-source", 
              data={"url": "https://example.com/weather.jpg"})

# Navigate to next image
requests.post(f"{base_url}/next-image")

# Update transformation for image 0
requests.post(f"{base_url}/update-transform", data={
    "index": 0,
    "scale_x": 1.5,
    "scale_y": 1.5,
    "offset_x": 0,
    "offset_y": 0,
    "rotation": 0
})
```

### Home Assistant Automation
```yaml
automation:
  - alias: "Update Display at Sunset"
    trigger:
      - platform: sun
        event: sunset
    action:
      - service: rest_command.display_next_image
        
rest_command:
  display_next_image:
    url: "http://192.168.1.100:8080/api/next-image"
    method: POST
    
  display_set_brightness:
    url: "http://192.168.1.100:8080/api/save"
    method: POST
    payload: "default_brightness={{ brightness }}"
```

### Node-RED Flow
```json
[
    {
        "type": "http request",
        "method": "POST",
        "url": "http://192.168.1.100:8080/api/next-image",
        "name": "Next Image"
    }
]
```
