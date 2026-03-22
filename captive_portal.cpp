#include "captive_portal.h"
#include "crash_logger.h"
#include "logging.h"
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
    running(false),
    scanInProgress(false)
{
}

bool CaptivePortal::begin(const char* apSSID, const char* apPassword) {
    LOG_DEBUG("[CaptivePortal] Starting WiFi setup captive portal");

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
        LOG_INFO_F("[CaptivePortal] Starting AP: %s (password protected)\n", apSSID);
        apStarted = WiFi.softAP(apSSID, apPassword);
    } else {
        LOG_INFO_F("[CaptivePortal] Starting AP: %s (open network)\n", apSSID);
        apStarted = WiFi.softAP(apSSID);
    }

    if (!apStarted) {
        LOG_ERROR("[CaptivePortal] Failed to start AP mode!");
        return false;
    }

    delay(500); // Give AP time to start
    esp_task_wdt_reset(); // Reset after AP startup delay

    IPAddress apIP = WiFi.softAPIP();
    LOG_INFO_F("[CaptivePortal] AP IP address: %s\n", apIP.toString().c_str());
    LOG_DEBUG_F("[CaptivePortal] AP MAC address: %s\n", WiFi.softAPmacAddress().c_str());

    // Start DNS server for captive portal (redirect all domains to AP IP)
    dnsServer = new DNSServer();
    dnsServer->start(DNS_PORT, "*", apIP);
    LOG_INFO("[CaptivePortal] DNS server started (redirecting all domains to AP)");
    esp_task_wdt_reset(); // Reset after DNS startup

    // Start web server
    server = new WebServer(80);

    // Register handlers
    LOG_DEBUG("[CaptivePortal] Registering web server routes");
    server->on("/", HTTP_GET, [this]() { handleRoot(); });
    server->on("/scan", HTTP_GET, [this]() { handleScan(); });
    server->on("/scan/results", HTTP_GET, [this]() { handleScanResults(); });
    server->on("/connect", HTTP_POST, [this]() { handleConnect(); });
    server->onNotFound([this]() { handleNotFound(); });

    server->begin();
    LOG_INFO("[CaptivePortal] Web server started on port 80");
    esp_task_wdt_reset(); // Reset after web server startup

    // Don't auto-scan on startup - let user initiate scan
    // scanNetworks();

    running = true;
    configured = false;

    LOG_INFO("[CaptivePortal] Setup complete - captive portal ready");
    esp_task_wdt_reset(); // Final reset before returning
    LOG_INFO_F("[CaptivePortal] Connect to WiFi network: %s\n", apSSID);
    LOG_INFO("[CaptivePortal] Configuration page should open automatically");
    LOG_INFO_F("[CaptivePortal] Manual access: http://%s or http://192.168.4.1\n", apIP.toString().c_str());

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

    // Collect async scan results when ready
    if (scanInProgress) {
        int result = WiFi.scanComplete();
        if (result >= 0) {
            collectScanResults();
            scanInProgress = false;
        } else if (result == WIFI_SCAN_FAILED) {
            scanInProgress = false;
            LOG_ERROR("[CaptivePortal] Async WiFi scan failed");
        }
        // result == WIFI_SCAN_RUNNING: still in progress, check next loop
    }
}

bool CaptivePortal::isConfigured() {
    return configured;
}

void CaptivePortal::stop() {
    LOG_INFO("[CaptivePortal] Stopping captive portal");
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

    LOG_INFO("[CaptivePortal] Captive portal stopped successfully");
}

String CaptivePortal::getAPIP() {
    return WiFi.softAPIP().toString();
}

// ---- Chunked HTML response handlers ----

void CaptivePortal::handleRoot() {
    LOG_DEBUG("[CaptivePortal] Serving WiFi setup page (chunked)");

    server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server->send(200, "text/html", "");

    sendHeader("WiFi Setup");
    sendCSS();
    server->sendContent("</head><body>");
    sendMainPage();
    sendJavaScript();
    sendFooter();

    server->sendContent("");  // Signal end of chunked transfer
}

void CaptivePortal::handleScan() {
    LOG_INFO("[CaptivePortal] WiFi network scan requested (async)");

    if (scanInProgress) {
        server->send(200, "application/json", "{\"status\":\"scanning\"}");
        return;
    }

    // Start async scan — results collected in handleClient()
    startAsyncScan();
    server->send(200, "application/json", "{\"status\":\"scanning\"}");
}

