#include "web_config.h"
#include "web_config_html.h"
#include "system_monitor.h"
#include "network_manager.h"
#include "mqtt_manager.h"
#include "display_manager.h"

// Page generation functions for the web configuration interface
// These functions generate the HTML content for each configuration page

String WebConfig::generateWiFiPortalPage() {
    String html = "<div class='main'><div class='container' style='max-width:600px;margin:2rem auto'>";
    
    // Welcome message
    html += "<div class='card' style='text-align:center;margin-bottom:2rem'>";
    html += "<h1 style='color:#10b981;margin-bottom:1rem'>🌐 WiFi Configuration</h1>";
    html += "<p style='color:#9ca3af;font-size:1.1rem'>Welcome to ESP32 AllSky Display setup!</p>";
    html += "<p style='color:#9ca3af;margin-top:0.5rem'>Please configure your WiFi credentials to get started.</p>";
    html += "</div>";
    
    // Scan results card
    html += "<div class='card' style='margin-bottom:1.5rem'>";
    html += "<h2 style='margin-bottom:1rem'>📡 Available Networks</h2>";
    html += "<div id='networks' style='max-height:200px;overflow-y:auto;border:1px solid #374151;border-radius:0.5rem;padding:0.5rem'>";
    html += "<p style='color:#9ca3af;text-align:center'>Scanning...</p>";
    html += "</div>";
    html += "</div>";
    
    // Configuration form
    html += "<div class='card'>";
    html += "<h2 style='margin-bottom:1.5rem'>🔧 WiFi Settings</h2>";
    html += "<form id='wifiForm' onsubmit='return saveWiFi(event)'>";
    
    html += "<div class='form-group'>";
    html += "<label for='ssid'>Network Name (SSID)</label>";
    html += "<input type='text' id='ssid' name='ssid' class='form-control' required placeholder='Enter WiFi SSID'>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<label for='password'>Password</label>";
    html += "<input type='password' id='password' name='password' class='form-control' placeholder='Enter WiFi password (leave blank for open network)'>";
    html += "</div>";
    
    html += "<button type='submit' class='btn-primary' style='width:100%;padding:0.875rem'>Connect to WiFi</button>";
    html += "</form>";
    
    html += "<div id='message' style='margin-top:1rem;padding:0.75rem;border-radius:0.5rem;display:none'></div>";
    html += "</div>";
    
    html += "</div></div>";
    
    // JavaScript for WiFi portal
    html += "<script>";
    html += "function scanNetworks(){";
    html += "  const container=document.getElementById('networks');";
    html += "  container.innerHTML='<p style=\"color:#9ca3af;text-align:center\">Scanning networks...</p>';";
    html += "  fetch('/api/scan-networks').then(r=>r.json()).then(data=>{";
    html += "    if(data.networks && data.networks.length>0){";
    html += "      let html='';";
    html += "      data.networks.forEach(net=>{";
    html += "        const signal=net.rssi>-50?'🟢':net.rssi>-70?'🟡':'🔴';";
    html += "        const secure=net.encryption!='OPEN'?'🔒':'';";
    html += "        html+=`<div style='padding:0.5rem;border-bottom:1px solid #374151;cursor:pointer' onclick='selectNetwork(\"${net.ssid}\")'>${signal} ${secure} ${net.ssid} (${net.rssi} dBm)</div>`;";
    html += "      });";
    html += "      container.innerHTML=html;";
    html += "    }else{";
    html += "      container.innerHTML='<p style=\"color:#9ca3af;text-align:center\">No networks found</p>';";
    html += "    }";
    html += "  }).catch(err=>{container.innerHTML='<p style=\"color:#ef4444;text-align:center\">Scan failed</p>';});";
    html += "}";
    html += "function selectNetwork(ssid){document.getElementById('ssid').value=ssid;}";
    html += "function saveWiFi(e){";
    html += "  e.preventDefault();";
    html += "  const msg=document.getElementById('message');";
    html += "  const form=document.getElementById('wifiForm');";
    html += "  const formData=new FormData(form);";
    html += "  msg.style.display='block';msg.style.backgroundColor='#1e3a8a';msg.style.color='#60a5fa';";
    html += "  msg.textContent='Connecting to WiFi...';";
    html += "  fetch('/api/save-wifi',{method:'POST',body:new URLSearchParams(formData)})";
    html += "    .then(r=>r.json()).then(data=>{";
    html += "      if(data.status==='success'){";
    html += "        msg.style.backgroundColor='#065f46';msg.style.color='#10b981';";
    html += "        msg.textContent='✓ '+data.message;";
    html += "        setTimeout(()=>{msg.textContent+=' Restarting...';},2000);";
    html += "      }else{";
    html += "        msg.style.backgroundColor='#7f1d1d';msg.style.color='#ef4444';";
    html += "        msg.textContent='✗ '+data.message;";
    html += "      }";
    html += "    }).catch(err=>{";
    html += "      msg.style.backgroundColor='#7f1d1d';msg.style.color='#ef4444';";
    html += "      msg.textContent='✗ Connection failed: '+err.message;";
    html += "    });";
    html += "  return false;";
    html += "}";
    html += "window.onload=function(){scanNetworks();setInterval(scanNetworks,15000);};";
    html += "</script>";
    
    return html;
}

