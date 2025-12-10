/**
 * Command Interpreter Implementation
 * 
 * Handles serial command processing with clean separation from main code.
 */

#include "command_interpreter.h"
#include "device_health.h"

// Global instance definition
CommandInterpreter& commandInterpreter = CommandInterpreter::getInstance();

// Main command processing loop
void CommandInterpreter::processCommands() {
    if (Serial.available()) {
        char command = Serial.read();
        
        switch (command) {
            // Scaling commands
            case '+':
                handleScaleIncrease();
                break;
            case '-':
                handleScaleDecrease();
                break;
                
            // Movement commands
            case 'W':
            case 'w':
                handleMoveUp();
                break;
            case 'S':
            case 's':
                handleMoveDown();
                break;
            case 'A':
            case 'a':
                handleMoveLeft();
                break;
            case 'D':
            case 'd':
                handleMoveRight();
                break;
                
            // Rotation commands
            case 'Q':
            case 'q':
                handleRotateCCW();
                break;
            case 'E':
            case 'e':
                handleRotateCW();
                break;
                
            // Navigation commands
            case 'N':
            case 'n':
                handleNextImage();
                break;
            case 'F':
            case 'f':
                handleForceRefresh();
                break;
                
            // Reset/Save commands
            case 'R':
            case 'r':
                handleResetTransforms();
                break;
            case 'V':
            case 'v':
                handleSaveTransforms();
                break;
                
            // System control
            case 'B':
            case 'b':
                handleReboot();
                break;
            case 'H':
            case 'h':
            case '?':
                handleHelp();
                break;
                
            // Brightness commands
            case 'L':
            case 'l':
                handleBrightnessUp();
                break;
            case 'K':
            case 'k':
                handleBrightnessDown();
                break;
                
            // System info commands
            case 'M':
            case 'm':
                handleMemoryInfo();
                break;
            case 'I':
            case 'i':
                handleNetworkInfo();
                break;
            case 'P':
            case 'p':
                handlePPAInfo();
                break;
            case 'T':
            case 't':
                handleMQTTInfo();
                break;
            case 'X':
            case 'x':
                handleWebServerStatus();
                break;
            case 'G':
            case 'g':
                handleHealthDiagnostics();
                break;
            
            default:
                // Ignore unknown commands silently
                break;
        }
        
        // Clear any remaining characters in buffer
        while (Serial.available()) {
            Serial.read();
        }
    }
}

// ============================================================================
// COMMAND HANDLERS - To be implemented in subtasks 1.3b-1.3d
// ============================================================================

// Scaling commands
void CommandInterpreter::handleScaleIncrease() {
    scaleX = constrain(scaleX + SCALE_STEP, MIN_SCALE, MAX_SCALE);
    scaleY = constrain(scaleY + SCALE_STEP, MIN_SCALE, MAX_SCALE);
    configStorage.setImageScaleX(currentImageIndex, scaleX);
    configStorage.setImageScaleY(currentImageIndex, scaleY);
    configStorage.saveConfig();
    renderFullImage();
    LOG_INFO_F("[Serial] Scale increased: %.1fx%.1f (saved for image %d)\n", scaleX, scaleY, currentImageIndex + 1);
}

void CommandInterpreter::handleScaleDecrease() {
    scaleX = constrain(scaleX - SCALE_STEP, MIN_SCALE, MAX_SCALE);
    scaleY = constrain(scaleY - SCALE_STEP, MIN_SCALE, MAX_SCALE);
    configStorage.setImageScaleX(currentImageIndex, scaleX);
    configStorage.setImageScaleY(currentImageIndex, scaleY);
    configStorage.saveConfig();
    renderFullImage();
    LOG_INFO_F("[Serial] Scale decreased: %.1fx%.1f (saved for image %d)\n", scaleX, scaleY, currentImageIndex + 1);
}

// Movement commands
void CommandInterpreter::handleMoveUp() {
    offsetY -= MOVE_STEP;
    configStorage.setImageOffsetY(currentImageIndex, offsetY);
    configStorage.saveConfig();
    renderFullImage();
    LOG_INFO_F("[Serial] Move up: offset=%d,%d (saved for image %d)\n", offsetX, offsetY, currentImageIndex + 1);
}

