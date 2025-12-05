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
    
    // Quick status cards
    html += "<div class='grid'>";
    
    // WiFi Status
    html += "<div class='card'><h2>📡 Network Status</h2>";
    if (wifiManager.isConnected()) {
        html += "<div style='flex:1'><p><span class='status-indicator status-online'></span>Connected to <strong style='color:#38bdf8'>" + String(WiFi.SSID()) + "</strong></p>";
        html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:0.5rem;margin-top:0.75rem;font-size:0.9rem;color:#94a3b8'>";
        html += "<div><strong style='color:#64748b'>IP Address:</strong><br>" + WiFi.localIP().toString() + "</div>";
        html += "<div><strong style='color:#64748b'>Signal:</strong><br>" + String(WiFi.RSSI()) + " dBm</div>";
        html += "<div><strong style='color:#64748b'>MAC Address:</strong><br>" + WiFi.macAddress() + "</div>";
        html += "<div><strong style='color:#64748b'>Gateway:</strong><br>" + WiFi.gatewayIP().toString() + "</div>";
        html += "<div><strong style='color:#64748b'>DNS:</strong><br>" + WiFi.dnsIP().toString() + "</div>";
        html += "</div></div>";
    } else {
        html += "<div style='flex:1'><p><span class='status-indicator status-offline'></span>Not connected</p></div>";
    }
    html += "</div>";
    
    // MQTT Status
    html += "<div class='card'><h2>🔗 MQTT Status</h2>";
    if (mqttManager.isConnected()) {
        html += "<div style='flex:1'><p><span class='status-indicator status-online'></span>Connected to broker</p>";
        html += "<div style='margin-top:0.75rem;font-size:0.9rem;color:#94a3b8'>";
        html += "<p style='margin:0.25rem 0'><strong style='color:#64748b'>Server:</strong> " + configStorage.getMQTTServer() + ":" + String(configStorage.getMQTTPort()) + "</p>";
        html += "<p style='margin:0.25rem 0'><strong style='color:#64748b'>Client ID:</strong> " + escapeHtml(configStorage.getMQTTClientID()) + "</p>";
        html += "<p style='margin:0.25rem 0'><strong style='color:#64748b'>HA Discovery:</strong> " + String(configStorage.getHADiscoveryEnabled() ? "Enabled" : "Disabled") + "</p>";
        html += "</div></div>";
    } else {
        html += "<div style='flex:1'><p><span class='status-indicator status-offline'></span>Not connected</p></div>";
    }
    html += "</div>";
    html += "</div>";
    
    // Image Status - Configured Sources List
    html += "<div class='card' style='margin-top:1.5rem'><h2>🖼️ Image Status</h2>";
    
    if (configStorage.getCyclingEnabled()) {
        int sourceCount = configStorage.getImageSourceCount();
        int currentIndex = configStorage.getCurrentImageIndex();
        
        // Summary info
        html += "<div style='display:flex;justify-content:space-between;align-items:center;padding:1rem;background:#1e293b;border-radius:8px;margin-bottom:1rem'>";
        html += "<div><p style='margin:0;font-size:0.9rem;color:#94a3b8'><strong style='color:#e2e8f0'>Mode:</strong> Cycling</p></div>";
        html += "<div><p style='margin:0;font-size:0.9rem;color:#94a3b8'><strong style='color:#e2e8f0'>Active:</strong> [" + String(currentIndex + 1) + "/" + String(sourceCount) + "]</p></div>";
        html += "<div><p style='margin:0;font-size:0.9rem;color:#94a3b8'><strong style='color:#e2e8f0'>Cycle:</strong> " + String(configStorage.getCycleInterval() / 1000) + "s</p></div>";
        html += "<div><p style='margin:0;font-size:0.9rem;color:#94a3b8'><strong style='color:#e2e8f0'>Update:</strong> " + String(configStorage.getUpdateInterval() / 1000 / 60) + "m</p></div>";
        html += "</div>";
        
        // Explanation
        html += "<div style='background:rgba(14,165,233,0.1);border:1px solid #0ea5e9;border-radius:8px;padding:1rem;margin-bottom:1.5rem'>";
        html += "<p style='color:#38bdf8;margin:0;font-size:0.85rem;line-height:1.6'><i class='fas fa-info-circle' style='margin-right:8px'></i>";
        html += "<strong>Cycling Mode:</strong> Display rotates through all configured sources every <strong>" + String(configStorage.getCycleInterval() / 1000) + " seconds</strong>. ";
        html += "Each source is re-downloaded every <strong>" + String(configStorage.getUpdateInterval() / 1000 / 60) + " minutes</strong> to fetch fresh content (e.g., updated sky photos). ";
        html += "Sources appear in order or randomly based on your settings.</p>";
        html += "</div>";
        
        // List all configured sources
        if (sourceCount > 0) {
            html += "<h3 style='color:#94a3b8;font-size:1rem;margin-bottom:1rem'>Configured Sources:</h3>";
            for (int i = 0; i < sourceCount; i++) {
                String sourceUrl = configStorage.getImageSource(i);
                String activeIndicator = (i == currentIndex) ? "<span style='color:#10b981;margin-right:8px;font-size:1.2rem'>►</span>" : "<span style='color:#64748b;margin-right:8px'>•</span>";
                html += "<div style='margin-bottom:0.75rem;padding:0.75rem;background:" + String(i == currentIndex ? "#1e3a2e" : "#1e293b") + ";border-radius:8px;border-left:4px solid " + String(i == currentIndex ? "#10b981" : "#475569") + ";overflow-wrap:break-word;word-break:break-all'>";
                html += "<div style='font-size:0.85rem;color:#94a3b8;margin-bottom:0.25rem'>" + activeIndicator + "<strong style='color:" + String(i == currentIndex ? "#10b981" : "#64748b") + "'>Source " + String(i + 1) + String(i == currentIndex ? " (Active)" : "") + "</strong></div>";
                html += "<div style='font-size:0.85rem;color:#cbd5e1;font-family:monospace;padding-left:1.5rem'>" + escapeHtml(sourceUrl) + "</div>";
                html += "</div>";
            }
        }
    } else {
        html += "<div style='display:flex;justify-content:space-between;align-items:center;padding:1rem;background:#1e293b;border-radius:8px;margin-bottom:1rem'>";
        html += "<div><p style='margin:0;font-size:0.9rem;color:#94a3b8'><strong style='color:#e2e8f0'>Mode:</strong> Single Image</p></div>";
        html += "<div><p style='margin:0;font-size:0.9rem;color:#94a3b8'><strong style='color:#e2e8f0'>Update:</strong> " + String(configStorage.getUpdateInterval() / 1000 / 60) + " minutes</p></div>";
        html += "</div>";
        
        // Explanation
        html += "<div style='background:rgba(14,165,233,0.1);border:1px solid #0ea5e9;border-radius:8px;padding:1rem;margin-bottom:1.5rem'>";
        html += "<p style='color:#38bdf8;margin:0;font-size:0.85rem;line-height:1.6'><i class='fas fa-info-circle' style='margin-right:8px'></i>";
        html += "<strong>Single Image Mode:</strong> Display shows only one image source. ";
        html += "The image is re-downloaded every <strong>" + String(configStorage.getUpdateInterval() / 1000 / 60) + " minutes</strong> to fetch fresh content.</p>";
        html += "</div>";
        
        html += "<h3 style='color:#94a3b8;font-size:1rem;margin-bottom:1rem'>Image Source:</h3>";
        html += "<div style='padding:0.75rem;background:#1e293b;border-radius:8px;border-left:4px solid #0ea5e9;overflow-wrap:break-word;word-break:break-all;font-size:0.9rem;color:#cbd5e1;font-family:monospace'>" + escapeHtml(configStorage.getImageURL()) + "</div>";
    }
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
    String html = "<div class='main'><div class='container'>";
    
    // Current brightness control - Live adjustment (not saved)
    html += "<div class='card'><h2>💡 Current Brightness Control</h2>";
    html += "<p style='color:#94a3b8;font-size:0.9rem;margin-bottom:1rem'>Adjust screen brightness in real-time. Changes take effect immediately but are not saved.</p>";
    
    html += "<div class='form-group'><label>Control Mode</label>";
    html += "<div style='margin-top:0.5rem;display:flex;align-items:center'>";
    html += "<input type='checkbox' id='brightness_auto_mode' name='brightness_auto_mode' style='width:20px;height:20px;accent-color:#0ea5e9;margin-right:10px'";
    if (configStorage.getBrightnessAutoMode()) html += " checked";
    html += " onchange='updateBrightnessMode(this.checked)'> ";
    html += "<label for='brightness_auto_mode' style='margin-bottom:0;cursor:pointer'>Auto (MQTT controlled)</label>";
    html += "</div></div>";
    
    html += "<div class='form-group' id='brightness_slider_container' style='";
    if (configStorage.getBrightnessAutoMode()) html += "opacity:0.5;";
    html += "'><label for='main_brightness'>Current Brightness (%)</label>";
    html += "<input type='range' id='main_brightness' name='default_brightness' class='form-control' style='height:6px;padding:0' value='" + 
            String(displayManager.getBrightness()) + "' min='0' max='100' oninput='updateMainBrightnessValue(this.value)'";
    if (configStorage.getBrightnessAutoMode()) html += " disabled";
    html += "><div style='text-align:center;margin-top:0.5rem;color:#38bdf8;font-weight:bold'><span id='mainBrightnessValue'>" + 
            String(displayManager.getBrightness()) + "</span>%</div></div>";
    
    html += "<button type='button' class='btn btn-primary' onclick='saveMainBrightness(this)'";
    if (configStorage.getBrightnessAutoMode()) html += " disabled";
    html += " id='save_brightness_btn'>Apply Brightness</button></div>";
    
    // Display settings form - Saved configuration
    html += "<form id='displayForm'>";
    
    // Brightness Settings Card
    html += "<div class='card'><h2>⚙️ Brightness Settings</h2>";
    html += "<p style='color:#94a3b8;font-size:0.9rem;margin-bottom:1rem'>Configure default brightness and backlight hardware settings. These are saved permanently.</p>";
    
    html += "<div class='form-group'><label for='default_brightness'>Default Brightness at Startup (%)</label>";
    html += "<input type='range' id='default_brightness' name='default_brightness' class='form-control' value='" + String(configStorage.getDefaultBrightness()) + "' min='0' max='100' oninput='updateBrightnessValue(this.value)'>";
    html += "<div style='text-align:center;margin-top:0.5rem;color:#38bdf8;font-weight:bold'><span id='brightnessValue'>" + String(configStorage.getDefaultBrightness()) + "</span>%</div>";
    html += "<p style='color:#64748b;font-size:0.85rem;margin-top:0.5rem'>This brightness will be applied when the device boots up.</p></div>";
    
    html += "<div class='form-group'><label for='backlight_freq'>PWM Frequency (Hz)</label>";
    html += "<input type='number' id='backlight_freq' name='backlight_freq' class='form-control' value='" + String(configStorage.getBacklightFreq()) + "' min='1000' max='20000'>";
    html += "<p style='color:#64748b;font-size:0.85rem;margin-top:0.5rem'>Higher frequency reduces flicker. Typical: 5000 Hz</p></div>";
    
    html += "<div class='form-group'><label for='backlight_resolution'>PWM Resolution (bits)</label>";
    html += "<input type='number' id='backlight_resolution' name='backlight_resolution' class='form-control' value='" + String(configStorage.getBacklightResolution()) + "' min='8' max='16'>";
    html += "<p style='color:#64748b;font-size:0.85rem;margin-top:0.5rem'>Higher resolution provides smoother brightness control. Typical: 10-12 bits</p></div>";
    
    html += "<button type='submit' class='btn btn-primary'>💾 Save Brightness Settings</button></div>";
    
    html += "</form></div></div>";
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

