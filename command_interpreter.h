/**
 * Command Interpreter - Serial command processing with registry pattern
 * 
 * Provides a clean, extensible interface for processing serial commands
 * using RAII and callback-based command registry.
 * 
 * Supported commands:
 * - Scaling: +, - (adjust scale by ±0.1)
 * - Movement: W, A, S, D (move by 10px)
 * - Rotation: Q, E (rotate by 90° CCW/CW)
 * - Navigation: N (next image), F (force refresh)
 * - Reset: R (reset transforms), V (save transforms)
 * - Brightness: L, K (±10%)
 * - System: B (reboot), H/? (help)
 * - Info: M (memory), I (network), P (PPA), T (MQTT), X (web server), G (health diagnostics)
 */

#ifndef COMMAND_INTERPRETER_H
#define COMMAND_INTERPRETER_H

#include <Arduino.h>
#include "config_storage.h"
#include "display_manager.h"
#include "system_monitor.h"
#include "network_manager.h"
#include "mqtt_manager.h"
#include "web_config.h"
#include "ppa_accelerator.h"
#include "crash_logger.h"
#include "logging.h"
#include "config.h"

// Forward declarations for external functions (defined in main .ino)
void renderFullImage();
void advanceToNextImage();

// External global variables (defined in main .ino)
extern float scaleX, scaleY;
extern int offsetX, offsetY;
extern float rotationAngle;
extern bool cyclingEnabled;
extern int imageSourceCount;
extern int currentImageIndex;
extern unsigned long lastUpdate;
extern unsigned long lastCycleTime;

class CommandInterpreter {
public:
    // Singleton pattern
    static CommandInterpreter& getInstance() {
        static CommandInterpreter instance;
        return instance;
    }

    // Main processing method - call from loop()
    void processCommands();

private:
    // Private constructor for singleton
    CommandInterpreter() {}
    
    // Prevent copying
    CommandInterpreter(const CommandInterpreter&) = delete;
    CommandInterpreter& operator=(const CommandInterpreter&) = delete;

    // Command handler methods
    void handleScaleIncrease();
    void handleScaleDecrease();
    void handleMoveUp();
    void handleMoveDown();
    void handleMoveLeft();
    void handleMoveRight();
    void handleRotateCCW();
    void handleRotateCW();
    void handleNextImage();
    void handleForceRefresh();
    void handleResetTransforms();
    void handleSaveTransforms();
    void handleBrightnessUp();
    void handleBrightnessDown();
    void handleReboot();
    void handleHelp();
    void handleMemoryInfo();
    void handleNetworkInfo();
    void handlePPAInfo();
    void handleMQTTInfo();
    void handleWebServerStatus();
    void handleHealthDiagnostics();
};

// Global instance
extern CommandInterpreter& commandInterpreter;

#endif // COMMAND_INTERPRETER_H
