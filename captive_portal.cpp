#include "captive_portal.h"
#include "crash_logger.h"
#include <esp_task_wdt.h>

// Global instances
CaptivePortal captivePortal;
extern CrashLogger crashLogger;

// DNS Server settings
const byte DNS_PORT = 53;

CaptivePortal::CaptivePortal() :
    server(nullptr),
    dnsServer(nullptr),
    configured(false),
    running(false)
{
}

bool CaptivePortal::begin(const char* apSSID, const char* apPassword) {
    Serial.println("\n=== Starting WiFi Captive Portal ===");
    
    // Reset watchdog before potentially long operations
    esp_task_wdt_reset();
    
    // Stop any existing WiFi connection
    WiFi.disconnect(true);
    delay(100);
    esp_task_wdt_reset();
    
    // Start WiFi in AP mode
    WiFi.mode(WIFI_AP);
    delay(100);
    esp_task_wdt_reset();
    
    bool apStarted;
    if (apPassword && strlen(apPassword) > 0) {
        apStarted = WiFi.softAP(apSSID, apPassword);
        Serial.printf("Starting AP: %s (password protected)\n", apSSID);
    } else {
        apStarted = WiFi.softAP(apSSID);
        Serial.printf("Starting AP: %s (open network)\n", apSSID);
    }
    
    if (!apStarted) {
        Serial.println("ERROR: Failed to start AP mode!");
        return false;
    }
    
    delay(500); // Give AP time to start
    esp_task_wdt_reset(); // Reset after AP startup delay
    
    IPAddress apIP = WiFi.softAPIP();
    Serial.printf("AP IP address: %s\n", apIP.toString().c_str());
    Serial.printf("AP MAC address: %s\n", WiFi.softAPmacAddress().c_str());
    
    // Start DNS server for captive portal (redirect all domains to AP IP)
    dnsServer = new DNSServer();
    dnsServer->start(DNS_PORT, "*", apIP);
    Serial.println("DNS server started for captive portal");
    esp_task_wdt_reset(); // Reset after DNS startup
    
    // Start web server
    server = new WebServer(80);
    
    // Register handlers
    server->on("/", HTTP_GET, [this]() { handleRoot(); });
    server->on("/scan", HTTP_GET, [this]() { handleScan(); });
    server->on("/connect", HTTP_POST, [this]() { handleConnect(); });
    server->onNotFound([this]() { handleNotFound(); });
    
    server->begin();
    Serial.println("Web server started on port 80");
    esp_task_wdt_reset(); // Reset after web server startup
    
    // Don't auto-scan on startup - let user initiate scan
    // scanNetworks();
    
    running = true;
    configured = false;
    
    Serial.println("=== Captive Portal Ready ===");
    esp_task_wdt_reset(); // Final reset before returning
    Serial.printf("Connect to WiFi network: %s\n", apSSID);
    Serial.println("Configuration page should open automatically");
    Serial.printf("If not, open browser and go to: http://%s\n", apIP.toString().c_str());
    Serial.printf("Or try: http://192.168.4.1\n");
    
    return true;
}

void CaptivePortal::handleClient() {
    if (!running) return;
    
    if (dnsServer) {
        dnsServer->processNextRequest();
    }
    
    if (server) {
        server->handleClient();
    }
}

bool CaptivePortal::isConfigured() {
    return configured;
}

void CaptivePortal::stop() {
    if (server) {
        server->stop();
        delete server;
        server = nullptr;
    }
    
    if (dnsServer) {
        dnsServer->stop();
        delete dnsServer;
        dnsServer = nullptr;
    }
    
    WiFi.softAPdisconnect(true);
    running = false;
    
    Serial.println("Captive portal stopped");
}

String CaptivePortal::getAPIP() {
    return WiFi.softAPIP().toString();
}

void CaptivePortal::handleRoot() {
    Serial.println("Serving captive portal main page");
    
    String html = generateHeader("WiFi Setup");
    html += generateCSS();
    html += "</head><body>";
    html += generateMainPage();
    html += generateJavaScript();
    html += generateFooter();
    
    server->send(200, "text/html", html);
}

