// display_config.h
#pragma once
#ifndef DISPLAYS_CONFIG_H
#define DISPLAYS_CONFIG_H
#include <Arduino_GFX_Library.h>
#include "i2c.h"

// Forward declaration for runtime display type selection
class ConfigStorage;
extern ConfigStorage configStorage;

struct DisplayConfig
{
    const char *name;

    uint32_t hsync_pulse_width;
    uint32_t hsync_back_porch;
    uint32_t hsync_front_porch;
    uint32_t vsync_pulse_width;
    uint32_t vsync_back_porch;
    uint32_t vsync_front_porch;
    uint32_t prefer_speed;
    uint32_t lane_bit_rate;

    uint16_t width;
    uint16_t height;
    int8_t rotation;
    bool auto_flush;
    int8_t rst_pin;

    const lcd_init_cmd_t *init_cmds;
    size_t init_cmds_size;

    int8_t i2c_sda_pin;
    int8_t i2c_scl_pin;
    uint32_t i2c_clock_speed;
    int8_t lcd_rst;

};

#define SCREEN_3INCH_4_DSI 1
#define SCREEN_4INCH_DSI 2

#ifndef CURRENT_SCREEN
#define CURRENT_SCREEN SCREEN_3INCH_4_DSI
#endif

// Display init command arrays and configs are defined in displays_config.cpp
extern const lcd_init_cmd_t vendor_specific_init_3_4[];
extern const size_t vendor_specific_init_3_4_size;
extern const lcd_init_cmd_t vendor_specific_init_4_0[];
extern const size_t vendor_specific_init_4_0_size;

extern const DisplayConfig SCREEN_3_4_INCH_CONFIG;
extern const DisplayConfig SCREEN_4_INCH_CONFIG;

// Compile-time default selection (actual runtime selection happens in display_manager.cpp setup())
#if CURRENT_SCREEN == SCREEN_4INCH_DSI
inline const DisplayConfig& display_cfg = SCREEN_4_INCH_CONFIG;
#else
inline const DisplayConfig& display_cfg = SCREEN_3_4_INCH_CONFIG;
#endif

#endif