void CaptivePortal::handleScanResults() {
    if (scanInProgress) {
        server->send(200, "application/json", "{\"status\":\"scanning\"}");
        return;
    }

    // Build JSON from cached results using proper JSON escaping
    String json;
    json.reserve(128 * scannedNetworks.size() + 64);
    json = "{\"status\":\"complete\",\"networks\":[";

    for (size_t i = 0; i < scannedNetworks.size(); i++) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"ssid\":\"" + escapeJson(scannedNetworks[i].ssid) + "\",";
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

    LOG_INFO_F("[CaptivePortal] Connection request for SSID: %s\n", ssid.c_str());

    if (ssid.length() == 0) {
        LOG_WARNING("[CaptivePortal] Connection request rejected - SSID is empty");
        server->send(400, "application/json", "{\"status\":\"error\",\"message\":\"SSID is required\"}");
        return;
    }

    // Save credentials to configuration
    configStorage.setWiFiSSID(ssid);
    configStorage.setWiFiPassword(password);
    configStorage.setWiFiProvisioned(true);
    configStorage.saveConfig();

    LOG_INFO("[CaptivePortal] WiFi credentials saved to NVS storage");

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
        LOG_INFO("[CaptivePortal] WiFi connection successful!");
        LOG_INFO_F("[CaptivePortal] Assigned IP address: %s\n", WiFi.localIP().toString().c_str());
        LOG_INFO("[CaptivePortal] Rebooting to apply configuration");
        configured = true;
    } else {
        LOG_WARNING("[CaptivePortal] WiFi connection test failed");
        LOG_INFO("[CaptivePortal] Rebooting to retry with saved credentials");
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
    LOG_DEBUG_F("[CaptivePortal] Redirecting request to root: %s\n", server->uri().c_str());
    handleRoot();
}

// ---- Chunked HTML senders ----

void CaptivePortal::sendHeader(const String& title) {
    server->sendContent("<!DOCTYPE html><html><head>");
    server->sendContent("<meta charset='utf-8'>");
    server->sendContent("<meta name='viewport' content='width=device-width,initial-scale=1'>");
    server->sendContent("<title>" + title + "</title>");
}

void CaptivePortal::sendFooter() {
    server->sendContent("<div class='footer'>");
    server->sendContent("<p>" + configStorage.getDeviceName() + " - WiFi Configuration</p>");
    server->sendContent("<p style='font-size:0.8rem;color:#64748b;margin-top:0.5rem'>Select a network and enter the password to continue</p>");
    server->sendContent("</div></body></html>");
}