String WebConfig::generateSerialCommandsPage() {
    String html = "<div class='main'><div class='container'>";
    
    // Introduction
    html += "<div class='card'><h2>📟 Serial Commands Reference</h2>";
    html += "<p style='color:#94a3b8;margin-bottom:1rem'>Control your display using serial commands via USB connection. Open the Serial Monitor at 9600 baud to send commands.</p>";
    html += "<div style='background:rgba(14,165,233,0.1);border:1px solid #0ea5e9;border-radius:8px;padding:1rem;margin-top:1rem'>";
    html += "<p style='color:#38bdf8;margin:0;font-size:0.9rem'><i class='fas fa-info-circle' style='margin-right:8px'></i><strong>Tip:</strong> Type 'H' or '?' in the Serial Monitor to display this help in your terminal.</p>";
    html += "</div></div>";
    
    // Image Transformation Commands
    html += "<div class='card'><h2>🔄 Image Transformations</h2>";
    html += "<table style='width:100%;border-collapse:collapse'>";
    html += "<thead><tr style='background:#1e293b;border-bottom:2px solid #334155'>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Key</th>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Action</th>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Description</th></tr></thead><tbody>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>+</td>";
    html += "<td style='padding:0.75rem'>Scale Up</td><td style='padding:0.75rem;color:#94a3b8'>Increase image scale on both axes by 0.1</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>-</td>";
    html += "<td style='padding:0.75rem'>Scale Down</td><td style='padding:0.75rem;color:#94a3b8'>Decrease image scale on both axes by 0.1</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>W</td>";
    html += "<td style='padding:0.75rem'>Move Up</td><td style='padding:0.75rem;color:#94a3b8'>Move image up by 10 pixels</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>S</td>";
    html += "<td style='padding:0.75rem'>Move Down</td><td style='padding:0.75rem;color:#94a3b8'>Move image down by 10 pixels</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>A</td>";
    html += "<td style='padding:0.75rem'>Move Left</td><td style='padding:0.75rem;color:#94a3b8'>Move image left by 10 pixels</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>D</td>";
    html += "<td style='padding:0.75rem'>Move Right</td><td style='padding:0.75rem;color:#94a3b8'>Move image right by 10 pixels</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>Q</td>";
    html += "<td style='padding:0.75rem'>Rotate CCW</td><td style='padding:0.75rem;color:#94a3b8'>Rotate image 90° counter-clockwise</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>E</td>";
    html += "<td style='padding:0.75rem'>Rotate CW</td><td style='padding:0.75rem;color:#94a3b8'>Rotate image 90° clockwise</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>R</td>";
    html += "<td style='padding:0.75rem'>Reset All</td><td style='padding:0.75rem;color:#94a3b8'>Reset all transformations to defaults</td></tr>";
    
    html += "</tbody></table></div>";
    
    // Display Control Commands
    html += "<div class='card'><h2>💡 Display Controls</h2>";
    html += "<table style='width:100%;border-collapse:collapse'>";
    html += "<thead><tr style='background:#1e293b;border-bottom:2px solid #334155'>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Key</th>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Action</th>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Description</th></tr></thead><tbody>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>L</td>";
    html += "<td style='padding:0.75rem'>Brightness Up</td><td style='padding:0.75rem;color:#94a3b8'>Increase brightness by 10%</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>K</td>";
    html += "<td style='padding:0.75rem'>Brightness Down</td><td style='padding:0.75rem;color:#94a3b8'>Decrease brightness by 10%</td></tr>";
    
    html += "</tbody></table></div>";
    
    // System Commands
    html += "<div class='card'><h2>⚙️ System Commands</h2>";
    html += "<table style='width:100%;border-collapse:collapse'>";
    html += "<thead><tr style='background:#1e293b;border-bottom:2px solid #334155'>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Key</th>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Action</th>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Description</th></tr></thead><tbody>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>B</td>";
    html += "<td style='padding:0.75rem'>Reboot Device</td><td style='padding:0.75rem;color:#94a3b8'>Restart the ESP32 device</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>M</td>";
    html += "<td style='padding:0.75rem'>Memory Info</td><td style='padding:0.75rem;color:#94a3b8'>Display heap and PSRAM memory status</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>I</td>";
    html += "<td style='padding:0.75rem'>Network Info</td><td style='padding:0.75rem;color:#94a3b8'>Show WiFi connection details</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>P</td>";
    html += "<td style='padding:0.75rem'>PPA Info</td><td style='padding:0.75rem;color:#94a3b8'>Display hardware accelerator status</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>T</td>";
    html += "<td style='padding:0.75rem'>MQTT Info</td><td style='padding:0.75rem;color:#94a3b8'>Show MQTT connection status</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>X</td>";
    html += "<td style='padding:0.75rem'>Web Server</td><td style='padding:0.75rem;color:#94a3b8'>Show web server status and restart</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>H / ?</td>";
    html += "<td style='padding:0.75rem'>Help</td><td style='padding:0.75rem;color:#94a3b8'>Display command reference in Serial Monitor</td></tr>";
    
    html += "</tbody></table></div>";
    
    // Touch Controls
    html += "<div class='card'><h2>👆 Touch Controls</h2>";
    html += "<table style='width:100%;border-collapse:collapse'>";
    html += "<thead><tr style='background:#1e293b;border-bottom:2px solid #334155'>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Gesture</th>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Action</th>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Description</th></tr></thead><tbody>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;color:#10b981'>Single Tap</td>";
    html += "<td style='padding:0.75rem'>Next Image</td><td style='padding:0.75rem;color:#94a3b8'>Switch to the next image in cycling mode</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;color:#10b981'>Double Tap</td>";
    html += "<td style='padding:0.75rem'>Toggle Mode</td><td style='padding:0.75rem;color:#94a3b8'>Switch between cycling and single refresh modes</td></tr>";
    
    html += "</tbody></table></div>";
    
    // Usage Instructions
    html += "<div class='card'><h2>🔧 How to Use Serial Commands</h2>";
    html += "<ol style='color:#94a3b8;line-height:2;margin-left:1.5rem'>";
    html += "<li>Connect your ESP32 device to your computer via USB</li>";
    html += "<li>Open Arduino IDE or any serial terminal</li>";
    html += "<li>Set baud rate to <strong style='color:#38bdf8'>9600</strong></li>";
    html += "<li>Type a command key and press Enter</li>";
    html += "<li>Commands are <strong style='color:#38bdf8'>case-insensitive</strong> (W or w both work)</li>";
    html += "<li>Serial output will confirm the action and show current values</li>";
    html += "</ol>";
    html += "<div style='background:rgba(14,165,233,0.1);border:1px solid #0ea5e9;border-radius:8px;padding:1rem;margin-top:1rem'>";
    html += "<p style='color:#38bdf8;margin:0;font-size:0.9rem'><i class='fas fa-info-circle' style='margin-right:8px'></i><strong>Image Transformations:</strong> Changes made with +, -, W, S, A, D, Q, E, R are automatically saved to configuration for the current image.</p>";
    html += "</div>";
    html += "<div style='background:rgba(245,158,11,0.1);border:1px solid #f59e0b;border-radius:8px;padding:1rem;margin-top:1rem'>";
    html += "<p style='color:#f59e0b;margin:0;font-size:0.9rem'><i class='fas fa-exclamation-triangle' style='margin-right:8px'></i><strong>Brightness:</strong> L and K commands take effect immediately but are NOT saved. Brightness settings persist only when changed via the web interface or MQTT.</p>";
    html += "</div></div>";
    
    html += "</div></div>";
    return html;
}