void CommandInterpreter::handleMoveDown() {
    offsetY += MOVE_STEP;
    configStorage.setImageOffsetY(currentImageIndex, offsetY);
    configStorage.saveConfig();
    renderFullImage();
    LOG_INFO_F("[Serial] Move down: offset=%d,%d (saved for image %d)\n", offsetX, offsetY, currentImageIndex + 1);
}

void CommandInterpreter::handleMoveLeft() {
    offsetX -= MOVE_STEP;
    configStorage.setImageOffsetX(currentImageIndex, offsetX);
    configStorage.saveConfig();
    renderFullImage();
    LOG_INFO_F("[Serial] Move left: offset=%d,%d (saved for image %d)\n", offsetX, offsetY, currentImageIndex + 1);
}

void CommandInterpreter::handleMoveRight() {
    offsetX += MOVE_STEP;
    configStorage.setImageOffsetX(currentImageIndex, offsetX);
    configStorage.saveConfig();
    renderFullImage();
    LOG_INFO_F("[Serial] Move right: offset=%d,%d (saved for image %d)\n", offsetX, offsetY, currentImageIndex + 1);
}

// Rotation commands
void CommandInterpreter::handleRotateCCW() {
    rotationAngle -= ROTATION_STEP;
    if (rotationAngle < 0) rotationAngle += 360.0;
    configStorage.setImageRotation(currentImageIndex, rotationAngle);
    configStorage.saveConfig();
    renderFullImage();
    LOG_INFO_F("[Serial] Rotate CCW: %.0f° (saved for image %d)\n", rotationAngle, currentImageIndex + 1);
}

void CommandInterpreter::handleRotateCW() {
    rotationAngle += ROTATION_STEP;
    if (rotationAngle >= 360.0) rotationAngle -= 360.0;
    configStorage.setImageRotation(currentImageIndex, rotationAngle);
    configStorage.saveConfig();
    renderFullImage();
    LOG_INFO_F("[Serial] Rotate CW: %.0f° (saved for image %d)\n", rotationAngle, currentImageIndex + 1);
}

// Navigation commands
void CommandInterpreter::handleNextImage() {
    if (cyclingEnabled && imageSourceCount > 1) {
        LOG_INFO_F("[Serial] Next image command - advancing to next (image %d of %d)\n", currentImageIndex + 1, imageSourceCount);
        advanceToNextImage();
        lastCycleTime = millis(); // Reset cycle timer for fresh interval
        lastUpdate = 0; // Force immediate image download
    } else {
        LOG_WARNING("[Serial] Next image command ignored - cycling not enabled or only one source configured");
    }
}

void CommandInterpreter::handleForceRefresh() {
    lastUpdate = 0; // Force immediate refresh
    LOG_INFO("[Serial] Force image refresh requested");
}

// Reset/Save commands
void CommandInterpreter::handleResetTransforms() {
    LOG_INFO_F("[Serial] Reset transformations for image %d\n", currentImageIndex + 1);
    scaleX = DEFAULT_SCALE_X;
    scaleY = DEFAULT_SCALE_Y;
    offsetX = DEFAULT_OFFSET_X;
    offsetY = DEFAULT_OFFSET_Y;
    rotationAngle = DEFAULT_ROTATION;
    configStorage.setImageScaleX(currentImageIndex, scaleX);
    configStorage.setImageScaleY(currentImageIndex, scaleY);
    configStorage.setImageOffsetX(currentImageIndex, offsetX);
    configStorage.setImageOffsetY(currentImageIndex, offsetY);
    configStorage.setImageRotation(currentImageIndex, rotationAngle);
    configStorage.saveConfig();
    renderFullImage();
    LOG_INFO("[Serial] All transformations reset to defaults");
}

void CommandInterpreter::handleSaveTransforms() {
    configStorage.setImageScaleX(currentImageIndex, scaleX);
    configStorage.setImageScaleY(currentImageIndex, scaleY);
    configStorage.setImageOffsetX(currentImageIndex, offsetX);
    configStorage.setImageOffsetY(currentImageIndex, offsetY);
    configStorage.setImageRotation(currentImageIndex, rotationAngle);
    configStorage.saveConfig();
    Serial.printf("Saved transform settings for image %d: scale=%.1fx%.1f, offset=%d,%d, rotation=%.0f°\n",
                 currentImageIndex + 1, scaleX, scaleY, offsetX, offsetY, rotationAngle);
}