void CaptivePortal::sendCSS() {
    // Send CSS in chunks to avoid building one huge String
    server->sendContent("<style>");
    server->sendContent("@import url('https://fonts.googleapis.com/css2?family=Roboto:wght@300;400;500;700&display=swap');");
    server->sendContent("*{margin:0;padding:0;box-sizing:border-box}");
    server->sendContent("body{font-family:'Roboto',-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background-color:#0f172a;color:#f8fafc;min-height:100vh;display:flex;flex-direction:column;justify-content:center;align-items:center;padding:1rem;line-height:1.6}");
    server->sendContent(".container{max-width:600px;width:100%;background:#1e293b;border:1px solid #334155;border-radius:16px;padding:2rem;box-shadow:0 25px 50px -12px rgba(0,0,0,0.5)}");
    server->sendContent(".header{text-align:center;margin-bottom:2rem;padding-bottom:1.5rem;border-bottom:1px solid #334155}");
    server->sendContent("h1{color:#38bdf8;font-size:1.75rem;font-weight:700;margin-bottom:0.5rem}");
    server->sendContent(".subtitle{color:#94a3b8;font-size:0.95rem}");
    server->sendContent(".network-list{margin:1.5rem 0}");
    server->sendContent(".network-item{background:#0f172a;border:1px solid #334155;border-radius:12px;padding:1rem;margin-bottom:0.75rem;cursor:pointer;transition:all 0.2s ease;display:flex;align-items:center;justify-content:space-between}");
    server->sendContent(".network-item:hover{border-color:#38bdf8;transform:translateY(-2px);box-shadow:0 4px 12px rgba(56,189,248,0.2)}");
    server->sendContent(".network-item.selected{border-color:#38bdf8;background:rgba(56,189,248,0.1)}");
    server->sendContent(".network-info{flex:1}");
    server->sendContent(".network-ssid{color:#f8fafc;font-weight:500;font-size:1rem;margin-bottom:0.25rem}");
    server->sendContent(".network-signal{color:#94a3b8;font-size:0.85rem}");
    server->sendContent(".network-lock{color:#94a3b8;font-size:1.2rem}");
    server->sendContent(".password-section{margin-top:1.5rem;display:none}");
    server->sendContent(".password-section.show{display:block;animation:slideIn 0.3s ease}");
    server->sendContent("@keyframes slideIn{from{opacity:0;transform:translateY(-10px)}to{opacity:1;transform:translateY(0)}}");
    server->sendContent(".form-group{margin-bottom:1.25rem}");
    server->sendContent("label{display:block;margin-bottom:0.5rem;font-weight:500;color:#cbd5e1;font-size:0.95rem}");
    server->sendContent(".form-control{width:100%;padding:0.85rem;border:1px solid #475569;border-radius:8px;font-size:1rem;background:#334155;color:#f8fafc;transition:all 0.2s ease}");
    server->sendContent(".form-control:focus{outline:none;border-color:#38bdf8;box-shadow:0 0 0 3px rgba(56,189,248,0.2);background:#1e293b}");
    server->sendContent(".btn{width:100%;padding:0.85rem 1.5rem;border:none;border-radius:8px;font-weight:500;cursor:pointer;transition:all 0.2s ease;font-size:1rem;letter-spacing:0.3px}");
    server->sendContent(".btn-primary{background:#0ea5e9;color:white}");
    server->sendContent(".btn-primary:hover:not(:disabled){background:#0284c7;transform:translateY(-1px);box-shadow:0 4px 12px rgba(14,165,233,0.4)}");
    server->sendContent(".btn-primary:disabled{opacity:0.5;cursor:not-allowed}");
    server->sendContent(".btn-secondary{background:#475569;color:white;margin-top:0.5rem}");
    server->sendContent(".btn-secondary:hover{background:#334155}");
    server->sendContent(".alert{padding:1rem;border-radius:8px;margin-bottom:1.5rem;border-left:4px solid;font-size:0.95rem}");
    server->sendContent(".alert-info{background:rgba(14,165,233,0.1);border-color:#0ea5e9;color:#7dd3fc}");
    server->sendContent(".alert-success{background:rgba(16,185,129,0.1);border-color:#10b981;color:#6ee7b7}");
    server->sendContent(".alert-error{background:rgba(239,68,68,0.1);border-color:#ef4444;color:#fca5a5}");
    server->sendContent(".spinner{display:inline-block;width:16px;height:16px;border:2px solid rgba(255,255,255,0.3);border-top-color:#fff;border-radius:50%;animation:spin 0.6s linear infinite;margin-right:0.5rem}");
    server->sendContent("@keyframes spin{to{transform:rotate(360deg)}}");
    server->sendContent(".footer{text-align:center;margin-top:2rem;padding-top:1.5rem;border-top:1px solid #334155;color:#64748b;font-size:0.9rem}");
    server->sendContent(".scan-network-btn{background:#1e293b;padding:1.5rem;border-radius:12px;text-align:center;color:#f8fafc;border:1px solid #334155;cursor:pointer;transition:all 0.2s ease;position:relative;overflow:hidden;margin:1.5rem 0}");
    server->sendContent(".scan-network-btn:hover{transform:translateY(-2px);border-color:#38bdf8;box-shadow:0 10px 15px -3px rgba(0,0,0,0.3)}");
    server->sendContent(".scan-icon{font-size:2.5rem;margin-bottom:0.5rem;display:block}");
    server->sendContent(".scan-label{font-size:0.95rem;font-weight:500;color:#94a3b8;text-transform:uppercase;letter-spacing:0.5px}");
    server->sendContent("</style>");
}

