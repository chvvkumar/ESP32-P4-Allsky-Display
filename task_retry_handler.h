#pragma once
#ifndef TASK_RETRY_HANDLER_H
#define TASK_RETRY_HANDLER_H

#include <Arduino.h>
#include <vector>

// Task callback function type - returns true if task succeeded, false if it should retry
typedef bool (*TaskCallback)();

// Task types for logging and management
enum TaskType {
    TASK_NETWORK_CONNECT,
    TASK_MQTT_CONNECT,
    TASK_IMAGE_DOWNLOAD,
    TASK_SYSTEM_INIT,
    TASK_CUSTOM
};

// Task status for tracking
enum TaskStatus {
    TASK_PENDING,
    TASK_RUNNING,
    TASK_SUCCESS,
    TASK_FAILED,
    TASK_RETRYING,
    TASK_CANCELLED
};

// Single retry task structure
struct RetryTask {
    TaskType type;
    TaskCallback callback;
    TaskStatus status;
    int attemptCount;
    int maxAttempts;
    unsigned long lastAttemptTime;
    unsigned long nextRetryTime;
    unsigned long baseRetryInterval;  // Starting interval in ms
    const char* taskName;
    const char* errorMessage;
    bool exponentialBackoff;
    
    RetryTask() : 
        type(TASK_CUSTOM), 
        callback(nullptr),
        status(TASK_PENDING),
        attemptCount(0),
        maxAttempts(5),
        lastAttemptTime(0),
        nextRetryTime(0),
        baseRetryInterval(5000),
        taskName("Unknown"),
        errorMessage(""),
        exponentialBackoff(true)
    {}
};

class TaskRetryHandler {
private:
    std::vector<RetryTask> taskQueue;
    unsigned long lastProcessTime;
    static const unsigned long PROCESS_INTERVAL = 100;  // Check tasks every 100ms
    
    // Calculate next retry interval with exponential backoff
    unsigned long calculateNextRetryInterval(const RetryTask& task);
    
public:
    TaskRetryHandler();
    
    // Add a new retry task to the queue
    void addTask(TaskType type, TaskCallback callback, const char* taskName, 
                 int maxAttempts = 5, unsigned long baseInterval = 5000, 
                 const char* errorMessage = "");
    
    // Process all pending tasks (call from main loop)
    void process();
    
    // Check if a task is currently running/pending
    TaskStatus getTaskStatus(TaskType type);
    
    // Cancel a specific task
    void cancelTask(TaskType type);
    
    // Clear all completed tasks
    void clearCompletedTasks();
    
    // Get number of active/pending tasks
    int getActiveTasks();
    
    // Get detailed task status for display
    String getTaskStatusString(TaskType type);
    
    // Get all pending task info
    String getAllTasksStatus();
    
    // Remove a specific task type
    void removeTask(TaskType type);
    
    // Check if any critical tasks are failing
    bool hasCriticalFailures();
};

// Global instance
extern TaskRetryHandler taskRetryHandler;

#endif // TASK_RETRY_HANDLER_H
