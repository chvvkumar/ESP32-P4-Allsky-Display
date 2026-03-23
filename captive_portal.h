#pragma once
#ifndef CAPTIVE_PORTAL_H
#define CAPTIVE_PORTAL_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "config_storage.h"

// Captive portal for WiFi configuration on first boot
class CaptivePortal {
public:
    CaptivePortal();
    
    // Start captive portal in AP mode
    bool begin(const char* apSSID = "AllSky-Display-Setup", const char* apPassword = nullptr);
    
    // Handle client requests
    void handleClient();
    
    // Check if WiFi credentials have been configured
    bool isConfigured();
    
    // Stop the captive portal
    void stop();
    
    // Get the AP IP address
    String getAPIP();

private:
    WebServer* server;
    DNSServer* dnsServer;
    bool configured;
    bool running;
    
    // Scanned networks
    struct WiFiNetwork {
        String ssid;
        int rssi;
        bool encrypted;
    };
    std::vector<WiFiNetwork> scannedNetworks;
    
    // Async scan state
    bool scanInProgress;

    // Route handlers
    void handleRoot();
    void handleScan();
    void handleScanResults();
    void handleConnect();
    void handleNotFound();

    // Chunked HTML helpers — send parts directly to avoid huge String
    void sendHeader(const String& title);
    void sendCSS();
    void sendMainPage();
    void sendNetworkList();
    void sendJavaScript();
    void sendFooter();

    // Utility functions
    void startAsyncScan();
    void collectScanResults();
    String encryptionTypeToString(wifi_auth_mode_t encryptionType);
    String escapeHtml(const String& input);
    String escapeJson(const String& input);
};

// Global instance
extern CaptivePortal captivePortal;

#endif // CAPTIVE_PORTAL_H