void CaptivePortal::handleScan() {
    Serial.println("Rescanning WiFi networks...");
    scanNetworks();
    
    String json = "{\"networks\":[";
    for (size_t i = 0; i < scannedNetworks.size(); i++) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"ssid\":\"" + escapeHtml(scannedNetworks[i].ssid) + "\",";
        json += "\"rssi\":" + String(scannedNetworks[i].rssi) + ",";
        json += "\"encrypted\":" + String(scannedNetworks[i].encrypted ? "true" : "false");
        json += "}";
    }
    json += "]}";
    
    server->send(200, "application/json", json);
}

void CaptivePortal::handleConnect() {
    String ssid = server->arg("ssid");
    String password = server->arg("password");
    
    Serial.printf("Attempting to connect to: %s\n", ssid.c_str());
    
    if (ssid.length() == 0) {
        server->send(400, "application/json", "{\"status\":\"error\",\"message\":\"SSID is required\"}");
        return;
    }
    
    // Save credentials to configuration
    configStorage.setWiFiSSID(ssid);
    configStorage.setWiFiPassword(password);
    configStorage.setWiFiProvisioned(true);
    configStorage.saveConfig();
    
    Serial.println("WiFi credentials saved to configuration");
    
    // Send response immediately before attempting connection
    String response = "{\"status\":\"success\",\"message\":\"WiFi credentials saved. Device will reboot in 3 seconds to apply changes.\"}";
    server->send(200, "application/json", response);
    
    // Give time for response to be sent
    delay(500);
    
    // Reset watchdog before attempting connection
    esp_task_wdt_reset();
    
    // Try to connect to verify credentials
    WiFi.mode(WIFI_AP_STA); // Keep AP running while testing connection
    WiFi.begin(ssid.c_str(), password.c_str());
    
    // Wait up to 10 seconds for connection with watchdog resets
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        esp_task_wdt_reset(); // Reset watchdog during connection
        attempts++;
    }
    Serial.println();
    
    // Always reboot after configuration attempt for clean startup
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connection successful!");
        Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
        Serial.println("Rebooting to apply configuration...");
        configured = true;
    } else {
        Serial.println("WiFi connection failed!");
        Serial.println("Rebooting to retry with saved credentials...");
        // Keep credentials saved - device will retry on boot
    }
    
    // Wait for HTTP response to be fully sent
    delay(2000);
    
    // Save crash logs before intentional reboot
    crashLogger.saveBeforeReboot();
    
    // Clean reboot
    ESP.restart();
}

void CaptivePortal::handleNotFound() {
    // Redirect all unknown requests to root (captive portal behavior)
    Serial.printf("Redirecting request: %s\n", server->uri().c_str());
    handleRoot();
}

String CaptivePortal::generateHeader(const String& title) {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='utf-8'>";
    html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>" + title + "</title>";
    return html;
}

String CaptivePortal::generateFooter() {
    String html = "<div class='footer'>";
    html += "<p>ESP32 AllSky Display - WiFi Configuration</p>";
    html += "<p style='font-size:0.8rem;color:#64748b;margin-top:0.5rem'>Select a network and enter the password to continue</p>";
    html += "</div></body></html>";
    return html;
}

