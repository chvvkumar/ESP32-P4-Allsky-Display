#include "web_config.h"
#include "web_config_html.h"
#include "system_monitor.h"
#include "network_manager.h"
#include "mqtt_manager.h"
#include "display_manager.h"

// Page generation functions for the web configuration interface
// These functions generate the HTML content for each configuration page

String WebConfig::generateMainPage() {
    String html = "<div class='main'><div class='container'>";
    
    // System status cards
    html += "<div class='stats'>";
    html += "<div class='stat-card'><i class='fas fa-clock stat-icon'></i>";
    html += "<div class='stat-value'>" + formatUptime(millis()) + "</div>";
    html += "<div class='stat-label'>Uptime</div></div>";
    
    html += "<div class='stat-card'><i class='fas fa-microchip stat-icon'></i>";
    html += "<div class='stat-value'>" + formatBytes(systemMonitor.getCurrentFreeHeap()) + "</div>";
    html += "<div class='stat-label'>Free Heap</div></div>";
    
    html += "<div class='stat-card'><i class='fas fa-memory stat-icon'></i>";
    html += "<div class='stat-value'>" + formatBytes(systemMonitor.getCurrentFreePsram()) + "</div>";
    html += "<div class='stat-label'>Free PSRAM</div></div>";
    
    html += "<div class='stat-card'><i class='fas fa-sun stat-icon'></i>";
    html += "<div class='stat-value'>" + String(displayManager.getBrightness()) + "%</div>";
    html += "<div class='stat-label'>Brightness</div></div>";
    html += "</div>";
    
    // Brightness control card
    html += "<div class='card'><h2>💡 Screen Brightness</h2>";
    html += "<div class='form-group'><label>Control Mode</label>";
    html += "<div style='margin-top:0.5rem;display:flex;align-items:center'>";
    html += "<input type='checkbox' id='brightness_auto_mode' name='brightness_auto_mode' style='width:20px;height:20px;accent-color:#0ea5e9;margin-right:10px'";
    if (configStorage.getBrightnessAutoMode()) html += " checked";
    html += " onchange='updateBrightnessMode(this.checked)'> ";
    html += "<label for='brightness_auto_mode' style='margin-bottom:0;cursor:pointer'>Auto (MQTT controlled)</label>";
    html += "</div></div>";
    
    html += "<div class='form-group' id='brightness_slider_container' style='";
    if (configStorage.getBrightnessAutoMode()) html += "opacity:0.5;";
    html += "'><label for='main_brightness'>Brightness (%)</label>";
    html += "<input type='range' id='main_brightness' name='default_brightness' class='form-control' style='height:6px;padding:0' value='" + 
            String(displayManager.getBrightness()) + "' min='0' max='100' oninput='updateMainBrightnessValue(this.value)'";
    if (configStorage.getBrightnessAutoMode()) html += " disabled";
    html += "><div style='text-align:center;margin-top:0.5rem;color:#38bdf8;font-weight:bold'><span id='mainBrightnessValue'>" + 
            String(displayManager.getBrightness()) + "</span>%</div></div>";
    
    html += "<button type='button' class='btn btn-primary' onclick='saveMainBrightness(this)'";
    if (configStorage.getBrightnessAutoMode()) html += " disabled";
    html += " id='save_brightness_btn'>Apply Brightness</button></div>";
    
    // Quick status cards
    html += "<div class='grid'>";
    
    // WiFi Status
    html += "<div class='card'><h2>📡 Network Status</h2>";
    if (wifiManager.isConnected()) {
        html += "<div style='flex:1'><p><span class='status-indicator status-online'></span>Connected to <strong style='color:#38bdf8'>" + String(WiFi.SSID()) + "</strong></p>";
        html += "<p style='margin-top:0.5rem;font-size:0.9rem;color:#94a3b8'>IP Address: " + WiFi.localIP().toString() + "</p>";
        html += "<p style='font-size:0.9rem;color:#94a3b8'>Signal: " + String(WiFi.RSSI()) + " dBm</p></div>";
    } else {
        html += "<div style='flex:1'><p><span class='status-indicator status-offline'></span>Not connected</p></div>";
    }
    html += "</div>";
    
    // MQTT Status
    html += "<div class='card'><h2>🔗 MQTT Status</h2>";
    if (mqttManager.isConnected()) {
        html += "<div style='flex:1'><p><span class='status-indicator status-online'></span>Connected to broker</p>";
        html += "<p style='margin-top:0.5rem;font-size:0.9rem;color:#94a3b8'>Server: " + configStorage.getMQTTServer() + ":" + String(configStorage.getMQTTPort()) + "</p></div>";
    } else {
        html += "<div style='flex:1'><p><span class='status-indicator status-offline'></span>Not connected</p></div>";
    }
    html += "</div>";
    
    // Image Status
    html += "<div class='card'><h2>🖼️ Image Status</h2><div style='flex:1'>";
    if (configStorage.getCyclingEnabled()) {
        int sourceCount = configStorage.getImageSourceCount();
        int currentIndex = configStorage.getCurrentImageIndex();
        html += "<p><strong>Mode:</strong> Cycling (" + String(sourceCount) + " sources)</p>";
        html += "<p style='word-break:break-all;margin-top:0.5rem;font-size:0.9rem;color:#94a3b8'><strong>Current Source:</strong> [" + String(currentIndex + 1) + "/" + String(sourceCount) + "] " + escapeHtml(configStorage.getCurrentImageURL()) + "</p>";
        html += "<p style='font-size:0.9rem;color:#94a3b8'><strong>Cycle Interval:</strong> " + String(configStorage.getCycleInterval() / 1000) + " seconds</p>";
    } else {
        html += "<p><strong>Mode:</strong> Single Image</p>";
        html += "<p style='word-break:break-all;margin-top:0.5rem;font-size:0.9rem;color:#94a3b8'><strong>Source:</strong> " + escapeHtml(configStorage.getImageURL()) + "</p>";
    }
    html += "</div><p style='font-size:0.8rem;color:#64748b;margin-top:1rem'><strong>Update Interval:</strong> " + String(configStorage.getUpdateInterval() / 1000 / 60) + " minutes</p></div>";
    html += "</div></div></div>";
    
    return html;
}

