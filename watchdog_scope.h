#pragma once
#ifndef WATCHDOG_SCOPE_H
#define WATCHDOG_SCOPE_H

#include "system_monitor.h"

/**
 * @brief RAII wrapper for automatic watchdog management
 * 
 * This class uses RAII (Resource Acquisition Is Initialization) pattern to automatically
 * reset the watchdog timer on both entry and exit of a scope. This eliminates the need
 * for manual watchdog resets and ensures protection even if early returns occur.
 * 
 * Usage:
 * ```cpp
 * void criticalFunction() {
 *     WatchdogScope wd;  // Watchdog reset on entry
 *     // ... long-running operation ...
 *     // Watchdog automatically reset on exit (even if early return)
 * }
 * ```
 * 
 * Or use the convenience macro:
 * ```cpp
 * void criticalFunction() {
 *     WATCHDOG_SCOPE();  // Creates unnamed WatchdogScope instance
 *     // ... long-running operation ...
 * }
 * ```
 */
class WatchdogScope {
public:
    /**
     * @brief Constructor - resets watchdog on scope entry
     */
    WatchdogScope() {
        systemMonitor.forceResetWatchdog();
    }
    
    /**
     * @brief Destructor - resets watchdog on scope exit
     * 
     * Ensures watchdog is reset even if function exits early via return,
     * exception, or other control flow.
     */
    ~WatchdogScope() {
        systemMonitor.forceResetWatchdog();
    }
    
    /**
     * @brief Manual reset during long operations within the scope
     * 
     * While constructor/destructor provide automatic resets at scope boundaries,
     * this method allows manual resets during long-running operations within the scope.
     */
    void reset() {
        systemMonitor.forceResetWatchdog();
    }
    
    // Prevent copying (watchdog resets should be scoped, not copied)
    WatchdogScope(const WatchdogScope&) = delete;
    WatchdogScope& operator=(const WatchdogScope&) = delete;
    
    // Allow moving (transfer ownership of watchdog scope)
    WatchdogScope(WatchdogScope&&) noexcept = default;
    WatchdogScope& operator=(WatchdogScope&&) noexcept = default;
};

/**
 * @brief Convenience macro for creating a watchdog scope guard
 * 
 * Creates an unnamed WatchdogScope instance that will reset the watchdog
 * on entry and exit of the current scope.
 */
#define WATCHDOG_SCOPE() WatchdogScope _watchdog_scope_guard_##__LINE__

#endif // WATCHDOG_SCOPE_H