String CaptivePortal::generateCSS() {
    // Use same theme as main web UI
    String css = "<style>";
    css += "@import url('https://fonts.googleapis.com/css2?family=Roboto:wght@300;400;500;700&display=swap');";
    css += "*{margin:0;padding:0;box-sizing:border-box}";
    css += "body{font-family:'Roboto',-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background-color:#0f172a;color:#f8fafc;min-height:100vh;display:flex;flex-direction:column;justify-content:center;align-items:center;padding:1rem;line-height:1.6}";
    css += ".container{max-width:600px;width:100%;background:#1e293b;border:1px solid #334155;border-radius:16px;padding:2rem;box-shadow:0 25px 50px -12px rgba(0,0,0,0.5)}";
    css += ".header{text-align:center;margin-bottom:2rem;padding-bottom:1.5rem;border-bottom:1px solid #334155}";
    css += "h1{color:#38bdf8;font-size:1.75rem;font-weight:700;margin-bottom:0.5rem}";
    css += ".subtitle{color:#94a3b8;font-size:0.95rem}";
    css += ".network-list{margin:1.5rem 0}";
    css += ".network-item{background:#0f172a;border:1px solid #334155;border-radius:12px;padding:1rem;margin-bottom:0.75rem;cursor:pointer;transition:all 0.2s ease;display:flex;align-items:center;justify-content:space-between}";
    css += ".network-item:hover{border-color:#38bdf8;transform:translateY(-2px);box-shadow:0 4px 12px rgba(56,189,248,0.2)}";
    css += ".network-item.selected{border-color:#38bdf8;background:rgba(56,189,248,0.1)}";
    css += ".network-info{flex:1}";
    css += ".network-ssid{color:#f8fafc;font-weight:500;font-size:1rem;margin-bottom:0.25rem}";
    css += ".network-signal{color:#94a3b8;font-size:0.85rem}";
    css += ".network-lock{color:#94a3b8;font-size:1.2rem}";
    css += ".password-section{margin-top:1.5rem;display:none}";
    css += ".password-section.show{display:block;animation:slideIn 0.3s ease}";
    css += "@keyframes slideIn{from{opacity:0;transform:translateY(-10px)}to{opacity:1;transform:translateY(0)}}";
    css += ".form-group{margin-bottom:1.25rem}";
    css += "label{display:block;margin-bottom:0.5rem;font-weight:500;color:#cbd5e1;font-size:0.95rem}";
    css += ".form-control{width:100%;padding:0.85rem;border:1px solid #475569;border-radius:8px;font-size:1rem;background:#334155;color:#f8fafc;transition:all 0.2s ease}";
    css += ".form-control:focus{outline:none;border-color:#38bdf8;box-shadow:0 0 0 3px rgba(56,189,248,0.2);background:#1e293b}";
    css += ".btn{width:100%;padding:0.85rem 1.5rem;border:none;border-radius:8px;font-weight:500;cursor:pointer;transition:all 0.2s ease;font-size:1rem;letter-spacing:0.3px}";
    css += ".btn-primary{background:#0ea5e9;color:white}";
    css += ".btn-primary:hover:not(:disabled){background:#0284c7;transform:translateY(-1px);box-shadow:0 4px 12px rgba(14,165,233,0.4)}";
    css += ".btn-primary:disabled{opacity:0.5;cursor:not-allowed}";
    css += ".btn-secondary{background:#475569;color:white;margin-top:0.5rem}";
    css += ".btn-secondary:hover{background:#334155}";
    css += ".alert{padding:1rem;border-radius:8px;margin-bottom:1.5rem;border-left:4px solid;font-size:0.95rem}";
    css += ".alert-info{background:rgba(14,165,233,0.1);border-color:#0ea5e9;color:#7dd3fc}";
    css += ".alert-success{background:rgba(16,185,129,0.1);border-color:#10b981;color:#6ee7b7}";
    css += ".alert-error{background:rgba(239,68,68,0.1);border-color:#ef4444;color:#fca5a5}";
    css += ".spinner{display:inline-block;width:16px;height:16px;border:2px solid rgba(255,255,255,0.3);border-top-color:#fff;border-radius:50%;animation:spin 0.6s linear infinite;margin-right:0.5rem}";
    css += "@keyframes spin{to{transform:rotate(360deg)}}";
    css += ".footer{text-align:center;margin-top:2rem;padding-top:1.5rem;border-top:1px solid #334155;color:#64748b;font-size:0.9rem}";
    css += ".scan-network-btn{background:#1e293b;padding:1.5rem;border-radius:12px;text-align:center;color:#f8fafc;border:1px solid #334155;cursor:pointer;transition:all 0.2s ease;position:relative;overflow:hidden;margin:1.5rem 0}";
    css += ".scan-network-btn:hover{transform:translateY(-2px);border-color:#38bdf8;box-shadow:0 10px 15px -3px rgba(0,0,0,0.3)}";
    css += ".scan-icon{font-size:2.5rem;margin-bottom:0.5rem;display:block}";
    css += ".scan-label{font-size:0.95rem;font-weight:500;color:#94a3b8;text-transform:uppercase;letter-spacing:0.5px}";
    css += "</style>";
    return css;
}