String WebConfig::generateMainPage() {
    String html = "<div class='main'><div class='container'>";
    
    // Calculate usage percentages
    int heapUsedPercent = 100 - (ESP.getFreeHeap() * 100 / ESP.getHeapSize());
    int psramUsedPercent = 100 - (ESP.getFreePsram() * 100 / ESP.getPsramSize());
    int flashUsedPercent = (ESP.getSketchSize() * 100) / ESP.getFlashChipSize();
    
    // Get cycling info
    bool isCycling = configStorage.getCyclingEnabled();
    unsigned long cycleIntervalMs = configStorage.getCycleInterval();
    unsigned long cycleIntervalSec = cycleIntervalMs / 1000;  // Convert milliseconds to seconds
    
    // Format interval: convert seconds to readable format
    String cycleIntervalStr;
    if (cycleIntervalSec >= 3600) {
        // Hours and minutes
        int hours = cycleIntervalSec / 3600;
        int mins = (cycleIntervalSec % 3600) / 60;
        cycleIntervalStr = String(hours) + "h";
        if (mins > 0) cycleIntervalStr += " " + String(mins) + "m";
    } else if (cycleIntervalSec >= 60) {
        // Minutes and seconds
        int mins = cycleIntervalSec / 60;
        int secs = cycleIntervalSec % 60;
        cycleIntervalStr = String(mins) + "m";
        if (secs > 0) cycleIntervalStr += " " + String(secs) + "s";
    } else {
        // Just seconds
        cycleIntervalStr = String(cycleIntervalSec) + "s";
    }
    
    // Key metrics in top row - 4 evenly spaced cards
    html += "<div class='stats' style='grid-template-columns:repeat(4,1fr)'>";
    
    html += "<div class='stat-card'><i class='fas fa-clock stat-icon'></i>";
    html += "<div class='stat-value'>" + formatUptime(millis()) + "</div>";
    html += "<div class='stat-label'>Uptime</div></div>";
    
    html += "<div class='stat-card'><i class='fas fa-sun stat-icon'></i>";
    html += "<div class='stat-value'>" + String(displayManager.getBrightness()) + "%</div>";
    html += "<div class='stat-label'>Brightness (" + String(configStorage.getBrightnessAutoMode() ? "Auto" : "Manual") + ")</div></div>";
    
    html += "<div class='stat-card'><i class='fas fa-image stat-icon'></i>";
    html += "<div class='stat-value'>" + String(isCycling ? "Multi" : "Single") + "</div>";
    html += "<div class='stat-label'>Image Mode</div></div>";
    
    html += "<div class='stat-card'><i class='fas fa-sync-alt stat-icon'></i>";
    html += "<div class='stat-value'>" + cycleIntervalStr + "</div>";
    html += "<div class='stat-label'>Update Frequency</div></div>";
    
    html += "</div>";
    
    // Information cards grid
    html += "<div class='grid' style='margin-top:2rem'>";
    
    // System Information Card - now includes CPU, temp, health
    html += "<div class='card'><h2>💻 System Information</h2>";
    html += "<div style='flex:1'>";
    html += "<div class='stat-grid'>";
    html += "<div><strong class='text-label'>Chip Model:</strong><br>ESP32-P4 (Rev " + String(ESP.getChipRevision()) + ")</div>";
    html += "<div><strong class='text-label'>CPU Cores:</strong><br>" + String(ESP.getChipCores()) + " @ " + String(ESP.getCpuFreqMHz()) + " MHz</div>";
    html += "<div><strong class='text-label'>Temperature:</strong><br>" + String(temperatureRead(), 1) + "°C</div>";
    html += "<div><strong class='text-label'>SDK Version:</strong><br>" + String(ESP.getSdkVersion()) + "</div>";
    html += "<div><strong class='text-label'>Flash Size:</strong><br>" + formatBytes(ESP.getFlashChipSize()) + " (" + String(flashUsedPercent) + "% used)</div>";
    html += "<div><strong class='text-label'>Sketch Size:</strong><br>" + formatBytes(ESP.getSketchSize()) + "</div>";
    html += "<div><strong class='text-label'>Free Flash:</strong><br>" + formatBytes(ESP.getFreeSketchSpace()) + "</div>";
    html += "<div><strong class='text-label'>System Health:</strong><br>" + String(systemMonitor.isSystemHealthy() ? "✓ Healthy" : "⚠ Warning") + "</div>";
    html += "</div></div></div>";
    
    // Memory Details Card - now includes heap and PSRAM with percentages
    html += "<div class='card'><h2>🧠 Memory Details</h2>";
    html += "<div style='flex:1'>";
    html += "<h3 style='color:#10b981;font-size:1rem;margin-bottom:0.75rem'>Heap Memory</h3>";
    html += "<div style='margin-bottom:1.5rem'>";
    html += "<div style='display:flex;justify-content:space-between;margin-bottom:0.5rem'>";
    html += "<span class='text-muted-sm'>Used: " + formatBytes(ESP.getHeapSize() - ESP.getFreeHeap()) + " / " + formatBytes(ESP.getHeapSize()) + "</span>";
    html += "<span class='text-cyan-xs'>" + String(heapUsedPercent) + "%</span>";
    html += "</div>";
    html += "<div class='progress'><div class='progress-bar' style='width:" + String(heapUsedPercent) + "%'></div></div>";
    html += "<p class='text-muted' style='margin-top:0.5rem;font-size:0.85rem'>Free: " + formatBytes(ESP.getFreeHeap()) + " | Min Free: " + formatBytes(systemMonitor.getMinFreeHeap()) + "</p>";
    html += "</div>";
    html += "<h3 style='color:#10b981;font-size:1rem;margin-bottom:0.75rem'>PSRAM Memory</h3>";
    html += "<div>";
    html += "<div style='display:flex;justify-content:space-between;margin-bottom:0.5rem'>";
    html += "<span class='text-muted-sm'>Used: " + formatBytes(ESP.getPsramSize() - ESP.getFreePsram()) + " / " + formatBytes(ESP.getPsramSize()) + "</span>";
    html += "<span class='text-cyan-xs'>" + String(psramUsedPercent) + "%</span>";
    html += "</div>";
    html += "<div class='progress'><div class='progress-bar' style='width:" + String(psramUsedPercent) + "%'></div></div>";
    html += "<p class='text-muted' style='margin-top:0.5rem;font-size:0.85rem'>Free: " + formatBytes(ESP.getFreePsram()) + " | Min Free: " + formatBytes(systemMonitor.getMinFreePsram()) + "</p>";
    html += "</div></div></div>";
    
    html += "</div>";
    
    // Status cards grid
    html += "<div class='grid' style='margin-top:1.5rem'>";
    
    // WiFi Status
    html += "<div class='card'><h2>📡 Network Status</h2>";
    if (wifiManager.isConnected()) {
        html += "<div style='flex:1'><p><span class='status-indicator status-online'></span>Connected to <strong class='text-cyan'>" + String(WiFi.SSID()) + "</strong></p>";
        html += "<div class='stat-grid'>";
        html += "<div><strong class='text-label'>IP Address:</strong><br>" + WiFi.localIP().toString() + "</div>";
        html += "<div><strong class='text-label'>Signal:</strong><br>" + String(WiFi.RSSI()) + " dBm</div>";
        html += "<div><strong class='text-label'>MAC Address:</strong><br>" + WiFi.macAddress() + "</div>";
        html += "<div><strong class='text-label'>Gateway:</strong><br>" + WiFi.gatewayIP().toString() + "</div>";
        html += "<div><strong class='text-label'>DNS:</strong><br>" + WiFi.dnsIP().toString() + "</div>";
        html += "</div></div>";
    } else {
        html += "<div style='flex:1'><p><span class='status-indicator status-offline'></span>Not connected</p></div>";
    }
    html += "</div>";
    
    // MQTT Status
    html += "<div class='card'><h2>🔗 MQTT Status</h2>";
    if (mqttManager.isConnected()) {
        html += "<div style='flex:1'><p><span class='status-indicator status-online'></span>Connected to broker</p>";
        html += "<div class='meta-info'>";
        html += "<p style='margin:0.25rem 0'><strong class='text-label'>Server:</strong> " + configStorage.getMQTTServer() + ":" + String(configStorage.getMQTTPort()) + "</p>";
        html += "<p style='margin:0.25rem 0'><strong class='text-label'>Client ID:</strong> " + escapeHtml(configStorage.getMQTTClientID()) + "</p>";
        html += "<p style='margin:0.25rem 0'><strong class='text-label'>HA Discovery:</strong> " + String(configStorage.getHADiscoveryEnabled() ? "Enabled" : "Disabled") + "</p>";
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
        html += "<div class='stat-info'>";
        html += "<div><p class='text-muted-sm' style='margin:0'><strong class='text-white'>Mode:</strong> Cycling</p></div>";
        html += "<div><p class='text-muted-sm' style='margin:0'><strong class='text-white'>Active:</strong> [" + String(currentIndex + 1) + "/" + String(sourceCount) + "]</p></div>";
        html += "<div><p class='text-muted-sm' style='margin:0'><strong class='text-white'>Cycle:</strong> " + String(configStorage.getCycleInterval() / 1000) + "s</p></div>";
        html += "<div><p class='text-muted-sm' style='margin:0'><strong class='text-white'>Update:</strong> " + String(configStorage.getUpdateInterval() / 1000 / 60) + "m</p></div>";
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
            html += "<h3 class='text-muted-sm' style='margin-bottom:1rem'>Configured Sources:</h3>";
            for (int i = 0; i < sourceCount; i++) {
                String sourceUrl = configStorage.getImageSource(i);
                String activeIndicator = (i == currentIndex) ? "<span style='color:#10b981;margin-right:8px;font-size:1.2rem'>►</span>" : "<span class='text-label' style='margin-right:8px'>•</span>";
                html += "<div style='margin-bottom:0.75rem;padding:0.75rem;background:" + String(i == currentIndex ? "#1e3a2e" : "#1e293b") + ";border-radius:8px;border-left:4px solid " + String(i == currentIndex ? "#10b981" : "#475569") + ";overflow-wrap:break-word;word-break:break-all'>";
                html += "<div class='text-muted' style='margin-bottom:0.25rem'>" + activeIndicator + "<strong style='color:" + String(i == currentIndex ? "#10b981" : "#64748b") + "'>Source " + String(i + 1) + String(i == currentIndex ? " (Active)" : "") + "</strong></div>";
                html += "<div class='text-light' style='font-family:monospace;padding-left:1.5rem'>" + escapeHtml(sourceUrl) + "</div>";
                html += "</div>";
            }
        }
    } else {
        html += "<div class='stat-info'>";
        html += "<div><p class='text-muted-sm' style='margin:0'><strong class='text-white'>Mode:</strong> Single Image</p></div>";
        html += "<div><p class='text-muted-sm' style='margin:0'><strong class='text-white'>Update:</strong> " + String(configStorage.getUpdateInterval() / 1000 / 60) + " minutes</p></div>";
        html += "</div>";
        
        // Explanation
        html += "<div class='info-box' style='margin-bottom:1.5rem'>";
        html += "<p class='text-cyan-xs' style='margin:0;line-height:1.6'><i class='fas fa-info-circle' style='margin-right:8px'></i>";
        html += "<strong>Single Image Mode:</strong> Display shows only one image source. ";
        html += "The image is re-downloaded every <strong>" + String(configStorage.getUpdateInterval() / 1000 / 60) + " minutes</strong> to fetch fresh content.</p>";
        html += "</div>";
        
        html += "<h3 class='text-muted-sm' style='margin-bottom:1rem'>Image Source:</h3>";
        html += "<div class='text-light text-muted-sm' style='padding:0.75rem;background:#1e293b;border-radius:8px;border-left:4px solid #0ea5e9;overflow-wrap:break-word;word-break:break-all;font-family:monospace'>" + escapeHtml(configStorage.getImageURL()) + "</div>";
    }
    
    // Add image navigation controls
    if (configStorage.getCyclingEnabled() && configStorage.getImageSourceCount() > 1) {
        html += "<div style='margin-top:1.5rem;display:flex;gap:0.75rem;justify-content:center'>";
        html += "<button type='button' class='btn btn-secondary' onclick='previousImage(this)' style='flex:1;max-width:200px'><i class='fas fa-chevron-left'></i> Previous</button>";
        html += "<button type='button' class='btn btn-secondary' onclick='nextImage(this)' style='flex:1;max-width:200px'>Next <i class='fas fa-chevron-right'></i></button>";
        html += "</div>";
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
    html += "<p class='text-muted-sm' style='margin-top:-0.5rem;margin-bottom:1rem'>Automatically creates entities in Home Assistant for all device controls and sensors</p></div>";
    
    html += "<div class='form-group'><label for='ha_device_name'>Device Name</label>";
    html += "<input type='text' id='ha_device_name' name='ha_device_name' class='form-control' value='" + escapeHtml(configStorage.getHADeviceName()) + "' placeholder='ESP32 AllSky Display'></div>";
    
    html += "<div class='form-group'><label for='ha_discovery_prefix'>Discovery Prefix</label>";
    html += "<input type='text' id='ha_discovery_prefix' name='ha_discovery_prefix' class='form-control' value='" + escapeHtml(configStorage.getHADiscoveryPrefix()) + "' placeholder='homeassistant'>";
    html += "<p class='text-muted' style='margin-top:0.5rem'>Default is 'homeassistant'. Change only if you've customized your HA MQTT discovery prefix.</p></div>";
    
    html += "<div class='form-group'><label for='ha_state_topic'>State Topic Prefix</label>";
    html += "<input type='text' id='ha_state_topic' name='ha_state_topic' class='form-control' value='" + escapeHtml(configStorage.getHAStateTopic()) + "' placeholder='allsky_display'>";
    html += "<p class='text-muted' style='margin-top:0.5rem'>Base MQTT topic for all device state and command messages.</p></div>";
    
    html += "<div class='form-group'><label for='ha_sensor_update_interval'>Sensor Update Interval (seconds)</label>";
    html += "<input type='number' id='ha_sensor_update_interval' name='ha_sensor_update_interval' class='form-control' value='" + String(configStorage.getHASensorUpdateInterval()) + "' min='10' max='300'>";
    html += "<p class='text-muted' style='margin-top:0.5rem'>How often to publish sensor data (heap, PSRAM, WiFi signal, uptime) to Home Assistant.</p></div>";
    
    html += "<div class='info-box' style='margin-top:1rem'>";
    html += "<p class='text-cyan-sm' style='margin:0'><i class='fas fa-info-circle' style='margin-right:8px'></i><strong>Note:</strong> After saving, reconnect MQTT to trigger discovery. All device controls will appear in Home Assistant automatically.</p>";
    html += "</div></div>";
    
    html += "</div><div class='card' style='margin-top:1.5rem'>";
    html += "<button type='submit' class='btn btn-primary'>💾 Save MQTT Settings</button></div></form></div></div>";
    return html;
}

