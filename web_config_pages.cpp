#include "web_config.h"
#include "web_config_html.h"
#include "system_monitor.h"
#include "network_manager.h"
#include "mqtt_manager.h"
#include "display_manager.h"

// Page generation functions for the web configuration interface
// These functions generate the HTML content for each configuration page

String WebConfig::generateMainPage() {
    String html;
    html.reserve(12000);  // Pre-allocate ~12KB to prevent fragmentation during HTML generation
    html = "<div class='main'><div class='container'>";
    
    // Top row - Uptime and Image status cards
    html += "<div class='stats'>";
    html += "<div class='stat-card'><i class='fas fa-clock stat-icon'></i>";
    html += "<div class='stat-value'>" + formatUptime(millis()) + "</div>";
    html += "<div class='stat-label'>Uptime</div></div>";
    
    html += "<div class='stat-card'><i class='fas fa-list stat-icon'></i>";
    html += "<div class='stat-value'>" + String(configStorage.getCurrentImageIndex() + 1) + "/" + String(configStorage.getImageSourceCount()) + "</div>";
    html += "<div class='stat-label'>Active Source</div></div>";
    
    html += "<div class='stat-card'><i class='fas fa-sync-alt stat-icon'></i>";
    html += "<div class='stat-value'>" + String(configStorage.getCycleInterval() / 1000) + "s</div>";
    html += "<div class='stat-label'>Display Time</div></div>";
    
    html += "<div class='stat-card'><i class='fas fa-download stat-icon'></i>";
    html += "<div class='stat-value'>" + String(configStorage.getUpdateInterval() / 1000 / 60) + "m</div>";
    html += "<div class='stat-label'>Refresh Interval</div></div>";
    html += "</div>";
    
    // Row 1: Connection Status - Network & MQTT
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
    
    // Row 2: Hardware Status - System & Display
    html += "<div class='grid' style='margin-top:1.5rem'>";
    
    // System Info
    html += "<div class='card'><h2>💻 System Information</h2>";
    html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:0.5rem;font-size:0.9rem;color:#94a3b8'>";
    html += "<div><strong style='color:#64748b'>Chip:</strong><br>" + String(ESP.getChipModel()) + " rev" + String(ESP.getChipRevision()) + "</div>";
    html += "<div><strong style='color:#64748b'>Cores:</strong><br>" + String(ESP.getChipCores()) + " @ " + String(ESP.getCpuFreqMHz()) + " MHz</div>";
    html += "<div><strong style='color:#64748b'>Free Heap:</strong><br>" + formatBytes(systemMonitor.getCurrentFreeHeap()) + " / " + formatBytes(ESP.getHeapSize()) + "</div>";
    html += "<div><strong style='color:#64748b'>Free PSRAM:</strong><br>" + formatBytes(systemMonitor.getCurrentFreePsram()) + " / " + formatBytes(ESP.getPsramSize()) + "</div>";
    html += "<div><strong style='color:#64748b'>Temperature:</strong><br>" + String(temperatureRead(), 1) + "°C / " + String(temperatureRead() * 9.0 / 5.0 + 32.0, 1) + "°F</div>";
    html += "<div><strong style='color:#64748b'>Health:</strong><br>" + String(systemMonitor.isSystemHealthy() ? "<span style='color:#10b981'>Healthy</span>" : "<span style='color:#ef4444'>Issues</span>") + "</div>";
    html += "</div></div>";
    
    // Display Info
    html += "<div class='card'><h2>🖥️ Display Information</h2>";
    html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:0.5rem;font-size:0.9rem;color:#94a3b8'>";
    html += "<div><strong style='color:#64748b'>Resolution:</strong><br>" + String(displayManager.getWidth()) + " × " + String(displayManager.getHeight()) + "</div>";
    html += "<div><strong style='color:#64748b'>Brightness:</strong><br>" + String(displayManager.getBrightness()) + "% " + String(configStorage.getBrightnessAutoMode() ? "(Auto)" : "(Manual)") + "</div>";
    html += "<div><strong style='color:#64748b'>Backlight Freq:</strong><br>" + String(configStorage.getBacklightFreq()) + " Hz</div>";
    html += "<div><strong style='color:#64748b'>Resolution:</strong><br>" + String(configStorage.getBacklightResolution()) + "-bit</div>";
    html += "</div></div>";
    
    html += "</div>";
    
    // Row 3: Software Configuration - Firmware & Home Assistant
    html += "<div class='grid' style='margin-top:1.5rem'>";
    
    // Firmware Info
    html += "<div class='card'><h2>📦 Firmware Information</h2>";
    html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:0.5rem;font-size:0.9rem;color:#94a3b8'>";
    html += "<div><strong style='color:#64748b'>SDK Version:</strong><br>" + String(ESP.getSdkVersion()) + "</div>";
    html += "<div><strong style='color:#64748b'>Flash Size:</strong><br>" + formatBytes(ESP.getFlashChipSize()) + " @ " + String(ESP.getFlashChipSpeed() / 1000000) + " MHz</div>";
    html += "<div><strong style='color:#64748b'>Sketch Size:</strong><br>" + formatBytes(ESP.getSketchSize()) + "</div>";
    html += "<div><strong style='color:#64748b'>Free Space:</strong><br>" + formatBytes(ESP.getFreeSketchSpace()) + "</div>";
    html += "<div style='grid-column:1/-1'><strong style='color:#64748b'>MD5:</strong><br><span style='font-family:monospace;font-size:0.8rem;word-break:break-all'>" + String(ESP.getSketchMD5()) + "</span></div>";
    html += "</div></div>";
    
    // Home Assistant Info
    html += "<div class='card'><h2>🏠 Home Assistant</h2>";
    html += "<div style='font-size:0.9rem;color:#94a3b8'>";
    html += "<p style='margin:0.5rem 0'><strong style='color:#64748b'>Discovery:</strong> " + String(configStorage.getHADiscoveryEnabled() ? "<span style='color:#10b981'>Enabled</span>" : "<span style='color:#64748b'>Disabled</span>") + "</p>";
    html += "<p style='margin:0.5rem 0'><strong style='color:#64748b'>Device Name:</strong> " + escapeHtml(configStorage.getHADeviceName()) + "</p>";
    html += "<p style='margin:0.5rem 0'><strong style='color:#64748b'>Discovery Prefix:</strong> " + escapeHtml(configStorage.getHADiscoveryPrefix()) + "</p>";
    html += "<p style='margin:0.5rem 0'><strong style='color:#64748b'>State Topic:</strong> " + escapeHtml(configStorage.getHAStateTopic()) + "</p>";
    html += "<p style='margin:0.5rem 0'><strong style='color:#64748b'>Update Interval:</strong> " + String(configStorage.getHASensorUpdateInterval() / 1000) + "s</p>";
    html += "</div></div>";
    
    html += "</div>";
    
    // Image Status - Configured Sources List
    html += "<div class='card' style='margin-top:1.5rem'><h2>🖼️ Image Status</h2>";
    
    int sourceCount = configStorage.getImageSourceCount();
    int enabledCount = configStorage.getEnabledImageCount();
    int currentIndex = configStorage.getCurrentImageIndex();
    
    // Summary info
    html += "<div id='imageStatusSummary' style='display:flex;justify-content:space-between;align-items:center;padding:1rem;background:#1e293b;border-radius:8px;margin-bottom:1rem'>";
    html += "<div id='imgTotal'><p style='margin:0;font-size:0.9rem;color:#94a3b8'><strong style='color:#e2e8f0'>Total:</strong> " + String(sourceCount) + " source" + String(sourceCount != 1 ? "s" : "") + "</p></div>";
    html += "<div id='imgEnabled'><p style='margin:0;font-size:0.9rem;color:#94a3b8'><strong style='color:#e2e8f0'>Enabled:</strong> " + String(enabledCount) + "</p></div>";
    html += "<div id='imgActive'><p style='margin:0;font-size:0.9rem;color:#94a3b8'><strong style='color:#e2e8f0'>Active:</strong> #" + String(currentIndex + 1) + "</p></div>";
    html += "<div id='imgUpdate'><p style='margin:0;font-size:0.9rem;color:#94a3b8'><strong style='color:#e2e8f0'>Refresh:</strong> " + String(configStorage.getUpdateInterval() / 1000 / 60) + "m</p></div>";
    html += "</div>";
    
    // Explanation
    html += "<div style='background:rgba(14,165,233,0.1);border:1px solid #0ea5e9;border-radius:8px;padding:1rem;margin-bottom:1.5rem'>";
    html += "<p style='color:#38bdf8;margin:0;font-size:0.85rem;line-height:1.6'><i class='fas fa-info-circle' style='margin-right:8px'></i>";
    html += "Display cycles through <strong>" + String(enabledCount) + " enabled image" + String(enabledCount != 1 ? "s" : "") + "</strong> with individual durations (5-3600s per image). ";
    html += "Sources are refreshed every <strong>" + String(configStorage.getUpdateInterval() / 1000 / 60) + " minutes</strong>. ";
    html += String(configStorage.getRandomOrder() ? "Cycling in <strong>random order</strong>." : "Cycling in <strong>sequential order</strong>.") + "</p>";
    html += "</div>";
        
        // List all configured sources
        if (sourceCount > 0) {
            html += "<h3 style='color:#94a3b8;font-size:1rem;margin-bottom:1rem'>Configured Sources:</h3>";
            for (int i = 0; i < sourceCount; i++) {
                String sourceUrl = configStorage.getImageSource(i);
                String activeIndicator = (i == currentIndex) ? "<span style='color:#10b981;margin-right:8px;font-size:1.2rem'>►</span>" : "<span style='color:#64748b;margin-right:8px'>•</span>";
                html += "<div id='source-" + String(i) + "' style='margin-bottom:0.75rem;padding:0.75rem;background:" + String(i == currentIndex ? "#1e3a2e" : "#1e293b") + ";border-radius:8px;border-left:4px solid " + String(i == currentIndex ? "#10b981" : "#475569") + ";overflow-wrap:break-word;word-break:break-all'>";
                html += "<div id='source-label-" + String(i) + "' style='font-size:0.85rem;color:#94a3b8;margin-bottom:0.25rem'>" + activeIndicator + "<strong style='color:" + String(i == currentIndex ? "#10b981" : "#64748b") + "'>Source " + String(i + 1) + String(i == currentIndex ? " (Active)" : "") + "</strong></div>";
                html += "<div style='font-size:0.85rem;color:#cbd5e1;font-family:monospace;padding-left:1.5rem'>" + escapeHtml(sourceUrl) + "</div>";
                html += "</div>";
            }
        }
    html += "</div></div></div>";
    
    return html;
}

String WebConfig::generateNetworkPage() {
    String html;
    html.reserve(2000);  // Pre-allocate ~2KB for network config page
    html = "<div class='main'><div class='container'>";
    html += "<form id='networkForm'><div class='card'>";
    html += "<h2>📡 WiFi Configuration</h2>";
    
    // Info box
    html += "<div style='background:rgba(56,189,248,0.1);border:1px solid #38bdf8;border-radius:8px;padding:1rem;margin-bottom:1.5rem'>";
    html += "<p style='color:#38bdf8;margin:0;font-size:0.9rem'><i class='fas fa-info-circle' style='margin-right:8px'></i>";
    html += "<strong>WiFi Setup Mode:</strong> To reconfigure WiFi from scratch (with network scanning), use Factory Reset which will trigger the WiFi setup portal on next boot.</p>";
    html += "</div>";
    
    html += "<div class='form-group'><label for='wifi_ssid'>Network Name (SSID)</label>";
    html += "<input type='text' id='wifi_ssid' name='wifi_ssid' class='form-control' value='" + escapeHtml(configStorage.getWiFiSSID()) + "' required></div>";
    html += "<div class='form-group'><label for='wifi_password'>Password</label>";
    html += "<input type='password' id='wifi_password' name='wifi_password' class='form-control' value='" + escapeHtml(configStorage.getWiFiPassword()) + "'></div>";
    html += "<button type='submit' class='btn btn-primary'>💾 Save Network Settings</button>";
    html += "</div></form></div></div>";
    return html;
}