void CaptivePortal::sendJavaScript() {
    // JavaScript uses async scan polling via /scan and /scan/results endpoints
    server->sendContent("<script>");
    server->sendContent("let selectedSSID='';let selectedEncrypted=false;");

    // Network selection handler
    server->sendContent("function selectNetwork(ssid,encrypted,evt){");
    server->sendContent("selectedSSID=ssid;selectedEncrypted=encrypted;");
    server->sendContent("document.querySelectorAll('.network-item').forEach(el=>el.classList.remove('selected'));");
    server->sendContent("if(evt&&evt.currentTarget){evt.currentTarget.classList.add('selected')}");
    server->sendContent("const pwdSection=document.getElementById('passwordSection');");
    server->sendContent("pwdSection.classList.add('show');");
    server->sendContent("document.getElementById('ssid_display').textContent=ssid;");
    server->sendContent("const pwdInput=document.getElementById('password');");
    server->sendContent("if(encrypted){pwdInput.disabled=false;pwdInput.focus()}else{pwdInput.disabled=true;pwdInput.value=''}");
    server->sendContent("}");

    // Connect handler
    server->sendContent("function connectWiFi(){");
    server->sendContent("if(!selectedSSID){alert('Please select a network');return}");
    server->sendContent("const password=document.getElementById('password').value;");
    server->sendContent("if(selectedEncrypted&&!password){alert('Password is required for this network');return}");
    server->sendContent("const btn=document.getElementById('connectBtn');");
    server->sendContent("btn.disabled=true;");
    server->sendContent("btn.innerHTML='<span class=\"spinner\"></span>Connecting...';");
    server->sendContent("const formData=new FormData();");
    server->sendContent("formData.append('ssid',selectedSSID);");
    server->sendContent("formData.append('password',password);");
    server->sendContent("fetch('/connect',{method:'POST',body:formData})");
    server->sendContent(".then(r=>r.json())");
    server->sendContent(".then(data=>{");
    server->sendContent("if(data.status==='success'){");
    server->sendContent("document.getElementById('message').innerHTML='<div class=\"alert alert-success\">'+data.message+'</div>';");
    server->sendContent("setTimeout(()=>{window.location.href='http://'+data.ip+':8080'},3000)");
    server->sendContent("}else{");
    server->sendContent("document.getElementById('message').innerHTML='<div class=\"alert alert-error\">'+data.message+'</div>';");
    server->sendContent("btn.disabled=false;btn.innerHTML='Connect';");
    server->sendContent("}");
    server->sendContent("}).catch(e=>{");
    server->sendContent("document.getElementById('message').innerHTML='<div class=\"alert alert-error\">Connection failed: '+e.message+'</div>';");
    server->sendContent("btn.disabled=false;btn.innerHTML='Connect';");
    server->sendContent("});");
    server->sendContent("}");

    // Async rescan handler — starts scan, then polls for results
    server->sendContent("function rescanNetworks(e){");
    server->sendContent("if(e)e.preventDefault();");
    server->sendContent("const btn=e?e.currentTarget:document.querySelector('.scan-network-btn');");
    server->sendContent("const originalHTML=btn.innerHTML;");
    server->sendContent("btn.innerHTML='<span class=\"scan-icon\">&#9203;</span><div class=\"scan-label\">Scanning...</div>';");
    server->sendContent("fetch('/scan').then(()=>{pollScanResults(btn,originalHTML)});");
    server->sendContent("}");

    // Poll /scan/results until complete
    server->sendContent("function pollScanResults(btn,originalHTML){");
    server->sendContent("setTimeout(()=>{");
    server->sendContent("fetch('/scan/results').then(r=>r.json()).then(data=>{");
    server->sendContent("if(data.status==='scanning'){pollScanResults(btn,originalHTML);return}");
    server->sendContent("const list=document.getElementById('networkList');");
    server->sendContent("list.innerHTML='';");
    server->sendContent("if(data.networks){data.networks.forEach(net=>{");
    server->sendContent("const signal=net.rssi>-60?'Excellent':net.rssi>-70?'Good':net.rssi>-80?'Fair':'Weak';");
    server->sendContent("const lockIcon=net.encrypted?'&#128274;':'';");
    server->sendContent("const item=document.createElement('div');");
    server->sendContent("item.className='network-item';");
    server->sendContent("item.onclick=function(evt){selectNetwork(net.ssid,net.encrypted,evt);};");
    server->sendContent("item.innerHTML='<div class=\"network-info\"><div class=\"network-ssid\">'+net.ssid+'</div>';");
    server->sendContent("item.innerHTML+='<div class=\"network-signal\">Signal: '+signal+' ('+net.rssi+' dBm)</div></div>';");
    server->sendContent("item.innerHTML+='<div class=\"network-lock\">'+lockIcon+'</div>';");
    server->sendContent("list.appendChild(item);");
    server->sendContent("});}");
    server->sendContent("btn.innerHTML=originalHTML;");
    server->sendContent("}).catch(e=>{btn.innerHTML=originalHTML;console.error(e);});");
    server->sendContent("},1500);");  // Poll every 1.5 seconds
    server->sendContent("}");

    // Auto-focus password field when network selected
    server->sendContent("document.addEventListener('DOMContentLoaded',function(){");
    server->sendContent("const pwdInput=document.getElementById('password');");
    server->sendContent("if(pwdInput){pwdInput.addEventListener('keypress',function(e){if(e.key==='Enter')connectWiFi()})}");
    server->sendContent("});");

    server->sendContent("</script>");
}