String CaptivePortal::generateJavaScript() {
    String js = "<script>";
    js += "let selectedSSID='';let selectedEncrypted=false;";
    
    // Network selection handler
    js += "function selectNetwork(ssid,encrypted){";
    js += "selectedSSID=ssid;selectedEncrypted=encrypted;";
    js += "document.querySelectorAll('.network-item').forEach(el=>el.classList.remove('selected'));";
    js += "event.currentTarget.classList.add('selected');";
    js += "const pwdSection=document.getElementById('passwordSection');";
    js += "pwdSection.classList.add('show');";
    js += "document.getElementById('ssid_display').textContent=ssid;";
    js += "const pwdInput=document.getElementById('password');";
    js += "if(encrypted){pwdInput.disabled=false;pwdInput.focus()}else{pwdInput.disabled=true;pwdInput.value=''}";
    js += "}";
    
    // Connect handler
    js += "function connectWiFi(){";
    js += "if(!selectedSSID){alert('Please select a network');return}";
    js += "const password=document.getElementById('password').value;";
    js += "if(selectedEncrypted&&!password){alert('Password is required for this network');return}";
    js += "const btn=document.getElementById('connectBtn');";
    js += "btn.disabled=true;";
    js += "btn.innerHTML='<span class=\"spinner\"></span>Connecting...';";
    js += "const formData=new FormData();";
    js += "formData.append('ssid',selectedSSID);";
    js += "formData.append('password',password);";
    js += "fetch('/connect',{method:'POST',body:formData})";
    js += ".then(r=>r.json())";
    js += ".then(data=>{";
    js += "if(data.status==='success'){";
    js += "document.getElementById('message').innerHTML='<div class=\"alert alert-success\">'+data.message+'</div>';";
    js += "setTimeout(()=>{window.location.href='http://'+data.ip+':8080'},3000)";
    js += "}else{";
    js += "document.getElementById('message').innerHTML='<div class=\"alert alert-error\">'+data.message+'</div>';";
    js += "btn.disabled=false;btn.innerHTML='Connect';";
    js += "}";
    js += "}).catch(e=>{";
    js += "document.getElementById('message').innerHTML='<div class=\"alert alert-error\">Connection failed: '+e.message+'</div>';";
    js += "btn.disabled=false;btn.innerHTML='Connect';";
    js += "});";
    js += "}";
    
    // Rescan handler
    js += "function rescanNetworks(e){";
    js += "if(e)e.preventDefault();";
    js += "const btn=e?e.currentTarget:document.querySelector('.scan-network-btn');";
    js += "const originalHTML=btn.innerHTML;";
    js += "btn.innerHTML='<span class=\"scan-icon\">⏳</span><div class=\"scan-label\">Scanning...</div>';";
    js += "fetch('/scan')";
    js += ".then(r=>r.json())";
    js += ".then(data=>{";
    js += "const list=document.getElementById('networkList');";
    js += "list.innerHTML='';";
    js += "data.networks.forEach(net=>{";
    js += "const signal=net.rssi>-60?'Excellent':net.rssi>-70?'Good':net.rssi>-80?'Fair':'Weak';";
    js += "const lockIcon=net.encrypted?'🔒':'';";
    js += "const item=document.createElement('div');";
    js += "item.className='network-item';";
    js += "item.onclick=function(){selectNetwork(net.ssid,net.encrypted);};";
    js += "item.innerHTML='<div class=\"network-info\"><div class=\"network-ssid\">'+net.ssid+'</div>';";
    js += "item.innerHTML+='<div class=\"network-signal\">Signal: '+signal+' ('+net.rssi+' dBm)</div></div>';";
    js += "item.innerHTML+='<div class=\"network-lock\">'+lockIcon+'</div>';";
    js += "list.appendChild(item);";
    js += "});";
    js += "btn.innerHTML=originalHTML;";
    js += "}).catch(e=>{btn.innerHTML=originalHTML;console.error(e);});";
    js += "}";
    
    // Auto-focus password field when network selected
    js += "document.addEventListener('DOMContentLoaded',function(){";
    js += "const pwdInput=document.getElementById('password');";
    js += "if(pwdInput){pwdInput.addEventListener('keypress',function(e){if(e.key==='Enter')connectWiFi()})}";
    js += "});";
    
    js += "</script>";
    return js;
}

