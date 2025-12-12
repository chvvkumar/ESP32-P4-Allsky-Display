#pragma once
#ifndef WEB_CONFIG_H
#define WEB_CONFIG_H

#include <Arduino.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>
#include <ElegantOTA.h>
#include "config_storage.h"
#include "config.h"  // For LogSeverity enum

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
    WebSocketsServer* wsServer;
    bool serverRunning;
    bool otaInProgress;
    
    // WebSocket handlers
    static void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
    
    // Route handlers
    void handleRoot();
    void handleConsole();
    void handleNetworkConfig();
    void handleMQTTConfig();
    void handleImageConfig();
    void handleDisplayConfig();
    void handleAdvancedConfig();
    void handleSerialCommands();
    void handleStatus();
    void handleSaveConfig();
    void handleAddImageSource();
    void handleRemoveImageSource();
    void handleUpdateImageSource();
    void handleClearImageSources();
    void handleBulkDeleteImageSources();
    void handleNextImage();
    void handleUpdateImageTransform();
    void handleCopyDefaultsToImage();
    void handleApplyTransform();
    void handleRestart();
    void handleFactoryReset();
    void handleSetLogSeverity();
    void handleClearCrashLogs();
    void handleGetHealth();
    
public:
    // WebSocket log broadcasting with severity filtering
    void broadcastLog(const char* message, uint16_t color = 0xFFFF, LogSeverity severity = LOG_INFO);
    
    // OTA status
    bool isOTAInProgress() const { return otaInProgress; }
    void setOTAInProgress(bool inProgress) { otaInProgress = inProgress; }
    
    // WebSocket loop handler
    void loopWebSocket();
    
private:
    void sendCrashLogsToClient(uint8_t clientNum);
    
private:
    void handleNotFound();
    void handleAPIReference();
    void handleGetAllInfo();
    void handleCurrentImage();
    
    // HTML generators
    String generateHeader(const String& title);
    String generateFooter();
    String generateNavigation(const String& currentPage = "");
    String generateMainPage();
    String generateNetworkPage();
    String generateConsolePage();
    String generateMQTTPage();
    String generateImagePage();
    String generateDisplayPage();
    String generateAdvancedPage();
    String generateStatusPage();
    String generateSerialCommandsPage();
    String generateAPIReferencePage();
    
    // Utility functions
    String getSystemStatus();
    String formatUptime(unsigned long ms);
    String formatBytes(size_t bytes);
    String getConnectionStatus();
    String escapeHtml(const String& input);
    String escapeJson(const String& input);
    void sendResponse(int code, const String& contentType, const String& content);
    void applyImageSettings();
    void reloadConfiguration();
};

// Global instance
extern WebConfig webConfig;

#endif // WEB_CONFIG_H
