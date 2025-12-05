#include "task_retry_handler.h"
#include "system_monitor.h"
#include "display_manager.h"

// Global instance
TaskRetryHandler taskRetryHandler;

TaskRetryHandler::TaskRetryHandler() : taskCount(0), lastProcessTime(0) {
}

unsigned long TaskRetryHandler::calculateNextRetryInterval(const RetryTask& task) {
    if (!task.exponentialBackoff) {
        return task.baseRetryInterval;
    }
    
    // Exponential backoff: baseInterval * 2^(attemptCount-1), capped at 60 seconds
    unsigned long interval = task.baseRetryInterval;
    for (int i = 1; i < task.attemptCount; i++) {
        interval *= 2;
        if (interval > 60000) {  // Cap at 60 seconds
            return 60000;
        }
    }
    return interval;
}

void TaskRetryHandler::addTask(TaskType type, TaskCallback callback, const char* taskName, 
                               int maxAttempts, unsigned long baseInterval, 
                               const char* errorMessage) {
    // Check if task already exists and remove it
    removeTask(type);
    
    // Check if we have space
    if (taskCount >= MAX_TASKS) {
        LOG_PRINTLN("TaskRetryHandler: Queue full, cannot add task");
        return;
    }
    
    RetryTask& task = taskQueue[taskCount];
    task.type = type;
    task.callback = callback;
    task.taskName = taskName;
    task.maxAttempts = maxAttempts;
    task.baseRetryInterval = baseInterval;
    task.errorMessage = errorMessage;
    task.status = TASK_PENDING;
    task.attemptCount = 0;
    task.nextRetryTime = millis();  // Start immediately
    
    taskCount++;
}

void TaskRetryHandler::process() {
    unsigned long now = millis();
    
    // Only process at regular intervals to avoid excessive overhead
    if (now - lastProcessTime < PROCESS_INTERVAL) {
        return;
    }
    lastProcessTime = now;
    
    for (uint8_t i = 0; i < taskCount; i++) {
        RetryTask& task = taskQueue[i];
        
        // Skip if already completed or cancelled
        if (task.status == TASK_SUCCESS || task.status == TASK_CANCELLED) {
            continue;
        }
        
        // Skip if not yet time to retry
        if (now < task.nextRetryTime) {
            continue;
        }
        
        // Check if max attempts exceeded
        if (task.attemptCount >= task.maxAttempts) {
            task.status = TASK_FAILED;
            LOG_PRINTF("Task FAILED: %s\n", task.taskName);
            continue;
        }
        
        // Execute task
        task.attemptCount++;
        task.status = TASK_RUNNING;
        task.lastAttemptTime = now;
        
        // Reset watchdog before task execution
        systemMonitor.forceResetWatchdog();
        
        // Call the task callback
        bool success = false;
        if (task.callback) {
            success = task.callback();
        }
        
        // Reset watchdog after task execution
        systemMonitor.forceResetWatchdog();
        
        if (success) {
            task.status = TASK_SUCCESS;
        } else {
            task.status = TASK_RETRYING;
            task.nextRetryTime = now + calculateNextRetryInterval(task);
        }
    }
}

TaskStatus TaskRetryHandler::getTaskStatus(TaskType type) {
    for (uint8_t i = 0; i < taskCount; i++) {
        if (taskQueue[i].type == type) {
            return taskQueue[i].status;
        }
    }
    return TASK_FAILED;  // Not found = failed
}

void TaskRetryHandler::cancelTask(TaskType type) {
    for (uint8_t i = 0; i < taskCount; i++) {
        if (taskQueue[i].type == type) {
            taskQueue[i].status = TASK_CANCELLED;
            break;
        }
    }
}

void TaskRetryHandler::clearCompletedTasks() {
    uint8_t writeIndex = 0;
    for (uint8_t readIndex = 0; readIndex < taskCount; readIndex++) {
        if (taskQueue[readIndex].status != TASK_SUCCESS && 
            taskQueue[readIndex].status != TASK_FAILED && 
            taskQueue[readIndex].status != TASK_CANCELLED) {
            if (writeIndex != readIndex) {
                taskQueue[writeIndex] = taskQueue[readIndex];
            }
            writeIndex++;
        }
    }
    taskCount = writeIndex;
}

int TaskRetryHandler::getActiveTasks() {
    int count = 0;
    for (uint8_t i = 0; i < taskCount; i++) {
        if (taskQueue[i].status != TASK_SUCCESS && taskQueue[i].status != TASK_CANCELLED) {
            count++;
        }
    }
    return count;
}

String TaskRetryHandler::getTaskStatusString(TaskType type) {
    for (uint8_t i = 0; i < taskCount; i++) {
        const RetryTask& task = taskQueue[i];
        if (task.type == type) {
            String status = "";
            switch (task.status) {
                case TASK_PENDING:
                    status = "PENDING";
                    break;
                case TASK_RUNNING:
                    status = "RUNNING";
                    break;
                case TASK_SUCCESS:
                    status = "SUCCESS";
                    break;
                case TASK_FAILED:
                    status = "FAILED";
                    break;
                case TASK_RETRYING:
                    status = String("RETRYING (") + task.attemptCount + "/" + task.maxAttempts + ")";
                    break;
                case TASK_CANCELLED:
                    status = "CANCELLED";
                    break;
            }
            return status + " - " + task.taskName;
        }
    }
    return "UNKNOWN";
}

String TaskRetryHandler::getAllTasksStatus() {
    String result = "Active Tasks: " + String(getActiveTasks()) + "\n";
    
    for (uint8_t i = 0; i < taskCount; i++) {
        const RetryTask& task = taskQueue[i];
        if (task.status != TASK_SUCCESS && task.status != TASK_CANCELLED) {
            result += "  [" + String(task.attemptCount) + "/" + String(task.maxAttempts) + "] ";
            result += task.taskName;
            result += " - ";
            
            switch (task.status) {
                case TASK_PENDING:
                    result += "Pending";
                    break;
                case TASK_RUNNING:
                    result += "Running";
                    break;
                case TASK_RETRYING:
                    result += "Retrying";
                    break;
                case TASK_FAILED:
                    result += "Failed";
                    break;
                default:
                    result += "Unknown";
            }
            result += "\n";
        }
    }
    
    return result;
}

void TaskRetryHandler::removeTask(TaskType type) {
    uint8_t writeIndex = 0;
    for (uint8_t readIndex = 0; readIndex < taskCount; readIndex++) {
        if (taskQueue[readIndex].type != type) {
            if (writeIndex != readIndex) {
                taskQueue[writeIndex] = taskQueue[readIndex];
            }
            writeIndex++;
        }
    }
    taskCount = writeIndex;
}

bool TaskRetryHandler::hasCriticalFailures() {
    for (uint8_t i = 0; i < taskCount; i++) {
        if (taskQueue[i].status == TASK_FAILED && 
            (taskQueue[i].type == TASK_NETWORK_CONNECT || taskQueue[i].type == TASK_SYSTEM_INIT)) {
            return true;
        }
    }
    return false;
}