// Brightness commands
void CommandInterpreter::handleBrightnessUp() {
    displayManager.setBrightness(min(displayManager.getBrightness() + 10, 100));
    LOG_INFO_F("[Serial] Brightness increased: %d%%\n", displayManager.getBrightness());
}

void CommandInterpreter::handleBrightnessDown() {
    displayManager.setBrightness(max(displayManager.getBrightness() - 10, 0));
    LOG_INFO_F("[Serial] Brightness decreased: %d%%\n", displayManager.getBrightness());
}

// System control
void CommandInterpreter::handleReboot() {
    LOG_WARNING("[Serial] Device reboot requested via serial command");
    delay(1000); // Give time for message to be sent
    crashLogger.saveBeforeReboot();
    delay(100);
    ESP.restart();
}

void CommandInterpreter::handleHelp() {
    Serial.println("\n=== Image Control Commands ===");
    Serial.println("Navigation:");
    Serial.println("  N   : Next image (resets cycle timer)");
    Serial.println("  F   : Force refresh current image");
    Serial.println("Scaling:");
    Serial.println("  +/- : Scale both axes");
    Serial.println("Movement:");
    Serial.println("  W/S : Move up/down");
    Serial.println("  A/D : Move left/right");
    Serial.println("Rotation:");
    Serial.println("  Q/E : Rotate 90° CCW/CW");
    Serial.println("Reset:");
    Serial.println("  R   : Reset all transformations");
    Serial.println("  V   : Save (persist) current transform settings for this image");
    Serial.println("Brightness:");
    Serial.println("  L/K : Brightness up/down");
    Serial.println("System:");
    Serial.println("  B   : Reboot device");
    Serial.println("  M   : Memory info");
    Serial.println("  I   : Network info");
    Serial.println("  P   : PPA info");
    Serial.println("  T   : MQTT info");
    Serial.println("  X   : Web server status/restart");
    Serial.println("  G   : Health diagnostics (comprehensive device health report)");
    Serial.println("Touch:");
    Serial.println("  Single tap : Next image");
    Serial.println("  Double tap : Toggle cycling/single refresh mode");
    Serial.println("Help:");
    Serial.println("  H/? : Show this help");
}

// System info commands
void CommandInterpreter::handleMemoryInfo() {
    systemMonitor.printMemoryStatus();
}

void CommandInterpreter::handleNetworkInfo() {
    wifiManager.printConnectionInfo();
}

void CommandInterpreter::handlePPAInfo() {
    ppaAccelerator.printStatus();
}

void CommandInterpreter::handleMQTTInfo() {
    mqttManager.printConnectionInfo();
}

void CommandInterpreter::handleWebServerStatus() {
    LOG_INFO("[Serial] Web server status check requested");
    Serial.println("\n=== Web Server Status ===");
    Serial.printf("WiFi connected: %s\n", wifiManager.isConnected() ? "YES" : "NO");
    if (wifiManager.isConnected()) {
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    }
    Serial.printf("Web server running: %s\n", webConfig.isRunning() ? "YES" : "NO");
    
    // Try to restart web server
    if (wifiManager.isConnected()) {
        LOG_INFO("[Serial] Attempting web server restart");
        Serial.println("Attempting to restart web server...");
        webConfig.stop();
        delay(500);
        if (webConfig.begin(8080)) {
            LOG_INFO_F("[Serial] Web server restarted successfully at: http://%s:8080\n", WiFi.localIP().toString().c_str());
        } else {
            LOG_ERROR("[Serial] Failed to restart web server");
        }
    } else {
        LOG_WARNING("[Serial] Cannot start web server - WiFi not connected");
    }
}

void CommandInterpreter::handleHealthDiagnostics() {
    LOG_INFO("[Serial] Health diagnostics requested");
    DeviceHealthReport report = deviceHealth.generateReport();
    deviceHealth.printReport(report);
}
