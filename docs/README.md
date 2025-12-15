# ESP32-P4 AllSky Display - Documentation Index

Welcome to the complete documentation for the ESP32-P4 AllSky Display project.

## 📚 Documentation Structure

The documentation follows a **User Journey** approach, guiding you from hardware selection through installation, configuration, and usage.

---

## Getting Started

### 1. [Hardware Requirements](01_hardware.md)
**What you need to build the display**

- Supported displays (3.4" vs 4.0")
- ESP32-P4 specifications
- Required libraries and installation
- Pin assignments and wiring diagrams
- 3D printed case options
- Memory configuration and buffer sizes

**Start here if:** You're planning to buy hardware or want to understand system requirements.

---

### 2. [Installation Guide](02_installation.md)
**Getting firmware on your device**

- Flash pre-compiled binary (quickest method)
- Compile from source (Arduino IDE, PowerShell script, PlatformIO)
- Critical GFX library patch (required for compilation)
- Display size selection (3.4" vs 4.0")
- First boot WiFi setup (captive portal)
- Hardcoded WiFi configuration (advanced)
- Installation troubleshooting

**Start here if:** You have hardware and want to flash firmware.

---

### 3. [Configuration Guide](03_configuration.md)
**Setting up your display**

- Configuration overview (compile-time vs runtime)
- WiFi configuration
- Image sources (single and multi-image)
- Per-image transforms (scale, offset, rotation)
- MQTT and Home Assistant setup
- Web UI walkthrough
- Serial commands reference
- Advanced configuration options

**Start here if:** Your device is running and you want to configure it.

---

## Using Your Display

### 4. [Features & Usage](04_features.md)
**Everything your display can do**

- Core features (multi-image, hardware acceleration)
- Touch controls and gestures
- Serial commands complete reference
- Health diagnostics system
- Health monitoring API
- API usage examples (bash, Python, JavaScript)
- Home Assistant integration
- Web interface features

**Start here if:** You want to learn all the features and capabilities.

---

### 5. [OTA Updates](05_ota_updates.md)
**Wireless firmware updates**

- ElegantOTA (web interface method)
- ArduinoOTA (Arduino IDE method)
- A/B partition system explained
- Update safety features
- Troubleshooting OTA updates
- Security considerations
- **Appendix:** ESP32-C6 co-processor updates

**Start here if:** You want to update firmware wirelessly.

---

### 6. [Troubleshooting](06_troubleshooting.md)
**Solving common problems**

- Common issues and solutions
- Crash diagnosis and backtrace decoding
- Display problems (blank screen, wrong colors)
- Network issues (WiFi, slow downloads)
- Image loading problems
- Memory issues
- Touch problems
- MQTT/Home Assistant issues
- Performance problems
- Diagnostic tools reference
- Getting help

**Start here if:** Something isn't working correctly.

---

## Developer Documentation

### [System Architecture](developer/architecture.md)
**Deep dive into system design**

- Architecture overview with Mermaid diagrams
- Memory management and buffer allocation
- Data flow diagrams
- Module interactions
- Control flow
- Threading model
- Design decisions and rationale

**For:** Developers who want to understand or modify the firmware.

---

### [API Reference](developer/api_reference.md)
**Complete API documentation**

- Core class documentation (Doxygen-style)
- REST API endpoints
- MQTT topic reference
- WebSocket protocol
- Serial command protocol
- Configuration storage format
- Health monitoring API

**For:** Developers integrating with the device or extending functionality.

---

## Quick Navigation

### By Task

- **I want to buy hardware** → [01_hardware.md](01_hardware.md)
- **I want to flash firmware** → [02_installation.md](02_installation.md)
- **I want to configure my display** → [03_configuration.md](03_configuration.md)
- **I want to add it to Home Assistant** → [04_features.md#home-assistant-integration](04_features.md#home-assistant-integration)
- **I want to update firmware** → [05_ota_updates.md](05_ota_updates.md)
- **Something is broken** → [06_troubleshooting.md](06_troubleshooting.md)
- **I want to modify the code** → [developer/architecture.md](developer/architecture.md)

### By Component

- **Display Configuration** → [01_hardware.md#display-configuration](01_hardware.md#display-configuration)
- **WiFi Setup** → [02_installation.md#first-boot--wifi-setup](02_installation.md#first-boot--wifi-setup)
- **MQTT Configuration** → [03_configuration.md#mqtt-configuration](03_configuration.md#mqtt-configuration)
- **Image Setup** → [03_configuration.md#multi-image-setup](03_configuration.md#multi-image-setup)
- **Touch Controls** → [04_features.md#touch-controls](04_features.md#touch-controls)
- **Health Monitoring** → [04_features.md#health-diagnostics](04_features.md#health-diagnostics)
- **OTA Updates** → [05_ota_updates.md](05_ota_updates.md)
- **Crash Analysis** → [06_troubleshooting.md#crash-diagnosis](06_troubleshooting.md#crash-diagnosis)

---

## Additional Resources

### In This Repository

- **[Main README](../README.md)** - Project overview and quick start
- **[Archive](archive/)** - Original documentation files (historical reference)
- **[Images](../images/)** - Screenshots and diagrams used in documentation

### External Links

- **[GitHub Repository](https://github.com/chvvkumar/ESP32-P4-Allsky-Display)**
- **[Issues & Bug Reports](https://github.com/chvvkumar/ESP32-P4-Allsky-Display/issues)**
- **[Discussions](https://github.com/chvvkumar/ESP32-P4-Allsky-Display/discussions)**
- **[Releases](https://github.com/chvvkumar/ESP32-P4-Allsky-Display/releases)**
- **[3D Printed Case](https://www.printables.com/model/1352883-desk-stand-for-waveshare-esp32-p4-wifi6-touch-lcd)**

---

## Documentation Conventions

### Icons & Symbols

- ✅ **Tested & Working** - Feature has been verified on hardware
- ⚠️ **Untested** - Configuration available but not verified
- 📖 **See Also** - Related documentation reference
- 🚨 **Warning** - Important information or potential issue
- 💡 **Tip** - Helpful suggestion or best practice

### Code Blocks

```cpp
// Arduino/C++ code samples
void example() {
    // Syntax-highlighted code
}
```

```bash
# Shell commands for Linux/macOS
command --with-flags
```

```powershell
# PowerShell commands for Windows
Cmdlet -Parameter Value
```

### File Paths

- Relative paths from project root: `config.h`, `docs/01_hardware.md`
- Absolute paths when needed: `%USERPROFILE%\Documents\Arduino\`
- Cross-platform notation where applicable

---

## Contributing to Documentation

Found an error or want to improve the documentation?

1. **Small fixes:** Submit a pull request with changes
2. **Larger updates:** Open an issue first to discuss
3. **New sections:** Check architecture first to ensure consistency

**Style Guidelines:**
- Use Markdown formatting consistently
- Include code examples where helpful
- Add screenshots for UI/visual instructions
- Cross-reference related documentation
- Keep language clear and concise
- Test all commands before documenting

---

## Changelog

### December 15, 2025 - Documentation Restructure
- Reorganized documentation into user journey structure (01-06)
- Created developer subfolder for technical documentation
- Consolidated redundant files
- Updated all cross-references
- Added comprehensive index (this file)
- Archived original files for historical reference

---

**Last Updated:** December 15, 2025  
**Documentation Version:** 2.0  
**Firmware Compatibility:** v1.0.0+