String WebConfig::generateNetworkPage() {
    String html = "<div class='main'><div class='container'>";
    html += "<form id='networkForm'><div class='card'>";
    html += "<h2>📡 WiFi Configuration</h2>";
    html += "<div class='form-group'><label for='wifi_ssid'>Network Name (SSID)</label>";
    html += "<input type='text' id='wifi_ssid' name='wifi_ssid' class='form-control' value='" + escapeHtml(configStorage.getWiFiSSID()) + "' required></div>";
    html += "<div class='form-group'><label for='wifi_password'>Password</label>";
    html += "<input type='password' id='wifi_password' name='wifi_password' class='form-control' value='" + escapeHtml(configStorage.getWiFiPassword()) + "'></div>";
    html += "<button type='submit' class='btn btn-primary'>💾 Save Network Settings</button>";
    html += "</div></form></div></div>";
    return html;
}

String WebConfig::generateMQTTPage() {
    String html = "<div class='main'><div class='container'><form id='mqttForm'><div class='grid'>";
    html += "<div class='card'><h2>🔗 MQTT Broker</h2>";
    html += "<div class='form-group'><label for='mqtt_server'>Broker Address</label>";
    html += "<input type='text' id='mqtt_server' name='mqtt_server' class='form-control' value='" + escapeHtml(configStorage.getMQTTServer()) + "' required></div>";
    html += "<div class='form-group'><label for='mqtt_port'>Port</label>";
    html += "<input type='number' id='mqtt_port' name='mqtt_port' class='form-control' value='" + String(configStorage.getMQTTPort()) + "' min='1' max='65535' required></div>";
    html += "<div class='form-group'><label for='mqtt_client_id'>Client ID</label>";
    html += "<input type='text' id='mqtt_client_id' name='mqtt_client_id' class='form-control' value='" + escapeHtml(configStorage.getMQTTClientID()) + "' required></div>";
    html += "<div class='form-group'><label for='mqtt_user'>Username (optional)</label>";
    html += "<input type='text' id='mqtt_user' name='mqtt_user' class='form-control' value='" + escapeHtml(configStorage.getMQTTUser()) + "'></div>";
    html += "<div class='form-group'><label for='mqtt_password'>Password (optional)</label>";
    html += "<input type='password' id='mqtt_password' name='mqtt_password' class='form-control' value='" + escapeHtml(configStorage.getMQTTPassword()) + "'></div></div>";
    
    html += "<div class='card'><h2>🏠 Home Assistant Discovery</h2>";
    html += "<div class='form-group'><div style='display:flex;align-items:center;margin-bottom:1rem'>";
    html += "<input type='checkbox' id='ha_discovery_enabled' name='ha_discovery_enabled' style='width:20px;height:20px;accent-color:#0ea5e9;margin-right:10px'";
    if (configStorage.getHADiscoveryEnabled()) html += " checked";
    html += "><label for='ha_discovery_enabled' style='margin-bottom:0;cursor:pointer;font-size:1rem'>Enable Home Assistant MQTT Discovery</label></div>";
    html += "<p style='color:#94a3b8;font-size:0.9rem;margin-top:-0.5rem;margin-bottom:1rem'>Automatically creates entities in Home Assistant for all device controls and sensors</p></div>";
    
    html += "<div class='form-group'><label for='ha_device_name'>Device Name</label>";
    html += "<input type='text' id='ha_device_name' name='ha_device_name' class='form-control' value='" + escapeHtml(configStorage.getHADeviceName()) + "' placeholder='ESP32 AllSky Display'></div>";
    
    html += "<div class='form-group'><label for='ha_discovery_prefix'>Discovery Prefix</label>";
    html += "<input type='text' id='ha_discovery_prefix' name='ha_discovery_prefix' class='form-control' value='" + escapeHtml(configStorage.getHADiscoveryPrefix()) + "' placeholder='homeassistant'>";
    html += "<p style='color:#94a3b8;font-size:0.85rem;margin-top:0.5rem'>Default is 'homeassistant'. Change only if you've customized your HA MQTT discovery prefix.</p></div>";
    
    html += "<div class='form-group'><label for='ha_state_topic'>State Topic Prefix</label>";
    html += "<input type='text' id='ha_state_topic' name='ha_state_topic' class='form-control' value='" + escapeHtml(configStorage.getHAStateTopic()) + "' placeholder='allsky_display'>";
    html += "<p style='color:#94a3b8;font-size:0.85rem;margin-top:0.5rem'>Base MQTT topic for all device state and command messages.</p></div>";
    
    html += "<div class='form-group'><label for='ha_sensor_update_interval'>Sensor Update Interval (seconds)</label>";
    html += "<input type='number' id='ha_sensor_update_interval' name='ha_sensor_update_interval' class='form-control' value='" + String(configStorage.getHASensorUpdateInterval()) + "' min='10' max='300'>";
    html += "<p style='color:#94a3b8;font-size:0.85rem;margin-top:0.5rem'>How often to publish sensor data (heap, PSRAM, WiFi signal, uptime) to Home Assistant.</p></div>";
    
    html += "<div style='background:rgba(14,165,233,0.1);border:1px solid #0ea5e9;border-radius:8px;padding:1rem;margin-top:1rem'>";
    html += "<p style='color:#38bdf8;margin:0;font-size:0.9rem'><i class='fas fa-info-circle' style='margin-right:8px'></i><strong>Note:</strong> After saving, reconnect MQTT to trigger discovery. All device controls will appear in Home Assistant automatically.</p>";
    html += "</div></div>";
    
    html += "</div><div class='card' style='margin-top:1.5rem'>";
    html += "<button type='submit' class='btn btn-primary'>💾 Save MQTT Settings</button></div></form></div></div>";
    return html;
}