void CaptivePortal::sendMainPage() {
    server->sendContent("<div class='container'>");
    server->sendContent("<div class='header'>");
    server->sendContent("<h1>&#128225; WiFi Setup</h1>");
    server->sendContent("<div class='subtitle'>Configure your AllSky Display network connection</div>");
    server->sendContent("</div>");

    server->sendContent("<div id='message'></div>");

    server->sendContent("<div class='alert alert-info'>");
    server->sendContent("Select your WiFi network from the list below");
    server->sendContent("</div>");

    server->sendContent("<div class='network-list'>");
    server->sendContent("<div id='networkList'>");
    sendNetworkList();
    server->sendContent("</div>");
    server->sendContent("<div class='scan-network-btn' onclick='rescanNetworks(event)'>");
    server->sendContent("<span class='scan-icon'>&#128225;</span>");
    server->sendContent("<div class='scan-label'>Scan for Networks</div>");
    server->sendContent("</div>");
    server->sendContent("</div>");

    server->sendContent("<div id='passwordSection' class='password-section'>");
    server->sendContent("<div class='form-group'>");
    server->sendContent("<label>Selected Network: <strong><span id='ssid_display'></span></strong></label>");
    server->sendContent("</div>");
    server->sendContent("<div class='form-group'>");
    server->sendContent("<label for='password'>WiFi Password</label>");
    server->sendContent("<input type='password' id='password' class='form-control' placeholder='Enter network password'>");
    server->sendContent("</div>");
    server->sendContent("<button id='connectBtn' class='btn btn-primary' onclick='connectWiFi()'>Connect</button>");
    server->sendContent("</div>");

    server->sendContent("</div>");
}

void CaptivePortal::sendNetworkList() {
    if (scannedNetworks.empty()) {
        server->sendContent("<div class='alert alert-info'>Click the Scan for Networks button below to find available WiFi networks</div>");
        return;
    }

    for (const auto& network : scannedNetworks) {
        String signalQuality;
        if (network.rssi > -60) signalQuality = "Excellent";
        else if (network.rssi > -70) signalQuality = "Good";
        else if (network.rssi > -80) signalQuality = "Fair";
        else signalQuality = "Weak";

        // Escape quotes properly for JavaScript onclick attribute
        String escapedSSID = network.ssid;
        escapedSSID.replace("\\", "\\\\");
        escapedSSID.replace("'", "\\'");
        escapedSSID.replace("\"", "&quot;");

        String html;
        html.reserve(512);
        html += "<div class='network-item' onclick=\"selectNetwork('" + escapedSSID + "'," + String(network.encrypted ? "true" : "false") + ",event)\">";
        html += "<div class='network-info'>";
        html += "<div class='network-ssid'>" + escapeHtml(network.ssid) + "</div>";
        html += "<div class='network-signal'>Signal: " + signalQuality + " (" + String(network.rssi) + " dBm)</div>";
        html += "</div>";
        html += "<div class='network-lock'>" + String(network.encrypted ? "&#128274;" : "") + "</div>";
        html += "</div>";

        server->sendContent(html);
    }
}

// ---- Async WiFi scan ----

void CaptivePortal::startAsyncScan() {
    Serial.println("[CaptivePortal] Starting async WiFi scan...");
    scannedNetworks.clear();
    scanInProgress = true;

    // Start non-blocking scan (async=true, show_hidden=true)
    WiFi.scanNetworks(true, true);
}

void CaptivePortal::collectScanResults() {
    int n = WiFi.scanComplete();
    if (n < 0) return;  // Not ready or failed

    Serial.printf("[CaptivePortal] Async scan found %d networks\n", n);
    scannedNetworks.clear();

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

    // Free scan memory
    WiFi.scanDelete();

    Serial.printf("[CaptivePortal] %d unique networks cached\n", scannedNetworks.size());
}

// ---- Utility ----

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

String CaptivePortal::escapeJson(const String& input) {
    String output;
    output.reserve(input.length() + 16);
    for (unsigned int i = 0; i < input.length(); i++) {
        char c = input.charAt(i);
        switch (c) {
            case '\\': output += "\\\\"; break;
            case '"':  output += "\\\""; break;
            case '\n': output += "\\n";  break;
            case '\r': output += "\\r";  break;
            case '\t': output += "\\t";  break;
            default:
                if (c < 0x20) {
                    // Escape other control characters
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
                    output += buf;
                } else {
                    output += c;
                }
                break;
        }
    }
    return output;
}