String WebConfig::generateImageSourcesPage() {
    String html = "<div class='main'><div class='container'>";
    html += "<form id='cyclingForm'><div class='card'><h2>🔄 Image Cycling Configuration</h2>";
    html += "<p class='text-muted-sm' style='margin-bottom:1rem;background:rgba(56,189,248,0.1);padding:0.75rem;border-radius:6px;border-left:4px solid #38bdf8'>ℹ️ Maximum supported image size: <strong>724x724 pixels</strong> (1MB buffer limit). Images larger than this will fail to display.</p>";
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
    html += "</div>";
    
    // Add image navigation controls for multi-image mode
    if (sourceCount > 1) {
        html += "<div style='margin-top:1.5rem;padding-top:1.5rem;border-top:1px solid #334155'>";
        html += "<h3 style='margin-bottom:1rem;color:#cbd5e1'>Manual Navigation</h3>";
        html += "<div style='display:flex;gap:0.75rem;justify-content:center'>";
        html += "<button type='button' class='btn btn-secondary' onclick='previousImage(this)' style='flex:1;max-width:200px'><i class='fas fa-chevron-left'></i> Previous Image</button>";
        html += "<button type='button' class='btn btn-secondary' onclick='nextImage(this)' style='flex:1;max-width:200px'>Next Image <i class='fas fa-chevron-right'></i></button>";
        html += "</div>";
        html += "<p class='text-muted-sm' style='margin-top:0.75rem;text-align:center'><i class='fas fa-info-circle' style='margin-right:4px'></i>Manually switch between images without waiting for the cycle interval</p>";
        html += "</div>";
    }
    
    html += "</div></div></div>";
    return html;
}

