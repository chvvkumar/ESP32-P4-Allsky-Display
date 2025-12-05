#pragma once
#ifndef WEB_CONFIG_H
#define WEB_CONFIG_H

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include "config_storage.h"

class WebConfig {
public:
    WebConfig();
    
    // Initialize web server
    bool begin(int port = 80);
    
    // Handle client requests
    void handleClient();
    
    // Check if server is running
    bool isRunning();
    
    // Stop the server
    void stop();

private:
    WebServer* server;
    bool serverRunning;
    
    // Route handlers
    void handleRoot();
    void handleNetworkConfig();
    void handleMQTTConfig();
    void handleImageConfig();
    void handleImageSources();
    void handleDisplayConfig();
    void handleAdvancedConfig();
    void handleSerialCommands();
    void handleStatus();
    void handleSaveConfig();
    void handleAddImageSource();
    void handleRemoveImageSource();
    void handleUpdateImageSource();
    void handleClearImageSources();
    void handleNextImage();
    void handleUpdateImageTransform();
    void handleCopyDefaultsToImage();
    void handleApplyTransform();
    void handleRestart();
    void handleFactoryReset();
    void handleNotFound();
    
    // HTML generators
    String generateHeader(const String& title);
    String generateFooter();
    String generateNavigation(const String& currentPage = "");
    String generateMainPage();
    String generateNetworkPage();
    String generateMQTTPage();
    String generateImagePage();
    String generateImageSourcesPage();
    String generateDisplayPage();
    String generateAdvancedPage();
    String generateStatusPage();
    String generateSerialCommandsPage();
    
    // Utility functions
    String getSystemStatus();
    String formatUptime(unsigned long ms);
    String formatBytes(size_t bytes);
    String getConnectionStatus();
    String escapeHtml(const String& input);
    void sendResponse(int code, const String& contentType, const String& content);
    void applyImageSettings();
    void reloadConfiguration();
};

// Global instance
extern WebConfig webConfig;

#endif // WEB_CONFIG_H