String WebConfig::generateImageSourcesPage() {
    String html = "<div class='main'><div class='container'>";
    html += "<form id='cyclingForm'><div class='card'><h2>🔄 Image Cycling Configuration</h2>";
    html += "<p style='color:#94a3b8;margin-bottom:1rem;font-size:0.9rem;background:rgba(56,189,248,0.1);padding:0.75rem;border-radius:6px;border-left:4px solid #38bdf8'>ℹ️ Maximum supported image size: <strong>724x724 pixels</strong> (1MB buffer limit). Images larger than this will fail to display.</p>";
    html += "<div class='form-group'><div style='display:flex;align-items:center;margin-bottom:1rem'>";
    html += "<input type='checkbox' id='cycling_enabled' name='cycling_enabled' style='width:20px;height:20px;accent-color:#0ea5e9;margin-right:10px' " + String(configStorage.getCyclingEnabled() ? "checked" : "") + ">";
    html += "<label for='cycling_enabled' style='margin-bottom:0;cursor:pointer;font-size:1rem'>Enable automatic cycling through multiple image sources</label></div></div>";
    html += "<div class='grid' style='margin-bottom:1rem'>";
    html += "<div class='form-group'><label for='cycle_interval'>Cycle Interval (seconds)</label>";
    html += "<input type='number' id='cycle_interval' name='cycle_interval' class='form-control' value='" + String(configStorage.getCycleInterval() / 1000) + "' min='10' max='3600'></div>";
    html += "<div class='form-group'><div style='display:flex;align-items:center;height:100%'>";
    html += "<input type='checkbox' id='random_order' name='random_order' style='width:20px;height:20px;accent-color:#0ea5e9;margin-right:10px' " + String(configStorage.getRandomOrder() ? "checked" : "") + ">";
    html += "<label for='random_order' style='margin-bottom:0;cursor:pointer'>Randomize image order</label></div></div></div>";
    html += "<button type='submit' class='btn btn-primary'>💾 Save Cycling Settings</button></div></form>";
    
    html += "<div class='card'><h2>📋 Image Sources & Transformations</h2><div id='imageSourcesList'>";
    int sourceCount = configStorage.getImageSourceCount();
    for (int i = 0; i < sourceCount; i++) {
        String url = configStorage.getImageSource(i);
        html += "<div class='image-source-item' style='margin-bottom:1.5rem;padding:1rem;border:1px solid #334155;border-radius:8px;background:#0f172a;'>";
        html += "<div style='display:flex;align-items:center;gap:1rem;margin-bottom:0.5rem;flex-wrap:wrap'>";
        html += "<span style='font-weight:bold;color:#38bdf8;font-size:1.1rem'>" + String(i + 1) + ".</span>";
        html += "<input type='url' class='form-control' id='imageUrl_" + String(i) + "' style='flex:1;min-width:200px' value='" + escapeHtml(url) + "' onchange='updateImageSource(" + String(i) + ", this)'>";
        html += "<button type='button' class='btn btn-secondary' onclick='toggleTransformSection(" + String(i) + ")'><i class='fas fa-cog'></i> Options</button>";
        if (sourceCount > 1) {
            html += "<button type='button' class='btn btn-danger' onclick='removeImageSource(" + String(i) + ", this)'><i class='fas fa-trash'></i></button>";
        }
        html += "</div>";
        html += "<div id='imageError_" + String(i) + "' style='display:none;margin-left:2.5rem;padding:0.5rem;background:rgba(239,68,68,0.1);border-left:3px solid #ef4444;border-radius:4px;font-size:0.85rem;color:#fca5a5;margin-bottom:0.5rem'></div>";
        html += "<div id='transformSection_" + String(i) + "' class='transform-section' style='display:none;margin-top:1rem;padding:1.5rem;background:#1e293b;border-radius:8px;border:1px solid #475569;'>";
        html += "<h3 style='margin-top:0;font-size:1rem;color:#cbd5e1;margin-bottom:1rem;border-bottom:1px solid #334155;padding-bottom:0.5rem'>Transform Settings</h3>";
        html += "<div style='display:grid;grid-template-columns:repeat(auto-fit, minmax(150px, 1fr));gap:1rem;'>";
        html += "<div class='form-group'><label>Scale X</label><input type='number' class='form-control' value='" + String(configStorage.getImageScaleX(i)) + "' step='0.1' min='0.1' max='5.0' onchange='updateImageTransform(" + String(i) + ", \"scaleX\", this)'></div>";
        html += "<div class='form-group'><label>Scale Y</label><input type='number' class='form-control' value='" + String(configStorage.getImageScaleY(i)) + "' step='0.1' min='0.1' max='5.0' onchange='updateImageTransform(" + String(i) + ", \"scaleY\", this)'></div>";
        html += "<div class='form-group'><label>Offset X</label><input type='number' class='form-control' value='" + String(configStorage.getImageOffsetX(i)) + "' onchange='updateImageTransform(" + String(i) + ", \"offsetX\", this)'></div>";
        html += "<div class='form-group'><label>Offset Y</label><input type='number' class='form-control' value='" + String(configStorage.getImageOffsetY(i)) + "' onchange='updateImageTransform(" + String(i) + ", \"offsetY\", this)'></div>";
        html += "<div class='form-group'><label>Rotation</label><select class='form-control' onchange='updateImageTransform(" + String(i) + ", \"rotation\", this)'>";
        int rotation = (int)configStorage.getImageRotation(i);
        html += String("<option value='0'") + (rotation == 0 ? " selected" : "") + ">0°</option>";
        html += String("<option value='90'") + (rotation == 90 ? " selected" : "") + ">90°</option>";
        html += String("<option value='180'") + (rotation == 180 ? " selected" : "") + ">180°</option>";
        html += String("<option value='270'") + (rotation == 270 ? " selected" : "") + ">270°</option>";
        html += "</select></div></div>";
        html += "<div style='margin-top:1rem;display:flex;gap:0.5rem;'>";
        html += "<button type='button' class='btn btn-secondary' onclick='copyDefaultsToImage(" + String(i) + ", this)'>Copy Defaults</button>";
        html += "<button type='button' class='btn btn-secondary' onclick='applyTransformImmediately(" + String(i) + ", this)'>Apply Now</button>";
        html += "</div></div></div>";
    }
    html += "</div><div style='margin-top:1rem;'>";
    html += "<button type='button' class='btn btn-success' onclick='addImageSource(this)'>➕ Add Image Source</button>";
    if (sourceCount > 1) {
        html += "<button type='button' class='btn btn-secondary' onclick='clearAllSources(this)' style='margin-left:1rem;'>🗑️ Clear All</button>";
    }
    html += "</div></div></div></div>";
    return html;
}

