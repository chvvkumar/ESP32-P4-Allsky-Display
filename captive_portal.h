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
    
    // Route handlers
    void handleRoot();
    void handleScan();
    void handleConnect();
    void handleNotFound();
    
    // HTML generators (matching current theme)
    String generateHeader(const String& title);
    String generateFooter();
    String generateCSS();
    String generateJavaScript();
    String generateMainPage();
    String generateNetworkList();
    
    // Utility functions
    void scanNetworks();
    String encryptionTypeToString(wifi_auth_mode_t encryptionType);
    String escapeHtml(const String& input);
};

// Global instance
extern CaptivePortal captivePortal;

#endif // CAPTIVE_PORTAL_H