String CaptivePortal::generateMainPage() {
    String html = "<div class='container'>";
    html += "<div class='header'>";
    html += "<h1>📡 WiFi Setup</h1>";
    html += "<div class='subtitle'>Configure your AllSky Display network connection</div>";
    html += "</div>";
    
    html += "<div id='message'></div>";
    
    html += "<div class='alert alert-info'>";
    html += "Select your WiFi network from the list below";
    html += "</div>";
    
    html += "<div class='network-list'>";
    html += "<div id='networkList'>";
    html += generateNetworkList();
    html += "</div>";
    html += "<div class='scan-network-btn' onclick='rescanNetworks(event)'>";
    html += "<span class='scan-icon'>📡</span>";
    html += "<div class='scan-label'>Scan for Networks</div>";
    html += "</div>";
    html += "</div>";
    
    html += "<div id='passwordSection' class='password-section'>";
    html += "<div class='form-group'>";
    html += "<label>Selected Network: <strong><span id='ssid_display'></span></strong></label>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label for='password'>WiFi Password</label>";
    html += "<input type='password' id='password' class='form-control' placeholder='Enter network password'>";
    html += "</div>";
    html += "<button id='connectBtn' class='btn btn-primary' onclick='connectWiFi()'>Connect</button>";
    html += "</div>";
    
    html += "</div>";
    
    return html;
}

String CaptivePortal::generateNetworkList() {
    String html = "";
    
    if (scannedNetworks.empty()) {
        html += "<div class='alert alert-info'>Click the Scan for Networks button below to find available WiFi networks</div>";
        return html;
    }
    
    for (const auto& network : scannedNetworks) {
        String signalQuality;
        if (network.rssi > -60) signalQuality = "Excellent";
        else if (network.rssi > -70) signalQuality = "Good";
        else if (network.rssi > -80) signalQuality = "Fair";
        else signalQuality = "Weak";
        
        // Escape quotes properly for JavaScript
        String escapedSSID = network.ssid;
        escapedSSID.replace("\\", "\\\\");
        escapedSSID.replace("'", "\\'");
        escapedSSID.replace("\"", "&quot;");
        
        html += "<div class='network-item' onclick=\"selectNetwork('" + escapedSSID + "'," + String(network.encrypted ? "true" : "false") + ")\">";
        html += "<div class='network-info'>";
        html += "<div class='network-ssid'>" + escapeHtml(network.ssid) + "</div>";
        html += "<div class='network-signal'>Signal: " + signalQuality + " (" + String(network.rssi) + " dBm)</div>";
        html += "</div>";
        html += "<div class='network-lock'>" + String(network.encrypted ? "🔒" : "") + "</div>";
        html += "</div>";
    }
    
    return html;
}

void CaptivePortal::scanNetworks() {
    Serial.println("Scanning WiFi networks...");
    scannedNetworks.clear();
    
    int n = WiFi.scanNetworks(false, true); // async=false, show_hidden=true
    
    Serial.printf("Found %d networks\n", n);
    
    for (int i = 0; i < n; i++) {
        WiFiNetwork network;
        network.ssid = WiFi.SSID(i);
        network.rssi = WiFi.RSSI(i);
        network.encrypted = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        
        // Skip duplicate SSIDs (keep strongest signal)
        bool isDuplicate = false;
        for (auto& existing : scannedNetworks) {
            if (existing.ssid == network.ssid) {
                isDuplicate = true;
                if (network.rssi > existing.rssi) {
                    existing = network; // Replace with stronger signal
                }
                break;
            }
        }
        
        if (!isDuplicate && network.ssid.length() > 0) {
            scannedNetworks.push_back(network);
        }
    }
    
    // Sort by signal strength (strongest first)
    std::sort(scannedNetworks.begin(), scannedNetworks.end(), 
              [](const WiFiNetwork& a, const WiFiNetwork& b) { return a.rssi > b.rssi; });
    
    Serial.printf("Displaying %d unique networks\n", scannedNetworks.size());
}

String CaptivePortal::encryptionTypeToString(wifi_auth_mode_t encryptionType) {
    switch (encryptionType) {
        case WIFI_AUTH_OPEN: return "Open";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2 Enterprise";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
        default: return "Unknown";
    }
}

String CaptivePortal::escapeHtml(const String& input) {
    String output = input;
    output.replace("&", "&amp;");
    output.replace("<", "&lt;");
    output.replace(">", "&gt;");
    output.replace("\"", "&quot;");
    output.replace("'", "&#39;");
    return output;
}