String WebConfig::generateImagePage() {
    String html = "<div class='main'><div class='container'><form id='imageForm'><div class='grid'>";
    html += "<div class='card'><h2>🖼️ Image Source</h2>";
    html += "<p style='color:#f59e0b;margin-bottom:1rem;background:rgba(245,158,11,0.1);padding:0.75rem;border-radius:6px;border-left:4px solid #f59e0b'>⚠️ For multiple image sources, use the <a href='/config/sources' style='color:#38bdf8;font-weight:bold'>Multi-Image</a> page.</p>";
    html += "<div class='form-group'><label for='image_url'>Image URL</label>";
    html += "<input type='url' id='image_url' name='image_url' class='form-control' value='" + escapeHtml(configStorage.getImageURL()) + "' required></div>";
    html += "<div class='form-group'><label for='update_interval'>Image Update Interval (minutes)</label>";
    html += "<input type='number' id='update_interval' name='update_interval' class='form-control' value='" + String(configStorage.getUpdateInterval() / 1000 / 60) + "' min='1' max='1440'></div></div>";
    html += "<div class='card'><h2>🔄 Default Transformations</h2>";
    html += "<div class='form-group'><label for='default_scale_x'>Scale X</label>";
    html += "<input type='number' id='default_scale_x' name='default_scale_x' class='form-control' value='" + String(configStorage.getDefaultScaleX()) + "' step='0.1' min='0.1' max='5.0'></div>";
    html += "<div class='form-group'><label for='default_scale_y'>Scale Y</label>";
    html += "<input type='number' id='default_scale_y' name='default_scale_y' class='form-control' value='" + String(configStorage.getDefaultScaleY()) + "' step='0.1' min='0.1' max='5.0'></div>";
    html += "<div class='form-group'><label for='default_offset_x'>Offset X (pixels)</label>";
    html += "<input type='number' id='default_offset_x' name='default_offset_x' class='form-control' value='" + String(configStorage.getDefaultOffsetX()) + "'></div>";
    html += "<div class='form-group'><label for='default_offset_y'>Offset Y (pixels)</label>";
    html += "<input type='number' id='default_offset_y' name='default_offset_y' class='form-control' value='" + String(configStorage.getDefaultOffsetY()) + "'></div>";
    html += "<div class='form-group'><label for='default_rotation'>Rotation (degrees)</label>";
    html += "<input type='number' id='default_rotation' name='default_rotation' class='form-control' value='" + String(configStorage.getDefaultRotation()) + "' step='90' min='0' max='270'></div></div>";
    html += "</div><div class='card' style='margin-top:1.5rem'>";
    html += "<button type='submit' class='btn btn-primary'>💾 Save Image Settings</button></div></form></div></div>";
    return html;
}

