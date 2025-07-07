#include "config.h"

// =============================================================================
// WIFI CONFIGURATION IMPLEMENTATION
// =============================================================================

// WiFi Configuration - Update these with your credentials
const char* WIFI_SSID = "IoT";
const char* WIFI_PASSWORD = "kkkkkkkk";

// =============================================================================
// MQTT CONFIGURATION IMPLEMENTATION
// =============================================================================

// MQTT Configuration - Update these with your MQTT broker details
const char* MQTT_SERVER = "192.168.1.250";    // Replace with your MQTT broker IP
const int MQTT_PORT = 1883;
const char* MQTT_USER = "";                   // Leave empty if no authentication
const char* MQTT_PASSWORD = "";               // Leave empty if no authentication
const char* MQTT_CLIENT_ID = "ESP32_Allsky_Display";

// MQTT Topics
const char* MQTT_REBOOT_TOPIC = "Astro/AllSky/display/reboot";
const char* MQTT_BRIGHTNESS_TOPIC = "Astro/AllSky/display/brightness";
const char* MQTT_BRIGHTNESS_STATUS_TOPIC = "Astro/AllSky/display/brightness/status";

// =============================================================================
// IMAGE CONFIGURATION IMPLEMENTATION
// =============================================================================

// Image Configuration
const char* IMAGE_URL = "https://allsky.challa.co:1982/current/resized/image.jpg";