String WebConfig::generateImagePage() {
    String html = "<div class='main'><div class='container'><form id='imageForm'><div class='grid'>";
    html += "<div class='card'><h2>🖼️ Image Source</h2>";
    html += "<p class='text-warning' style='margin-bottom:1rem;background:rgba(245,158,11,0.1);padding:0.75rem;border-radius:6px;border-left:4px solid #f59e0b'>⚠️ For multiple image sources, use the <a href='/config/sources' class='text-cyan' style='font-weight:bold'>Multi-Image</a> page.</p>";
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
    html += "<p class='text-muted-sm' style='margin-bottom:1rem'>Adjust screen brightness in real-time. Changes take effect immediately but are not saved.</p>";
    
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
    html += "<p class='text-muted-sm' style='margin-bottom:1rem'>Configure default brightness and backlight hardware settings. These are saved permanently.</p>";
    
    html += "<div class='form-group'><label for='default_brightness'>Default Brightness at Startup (%)</label>";
    html += "<input type='range' id='default_brightness' name='default_brightness' class='form-control' value='" + String(configStorage.getDefaultBrightness()) + "' min='0' max='100' oninput='updateBrightnessValue(this.value)'>";
    html += "<div class='text-cyan' style='text-align:center;margin-top:0.5rem;font-weight:bold'><span id='brightnessValue'>" + String(configStorage.getDefaultBrightness()) + "</span>%</div>";
    html += "<p class='text-label' style='margin-top:0.5rem'>This brightness will be applied when the device boots up.</p></div>";
    
    html += "<div class='form-group'><label for='backlight_freq'>PWM Frequency (Hz)</label>";
    html += "<input type='number' id='backlight_freq' name='backlight_freq' class='form-control' value='" + String(configStorage.getBacklightFreq()) + "' min='1000' max='20000'>";
    html += "<p class='text-label' style='margin-top:0.5rem'>Higher frequency reduces flicker. Typical: 5000 Hz</p></div>";
    
    html += "<div class='form-group'><label for='backlight_resolution'>PWM Resolution (bits)</label>";
    html += "<input type='number' id='backlight_resolution' name='backlight_resolution' class='form-control' value='" + String(configStorage.getBacklightResolution()) + "' min='8' max='16'>";
    html += "<p class='text-label' style='margin-top:0.5rem'>Higher resolution provides smoother brightness control. Typical: 10-12 bits</p></div>";
    
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
    html += "<p class='text-muted-sm' style='margin-bottom:1rem'>Control your display using serial commands via USB connection. Open the Serial Monitor at 9600 baud to send commands.</p>";
    html += "<div class='info-box' style='margin-top:1rem'>";
    html += "<p class='text-cyan-sm' style='margin:0'><i class='fas fa-info-circle' style='margin-right:8px'></i><strong>Tip:</strong> Type 'H' or '?' in the Serial Monitor to display this help in your terminal.</p>";
    html += "</div></div>";
    
    // Image Transformation Commands
    html += "<div class='card'><h2>🔄 Image Transformations</h2>";
    html += "<table style='width:100%;border-collapse:collapse'>";
    html += "<thead><tr style='background:#1e293b;border-bottom:2px solid #334155'>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Key</th>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Action</th>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Description</th></tr></thead><tbody>";
    
    html += "<tr style='background:#1e3a2e;border-left:4px solid #10b981'><td colspan='3' style='padding:0.75rem;font-weight:bold;color:#10b981;font-size:1.05rem'>🖼️ Image Navigation</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>→ / ↓</td>";
    html += "<td style='padding:0.75rem'>Next Image</td><td class='text-muted-sm' style='padding:0.75rem'>Switch to the next image (Right or Down arrow keys)</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>← / ↑</td>";
    html += "<td style='padding:0.75rem'>Previous Image</td><td class='text-muted-sm' style='padding:0.75rem'>Go back to the previous image (Left or Up arrow keys)</td></tr>";
    
    html += "<tr style='background:#1e3a2e;border-left:4px solid #10b981'><td colspan='3' style='padding:0.75rem;font-weight:bold;color:#10b981;font-size:1.05rem'>⚙️ Image Transformations</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>+</td>";
    html += "<td style='padding:0.75rem'>Scale Up</td><td class='text-muted-sm' style='padding:0.75rem'>Increase image scale on both axes by 0.1</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>-</td>";
    html += "<td style='padding:0.75rem'>Scale Down</td><td class='text-muted-sm' style='padding:0.75rem'>Decrease image scale on both axes by 0.1</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>W</td>";
    html += "<td style='padding:0.75rem'>Move Up</td><td class='text-muted-sm' style='padding:0.75rem'>Move image up by 10 pixels</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>S</td>";
    html += "<td style='padding:0.75rem'>Move Down</td><td class='text-muted-sm' style='padding:0.75rem'>Move image down by 10 pixels</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>A</td>";
    html += "<td style='padding:0.75rem'>Move Left</td><td class='text-muted-sm' style='padding:0.75rem'>Move image left by 10 pixels</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>D</td>";
    html += "<td style='padding:0.75rem'>Move Right</td><td class='text-muted-sm' style='padding:0.75rem'>Move image right by 10 pixels</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>Q</td>";
    html += "<td style='padding:0.75rem'>Rotate CCW</td><td class='text-muted-sm' style='padding:0.75rem'>Rotate image 90° counter-clockwise</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>E</td>";
    html += "<td style='padding:0.75rem'>Rotate CW</td><td class='text-muted-sm' style='padding:0.75rem'>Rotate image 90° clockwise</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>R</td>";
    html += "<td style='padding:0.75rem'>Reset All</td><td class='text-muted-sm' style='padding:0.75rem'>Reset all transformations to defaults</td></tr>";
    
    html += "</tbody></table></div>";
    
    // Display Control Commands
    html += "<div class='card'><h2>💡 Display Controls</h2>";
    html += "<table style='width:100%;border-collapse:collapse'>";
    html += "<thead><tr style='background:#1e293b;border-bottom:2px solid #334155'>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Key</th>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Action</th>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Description</th></tr></thead><tbody>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>L</td>";
    html += "<td style='padding:0.75rem'>Brightness Up</td><td class='text-muted-sm' style='padding:0.75rem'>Increase brightness by 10%</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>K</td>";
    html += "<td style='padding:0.75rem'>Brightness Down</td><td class='text-muted-sm' style='padding:0.75rem'>Decrease brightness by 10%</td></tr>";
    
    html += "</tbody></table></div>";
    
    // System Commands
    html += "<div class='card'><h2>⚙️ System Commands</h2>";
    html += "<table style='width:100%;border-collapse:collapse'>";
    html += "<thead><tr style='background:#1e293b;border-bottom:2px solid #334155'>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Key</th>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Action</th>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Description</th></tr></thead><tbody>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>B</td>";
    html += "<td style='padding:0.75rem'>Reboot Device</td><td class='text-muted-sm' style='padding:0.75rem'>Restart the ESP32 device</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>M</td>";
    html += "<td style='padding:0.75rem'>Memory Info</td><td class='text-muted-sm' style='padding:0.75rem'>Display heap and PSRAM memory status</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>I</td>";
    html += "<td style='padding:0.75rem'>Network Info</td><td class='text-muted-sm' style='padding:0.75rem'>Show WiFi connection details</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>P</td>";
    html += "<td style='padding:0.75rem'>PPA Info</td><td class='text-muted-sm' style='padding:0.75rem'>Display hardware accelerator status</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>T</td>";
    html += "<td style='padding:0.75rem'>MQTT Info</td><td class='text-muted-sm' style='padding:0.75rem'>Show MQTT connection status</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>X</td>";
    html += "<td style='padding:0.75rem'>Web Server</td><td class='text-muted-sm' style='padding:0.75rem'>Show web server status and restart</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;font-family:monospace;color:#10b981'>H / ?</td>";
    html += "<td style='padding:0.75rem'>Help</td><td class='text-muted-sm' style='padding:0.75rem'>Display command reference in Serial Monitor</td></tr>";
    
    html += "</tbody></table></div>";
    
    // Touch Controls
    html += "<div class='card'><h2>👆 Touch Controls</h2>";
    html += "<table style='width:100%;border-collapse:collapse'>";
    html += "<thead><tr style='background:#1e293b;border-bottom:2px solid #334155'>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Gesture</th>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Action</th>";
    html += "<th style='padding:0.75rem;text-align:left;color:#38bdf8'>Description</th></tr></thead><tbody>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;color:#10b981'>Single Tap</td>";
    html += "<td style='padding:0.75rem'>Next Image</td><td class='text-muted-sm' style='padding:0.75rem'>Switch to the next image in cycling mode</td></tr>";
    
    html += "<tr style='border-bottom:1px solid #334155'><td style='padding:0.75rem;color:#10b981'>Double Tap</td>";
    html += "<td style='padding:0.75rem'>Toggle Mode</td><td class='text-muted-sm' style='padding:0.75rem'>Switch between cycling and single refresh modes</td></tr>";
    
    html += "</tbody></table></div>";
    
    // Usage Instructions
    html += "<div class='card'><h2>🔧 How to Use Serial Commands</h2>";
    html += "<ol class='text-muted-sm' style='line-height:2;margin-left:1.5rem'>";
    html += "<li>Connect your ESP32 device to your computer via USB</li>";
    html += "<li>Open Arduino IDE or any serial terminal</li>";
    html += "<li>Set baud rate to <strong class='text-cyan'>9600</strong></li>";
    html += "<li>Type a command key and press Enter</li>";
    html += "<li>Commands are <strong class='text-cyan'>case-insensitive</strong> (W or w both work)</li>";
    html += "<li>Serial output will confirm the action and show current values</li>";
    html += "</ol>";
    html += "<div class='info-box' style='margin-top:1rem'>";
    html += "<p class='text-cyan-sm' style='margin:0'><i class='fas fa-info-circle' style='margin-right:8px'></i><strong>Image Transformations:</strong> Changes made with +, -, W, S, A, D, Q, E, R are automatically saved to configuration for the current image.</p>";
    html += "</div>";
    html += "<div class='info-box-warn' style='margin-top:1rem'>";
    html += "<p class='text-warning text-muted-sm' style='margin:0'><i class='fas fa-exclamation-triangle' style='margin-right:8px'></i><strong>Brightness:</strong> L and K commands take effect immediately but are NOT saved. Brightness settings persist only when changed via the web interface or MQTT.</p>";
    html += "</div></div>";
    
    html += "</div></div>";
    return html;
}