String WebConfig::generateDisplayPage() {
    String html = "<div class='main'><div class='container'><form id='displayForm'><div class='grid'>";
    html += "<div class='card'><h2>💡 Brightness Control</h2>";
    html += "<div class='form-group'><label for='default_brightness'>Default Brightness (%)</label>";
    html += "<input type='range' id='default_brightness' name='default_brightness' class='form-control' value='" + String(configStorage.getDefaultBrightness()) + "' min='0' max='100' oninput='updateBrightnessValue(this.value)'>";
    html += "<div style='text-align:center;margin-top:0.5rem;color:#38bdf8;font-weight:bold'><span id='brightnessValue'>" + String(configStorage.getDefaultBrightness()) + "</span>%</div></div></div>";
    html += "<div class='card'><h2>⚙️ Backlight Settings</h2>";
    html += "<div class='form-group'><label for='backlight_freq'>PWM Frequency (Hz)</label>";
    html += "<input type='number' id='backlight_freq' name='backlight_freq' class='form-control' value='" + String(configStorage.getBacklightFreq()) + "' min='1000' max='20000'></div>";
    html += "<div class='form-group'><label for='backlight_resolution'>PWM Resolution (bits)</label>";
    html += "<input type='number' id='backlight_resolution' name='backlight_resolution' class='form-control' value='" + String(configStorage.getBacklightResolution()) + "' min='8' max='16'></div></div>";
    html += "</div><div class='card' style='margin-top:1.5rem'>";
    html += "<button type='submit' class='btn btn-primary'>💾 Save Display Settings</button></div></form></div></div>";
    return html;
}