String WebConfig::generateMQTTPage() {
    String html;
    html.reserve(5000);  // Pre-allocate ~5KB for MQTT config page
    html = "<div class='main'><div class='container'><form id='mqttForm'><div class='grid'>";
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
    html += "><label for='ha_discovery_enabled' style='margin-bottom:0;cursor:pointer;font-size:1rem'>Enable Home Assistant MQTT Discovery</label>";
    html += "<input type='hidden' name='ha_discovery_enabled_present' value='1'></div>";
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

String WebConfig::generateImagePage() {
    String html;
    html.reserve(8000);  // Pre-allocate ~8KB
    html = "<div class='main'><div class='container'>";
    
    // Header with controls on same line
    html += "<div class='card'>";
    
    html += "<form id='imageForm' novalidate>";
    
    // 1. Force Cycling Enabled (Unified Mode)
    // We send these hidden fields so the backend always saves the device in "Cycling" mode
    html += "<input type='hidden' name='cycling_enabled' value='on'>";
    html += "<input type='hidden' name='cycling_enabled_present' value='1'>";
    
    // 2. Image Sources List header with controls
    int sourceCount = configStorage.getImageSourceCount();
    html += "<div style='display:flex;gap:0.75rem;align-items:center;justify-content:space-between;margin-bottom:0.75rem'>";
    html += "<h2 style='margin:0'>🖼️ Image Sources</h2>";
    html += "<div style='display:flex;gap:0.75rem;align-items:center'>";
    html += "<button type='button' class='btn btn-success' onclick='addImageSource(this)' style='font-size:0.9rem;padding:0.6rem 0.8rem'><i class='fas fa-plus'></i></button>";
    if (sourceCount > 1) {
        html += "<label style='display:flex;align-items:center;color:#94a3b8;cursor:pointer'>";
        html += "<input type='checkbox' id='selectAllImages' style='width:18px;height:18px;accent-color:#0ea5e9;margin-right:6px' onchange='toggleSelectAll(this.checked)'>";
        html += "<span style='font-size:0.9rem'>Select All</span></label>";
        html += "<button type='button' class='btn btn-danger' id='bulkDeleteBtn' onclick='bulkDeleteSelected(this)' style='font-size:0.9rem;padding:0.6rem 1rem;display:none'>";
        html += "<i class='fas fa-trash' style='margin-right:6px'></i>Delete Selected (<span id='selectedCount'>0</span>)</button>";
    }
    html += "</div></div>";
    html += "<div id='imageSourcesList'>";
    
    // Check if user is currently editing
    extern bool cyclingPausedForEditing;
    bool isEditingActive = cyclingPausedForEditing;
    
    int currentIndex = configStorage.getCurrentImageIndex();
    for (int i = 0; i < sourceCount; i++) {
        String url = configStorage.getImageSource(i);
        bool isEnabled = configStorage.isImageEnabled(i);
        bool isActive = isEditingActive && (i == currentIndex);
        
        // Show image as active only if user is in editing mode
        html += "<div class='image-source-item' id='imageItem_" + String(i) + "' style='margin-bottom:0.5rem;padding:0.75rem;border:1px solid " + String(isActive ? "#3b82f6" : "#334155") + ";border-radius:6px;background:" + String(isActive ? "#1e3a5f" : "#1e293b") + String(isEnabled ? "" : ";opacity:0.6") + "'>";
        html += "<div style='display:flex;align-items:center;gap:0.5rem'>";
        if (sourceCount > 1) {
            html += "<input type='checkbox' class='image-select-checkbox' data-index='" + String(i) + "' style='width:18px;height:18px;accent-color:#0ea5e9;cursor:pointer' onchange='updateBulkDeleteButton()'>";
        }
        
        // Enable/Disable toggle button
        html += "<button type='button' class='btn " + String(isEnabled ? "btn-success" : "btn-secondary") + "' ";
        html += "onclick='toggleImageEnabled(" + String(i) + ", this)' ";
        html += "title='" + String(isEnabled ? "Enabled - Click to disable" : "Disabled - Click to enable") + "' ";
        html += "style='min-width:40px;padding:0.4rem 0.6rem;font-size:0.85rem'>";
        html += "<i class='fas fa-" + String(isEnabled ? "eye" : "eye-slash") + "'></i>";
        html += "</button>";
        
        html += "<span style='font-weight:bold;color:#38bdf8;min-width:2rem'>" + String(i + 1) + ".</span>";
        html += "<input type='url' class='form-control' id='imageUrl_" + String(i) + "' style='flex:1' value='" + escapeHtml(url) + "' onchange='updateImageSource(" + String(i) + ", this)'>";
        
        // Duration field
        unsigned long duration = configStorage.getImageDuration(i);
        html += "<div style='display:flex;align-items:center;gap:0.25rem;min-width:90px'>";
        html += "<input type='number' class='form-control' id='imageDuration_" + String(i) + "' value='" + String(duration) + "' min='5' max='3600' ";
        html += "onchange='updateImageDuration(" + String(i) + ", this)' ";
        html += "title='Display duration in seconds' style='width:65px;padding:0.4rem;font-size:0.85rem;text-align:center'>";
        html += "<span style='color:#64748b;font-size:0.75rem'>s</span>";
        html += "</div>";
        
        // Select button to switch to this image for editing
        html += "<button type='button' class='btn " + String(isActive ? "btn-primary" : "btn-secondary") + "' id='editBtn_" + String(i) + "' ";
        html += "onclick='selectImageForEditing(" + String(i) + ", this)' ";
        html += "title='Select this image to edit transforms' ";
        html += "style='min-width:80px;padding:0.4rem 0.6rem;font-size:0.85rem'>";
        html += String(isActive ? "<i class='fas fa-check' style='margin-right:4px'></i>Selected" : "<i class='fas fa-edit' style='margin-right:4px'></i>Edit");
        html += "</button>";
        
        html += "</div></div>";
    }
    
    html += "</div>";
    
    // Single Transform Controls for Selected Image
    html += "<div id='transformSection' style='margin-top:0.75rem;padding:0.75rem;background:#0f172a;border-radius:8px;border:1px solid #3b82f6" + String(isEditingActive ? "" : ";display:none") + "'>";
    html += "<div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:0.5rem'>";
    html += "<h3 style='color:#38bdf8;margin:0;font-size:1rem'><i class='fas fa-sliders-h' style='margin-right:6px'></i>Transform Settings for Image #<span id='selectedImageNumber'>" + String(currentIndex + 1) + "</span></h3>";
    html += "<span id='cycleStatus' style='color:#f59e0b;font-size:0.75rem;padding:0.25rem 0.5rem;background:rgba(245,158,11,0.1);border-radius:4px;border:1px solid #f59e0b'>";
    html += "<i class='fas fa-pause-circle' style='margin-right:4px'></i>Paused - resuming in <span id='resumeTimer'>30</span>s</span></div>";
    html += "<p style='color:#64748b;font-size:0.8rem;margin:0 0 0.5rem 0'><i class='fas fa-info-circle' style='margin-right:4px'></i>Any change resets the timer. Auto-cycling resumes after 30s of inactivity.</p>";
    html += "<script>let cycleResumeTime=Date.now()+30000;let timerInterval=setInterval(()=>{const remaining=Math.max(0,Math.ceil((cycleResumeTime-Date.now())/1000));const timerEl=document.getElementById('resumeTimer');const statusEl=document.getElementById('cycleStatus');if(timerEl)timerEl.textContent=remaining;if(remaining===0&&statusEl){statusEl.style.color='#10b981';statusEl.style.background='rgba(16,185,129,0.1)';statusEl.style.borderColor='#10b981';statusEl.innerHTML='<i class=\"fas fa-sync-alt\" style=\"margin-right:4px\"></i>Cycling active';clearInterval(timerInterval)}},1000);function resetCycleTimer(){cycleResumeTime=Date.now()+30000;const statusEl=document.getElementById('cycleStatus');if(statusEl){statusEl.style.color='#f59e0b';statusEl.style.background='rgba(245,158,11,0.1)';statusEl.style.borderColor='#f59e0b';statusEl.innerHTML='<i class=\"fas fa-pause-circle\" style=\"margin-right:4px\"></i>Paused - resuming in <span id=\"resumeTimer\">30</span>s'}}</script>";
    
    html += "<div style='display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:0.5rem;margin-bottom:0.5rem'>";
    html += "<div><label style='font-size:0.85rem;color:#94a3b8'>Scale X</label>";
    html += "<input type='number' id='selectedScaleX' class='form-control' value='" + String(configStorage.getImageScaleX(currentIndex)) + "' step='0.01' min='0.1' max='" + String(MAX_SCALE, 1) + "' onchange='updateSelectedImageTransform(\"scaleX\", this.value)' style='font-size:0.9rem;padding:0.5rem'></div>";
    
    html += "<div><label style='font-size:0.85rem;color:#94a3b8'>Scale Y</label>";
    html += "<input type='number' id='selectedScaleY' class='form-control' value='" + String(configStorage.getImageScaleY(currentIndex)) + "' step='0.01' min='0.1' max='" + String(MAX_SCALE, 1) + "' onchange='updateSelectedImageTransform(\"scaleY\", this.value)' style='font-size:0.9rem;padding:0.5rem'></div>";
    
    html += "<div><label style='font-size:0.85rem;color:#94a3b8'>Offset X</label>";
    html += "<input type='number' id='selectedOffsetX' class='form-control' value='" + String(configStorage.getImageOffsetX(currentIndex)) + "' onchange='updateSelectedImageTransform(\"offsetX\", this.value)' style='font-size:0.9rem;padding:0.5rem'></div>";
    
    html += "<div><label style='font-size:0.85rem;color:#94a3b8'>Offset Y</label>";
    html += "<input type='number' id='selectedOffsetY' class='form-control' value='" + String(configStorage.getImageOffsetY(currentIndex)) + "' onchange='updateSelectedImageTransform(\"offsetY\", this.value)' style='font-size:0.9rem;padding:0.5rem'></div>";
    
    html += "<div><label style='font-size:0.85rem;color:#94a3b8'>Rotation</label>";
    html += "<select id='selectedRotation' class='form-control' onchange='updateSelectedImageTransform(\"rotation\", this.value)' style='font-size:0.9rem;padding:0.5rem'>";
    int selectedRotation = (int)configStorage.getImageRotation(currentIndex);
    html += String("<option value='0'") + (selectedRotation == 0 ? " selected" : "") + ">0°</option>";
    html += String("<option value='90'") + (selectedRotation == 90 ? " selected" : "") + ">90°</option>";
    html += String("<option value='180'") + (selectedRotation == 180 ? " selected" : "") + ">180°</option>";
    html += String("<option value='270'") + (selectedRotation == 270 ? " selected" : "") + ">270°</option>";
    html += "</select></div></div>";
    
    html += "<div style='display:flex;gap:0.5rem'>";
    html += "<button type='button' class='btn btn-secondary' onclick='copyDefaultsToSelectedImage(this)' style='font-size:0.8rem;padding:0.4rem 0.6rem'><i class='fas fa-undo' style='margin-right:4px'></i>Reset to Defaults</button>";
    html += "<button type='button' class='btn btn-primary' onclick='applySelectedTransformImmediately(this)' style='font-size:0.8rem;padding:0.4rem 0.6rem'><i class='fas fa-check' style='margin-right:4px'></i>Apply & Preview</button>";
    html += "</div></div>";
    
    html += "</div>"; // Close Image Config Card
    
    // 4. Default Settings Card (Global Defaults + Timing & Order) - Collapsible
    html += "<div class='card' style='margin-top:0.75rem'>";
    html += "<h2 onclick=\"document.getElementById('defaultSettings').style.display=document.getElementById('defaultSettings').style.display==='none'?'block':'none';this.querySelector('.toggle-icon').textContent=document.getElementById('defaultSettings').style.display==='none'?'▶':'▼'\" style='cursor:pointer;user-select:none'>";
    html += "🎨 Default Settings <span class='toggle-icon' style='font-size:0.7em;color:#64748b'>▶</span></h2>";
    html += "<div id='defaultSettings' style='display:none'>";
    html += "<p style='color:#94a3b8;font-size:0.8rem;margin:0 0 0.5rem 0'>Global settings for timing, order, and transformations.</p>";
    
    // Timing & Order Section
    html += "<h3 style='color:#38bdf8;margin:0 0 0.5rem 0;font-size:1rem'>⏱️ Timing & Order</h3>";
    html += "<div class='grid' style='margin-bottom:0.5rem'>";
    
    // Default Image Duration (for newly added images)
    html += "<div class='form-group'><label for='default_image_duration'>";
    html += "<span style='color:#38bdf8'>Default Duration for New Images</span> <span style='color:#94a3b8'>(seconds)</span></label>";
    html += "<input type='number' id='default_image_duration' name='default_image_duration' class='form-control' value='" + String(configStorage.getDefaultImageDuration()) + "' min='5' max='3600'>";
    html += "<p style='color:#64748b;font-size:0.8rem;margin-top:0.25rem;margin-bottom:0'>";
    html += "<i class='fas fa-clock' style='margin-right:4px;color:#22c55e'></i>";
    html += "Default display time for newly added images (can be customized per-image above)";
    html += "</p></div>";
    
    // Refresh Interval (Update Interval)
    html += "<div class='form-group'><label for='update_interval'>";
    html += "<span style='color:#38bdf8'>Refresh Interval</span> <span style='color:#94a3b8'>(minutes)</span></label>";
    html += "<input type='number' id='update_interval' name='update_interval' class='form-control' value='" + String(configStorage.getUpdateInterval() / 1000 / 60) + "' min='1' max='1440'>";
    html += "<p style='color:#64748b;font-size:0.8rem;margin-top:0.25rem;margin-bottom:0'>";
    html += "<i class='fas fa-download' style='margin-right:4px;color:#0ea5e9'></i>";
    html += "How often to re-download images from their URLs";
    html += "</p></div>";
    
    html += "</div>"; // End Grid
    
    // Random Order Checkbox
    html += "<div class='form-group' style='margin-bottom:0.5rem'><div style='display:flex;align-items:center'>";
    html += "<input type='checkbox' id='random_order' name='random_order' style='width:20px;height:20px;accent-color:#0ea5e9;margin-right:10px'" + String(configStorage.getRandomOrder() ? " checked" : "") + ">";
    html += "<label for='random_order' style='margin-bottom:0;cursor:pointer'>Randomize display order</label>";
    html += "<input type='hidden' name='random_order_present' value='1'></div></div>";
    
    // Transformation Defaults Section
    html += "<h3 style='color:#38bdf8;margin:0.5rem 0 0.5rem 0;font-size:1rem'>🔧 Transformation Defaults</h3>";
    html += "<p style='color:#94a3b8;font-size:0.8rem;margin:0 0 0.5rem 0'>These settings apply to all new images or those without overrides.</p>";
    html += "<div style='display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:0.75rem'>";
    html += "<div class='form-group' style='margin-bottom:0'><label for='default_scale_x' style='font-size:0.8rem;margin-bottom:0.25rem'>Scale X</label>";
    html += "<input type='number' id='default_scale_x' name='default_scale_x' class='form-control' style='padding:0.4rem 0.5rem;font-size:0.9rem' value='" + String(configStorage.getDefaultScaleX()) + "' step='0.01' min='0.1' max='" + String(MAX_SCALE, 1) + "'></div>";
    html += "<div class='form-group' style='margin-bottom:0'><label for='default_scale_y' style='font-size:0.8rem;margin-bottom:0.25rem'>Scale Y</label>";
    html += "<input type='number' id='default_scale_y' name='default_scale_y' class='form-control' style='padding:0.4rem 0.5rem;font-size:0.9rem' value='" + String(configStorage.getDefaultScaleY()) + "' step='0.01' min='0.1' max='" + String(MAX_SCALE, 1) + "'></div>";
    html += "<div class='form-group' style='margin-bottom:0'><label for='default_offset_x' style='font-size:0.8rem;margin-bottom:0.25rem'>Offset X</label>";
    html += "<input type='number' id='default_offset_x' name='default_offset_x' class='form-control' style='padding:0.4rem 0.5rem;font-size:0.9rem' value='" + String(configStorage.getDefaultOffsetX()) + "'></div>";
    html += "<div class='form-group' style='margin-bottom:0'><label for='default_offset_y' style='font-size:0.8rem;margin-bottom:0.25rem'>Offset Y</label>";
    html += "<input type='number' id='default_offset_y' name='default_offset_y' class='form-control' style='padding:0.4rem 0.5rem;font-size:0.9rem' value='" + String(configStorage.getDefaultOffsetY()) + "'></div>";
    html += "<div class='form-group' style='margin-bottom:0'><label for='default_rotation' style='font-size:0.8rem;margin-bottom:0.25rem'>Rotation</label>";
    html += "<select id='default_rotation' name='default_rotation' class='form-control' style='padding:0.4rem 0.5rem;font-size:0.9rem'>";
    int defRot = (int)configStorage.getDefaultRotation();
    html += String("<option value='0'") + (defRot == 0 ? " selected" : "") + ">0°</option>";
    html += String("<option value='90'") + (defRot == 90 ? " selected" : "") + ">90°</option>";
    html += String("<option value='180'") + (defRot == 180 ? " selected" : "") + ">180°</option>";
    html += String("<option value='270'") + (defRot == 270 ? " selected" : "") + ">270°</option>";
    html += "</select></div>";
    html += "</div></div></div>"; // Close defaultSettings div, card
    
    // Save Button
    html += "<button type='submit' class='btn btn-primary' style='margin-top:0.75rem'><i class='fas fa-save' style='margin-right:6px'></i>Save All Settings</button>";
    
    html += "</form></div></div>";
    
    return html;
}

String WebConfig::generateDisplayPage() {
    String html;
    html.reserve(4000);  // Pre-allocate ~4KB for display config page
    html = "<div class='main'><div class='container'>";
    
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
    html += "</div>";
    
    // Home Assistant REST Control Card
    html += "<div class='card' style='border-left:4px solid #38bdf8'><h2> Home Assistant Light Control</h2>";
    html += "<p style='color:#94a3b8;font-size:0.9rem;margin-bottom:1.5rem'>Automatically adjust screen brightness using a Home Assistant light sensor entity. Runs on Core 0 (non-blocking).</p>";
    
    // Enable Switch
    html += "<div class='form-group'>";
    html += "<div style='display:flex;align-items:center;margin-bottom:1rem'>";
    html += "<input type='checkbox' id='use_ha_rest_control' name='use_ha_rest_control' style='width:20px;height:20px;accent-color:#0ea5e9;margin-right:10px' " + String(configStorage.getUseHARestControl() ? "checked" : "") + ">";
    html += "<label for='use_ha_rest_control' style='margin-bottom:0;cursor:pointer;font-size:1rem'>Enable Ambient Light Sensor</label>";
    html += "</div>";
    html += "<input type='hidden' name='use_ha_rest_control_present' value='1'>";
    html += "<p style='color:#64748b;font-size:0.85rem;background:rgba(245,158,11,0.1);padding:0.5rem;border-radius:6px;border-left:3px solid #f59e0b'><i class='fas fa-info-circle' style='margin-right:0.5rem'></i>Enabling this will automatically disable MQTT Auto Mode to prevent conflicts.</p>";
    html += "</div>";
    
    // Connection Details Section
    html += "<div style='background:#0f172a;padding:1rem;border-radius:8px;border:1px solid #334155;margin-bottom:1rem'>";
    html += "<h3 style='color:#38bdf8;font-size:1rem;margin-bottom:1rem;display:flex;align-items:center'><i class='fas fa-plug' style='margin-right:8px'></i>Connection Details</h3>";
    
    html += "<div class='form-group'><label for='ha_base_url'>Home Assistant URL</label>";
    html += "<input type='text' id='ha_base_url' name='ha_base_url' class='form-control' value='" + configStorage.getHABaseUrl() + "' placeholder='http://homeassistant.local:8123'></div>";
    
    html += "<div class='form-group'><label for='ha_access_token'>Long-Lived Access Token</label>";
    html += "<input type='password' id='ha_access_token' name='ha_access_token' class='form-control' placeholder='Leave blank to keep existing token'>";
    html += "<p style='color:#64748b;font-size:0.8rem;margin-top:0.5rem'>Create at <strong>Profile  Long-Lived Access Tokens</strong></p></div>";
    
    html += "<div class='grid' style='grid-template-columns:2fr 1fr'>";
    html += "<div class='form-group'><label for='ha_light_sensor_entity'>Sensor Entity ID</label>";
    html += "<input type='text' id='ha_light_sensor_entity' name='ha_light_sensor_entity' class='form-control' value='" + configStorage.getHALightSensorEntity() + "' placeholder='sensor.living_room_illuminance'></div>";
    html += "<div class='form-group'><label for='ha_poll_interval'>Poll Interval (s)</label>";
    html += "<input type='number' id='ha_poll_interval' name='ha_poll_interval' class='form-control' value='" + String(configStorage.getHAPollInterval()) + "' min='10' max='3600'></div>";
    html += "</div></div>";
    
    // Brightness Mapping Section
    html += "<div style='background:#0f172a;padding:1rem;border-radius:8px;border:1px solid #334155'>";
    html += "<h3 style='color:#38bdf8;font-size:1rem;margin-bottom:1rem;display:flex;align-items:center'><i class='fas fa-sliders-h' style='margin-right:8px'></i>Brightness Mapping</h3>";
    
    html += "<div class='form-group'><label for='light_sensor_mapping_mode'>Response Curve</label>";
    html += "<select id='light_sensor_mapping_mode' name='light_sensor_mapping_mode' class='form-control'>";
    html += "<option value='0'" + String(configStorage.getLightSensorMappingMode() == 0 ? " selected" : "") + ">Linear (Indoor / Low Range)</option>";
    html += "<option value='1'" + String(configStorage.getLightSensorMappingMode() == 1 ? " selected" : "") + ">Logarithmic (Outdoor / High Range)</option>";
    html += "<option value='2'" + String(configStorage.getLightSensorMappingMode() == 2 ? " selected" : "") + ">Threshold Switch (Day/Night)</option>";
    html += "</select>";
    html += "<p style='color:#64748b;font-size:0.8rem;margin-top:0.5rem'>Use <strong>Logarithmic</strong> for sensors that go from 0 to 100,000+ lux (outdoors). Use <strong>Linear</strong> for dark rooms (0-500 lux).</p></div>";
    
    html += "<div class='grid'>";
    // Sensor Range
    html += "<div><label style='color:#94a3b8;font-size:0.9rem'>Sensor Range (Lux)</label>";
    html += "<div style='display:flex;gap:0.5rem;align-items:center'>";
    html += "<input type='number' id='light_sensor_min_lux' name='light_sensor_min_lux' class='form-control' value='" + String(configStorage.getLightSensorMinLux(), 1) + "' min='0' max='10000' step='0.1' placeholder='Min'>";
    html += "<span style='color:#64748b'>to</span>";
    html += "<input type='number' id='light_sensor_max_lux' name='light_sensor_max_lux' class='form-control' value='" + String(configStorage.getLightSensorMaxLux(), 1) + "' min='0' max='100000' step='0.1' placeholder='Max'>";
    html += "</div></div>";
    // Display Range
    html += "<div><label style='color:#94a3b8;font-size:0.9rem'>Display Brightness (%)</label>";
    html += "<div style='display:flex;gap:0.5rem;align-items:center'>";
    html += "<input type='number' id='display_min_brightness' name='display_min_brightness' class='form-control' value='" + String(configStorage.getDisplayMinBrightness()) + "' min='0' max='100' placeholder='Min'>";
    html += "<span style='color:#64748b'>to</span>";
    html += "<input type='number' id='display_max_brightness' name='display_max_brightness' class='form-control' value='" + String(configStorage.getDisplayMaxBrightness()) + "' min='0' max='100' placeholder='Max'>";
    html += "</div></div>";
    html += "</div></div></div>";
    
    // Save button card
    html += "<div class='card'>";
    html += "<button type='submit' class='btn btn-primary'>💾 Save Display Settings</button>";
    html += "</div>";
    
    html += "</form></div></div>";
    return html;
}

String WebConfig::generateAdvancedPage() {
    String html;
    html.reserve(6000);  // Pre-allocate ~6KB for advanced settings page
    html = "<div class='main'><div class='container'><form id='advancedForm'><div class='grid'>";
    html += "<div class='card'><h2>⏱️ Timing Settings</h2>";
    html += "<div class='form-group'><label for='mqtt_reconnect_interval'>MQTT Reconnect Interval (seconds)</label>";
    html += "<input type='number' id='mqtt_reconnect_interval' name='mqtt_reconnect_interval' class='form-control' value='" + String(configStorage.getMQTTReconnectInterval() / 1000) + "' min='1' max='300'></div>";
    html += "<div class='form-group'><label for='watchdog_timeout'>Watchdog Timeout (seconds)</label>";
    html += "<input type='number' id='watchdog_timeout' name='watchdog_timeout' class='form-control' value='" + String(configStorage.getWatchdogTimeout() / 1000) + "' min='10' max='120'></div></div>";
    
    html += "<div class='card'><h2>🕐 Time Settings</h2>";
    html += "<div class='form-group'><label><input type='checkbox' id='ntp_enabled' name='ntp_enabled' " + String(configStorage.getNTPEnabled() ? "checked" : "") + "> Enable NTP Time Sync</label>";
    html += "<input type='hidden' name='ntp_enabled_present' value='1'></div>";
    html += "<div class='form-group'><label for='ntp_server'>NTP Server</label>";
    html += "<input type='text' id='ntp_server' name='ntp_server' class='form-control' value='" + configStorage.getNTPServer() + "' placeholder='pool.ntp.org'></div>";
    html += "<div class='form-group'><label for='timezone'>Timezone</label>";
    html += "<select id='timezone' name='timezone' class='form-control'>";
    
    String currentTz = configStorage.getTimezone();
    
    // Common timezones with friendly names
    const char* timezones[][2] = {
        {"UTC0", "UTC (Universal Time)"},
        {"GMT0BST,M3.5.0/1,M10.5.0", "Europe/London (GMT/BST)"},
        {"CET-1CEST,M3.5.0,M10.5.0/3", "Europe/Paris (CET/CEST)"},
        {"EET-2EEST,M3.5.0/3,M10.5.0/4", "Europe/Athens (EET/EEST)"},
        {"MSK-3", "Europe/Moscow (MSK)"},
        {"EST5EDT,M3.2.0,M11.1.0", "US/Eastern (EST/EDT)"},
        {"CST6CDT,M3.2.0,M11.1.0", "US/Central (CST/CDT)"},
        {"MST7MDT,M3.2.0,M11.1.0", "US/Mountain (MST/MDT)"},
        {"PST8PDT,M3.2.0,M11.1.0", "US/Pacific (PST/PDT)"},
        {"AKST9AKDT,M3.2.0,M11.1.0", "US/Alaska (AKST/AKDT)"},
        {"HST10", "US/Hawaii (HST)"},
        {"AST4ADT,M3.2.0,M11.1.0", "Canada/Atlantic (AST/ADT)"},
        {"NST3:30NDT,M3.2.0,M11.1.0", "Canada/Newfoundland (NST/NDT)"},
        {"<-03>3", "South America/Buenos Aires (ART)"},
        {"<-03>3<-02>,M10.1.0/0,M2.3.0/0", "South America/São Paulo (BRT/BRST)"},
        {"AEST-10AEDT,M10.1.0,M4.1.0/3", "Australia/Sydney (AEST/AEDT)"},
        {"ACST-9:30ACDT,M10.1.0,M4.1.0/3", "Australia/Adelaide (ACST/ACDT)"},
        {"AWST-8", "Australia/Perth (AWST)"},
        {"NZST-12NZDT,M9.5.0,M4.1.0/3", "Pacific/Auckland (NZST/NZDT)"},
        {"JST-9", "Asia/Tokyo (JST)"},
        {"KST-9", "Asia/Seoul (KST)"},
        {"CST-8", "Asia/Shanghai (CST)"},
        {"HKT-8", "Asia/Hong Kong (HKT)"},
        {"SGT-8", "Asia/Singapore (SGT)"},
        {"IST-5:30", "Asia/Kolkata (IST)"},
        {"PKT-5", "Asia/Karachi (PKT)"},
        {"<+03>-3", "Asia/Dubai (GST)"},
        {"EAT-3", "Africa/Nairobi (EAT)"},
        {"SAST-2", "Africa/Johannesburg (SAST)"},
        {"WAT-1", "Africa/Lagos (WAT)"}
    };
    
    int tzCount = sizeof(timezones) / sizeof(timezones[0]);
    for (int i = 0; i < tzCount; i++) {
        String selected = (currentTz == timezones[i][0]) ? " selected" : "";
        html += "<option value='" + String(timezones[i][0]) + "'" + selected + ">" + String(timezones[i][1]) + "</option>";
    }
    
    html += "</select>";
    html += "<small style='color:#94a3b8;display:block;margin-top:0.5rem'>Select your timezone from the list. Time will be displayed in local time after NTP sync.</small></div></div>";
    
    html += "<div class='card'><h2>💾 Memory Thresholds</h2>";
    html += "<div class='form-group'><label for='critical_heap_threshold'>Critical Heap Threshold (bytes)</label>";
    html += "<input type='number' id='critical_heap_threshold' name='critical_heap_threshold' class='form-control' value='" + String(configStorage.getCriticalHeapThreshold()) + "' min='10000' max='1000000'></div>";
    html += "<div class='form-group'><label for='critical_psram_threshold'>Critical PSRAM Threshold (bytes)</label>";
    html += "<input type='number' id='critical_psram_threshold' name='critical_psram_threshold' class='form-control' value='" + String(configStorage.getCriticalPSRAMThreshold()) + "' min='10000' max='10000000'></div></div>";
    html += "</div><div class='card' style='margin-top:1.5rem'>";
    html += "<button type='submit' class='btn btn-primary'>💾 Save Advanced Settings</button></div></form>";
    
    // OTA Firmware Update Section
    html += "<div class='card' style='margin-top:1.5rem'><h2>📦 Firmware Update (OTA)</h2>";
    html += "<p style='color:#94a3b8;margin-bottom:1rem'>Upload new firmware over-the-air using ElegantOTA. The device will automatically restart after a successful update.</p>";
    html += "<p style='color:#94a3b8;margin-bottom:1rem'><strong>Note:</strong> To clear settings after OTA update, use the Factory Reset button before updating, or use the serial command 'F' after the update.</p>";
    html += "<div style='margin-top:1rem'><a href='/update' class='btn btn-primary' style='text-decoration:none;display:inline-block'>🚀 Open OTA Update Page</a></div>";
    html += "</div>";
    
    html += "</div></div>";
    return html;
}

String WebConfig::generateStatusPage() {
    String html = "<div class='main'><div class='container'>";
    html += "<div class='card'><h2>📊 System Status</h2><div id='statusData'>Loading...</div></div></div></div>";
    return html;
}

String WebConfig::generateSerialCommandsPage() {
    String html;
    html.reserve(8000);  // Pre-allocate ~8KB for serial commands reference
    html = "<div class='main'><div class='container'>";
    
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

String WebConfig::generateAPIReferencePage() {
    // Reserve large buffer for API docs to prevent repeated allocations
    String deviceUrl = "http://allskyesp32.lan:8080";
    if (wifiManager.isConnected()) {
        deviceUrl = "http://" + WiFi.localIP().toString() + ":8080";
    }
    
    String html;
    html.reserve(15000);  // Pre-allocate ~15KB for large API documentation page
    html = "<div class='main'><div class='container'>";
    
    // Introduction
    html += "<div class='card'>";
    html += "<h1 style='color:#38bdf8;margin-bottom:1rem'>📚 API Reference</h1>";
    html += "<p style='color:#94a3b8;font-size:1rem;line-height:1.8'>Complete REST API documentation for the ESP32 AllSky Display. ";
    html += "All endpoints return JSON responses and support CORS for cross-origin requests.</p>";
    html += "<div style='background:rgba(14,165,233,0.1);border:1px solid #0ea5e9;border-radius:8px;padding:1rem;margin-top:1rem'>";
    html += "<p style='color:#38bdf8;margin:0'><strong>Base URL:</strong> <code style='background:#1e293b;padding:0.25rem 0.5rem;border-radius:4px;color:#10b981'>" + deviceUrl + "</code></p>";
    html += "</div></div>";
    
    // GET Endpoints Section
    html += "<div class='card'><h2 style='color:#10b981;border-bottom:2px solid #10b981;padding-bottom:0.5rem'>📥 GET Endpoints (Read Data)</h2>";
    
    // GET /api/info
    html += "<div style='margin-top:1.5rem;padding:1rem;background:#0f172a;border-left:4px solid #10b981;border-radius:8px'>";
    html += "<h3 style='color:#38bdf8;margin-bottom:0.5rem'><span style='background:#10b981;color:#000;padding:0.25rem 0.5rem;border-radius:4px;font-size:0.8rem;margin-right:0.5rem'>GET</span>/api/info</h3>";
    html += "<p style='color:#94a3b8;margin-bottom:1rem'>Get comprehensive device information including system status, network details, MQTT configuration, display settings, and all image sources.</p>";
    
    html += "<div style='margin-bottom:1rem'>";
    html += "<p style='color:#64748b;font-weight:bold;margin-bottom:0.5rem'>Request Example:</p>";
    html += "<pre style='background:#1e293b;padding:1rem;border-radius:6px;overflow-x:auto;color:#cbd5e1;margin:0'>";
    html += "curl -X GET " + deviceUrl + "/api/info</pre></div>";
    
    html += "<div style='margin-bottom:1rem'>";
    html += "<p style='color:#64748b;font-weight:bold;margin-bottom:0.5rem'>Response Fields:</p>";
    html += "<ul style='color:#94a3b8;line-height:1.8;list-style-type:none;padding-left:0'>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>firmware</code> - Sketch size, free space, MD5 hash</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>system</code> - Uptime, heap, PSRAM, CPU, flash, chip info, temperature (°C and °F)</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>network</code> - WiFi connection, IP, RSSI, MAC, hostname</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>mqtt</code> - Broker connection status and configuration</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>home_assistant</code> - HA discovery settings</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>display</code> - Resolution, brightness, backlight settings</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>image</code> - Cycling status, current URL, sources array with transformations</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>defaults</code> - Default transformation values</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>advanced</code> - Watchdog, thresholds, intervals</li>";
    html += "</ul></div>";
    
    html += "<div>";
    html += "<p style='color:#64748b;font-weight:bold;margin-bottom:0.5rem'>Response Example (Partial):</p>";
    html += "<pre style='background:#1e293b;padding:1rem;border-radius:6px;overflow-x:auto;color:#cbd5e1;margin:0;font-size:0.85rem'>";
    html += "{\n  \"firmware\": {\n    \"sketch_size\": 2359600,\n    \"free_sketch_space\": 15073296\n  },\n";
    html += "  \"system\": {\n    \"uptime\": 31568,\n    \"free_heap\": 400828,\n    \"cpu_freq\": 360,\n    \"temperature_celsius\": 32.5,\n    \"temperature_fahrenheit\": 90.5,\n    \"chip_model\": \"ESP32-P4\"\n  },\n";
    html += "  \"network\": {\n    \"connected\": true,\n    \"ssid\": \"MyWiFi\",\n    \"ip\": \"192.168.1.100\",\n    \"rssi\": -45\n  },\n";
    html += "  \"image\": {\n    \"cycling_enabled\": true,\n    \"default_image_duration\": 30,\n    \"current_url\": \"http://...\",\n    \"current_index\": 0,\n    \"sources\": [\n      {\n        \"index\": 0,\n        \"url\": \"http://...\",\n        \"enabled\": true,\n        \"active\": true,\n        \"duration\": 30,\n        \"scale_x\": 1.0\n      }\n    ]\n  }\n}</pre>";
    html += "</div></div>";
    
    // GET /status
    html += "<div style='margin-top:1.5rem;padding:1rem;background:#0f172a;border-left:4px solid #10b981;border-radius:8px'>";
    html += "<h3 style='color:#38bdf8;margin-bottom:0.5rem'><span style='background:#10b981;color:#000;padding:0.25rem 0.5rem;border-radius:4px;font-size:0.8rem;margin-right:0.5rem'>GET</span>/status</h3>";
    html += "<p style='color:#94a3b8;margin-bottom:1rem'>Get quick system status summary (lightweight version of /api/info).</p>";
    
    html += "<div style='margin-bottom:1rem'>";
    html += "<p style='color:#64748b;font-weight:bold;margin-bottom:0.5rem'>Request Example:</p>";
    html += "<pre style='background:#1e293b;padding:1rem;border-radius:6px;overflow-x:auto;color:#cbd5e1;margin:0'>";
    html += "curl -X GET " + deviceUrl + "/status</pre></div>";
    
    html += "<div style='margin-bottom:1rem'>";
    html += "<p style='color:#64748b;font-weight:bold;margin-bottom:0.5rem'>Response Fields:</p>";
    html += "<ul style='color:#94a3b8;line-height:1.8;list-style-type:none;padding-left:0'>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>wifi_connected</code> - Boolean WiFi status</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>mqtt_connected</code> - Boolean MQTT status</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>free_heap</code> - Available heap memory in bytes</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>free_psram</code> - Available PSRAM in bytes</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>uptime</code> - Uptime in milliseconds</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>brightness</code> - Current display brightness (0-100)</li>";
    html += "</ul></div></div>";
    
    // GET /api/health
    html += "<div style='margin-top:1.5rem;padding:1rem;background:#0f172a;border-left:4px solid #10b981;border-radius:8px'>";
    html += "<h3 style='color:#38bdf8;margin-bottom:0.5rem'><span style='background:#10b981;color:#000;padding:0.25rem 0.5rem;border-radius:4px;font-size:0.8rem;margin-right:0.5rem'>GET</span>/api/health</h3>";
    html += "<p style='color:#94a3b8;margin-bottom:1rem'>Get comprehensive device health diagnostics with status indicators and actionable recommendations.</p>";
    
    html += "<div style='margin-bottom:1rem'>";
    html += "<p style='color:#64748b;font-weight:bold;margin-bottom:0.5rem'>Request Example:</p>";
    html += "<pre style='background:#1e293b;padding:1rem;border-radius:6px;overflow-x:auto;color:#cbd5e1;margin:0'>";
    html += "curl -X GET " + escapeHtml(deviceUrl) + "/api/health</pre></div>";
    
    html += "<div style='margin-bottom:1rem'>";
    html += "<p style='color:#64748b;font-weight:bold;margin-bottom:0.5rem'>Response Fields:</p>";
    html += "<ul style='color:#94a3b8;line-height:1.8;list-style-type:none;padding-left:0'>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>overall</code> - Overall health status (EXCELLENT, GOOD, WARNING, CRITICAL, FAILING)</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>memory</code> - Heap/PSRAM usage, fragmentation analysis</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>network</code> - WiFi signal quality, disconnect count</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>mqtt</code> - MQTT connection stability, reconnect count</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>system</code> - Boot count, crash detection, temperature, watchdog resets</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>display</code> - Display health and brightness</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>recommendations</code> - Array of actionable suggestions to improve device health</li>";
    html += "</ul></div>";
    
    html += "<div>";
    html += "<p style='color:#64748b;font-weight:bold;margin-bottom:0.5rem'>Response Example (Partial):</p>";
    html += "<pre style='background:#1e293b;padding:1rem;border-radius:6px;overflow-x:auto;color:#cbd5e1;margin:0;font-size:0.85rem'>";
    html += "{\n  \"overall\": {\n    \"status\": \"GOOD\",\n    \"message\": \"Device is functional with minor issues\",\n";
    html += "    \"critical_issues\": 0,\n    \"warnings\": 1\n  },\n";
    html += "  \"memory\": {\n    \"status\": \"EXCELLENT\",\n    \"free_heap\": 400000,\n    \"heap_usage_percent\": 45.2,\n";
    html += "    \"free_psram\": 15000000,\n    \"psram_usage_percent\": 52.3\n  },\n";
    html += "  \"network\": {\n    \"status\": \"GOOD\",\n    \"rssi\": -68,\n    \"disconnect_count\": 2\n  },\n";
    html += "  \"recommendations\": [\n    \"Improve WiFi signal by relocating device or access point\"\n  ]\n}</pre>";
    html += "</div></div>";
    
    html += "</div>"; // End GET endpoints card
    
    // POST Endpoints Section
    html += "<div class='card'><h2 style='color:#f59e0b;border-bottom:2px solid #f59e0b;padding-bottom:0.5rem'>📤 POST Endpoints (Modify Settings)</h2>";
    
    // POST /api/save
    html += "<div style='margin-top:1.5rem;padding:1rem;background:#0f172a;border-left:4px solid #f59e0b;border-radius:8px'>";
    html += "<h3 style='color:#38bdf8;margin-bottom:0.5rem'><span style='background:#f59e0b;color:#000;padding:0.25rem 0.5rem;border-radius:4px;font-size:0.8rem;margin-right:0.5rem'>POST</span>/api/save</h3>";
    html += "<p style='color:#94a3b8;margin-bottom:1rem'>Save device configuration. Send form data with any combination of settings. Changes take effect immediately.</p>";
    
    html += "<div style='margin-bottom:1rem'>";
    html += "<p style='color:#64748b;font-weight:bold;margin-bottom:0.5rem'>Accepted Parameters:</p>";
    html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:0.5rem;font-size:0.9rem'>";
    html += "<div style='background:#1e293b;padding:0.75rem;border-radius:6px'><strong style='color:#38bdf8'>Network:</strong><br><code>wifi_ssid</code>, <code>wifi_password</code></div>";
    html += "<div style='background:#1e293b;padding:0.75rem;border-radius:6px'><strong style='color:#38bdf8'>MQTT:</strong><br><code>mqtt_server</code>, <code>mqtt_port</code>, <code>mqtt_user</code>, <code>mqtt_password</code>, <code>mqtt_client_id</code></div>";
    html += "<div style='background:#1e293b;padding:0.75rem;border-radius:6px'><strong style='color:#38bdf8'>Display:</strong><br><code>default_brightness</code>, <code>brightness_auto_mode</code></div>";
    html += "<div style='background:#1e293b;padding:0.75rem;border-radius:6px'><strong style='color:#38bdf8'>Image:</strong><br><code>image_url</code>, <code>update_interval</code></div>";
    html += "<div style='background:#1e293b;padding:0.75rem;border-radius:6px'><strong style='color:#38bdf8'>Cycling:</strong><br><code>cycling_enabled</code>, <code>cycle_interval</code>, <code>default_image_duration</code>, <code>random_order</code></div>";
    html += "<div style='background:#1e293b;padding:0.75rem;border-radius:6px'><strong style='color:#38bdf8'>Transform:</strong><br><code>default_scale_x</code>, <code>default_scale_y</code>, <code>default_offset_x</code>, <code>default_offset_y</code>, <code>default_rotation</code></div>";
    html += "</div></div>";
    
    html += "<div style='margin-bottom:1rem'>";
    html += "<p style='color:#64748b;font-weight:bold;margin-bottom:0.5rem'>Request Example:</p>";
    html += "<pre style='background:#1e293b;padding:1rem;border-radius:6px;overflow-x:auto;color:#cbd5e1;margin:0;font-size:0.85rem'>";
    html += "curl -X POST " + deviceUrl + "/api/save \\\n  -d \"default_brightness=80\" \\\n  -d \"cycling_enabled=true\" \\\n  -d \"default_image_duration=30\"</pre></div>";
    
    html += "<div>";
    html += "<p style='color:#64748b;font-weight:bold;margin-bottom:0.5rem'>Response Example:</p>";
    html += "<pre style='background:#1e293b;padding:1rem;border-radius:6px;overflow-x:auto;color:#cbd5e1;margin:0'>";
    html += "{\"status\":\"success\",\"message\":\"Configuration saved successfully\"}</pre>";
    html += "</div></div>";
    
    // POST /api/add-source
    html += "<div style='margin-top:1.5rem;padding:1rem;background:#0f172a;border-left:4px solid #f59e0b;border-radius:8px'>";
    html += "<h3 style='color:#38bdf8;margin-bottom:0.5rem'><span style='background:#f59e0b;color:#000;padding:0.25rem 0.5rem;border-radius:4px;font-size:0.8rem;margin-right:0.5rem'>POST</span>/api/add-source</h3>";
    html += "<p style='color:#94a3b8;margin-bottom:1rem'>Add a new image source to the cycling list.</p>";
    html += "<div style='margin-bottom:1rem'>";
    html += "<p style='color:#64748b;font-weight:bold;margin-bottom:0.5rem'>Parameters:</p>";
    html += "<ul style='color:#94a3b8;line-height:1.8;list-style-type:none;padding-left:0'>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>url</code> (required) - Full URL of the image to add</li></ul></div>";
    html += "<pre style='background:#1e293b;padding:1rem;border-radius:6px;overflow-x:auto;color:#cbd5e1;margin:0;font-size:0.85rem'>";
    html += "curl -X POST " + deviceUrl + "/api/add-source \\\n  -d \"url=http://example.com/allsky.jpg\"</pre></div>";
    
    // POST /api/remove-source
    html += "<div style='margin-top:1.5rem;padding:1rem;background:#0f172a;border-left:4px solid #f59e0b;border-radius:8px'>";
    html += "<h3 style='color:#38bdf8;margin-bottom:0.5rem'><span style='background:#f59e0b;color:#000;padding:0.25rem 0.5rem;border-radius:4px;font-size:0.8rem;margin-right:0.5rem'>POST</span>/api/remove-source</h3>";
    html += "<p style='color:#94a3b8;margin-bottom:1rem'>Remove an image source from the cycling list by index.</p>";
    html += "<div style='margin-bottom:1rem'>";
    html += "<p style='color:#64748b;font-weight:bold;margin-bottom:0.5rem'>Parameters:</p>";
    html += "<ul style='color:#94a3b8;line-height:1.8;list-style-type:none;padding-left:0'>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>index</code> (required) - Zero-based index of the source to remove</li></ul></div>";
    html += "<pre style='background:#1e293b;padding:1rem;border-radius:6px;overflow-x:auto;color:#cbd5e1;margin:0;font-size:0.85rem'>";
    html += "curl -X POST " + deviceUrl + "/api/remove-source -d \"index=0\"</pre></div>";
    
    // POST /api/update-source
    html += "<div style='margin-top:1.5rem;padding:1rem;background:#0f172a;border-left:4px solid #f59e0b;border-radius:8px'>";
    html += "<h3 style='color:#38bdf8;margin-bottom:0.5rem'><span style='background:#f59e0b;color:#000;padding:0.25rem 0.5rem;border-radius:4px;font-size:0.8rem;margin-right:0.5rem'>POST</span>/api/update-source</h3>";
    html += "<p style='color:#94a3b8;margin-bottom:1rem'>Update the URL of an existing image source.</p>";
    html += "<div style='margin-bottom:1rem'>";
    html += "<p style='color:#64748b;font-weight:bold;margin-bottom:0.5rem'>Parameters:</p>";
    html += "<ul style='color:#94a3b8;line-height:1.8;list-style-type:none;padding-left:0'>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>index</code> (required) - Zero-based index of the source</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>url</code> (required) - New URL for the source</li></ul></div>";
    html += "<pre style='background:#1e293b;padding:1rem;border-radius:6px;overflow-x:auto;color:#cbd5e1;margin:0;font-size:0.85rem'>";
    html += "curl -X POST " + deviceUrl + "/api/update-source \\\n  -d \"index=0\" \\\n  -d \"url=http://new-url.com/image.jpg\"</pre></div>";
    
    // POST /api/clear-sources
    html += "<div style='margin-top:1.5rem;padding:1rem;background:#0f172a;border-left:4px solid #f59e0b;border-radius:8px'>";
    html += "<h3 style='color:#38bdf8;margin-bottom:0.5rem'><span style='background:#f59e0b;color:#000;padding:0.25rem 0.5rem;border-radius:4px;font-size:0.8rem;margin-right:0.5rem'>POST</span>/api/clear-sources</h3>";
    html += "<p style='color:#94a3b8;margin-bottom:1rem'>Remove all image sources from the cycling list.</p>";
    html += "<pre style='background:#1e293b;padding:1rem;border-radius:6px;overflow-x:auto;color:#cbd5e1;margin:0;font-size:0.85rem'>";
    html += "curl -X POST " + deviceUrl + "/api/clear-sources</pre></div>";
    
    // POST /api/bulk-delete-sources
    html += "<div style='margin-top:1.5rem;padding:1rem;background:#0f172a;border-left:4px solid #f59e0b;border-radius:8px'>";
    html += "<h3 style='color:#38bdf8;margin-bottom:0.5rem'><span style='background:#f59e0b;color:#000;padding:0.25rem 0.5rem;border-radius:4px;font-size:0.8rem;margin-right:0.5rem'>POST</span>/api/bulk-delete-sources</h3>";
    html += "<p style='color:#94a3b8;margin-bottom:1rem'>Delete multiple image sources at once by providing an array of indices. At least one source must remain.</p>";
    html += "<div style='margin-bottom:1rem'>";
    html += "<p style='color:#64748b;font-weight:bold;margin-bottom:0.5rem'>Parameters:</p>";
    html += "<ul style='color:#94a3b8;line-height:1.8;list-style-type:none;padding-left:0'>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>indices</code> (required) - JSON array of zero-based indices to delete (e.g., \"[0,2,4]\")</li></ul></div>";
    html += "<div>";
    html += "<p style='color:#64748b;font-weight:bold;margin-bottom:0.5rem'>Example Request:</p>";
    html += "<pre style='background:#1e293b;padding:1rem;border-radius:6px;overflow-x:auto;color:#cbd5e1;margin:0;font-size:0.85rem'>";
    html += "curl -X POST " + deviceUrl + "/api/bulk-delete-sources \\\n  -d 'indices=[0,2,4]'</pre></div>";
    html += "<div style='margin-top:1rem'>";
    html += "<p style='color:#64748b;font-weight:bold;margin-bottom:0.5rem'>Response Example:</p>";
    html += "<pre style='background:#1e293b;padding:1rem;border-radius:6px;overflow-x:auto;color:#cbd5e1;margin:0'>";
    html += "{\"status\":\"success\",\"message\":\"Successfully deleted 3 of 3 source(s)\",\"deleted\":3,\"remaining\":5}</pre>";
    html += "</div></div>";
    
    // POST /api/next-image
    html += "<div style='margin-top:1.5rem;padding:1rem;background:#0f172a;border-left:4px solid #f59e0b;border-radius:8px'>";
    html += "<h3 style='color:#38bdf8;margin-bottom:0.5rem'><span style='background:#f59e0b;color:#000;padding:0.25rem 0.5rem;border-radius:4px;font-size:0.8rem;margin-right:0.5rem'>POST</span>/api/next-image</h3>";
    html += "<p style='color:#94a3b8;margin-bottom:1rem'>Manually trigger switching to the next image in cycling mode.</p>";
    html += "<pre style='background:#1e293b;padding:1rem;border-radius:6px;overflow-x:auto;color:#cbd5e1;margin:0;font-size:0.85rem'>";
    html += "curl -X POST " + deviceUrl + "/api/next-image</pre></div>";
    
    // POST /api/update-transform
    html += "<div style='margin-top:1.5rem;padding:1rem;background:#0f172a;border-left:4px solid #f59e0b;border-radius:8px'>";
    html += "<h3 style='color:#38bdf8;margin-bottom:0.5rem'><span style='background:#f59e0b;color:#000;padding:0.25rem 0.5rem;border-radius:4px;font-size:0.8rem;margin-right:0.5rem'>POST</span>/api/update-transform</h3>";
    html += "<p style='color:#94a3b8;margin-bottom:1rem'>Update transformation settings for a specific image source.</p>";
    html += "<div style='margin-bottom:1rem'>";
    html += "<p style='color:#64748b;font-weight:bold;margin-bottom:0.5rem'>Parameters:</p>";
    html += "<ul style='color:#94a3b8;line-height:1.8;list-style-type:none;padding-left:0'>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>index</code> (required) - Image source index</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>scale_x</code> (optional) - Horizontal scale factor</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>scale_y</code> (optional) - Vertical scale factor</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>offset_x</code> (optional) - Horizontal offset in pixels</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>offset_y</code> (optional) - Vertical offset in pixels</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>rotation</code> (optional) - Rotation angle in degrees</li>";
    html += "</ul></div>";
    html += "<pre style='background:#1e293b;padding:1rem;border-radius:6px;overflow-x:auto;color:#cbd5e1;margin:0;font-size:0.85rem'>";
    html += "curl -X POST " + deviceUrl + "/api/update-transform \\\n  -d \"index=0\" \\\n  -d \"scale_x=1.2\" \\\n  -d \"scale_y=1.2\" \\\n  -d \"offset_x=10\" \\\n  -d \"offset_y=20\" \\\n  -d \"rotation=45\"</pre></div>";
    
    // POST /api/copy-defaults
    html += "<div style='margin-top:1.5rem;padding:1rem;background:#0f172a;border-left:4px solid #f59e0b;border-radius:8px'>";
    html += "<h3 style='color:#38bdf8;margin-bottom:0.5rem'><span style='background:#f59e0b;color:#000;padding:0.25rem 0.5rem;border-radius:4px;font-size:0.8rem;margin-right:0.5rem'>POST</span>/api/copy-defaults</h3>";
    html += "<p style='color:#94a3b8;margin-bottom:1rem'>Copy default transformation settings to a specific image source.</p>";
    html += "<div style='margin-bottom:1rem'>";
    html += "<p style='color:#64748b;font-weight:bold;margin-bottom:0.5rem'>Parameters:</p>";
    html += "<ul style='color:#94a3b8;line-height:1.8;list-style-type:none;padding-left:0'>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>index</code> (required) - Image source index to update</li></ul></div>";
    html += "<pre style='background:#1e293b;padding:1rem;border-radius:6px;overflow-x:auto;color:#cbd5e1;margin:0;font-size:0.85rem'>";
    html += "curl -X POST " + deviceUrl + "/api/copy-defaults -d \"index=0\"</pre></div>";
    
    // POST /api/toggle-image-enabled
    html += "<div style='margin-top:1.5rem;padding:1rem;background:#0f172a;border-left:4px solid #f59e0b;border-radius:8px'>";
    html += "<h3 style='color:#38bdf8;margin-bottom:0.5rem'><span style='background:#f59e0b;color:#000;padding:0.25rem 0.5rem;border-radius:4px;font-size:0.8rem;margin-right:0.5rem'>POST</span>/api/toggle-image-enabled</h3>";
    html += "<p style='color:#94a3b8;margin-bottom:1rem'>Enable or disable an image in the cycling order. Disabled images are skipped during auto-cycling.</p>";
    html += "<div style='margin-bottom:1rem'>";
    html += "<p style='color:#64748b;font-weight:bold;margin-bottom:0.5rem'>Parameters:</p>";
    html += "<ul style='color:#94a3b8;line-height:1.8;list-style-type:none;padding-left:0'>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>index</code> (required) - Image source index to toggle</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>enabled</code> (required) - 'true' or 'false'</li></ul></div>";
    html += "<pre style='background:#1e293b;padding:1rem;border-radius:6px;overflow-x:auto;color:#cbd5e1;margin:0;font-size:0.85rem'>";
    html += "curl -X POST " + deviceUrl + "/api/toggle-image-enabled \\\n  -d \"index=0\" \\\n  -d \"enabled=false\"</pre></div>";
    
    // POST /api/update-image-duration
    html += "<div style='margin-top:1.5rem;padding:1rem;background:#0f172a;border-left:4px solid #f59e0b;border-radius:8px'>";
    html += "<h3 style='color:#38bdf8;margin-bottom:0.5rem'><span style='background:#f59e0b;color:#000;padding:0.25rem 0.5rem;border-radius:4px;font-size:0.8rem;margin-right:0.5rem'>POST</span>/api/update-image-duration</h3>";
    html += "<p style='color:#94a3b8;margin-bottom:1rem'>Set the display duration for a specific image (how long it shows before switching to next).</p>";
    html += "<div style='margin-bottom:1rem'>";
    html += "<p style='color:#64748b;font-weight:bold;margin-bottom:0.5rem'>Parameters:</p>";
    html += "<ul style='color:#94a3b8;line-height:1.8;list-style-type:none;padding-left:0'>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>index</code> (required) - Image source index</li>";
    html += "<li style='padding:0.5rem;background:#1e293b;border-radius:6px;margin-bottom:0.5rem'><code style='color:#10b981;font-weight:bold'>duration</code> (required) - Display duration in seconds (5-3600)</li></ul></div>";
    html += "<pre style='background:#1e293b;padding:1rem;border-radius:6px;overflow-x:auto;color:#cbd5e1;margin:0;font-size:0.85rem'>";
    html += "curl -X POST " + deviceUrl + "/api/update-image-duration \\\n  -d \"index=0\" \\\n  -d \"duration=60\"</pre></div>";
    
    // POST /api/apply-transform
    html += "<div style='margin-top:1.5rem;padding:1rem;background:#0f172a;border-left:4px solid #f59e0b;border-radius:8px'>";
    html += "<h3 style='color:#38bdf8;margin-bottom:0.5rem'><span style='background:#f59e0b;color:#000;padding:0.25rem 0.5rem;border-radius:4px;font-size:0.8rem;margin-right:0.5rem'>POST</span>/api/apply-transform</h3>";
    html += "<p style='color:#94a3b8;margin-bottom:1rem'>Apply transformation settings and re-render the current image immediately.</p>";
    html += "<pre style='background:#1e293b;padding:1rem;border-radius:6px;overflow-x:auto;color:#cbd5e1;margin:0;font-size:0.85rem'>";
    html += "curl -X POST " + deviceUrl + "/api/apply-transform</pre></div>";
    
    // POST /api/restart
    html += "<div style='margin-top:1.5rem;padding:1rem;background:#0f172a;border-left:4px solid #ef4444;border-radius:8px'>";
    html += "<h3 style='color:#38bdf8;margin-bottom:0.5rem'><span style='background:#ef4444;color:#fff;padding:0.25rem 0.5rem;border-radius:4px;font-size:0.8rem;margin-right:0.5rem'>POST</span>/api/restart</h3>";
    html += "<p style='color:#94a3b8;margin-bottom:1rem'>⚠️ Restart the ESP32 device. Connection will be lost temporarily.</p>";
    html += "<pre style='background:#1e293b;padding:1rem;border-radius:6px;overflow-x:auto;color:#cbd5e1;margin:0;font-size:0.85rem'>";
    html += "curl -X POST " + deviceUrl + "/api/restart</pre></div>";
    
    // POST /api/factory-reset
    html += "<div style='margin-top:1.5rem;padding:1rem;background:#0f172a;border-left:4px solid #ef4444;border-radius:8px'>";
    html += "<h3 style='color:#38bdf8;margin-bottom:0.5rem'><span style='background:#ef4444;color:#fff;padding:0.25rem 0.5rem;border-radius:4px;font-size:0.8rem;margin-right:0.5rem'>POST</span>/api/factory-reset</h3>";
    html += "<p style='color:#94a3b8;margin-bottom:1rem'>⚠️ <strong>DANGER:</strong> Reset all settings to factory defaults. This will erase all configuration!</p>";
    html += "<pre style='background:#1e293b;padding:1rem;border-radius:6px;overflow-x:auto;color:#cbd5e1;margin:0;font-size:0.85rem'>";
    html += "curl -X POST " + deviceUrl + "/api/factory-reset</pre></div>";
    
    html += "</div>"; // End POST endpoints card
    
    // MQTT API Section
    html += "<div class='card'><h2 style='color:#0ea5e9;border-bottom:2px solid #0ea5e9;padding-bottom:0.5rem'>🔗 MQTT API</h2>";
    html += "<p style='color:#94a3b8;margin-bottom:1rem'>Control the device via MQTT messages. All topics are prefixed with the configured state topic (default: <code>allsky_display</code>).</p>";
    
    html += "<div style='background:#0f172a;padding:1rem;border-radius:8px;margin-bottom:1rem'>";
    html += "<h3 style='color:#38bdf8;margin-bottom:0.5rem'>Command Topics</h3>";
    html += "<table style='width:100%;border-collapse:collapse'>";
    html += "<thead><tr style='background:#1e293b;border-bottom:2px solid #334155'>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Topic</th>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Payload</th>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Description</th></tr></thead><tbody>";
    
    html += "<tr style='border-bottom:1px solid #334155'>";
    html += "<td style='padding:0.75rem;color:#10b981'><code>PREFIX/brightness/set</code></td>";
    html += "<td style='padding:0.75rem'><code>0-100</code></td>";
    html += "<td style='padding:0.75rem;color:#94a3b8'>Set display brightness</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'>";
    html += "<td style='padding:0.75rem;color:#10b981'><code>PREFIX/cycling/set</code></td>";
    html += "<td style='padding:0.75rem'><code>ON/OFF</code></td>";
    html += "<td style='padding:0.75rem;color:#94a3b8'>Enable/disable image cycling</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'>";
    html += "<td style='padding:0.75rem;color:#10b981'><code>PREFIX/next</code></td>";
    html += "<td style='padding:0.75rem'><code>any</code></td>";
    html += "<td style='padding:0.75rem;color:#94a3b8'>Switch to next image</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'>";
    html += "<td style='padding:0.75rem;color:#10b981'><code>PREFIX/refresh</code></td>";
    html += "<td style='padding:0.75rem'><code>any</code></td>";
    html += "<td style='padding:0.75rem;color:#94a3b8'>Force refresh current image</td></tr>";
    
    html += "</tbody></table></div>";
    
    html += "<div style='background:#0f172a;padding:1rem;border-radius:8px'>";
    html += "<h3 style='color:#38bdf8;margin-bottom:0.5rem'>State Topics (Published by Device)</h3>";
    html += "<table style='width:100%;border-collapse:collapse'>";
    html += "<thead><tr style='background:#1e293b;border-bottom:2px solid #334155'>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Topic</th>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Payload Type</th>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Description</th></tr></thead><tbody>";
    
    html += "<tr style='border-bottom:1px solid #334155'>";
    html += "<td style='padding:0.75rem;color:#10b981'><code>PREFIX/brightness</code></td>";
    html += "<td style='padding:0.75rem'><code>number</code></td>";
    html += "<td style='padding:0.75rem;color:#94a3b8'>Current brightness value</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'>";
    html += "<td style='padding:0.75rem;color:#10b981'><code>PREFIX/cycling</code></td>";
    html += "<td style='padding:0.75rem'><code>ON/OFF</code></td>";
    html += "<td style='padding:0.75rem;color:#94a3b8'>Cycling mode status</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'>";
    html += "<td style='padding:0.75rem;color:#10b981'><code>PREFIX/sensor/heap</code></td>";
    html += "<td style='padding:0.75rem'><code>number</code></td>";
    html += "<td style='padding:0.75rem;color:#94a3b8'>Free heap memory (bytes)</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'>";
    html += "<td style='padding:0.75rem;color:#10b981'><code>PREFIX/sensor/psram</code></td>";
    html += "<td style='padding:0.75rem'><code>number</code></td>";
    html += "<td style='padding:0.75rem;color:#94a3b8'>Free PSRAM (bytes)</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'>";
    html += "<td style='padding:0.75rem;color:#10b981'><code>PREFIX/sensor/wifi_signal</code></td>";
    html += "<td style='padding:0.75rem'><code>number</code></td>";
    html += "<td style='padding:0.75rem;color:#94a3b8'>WiFi signal strength (dBm)</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'>";
    html += "<td style='padding:0.75rem;color:#10b981'><code>PREFIX/sensor/uptime</code></td>";
    html += "<td style='padding:0.75rem'><code>number</code></td>";
    html += "<td style='padding:0.75rem;color:#94a3b8'>Device uptime (seconds)</td></tr>";
    
    html += "</tbody></table></div></div>";
    
    // Usage Examples
    html += "<div class='card'><h2 style='color:#a855f7;border-bottom:2px solid #a855f7;padding-bottom:0.5rem'>💡 Usage Examples</h2>";
    
    html += "<div style='margin-top:1rem;padding:1rem;background:#0f172a;border-radius:8px'>";
    html += "<h3 style='color:#38bdf8;margin-bottom:0.75rem'>Python Example</h3>";
    html += "<pre style='background:#1e293b;padding:1rem;border-radius:6px;overflow-x:auto;color:#cbd5e1;margin:0;font-size:0.85rem'>";
    html += "import requests\n\n# Get all device info\nresponse = requests.get('" + deviceUrl + "/api/info')\ndata = response.json()\nprint(f\"Uptime: {data['system']['uptime']}ms\")\n\n";
    html += "# Set brightness\nrequests.post('" + deviceUrl + "/api/save',\n              data={'default_brightness': 75})\n\n";
    html += "# Add image source\nrequests.post('" + deviceUrl + "/api/add-source',\n              data={'url': 'http://example.com/sky.jpg'})</pre></div>";
    
    html += "<div style='margin-top:1rem;padding:1rem;background:#0f172a;border-radius:8px'>";
    html += "<h3 style='color:#38bdf8;margin-bottom:0.75rem'>JavaScript Example</h3>";
    html += "<pre style='background:#1e293b;padding:1rem;border-radius:6px;overflow-x:auto;color:#cbd5e1;margin:0;font-size:0.85rem'>";
    html += "// Get device info\nfetch('" + deviceUrl + "/api/info')\n  .then(res => res.json())\n  .then(data => {\n    console.log('Free Heap:', data.system.free_heap);\n    console.log('IP Address:', data.network.ip);\n  });\n\n";
    html += "// Trigger next image\nfetch('" + deviceUrl + "/api/next-image', {method: 'POST'})\n  .then(res => res.json())\n  .then(data => console.log(data.message));</pre></div>";
    
    html += "<div style='margin-top:1rem;padding:1rem;background:#0f172a;border-radius:8px'>";
    html += "<h3 style='color:#38bdf8;margin-bottom:0.75rem'>Home Assistant Automation Example</h3>";
    html += "<pre style='background:#1e293b;padding:1rem;border-radius:6px;overflow-x:auto;color:#cbd5e1;margin:0;font-size:0.85rem'>";
    html += "automation:\n  - alias: \"Set AllSky Brightness at Night\"\n    trigger:\n      - platform: sun\n        event: sunset\n    action:\n      - service: rest_command.allsky_brightness\n        data:\n          brightness: 30\n\n";
    html += "rest_command:\n  allsky_brightness:\n    url: " + deviceUrl + "/api/save\n    method: POST\n    payload: \"default_brightness={{ brightness }}\"</pre></div>";
    
    html += "</div>";
    
    // Response Codes
    html += "<div class='card'><h2 style='color:#64748b;border-bottom:2px solid #64748b;padding-bottom:0.5rem'>📋 HTTP Response Codes</h2>";
    html += "<table style='width:100%;border-collapse:collapse;margin-top:1rem'>";
    html += "<thead><tr style='background:#1e293b;border-bottom:2px solid #334155'>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Code</th>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Meaning</th>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Description</th></tr></thead><tbody>";
    
    html += "<tr style='border-bottom:1px solid #334155'>";
    html += "<td style='padding:0.75rem;color:#10b981'><strong>200</strong></td>";
    html += "<td style='padding:0.75rem'>OK</td>";
    html += "<td style='padding:0.75rem;color:#94a3b8'>Request successful</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'>";
    html += "<td style='padding:0.75rem;color:#f59e0b'><strong>400</strong></td>";
    html += "<td style='padding:0.75rem'>Bad Request</td>";
    html += "<td style='padding:0.75rem;color:#94a3b8'>Invalid parameters or missing required fields</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'>";
    html += "<td style='padding:0.75rem;color:#ef4444'><strong>404</strong></td>";
    html += "<td style='padding:0.75rem'>Not Found</td>";
    html += "<td style='padding:0.75rem;color:#94a3b8'>Endpoint does not exist</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'>";
    html += "<td style='padding:0.75rem;color:#ef4444'><strong>500</strong></td>";
    html += "<td style='padding:0.75rem'>Internal Server Error</td>";
    html += "<td style='padding:0.75rem;color:#94a3b8'>Server encountered an error processing the request</td></tr>";
    
    html += "</tbody></table></div>";
    
    // Notes
    html += "<div class='card' style='background:rgba(14,165,233,0.1);border:2px solid #0ea5e9'>";
    html += "<h2 style='color:#38bdf8;margin-bottom:1rem'>📝 Important Notes</h2>";
    html += "<ul style='color:#94a3b8;line-height:2;margin-left:1.5rem'>";
    html += "<li>All POST endpoints use <code>application/x-www-form-urlencoded</code> content type</li>";
    html += "<li>Configuration changes via <code>/api/save</code> are persisted to flash memory</li>";
    html += "<li>Brightness changes via <code>/api/save</code> apply immediately without restart</li>";
    html += "<li>Network and MQTT settings require a restart to take effect</li>";
    html += "<li>Maximum image size supported: <strong>724x724 pixels</strong> (1MB buffer)</li>";
    html += "<li>Image transformations are per-source when cycling is enabled</li>";
    html += "<li>MQTT topics depend on your configured state topic prefix</li>";
    html += "<li>Home Assistant Discovery creates entities automatically when enabled</li>";
    html += "</ul></div>";
    
    html += "</div></div>";
    return html;
}

String WebConfig::generateConsolePage() {
    String html;
    html.reserve(10000);  // Pre-allocate ~10KB for console page with WebSocket scripts
    html = "<div class='main'><div class='container'>";
    
    // Two-column layout: Controls on left, Console on right
    html += "<div style='display:grid;grid-template-columns:200px 1fr;gap:1.5rem;align-items:start'>";
    
    // Left column: Controls and info
    html += "<div style='display:flex;flex-direction:column;gap:0.75rem'>";
    
    // Connection controls card
    html += "<div class='card' style='padding:1rem'>";
    html += "<h3 style='margin:0 0 0.75rem 0;color:#38bdf8;font-size:1rem'>Connection</h3>";
    html += "<button class='btn btn-success' onclick='connectConsole()' id='connectBtn' style='width:100%;margin-bottom:0.5rem'><i class='fas fa-plug' style='margin-right:0.5rem'></i>Connect</button>";
    html += "<button class='btn btn-danger' onclick='disconnectConsole()' id='disconnectBtn' disabled style='width:100%'><i class='fas fa-times' style='margin-right:0.5rem'></i>Disconnect</button>";
    html += "</div>";
    
    // Console controls card
    html += "<div class='card' style='padding:1rem'>";
    html += "<h3 style='margin:0 0 0.75rem 0;color:#38bdf8;font-size:1rem'>Console Actions</h3>";
    html += "<button class='btn btn-secondary' onclick='clearConsole()' style='width:100%;margin-bottom:0.5rem'><i class='fas fa-eraser' style='margin-right:0.5rem'></i>Clear Display</button>";
    html += "<button class='btn btn-secondary' onclick='toggleAutoscroll()' id='autoscrollBtn' style='width:100%;margin-bottom:0.5rem'><i class='fas fa-arrow-down' style='margin-right:0.5rem'></i>Auto-scroll: ON</button>";
    html += "<button class='btn btn-secondary' onclick='downloadLogs()' style='width:100%;margin-bottom:0.5rem'><i class='fas fa-download' style='margin-right:0.5rem'></i>Download Logs</button>";
    html += "<button class='btn btn-warning' onclick='clearCrashLogs()' title='Clear buffered crash logs from device' style='width:100%'><i class='fas fa-trash-alt' style='margin-right:0.5rem'></i>Clear Device Logs</button>";
    html += "</div>";
    
    // Filter card
    html += "<div class='card' style='padding:1rem'>";
    html += "<h3 style='margin:0 0 0.75rem 0;color:#38bdf8;font-size:1rem'>Filter</h3>";
    html += "<label for='severityFilter' style='color:#94a3b8;font-size:0.9rem;display:block;margin-bottom:0.5rem'><i class='fas fa-filter' style='margin-right:0.5rem'></i>Min Severity:</label>";
    html += "<select id='severityFilter' onchange='updateSeverityFilter()' style='width:100%;background:#1e293b;color:#e2e8f0;border:1px solid #334155;border-radius:6px;padding:0.5rem;font-size:0.9rem;cursor:pointer'>";
    
    int currentSeverity = configStorage.getMinLogSeverity();
    html += "<option value='0'" + String(currentSeverity == 0 ? " selected" : "") + ">DEBUG</option>";
    html += "<option value='1'" + String(currentSeverity == 1 ? " selected" : "") + ">INFO</option>";
    html += "<option value='2'" + String(currentSeverity == 2 ? " selected" : "") + ">WARNING</option>";
    html += "<option value='3'" + String(currentSeverity == 3 ? " selected" : "") + ">ERROR</option>";
    html += "<option value='4'" + String(currentSeverity == 4 ? " selected" : "") + ">CRITICAL</option>";
    html += "</select>";
    html += "</div>";
    
    html += "</div>"; // End left column
    
    // Right column: Serial Console
    html += "<div class='card' style='padding:1rem'>";
    
    // Title row with status and message count
    html += "<div style='display:flex;align-items:center;justify-content:space-between;margin-bottom:0.5rem'>";
    html += "<h2 style='margin:0'>🖥️ Serial Console</h2>";
    html += "<div style='display:flex;align-items:center;gap:1rem'>";
    html += "<span style='color:#64748b;font-size:0.8rem' id='wsStats'>0 messages</span>";
    html += "<div style='display:flex;align-items:center;gap:0.5rem'>";
    html += "<span id='wsStatus' class='status-indicator status-offline'></span>";
    html += "<span id='wsStatusText' style='color:#94a3b8;font-size:0.85rem'>Disconnected</span>";
    html += "</div>";
    html += "</div>";
    html += "</div>";
    
    // Console output area
    html += "<div id='consoleOutput' style='";
    html += "background:#0f172a;";
    html += "border:1px solid #334155;";
    html += "border-radius:8px;";
    html += "padding:0.5rem;";
    html += "height:calc(100vh - 300px);";
    html += "min-height:250px;";
    html += "overflow-y:auto;";
    html += "font-family:\"Courier New\",monospace;";
    html += "font-size:0.85rem;";
    html += "line-height:1.5;";
    html += "color:#e2e8f0;";
    html += "white-space:pre-wrap;";
    html += "word-wrap:break-word;";
    html += "'></div>";
    
    html += "</div>"; // End card
    html += "</div>"; // End grid
    html += "</div></div>";
    
    // WebSocket JavaScript
    html += "<script>";
    html += "let ws = null;";
    html += "let messageCount = 0;";
    html += "let autoscroll = true;";
    html += "let reconnectAttempts = 0;";
    html += "let reconnectTimer = null;";
    html += "let manualDisconnect = false;";
    html += "const MAX_MESSAGES = 1000;";
    html += "const consoleOutput = document.getElementById('consoleOutput');";
    html += "const wsStatus = document.getElementById('wsStatus');";
    html += "const wsStatusText = document.getElementById('wsStatusText');";
    html += "const wsStats = document.getElementById('wsStats');";
    html += "const connectBtn = document.getElementById('connectBtn');";
    html += "const disconnectBtn = document.getElementById('disconnectBtn');";
    html += "const autoscrollBtn = document.getElementById('autoscrollBtn');";
    
    html += "function connectConsole() {";
    html += "  if (ws && ws.readyState === WebSocket.OPEN) return;";
    html += "  manualDisconnect = false;";
    html += "  if (reconnectTimer) clearTimeout(reconnectTimer);";
    html += "  const wsUrl = 'ws://' + window.location.hostname + ':81';";
    html += "  consoleOutput.textContent += '[CLIENT] Connecting to ' + wsUrl + '...\\n';";
    html += "  ws = new WebSocket(wsUrl);";
    html += "  ws.onopen = function() {";
    html += "    wsStatus.className = 'status-indicator status-online';";
    html += "    wsStatusText.textContent = 'Connected';";
    html += "    connectBtn.disabled = true;";
    html += "    disconnectBtn.disabled = false;";
    html += "    consoleOutput.textContent += '[CLIENT] Connected successfully\\n';";
    html += "    if (autoscroll) consoleOutput.scrollTop = consoleOutput.scrollHeight;";
    html += "  };";
    html += "  ws.onmessage = function(event) {";
    html += "    messageCount++;";
    html += "    reconnectAttempts = 0;";
    html += "    wsStats.textContent = messageCount + ' messages';";
    html += "    let msg = event.data;";
    // Add color coding for special markers - use proper escaping for HTML
    html += "    const msgLower = msg.toLowerCase();";
    html += "    let coloredMsg = null;";
    html += "    if (msg.includes('╔══════')) {";
    html += "      const escaped = msg.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');";
    html += "      coloredMsg = '<span style=\"color:#10b981;font-weight:bold\">' + escaped + '</span>';";
    html += "    } else if (msg.includes('BUFFERED LOGS') || msg.includes('END OF BUFFERED')) {";
    html += "      const escaped = msg.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');";
    html += "      coloredMsg = '<span style=\"color:#06b6d4;font-weight:bold\">' + escaped + '</span>';";
    html += "    } else if (msgLower.includes('crash') || msgLower.includes('boot #') || msgLower.includes('===== boot')) {";
    html += "      const escaped = msg.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');";
    html += "      coloredMsg = '<span style=\"color:#f59e0b;font-weight:bold;display:block\">' + escaped + '</span>';";
    html += "    }";
    html += "    if (coloredMsg) {";
    html += "      consoleOutput.innerHTML += coloredMsg;";
    html += "    } else {";
    html += "      const textNode = document.createTextNode(msg);";
    html += "      consoleOutput.appendChild(textNode);";
    html += "    }";
    html += "    const lines = consoleOutput.textContent.split('\\n');";
    html += "    if (lines.length > MAX_MESSAGES) {";
    html += "      const keepLines = lines.slice(-MAX_MESSAGES).join('\\n');";
    html += "      consoleOutput.textContent = keepLines;";
    html += "    }";
    html += "    if (autoscroll) consoleOutput.scrollTop = consoleOutput.scrollHeight;";
    html += "  };";
    html += "  ws.onerror = function(error) {";
    html += "    consoleOutput.textContent += '[CLIENT] WebSocket error\\n';";
    html += "    if (autoscroll) consoleOutput.scrollTop = consoleOutput.scrollHeight;";
    html += "  };";
    html += "  ws.onclose = function() {";
    html += "    wsStatus.className = 'status-indicator status-offline';";
    html += "    wsStatusText.textContent = 'Disconnected';";
    html += "    connectBtn.disabled = false;";
    html += "    disconnectBtn.disabled = true;";
    html += "    consoleOutput.textContent += '[CLIENT] Disconnected\\n';";
    html += "    if (autoscroll) consoleOutput.scrollTop = consoleOutput.scrollHeight;";
    html += "    if (!manualDisconnect && reconnectAttempts < 5) {";
    html += "      const delay = Math.min(1000 * Math.pow(2, reconnectAttempts), 30000);";
    html += "      reconnectAttempts++;";
    html += "      wsStatusText.textContent = 'Reconnecting in ' + (delay/1000) + 's...';";
    html += "      reconnectTimer = setTimeout(connectConsole, delay);";
    html += "    }";
    html += "  };";
    html += "}";
    
    html += "function disconnectConsole() {";
    html += "  manualDisconnect = true;";
    html += "  reconnectAttempts = 0;";
    html += "  if (reconnectTimer) {";
    html += "    clearTimeout(reconnectTimer);";
    html += "    reconnectTimer = null;";
    html += "  }";
    html += "  if (ws) {";
    html += "    ws.close();";
    html += "    ws = null;";
    html += "  }";
    html += "}";
    
    html += "function clearConsole() {";
    html += "  consoleOutput.textContent = '';";
    html += "  messageCount = 0;";
    html += "  wsStats.textContent = '0 messages';";
    html += "}";
    
    html += "function toggleAutoscroll() {";
    html += "  autoscroll = !autoscroll;";
    html += "  autoscrollBtn.innerHTML = '<i class=\"fas fa-arrow-down\" style=\"margin-right:0.5rem\"></i>Auto-scroll: ' + (autoscroll ? 'ON' : 'OFF');";
    html += "  autoscrollBtn.className = autoscroll ? 'btn btn-secondary' : 'btn btn-warning';";
    html += "}";
    
    html += "function downloadLogs() {";
    html += "  const blob = new Blob([consoleOutput.textContent], {type: 'text/plain'});";
    html += "  const url = URL.createObjectURL(blob);";
    html += "  const a = document.createElement('a');";
    html += "  a.href = url;";
    html += "  const timestamp = new Date().toISOString().replace(/[:.]/g, '-');";
    html += "  a.download = 'esp32-console-' + timestamp + '.txt';";
    html += "  a.click();";
    html += "  URL.revokeObjectURL(url);";
    html += "}";
    
    html += "function updateSeverityFilter() {";
    html += "  const severity = parseInt(document.getElementById('severityFilter').value);";
    html += "  fetch('/api/set-log-severity', {";
    html += "    method: 'POST',";
    html += "    headers: {'Content-Type': 'application/x-www-form-urlencoded'},";
    html += "    body: 'severity=' + severity";
    html += "  }).then(response => response.json())";
    html += "    .then(data => {";
    html += "      if (data.status === 'success') {";
    html += "        const levels = ['DEBUG','INFO','WARNING','ERROR','CRITICAL'];";
    html += "        consoleOutput.textContent += '[CLIENT] Severity filter updated to ' + levels[severity] + '\\n';";
    html += "        if (autoscroll) consoleOutput.scrollTop = consoleOutput.scrollHeight;";
    html += "      }";
    html += "    });";
    html += "}";
    
    html += "function clearCrashLogs() {";
    html += "  if (!confirm('Clear all buffered crash logs from device memory?\\n\\nThis will remove logs from RTC and NVS storage.')) return;";
    html += "  fetch('/api/clear-crash-logs', {method: 'POST'})";
    html += "    .then(response => response.json())";
    html += "    .then(data => {";
    html += "      if (data.status === 'success') {";
    html += "        consoleOutput.textContent += '[CLIENT] ✓ Crash logs cleared from device\\n';";
    html += "        if (autoscroll) consoleOutput.scrollTop = consoleOutput.scrollHeight;";
    html += "      } else {";
    html += "        consoleOutput.textContent += '[CLIENT] ✗ Failed to clear crash logs\\n';";
    html += "      }";
    html += "    }).catch(err => {";
    html += "      consoleOutput.textContent += '[CLIENT] ✗ Error: ' + err + '\\n';";
    html += "    });";
    html += "}";
    
    html += "window.addEventListener('beforeunload', function() {";
    html += "  if (ws) ws.close();";
    html += "});";
    
    // Auto-connect when page loads
    html += "window.addEventListener('load', function() {";
    html += "  setTimeout(connectConsole, 500);";
    html += "});";
    
    html += "</script>";
    
    return html;
}