String WebConfig::generateAPIReferencePage() {
    String deviceIP = WiFi.localIP().toString();
    String html = "<div class='main'><div class='container'>";
    
    html += "<div class='card'><h2><i class='fas fa-plug'></i> API Reference</h2>";
    html += "<p class='text-muted'>REST API for programmatic control of the ESP32 AllSky Display. All endpoints accept POST requests.</p>";
    html += "<div class='info-box' style='margin-top:1rem'>";
    html += "<p style='margin:0'><strong>Base URL:</strong> <code style='background:#0f172a;padding:4px 8px;border-radius:4px;color:#38bdf8'>http://" + deviceIP + ":8080/api/</code></p>";
    html += "</div></div>";
    
    // Configuration Management
    html += "<div class='card'><h3 style='color:#38bdf8;margin-bottom:1rem'><i class='fas fa-cog'></i> Configuration Management</h3>";
    html += "<div style='background:#0f172a;padding:1.5rem;border-radius:8px;border:1px solid #334155'>";
    html += "<h4 style='color:#10b981;margin-bottom:0.5rem'>POST /api/save</h4>";
    html += "<p class='text-muted-sm'>Save configuration settings. Changes to WiFi or MQTT may require restart.</p>";
    html += "<div style='margin-top:1rem'><strong class='text-label'>Parameters:</strong></div>";
    html += "<ul style='margin:0.5rem 0 0 1.5rem;color:#cbd5e1;font-size:0.9rem'>";
    html += "<li><code>wifi_ssid</code>, <code>wifi_password</code> - Network credentials</li>";
    html += "<li><code>mqtt_server</code>, <code>mqtt_port</code>, <code>mqtt_user</code>, <code>mqtt_password</code> - MQTT broker settings</li>";
    html += "<li><code>default_brightness</code> - Display brightness (0-255)</li>";
    html += "<li><code>default_scale_x/y</code>, <code>default_offset_x/y</code>, <code>default_rotation</code> - Image transforms</li>";
    html += "<li><code>cycle_interval</code> - Seconds between images</li>";
    html += "</ul>";
    html += "<div style='margin-top:1rem;background:#1e293b;padding:1rem;border-radius:6px;border-left:3px solid #38bdf8'>";
    html += "<strong class='text-cyan-xs'>Example:</strong><br><code style='color:#94a3b8;font-size:0.85rem'>curl -X POST http://" + deviceIP + ":8080/api/save -d \"default_brightness=200\"</code>";
    html += "</div></div></div>";
    
    // Image Source Management
    html += "<div class='card'><h3 style='color:#38bdf8;margin-bottom:1rem'><i class='fas fa-images'></i> Image Source Management</h3>";
    
    html += "<div style='background:#0f172a;padding:1.5rem;border-radius:8px;border:1px solid #334155;margin-bottom:1rem'>";
    html += "<h4 style='color:#10b981;margin-bottom:0.5rem'>POST /api/add-source</h4>";
    html += "<p class='text-muted-sm'>Add a new image source to the cycle.</p>";
    html += "<div style='margin-top:0.5rem'><strong class='text-label'>Parameters:</strong> <code>url</code> - Image URL</div>";
    html += "<div style='margin-top:1rem;background:#1e293b;padding:1rem;border-radius:6px;border-left:3px solid #38bdf8'>";
    html += "<strong class='text-cyan-xs'>Example:</strong><br><code style='color:#94a3b8;font-size:0.85rem'>curl -X POST http://" + deviceIP + ":8080/api/add-source -d \"url=https://example.com/image.jpg\"</code>";
    html += "</div></div>";
    
    html += "<div style='background:#0f172a;padding:1.5rem;border-radius:8px;border:1px solid #334155;margin-bottom:1rem'>";
    html += "<h4 style='color:#10b981;margin-bottom:0.5rem'>POST /api/remove-source</h4>";
    html += "<p class='text-muted-sm'>Remove an image source by index.</p>";
    html += "<div style='margin-top:0.5rem'><strong class='text-label'>Parameters:</strong> <code>index</code> - Image index (0-based)</div>";
    html += "<div style='margin-top:1rem;background:#1e293b;padding:1rem;border-radius:6px;border-left:3px solid #38bdf8'>";
    html += "<strong class='text-cyan-xs'>Example:</strong><br><code style='color:#94a3b8;font-size:0.85rem'>curl -X POST http://" + deviceIP + ":8080/api/remove-source -d \"index=2\"</code>";
    html += "</div></div>";
    
    html += "<div style='background:#0f172a;padding:1.5rem;border-radius:8px;border:1px solid #334155;margin-bottom:1rem'>";
    html += "<h4 style='color:#10b981;margin-bottom:0.5rem'>POST /api/update-source</h4>";
    html += "<p class='text-muted-sm'>Update an existing image source URL.</p>";
    html += "<div style='margin-top:0.5rem'><strong class='text-label'>Parameters:</strong> <code>index</code>, <code>url</code></div>";
    html += "<div style='margin-top:1rem;background:#1e293b;padding:1rem;border-radius:6px;border-left:3px solid #38bdf8'>";
    html += "<strong class='text-cyan-xs'>Example:</strong><br><code style='color:#94a3b8;font-size:0.85rem'>curl -X POST http://" + deviceIP + ":8080/api/update-source -d \"index=0&url=https://new.com/image.jpg\"</code>";
    html += "</div></div>";
    
    html += "<div style='background:#0f172a;padding:1.5rem;border-radius:8px;border:1px solid #334155'>";
    html += "<h4 style='color:#10b981;margin-bottom:0.5rem'>POST /api/clear-sources</h4>";
    html += "<p class='text-muted-sm'>Remove all image sources.</p>";
    html += "<div style='margin-top:1rem;background:#1e293b;padding:1rem;border-radius:6px;border-left:3px solid #38bdf8'>";
    html += "<strong class='text-cyan-xs'>Example:</strong><br><code style='color:#94a3b8;font-size:0.85rem'>curl -X POST http://" + deviceIP + ":8080/api/clear-sources</code>";
    html += "</div></div></div>";
    
    // Image Navigation
    html += "<div class='card'><h3 style='color:#38bdf8;margin-bottom:1rem'><i class='fas fa-arrows-alt-h'></i> Image Navigation</h3>";
    
    html += "<div style='background:#0f172a;padding:1.5rem;border-radius:8px;border:1px solid #334155;margin-bottom:1rem'>";
    html += "<h4 style='color:#10b981;margin-bottom:0.5rem'>POST /api/next-image</h4>";
    html += "<p class='text-muted-sm'>Advance to the next image in the cycle.</p>";
    html += "<div style='margin-top:1rem;background:#1e293b;padding:1rem;border-radius:6px;border-left:3px solid #38bdf8'>";
    html += "<strong class='text-cyan-xs'>Example:</strong><br><code style='color:#94a3b8;font-size:0.85rem'>curl -X POST http://" + deviceIP + ":8080/api/next-image</code>";
    html += "</div></div>";
    
    html += "<div style='background:#0f172a;padding:1.5rem;border-radius:8px;border:1px solid #334155'>";
    html += "<h4 style='color:#10b981;margin-bottom:0.5rem'>POST /api/previous-image</h4>";
    html += "<p class='text-muted-sm'>Go back to the previous image.</p>";
    html += "<div style='margin-top:1rem;background:#1e293b;padding:1rem;border-radius:6px;border-left:3px solid #38bdf8'>";
    html += "<strong class='text-cyan-xs'>Example:</strong><br><code style='color:#94a3b8;font-size:0.85rem'>curl -X POST http://" + deviceIP + ":8080/api/previous-image</code>";
    html += "</div></div></div>";
    
    // Image Transformations
    html += "<div class='card'><h3 style='color:#38bdf8;margin-bottom:1rem'><i class='fas fa-vector-square'></i> Image Transformations</h3>";
    
    html += "<div style='background:#0f172a;padding:1.5rem;border-radius:8px;border:1px solid #334155;margin-bottom:1rem'>";
    html += "<h4 style='color:#10b981;margin-bottom:0.5rem'>POST /api/update-transform</h4>";
    html += "<p class='text-muted-sm'>Update transformation settings for a specific image.</p>";
    html += "<div style='margin-top:0.5rem'><strong class='text-label'>Parameters:</strong> <code>index</code>, <code>scale_x</code>, <code>scale_y</code>, <code>offset_x</code>, <code>offset_y</code>, <code>rotation</code></div>";
    html += "<div style='margin-top:1rem;background:#1e293b;padding:1rem;border-radius:6px;border-left:3px solid #38bdf8'>";
    html += "<strong class='text-cyan-xs'>Example:</strong><br><code style='color:#94a3b8;font-size:0.85rem'>curl -X POST http://" + deviceIP + ":8080/api/update-transform -d \"index=0&scale_x=1.2&scale_y=1.2\"</code>";
    html += "</div></div>";
    
    html += "<div style='background:#0f172a;padding:1.5rem;border-radius:8px;border:1px solid #334155;margin-bottom:1rem'>";
    html += "<h4 style='color:#10b981;margin-bottom:0.5rem'>POST /api/copy-defaults</h4>";
    html += "<p class='text-muted-sm'>Copy default transformation settings to a specific image.</p>";
    html += "<div style='margin-top:0.5rem'><strong class='text-label'>Parameters:</strong> <code>index</code> - Target image index</div>";
    html += "<div style='margin-top:1rem;background:#1e293b;padding:1rem;border-radius:6px;border-left:3px solid #38bdf8'>";
    html += "<strong class='text-cyan-xs'>Example:</strong><br><code style='color:#94a3b8;font-size:0.85rem'>curl -X POST http://" + deviceIP + ":8080/api/copy-defaults -d \"index=1\"</code>";
    html += "</div></div>";
    
    html += "<div style='background:#0f172a;padding:1.5rem;border-radius:8px;border:1px solid #334155'>";
    html += "<h4 style='color:#10b981;margin-bottom:0.5rem'>POST /api/apply-transform</h4>";
    html += "<p class='text-muted-sm'>Apply transformation changes and refresh the display immediately.</p>";
    html += "<div style='margin-top:1rem;background:#1e293b;padding:1rem;border-radius:6px;border-left:3px solid #38bdf8'>";
    html += "<strong class='text-cyan-xs'>Example:</strong><br><code style='color:#94a3b8;font-size:0.85rem'>curl -X POST http://" + deviceIP + ":8080/api/apply-transform</code>";
    html += "</div></div></div>";
    
    // System Control
    html += "<div class='card'><h3 style='color:#38bdf8;margin-bottom:1rem'><i class='fas fa-power-off'></i> System Control</h3>";
    
    html += "<div style='background:#0f172a;padding:1.5rem;border-radius:8px;border:1px solid #334155;margin-bottom:1rem'>";
    html += "<h4 style='color:#10b981;margin-bottom:0.5rem'>POST /api/restart</h4>";
    html += "<p class='text-muted-sm'>Reboot the ESP32 device.</p>";
    html += "<div style='margin-top:1rem;background:#1e293b;padding:1rem;border-radius:6px;border-left:3px solid #38bdf8'>";
    html += "<strong class='text-cyan-xs'>Example:</strong><br><code style='color:#94a3b8;font-size:0.85rem'>curl -X POST http://" + deviceIP + ":8080/api/restart</code>";
    html += "</div></div>";
    
    html += "<div style='background:#0f172a;padding:1.5rem;border-radius:8px;border:1px solid #334155'>";
    html += "<h4 style='color:#10b981;margin-bottom:0.5rem'>POST /api/factory-reset</h4>";
    html += "<p class='text-muted-sm'>Reset all settings to factory defaults and reboot.</p>";
    html += "<div class='info-box-warn' style='margin-top:0.5rem;margin-bottom:1rem'>";
    html += "<p style='margin:0;font-size:0.85rem'><i class='fas fa-exclamation-triangle'></i> <strong>Warning:</strong> This will erase all configuration!</p>";
    html += "</div>";
    html += "<div style='margin-top:1rem;background:#1e293b;padding:1rem;border-radius:6px;border-left:3px solid #38bdf8'>";
    html += "<strong class='text-cyan-xs'>Example:</strong><br><code style='color:#94a3b8;font-size:0.85rem'>curl -X POST http://" + deviceIP + ":8080/api/factory-reset</code>";
    html += "</div></div></div>";
    
    // Device Information
    html += "<div class='card'><h3 style='color:#38bdf8;margin-bottom:1rem'><i class='fas fa-microchip'></i> Device Information</h3>";
    
    html += "<div style='background:#0f172a;padding:1.5rem;border-radius:8px;border:1px solid #334155'>";
    html += "<h4 style='color:#10b981;margin-bottom:0.5rem'>GET /api/device-info</h4>";
    html += "<p class='text-muted-sm'>Get comprehensive device information including hardware specs, memory usage, flash metrics, network status, and system health.</p>";
    html += "<div style='margin-top:1rem;background:#1e293b;padding:1rem;border-radius:6px;border-left:3px solid #38bdf8'>";
    html += "<strong class='text-cyan-xs'>Example:</strong><br><code style='color:#94a3b8;font-size:0.85rem'>curl http://" + deviceIP + ":8080/api/device-info</code>";
    html += "</div>";
    html += "<div style='margin-top:1rem'><strong class='text-label'>Response includes:</strong></div>";
    html += "<ul style='margin:0.5rem 0 0 1.5rem;color:#cbd5e1;font-size:0.9rem'>";
    html += "<li><strong>device</strong> - Chip model, revision, CPU cores/frequency, SDK version</li>";
    html += "<li><strong>flash</strong> - Size, speed, sketch size, usage percentage, free space, MD5</li>";
    html += "<li><strong>memory</strong> - Heap and PSRAM size, free/used amounts, min free values</li>";
    html += "<li><strong>network</strong> - WiFi status, SSID, IP, MAC, RSSI, gateway, DNS</li>";
    html += "<li><strong>mqtt</strong> - Connection status, server, port, client ID, HA discovery</li>";
    html += "<li><strong>display</strong> - Brightness, auto mode, resolution</li>";
    html += "<li><strong>imageCycling</strong> - Status, current index, sources, intervals</li>";
    html += "<li><strong>system</strong> - Uptime, health status, temperature</li>";
    html += "</ul></div></div>";
    
    // Response Format
    html += "<div class='card'><h3 style='color:#38bdf8;margin-bottom:1rem'><i class='fas fa-code'></i> Response Format</h3>";
    html += "<p class='text-muted'>All API endpoints return JSON responses.</p>";
    
    html += "<div style='margin-top:1rem'><strong style='color:#10b981'>Success Response:</strong></div>";
    html += "<pre style='background:#0f172a;padding:1rem;border-radius:8px;border:1px solid #334155;overflow-x:auto;margin-top:0.5rem'><code style='color:#94a3b8'>{\"status\": \"success\", \"message\": \"Operation completed\"}</code></pre>";
    
    html += "<div style='margin-top:1rem'><strong style='color:#ef4444'>Error Response:</strong></div>";
    html += "<pre style='background:#0f172a;padding:1rem;border-radius:8px;border:1px solid #334155;overflow-x:auto;margin-top:0.5rem'><code style='color:#94a3b8'>{\"status\": \"error\", \"message\": \"Error description\"}</code></pre>";
    html += "</div>";
    
    // Integration Examples
    html += "<div class='card'><h3 style='color:#38bdf8;margin-bottom:1rem'><i class='fas fa-puzzle-piece'></i> Integration Examples</h3>";
    
    html += "<div style='margin-bottom:1.5rem'>";
    html += "<h4 style='color:#10b981;margin-bottom:0.75rem'>Python Script</h4>";
    html += "<pre style='background:#0f172a;padding:1rem;border-radius:8px;border:1px solid #334155;overflow-x:auto;font-size:0.85rem'><code style='color:#94a3b8'>import requests\nimport json\n\ndevice_ip = \"" + deviceIP + "\"\nbase_url = f\"http://{device_ip}:8080/api\"\n\n# Get device information\ninfo = requests.get(f\"{base_url}/device-info\").json()\nprint(f\"Device: {info['device']['chipModel']}\")\nprint(f\"Flash used: {info['flash']['sketchUsedPercent']}%\")\nprint(f\"Free heap: {info['memory']['freeHeap']} bytes\")\n\n# Set brightness\nrequests.post(f\"{base_url}/save\", data={\"default_brightness\": 200})\n\n# Add image source\nrequests.post(f\"{base_url}/add-source\", \n              data={\"url\": \"https://example.com/image.jpg\"})\n\n# Next image\nrequests.post(f\"{base_url}/next-image\")</code></pre>";
    html += "</div>";
    
    html += "<div style='margin-bottom:1.5rem'>";
    html += "<h4 style='color:#10b981;margin-bottom:0.75rem'>Home Assistant Automation</h4>";
    html += "<pre style='background:#0f172a;padding:1rem;border-radius:8px;border:1px solid #334155;overflow-x:auto;font-size:0.85rem'><code style='color:#94a3b8'>automation:\n  - alias: \"Next Image at Sunset\"\n    trigger:\n      - platform: sun\n        event: sunset\n    action:\n      - service: rest_command.display_next\n\nrest_command:\n  display_next:\n    url: \"http://" + deviceIP + ":8080/api/next-image\"\n    method: POST</code></pre>";
    html += "</div>";
    
    html += "<div>";
    html += "<h4 style='color:#10b981;margin-bottom:0.75rem'>cURL Examples</h4>";
    html += "<pre style='background:#0f172a;padding:1rem;border-radius:8px;border:1px solid #334155;overflow-x:auto;font-size:0.85rem'><code style='color:#94a3b8'># Windows PowerShell\nInvoke-WebRequest -Method POST -Uri \"http://" + deviceIP + ":8080/api/next-image\"\n\n# Linux/Mac\ncurl -X POST http://" + deviceIP + ":8080/api/next-image\n\n# With parameters\ncurl -X POST http://" + deviceIP + ":8080/api/save \\\n  -d \"default_brightness=200\" \\\n  -d \"cycle_interval=30\"</code></pre>";
    html += "</div></div>";
    
    html += "</div></div>";
    return html;
}