String WebConfig::generateAdvancedPage() {
    String html = "<div class='main'><div class='container'><form id='advancedForm'><div class='grid'>";
    html += "<div class='card'><h2>⏱️ Timing Settings</h2>";
    html += "<div class='form-group'><label for='mqtt_reconnect_interval'>MQTT Reconnect Interval (seconds)</label>";
    html += "<input type='number' id='mqtt_reconnect_interval' name='mqtt_reconnect_interval' class='form-control' value='" + String(configStorage.getMQTTReconnectInterval() / 1000) + "' min='1' max='300'></div>";
    html += "<div class='form-group'><label for='watchdog_timeout'>Watchdog Timeout (seconds)</label>";
    html += "<input type='number' id='watchdog_timeout' name='watchdog_timeout' class='form-control' value='" + String(configStorage.getWatchdogTimeout() / 1000) + "' min='10' max='120'></div></div>";
    html += "<div class='card'><h2>💾 Memory Thresholds</h2>";
    html += "<div class='form-group'><label for='critical_heap_threshold'>Critical Heap Threshold (bytes)</label>";
    html += "<input type='number' id='critical_heap_threshold' name='critical_heap_threshold' class='form-control' value='" + String(configStorage.getCriticalHeapThreshold()) + "' min='10000' max='1000000'></div>";
    html += "<div class='form-group'><label for='critical_psram_threshold'>Critical PSRAM Threshold (bytes)</label>";
    html += "<input type='number' id='critical_psram_threshold' name='critical_psram_threshold' class='form-control' value='" + String(configStorage.getCriticalPSRAMThreshold()) + "' min='10000' max='10000000'></div></div>";
    html += "</div><div class='card' style='margin-top:1.5rem'>";
    html += "<button type='submit' class='btn btn-primary'>💾 Save Advanced Settings</button></div></form></div></div>";
    return html;
}

String WebConfig::generateStatusPage() {
    String html = "<div class='main'><div class='container'>";
    html += "<div class='card'><h2>📊 System Status</h2><div id='statusData'>Loading...</div></div></div></div>";
    return html;
}
