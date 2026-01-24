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

    // --- NEW FIELDS for Waveshare P4 Box and other variants ---
    int8_t ldo_channel_id;     // LDO Channel ID (-1 if unused)
    uint16_t ldo_voltage_mv;   // LDO Voltage in mV
    int8_t touch_rst_pin;      // Touch Reset Pin (-1 if unused)
    int8_t touch_int_pin;      // Touch Interrupt Pin (-1 if unused)
};

#define SCREEN_3INCH_4_DSI 1
#define SCREEN_4INCH_DSI 2
#define SCREEN_WAVESHARE_P4_BOX 3  // Waveshare ESP32-P4 Smart 86 Box

#ifndef CURRENT_SCREEN
#define CURRENT_SCREEN SCREEN_WAVESHARE_P4_BOX
#endif

// Define 3.4" display init commands
static const lcd_init_cmd_t vendor_specific_init_3_4[] = {
{0xE0, (uint8_t[]){0x00}, 1, 0},

    {0xE1, (uint8_t[]){0x93}, 1, 0},
    {0xE2, (uint8_t[]){0x65}, 1, 0},
    {0xE3, (uint8_t[]){0xF8}, 1, 0},
    {0x80, (uint8_t[]){0x01}, 1, 0},

    {0xE0, (uint8_t[]){0x01}, 1, 0},

    {0x00, (uint8_t[]){0x00}, 1, 0},
    {0x01, (uint8_t[]){0x41}, 1, 0},
    {0x03, (uint8_t[]){0x10}, 1, 0},
    {0x04, (uint8_t[]){0x44}, 1, 0},

    {0x17, (uint8_t[]){0x00}, 1, 0},
    {0x18, (uint8_t[]){0xD0}, 1, 0},
    {0x19, (uint8_t[]){0x00}, 1, 0},
    {0x1A, (uint8_t[]){0x00}, 1, 0},
    {0x1B, (uint8_t[]){0xD0}, 1, 0},
    {0x1C, (uint8_t[]){0x00}, 1, 0},

    {0x24, (uint8_t[]){0xFE}, 1, 0},
    {0x35, (uint8_t[]){0x26}, 1, 0},

    {0x37, (uint8_t[]){0x09}, 1, 0},

    {0x38, (uint8_t[]){0x04}, 1, 0},
    {0x39, (uint8_t[]){0x08}, 1, 0},
    {0x3A, (uint8_t[]){0x0A}, 1, 0},
    {0x3C, (uint8_t[]){0x78}, 1, 0},
    {0x3D, (uint8_t[]){0xFF}, 1, 0},
    {0x3E, (uint8_t[]){0xFF}, 1, 0},
    {0x3F, (uint8_t[]){0xFF}, 1, 0},

    {0x40, (uint8_t[]){0x00}, 1, 0},
    {0x41, (uint8_t[]){0x64}, 1, 0},
    {0x42, (uint8_t[]){0xC7}, 1, 0},
    {0x43, (uint8_t[]){0x18}, 1, 0},
    {0x44, (uint8_t[]){0x0B}, 1, 0},
    {0x45, (uint8_t[]){0x14}, 1, 0},

    {0x55, (uint8_t[]){0x02}, 1, 0},
    {0x57, (uint8_t[]){0x49}, 1, 0},
    {0x59, (uint8_t[]){0x0A}, 1, 0},
    {0x5A, (uint8_t[]){0x1B}, 1, 0},
    {0x5B, (uint8_t[]){0x19}, 1, 0},

    {0x5D, (uint8_t[]){0x7F}, 1, 0},
    {0x5E, (uint8_t[]){0x56}, 1, 0},
    {0x5F, (uint8_t[]){0x43}, 1, 0},
    {0x60, (uint8_t[]){0x37}, 1, 0},
    {0x61, (uint8_t[]){0x33}, 1, 0},
    {0x62, (uint8_t[]){0x25}, 1, 0},
    {0x63, (uint8_t[]){0x2A}, 1, 0},
    {0x64, (uint8_t[]){0x16}, 1, 0},
    {0x65, (uint8_t[]){0x30}, 1, 0},
    {0x66, (uint8_t[]){0x2F}, 1, 0},
    {0x67, (uint8_t[]){0x32}, 1, 0},
    {0x68, (uint8_t[]){0x53}, 1, 0},
    {0x69, (uint8_t[]){0x43}, 1, 0},
    {0x6A, (uint8_t[]){0x4C}, 1, 0},
    {0x6B, (uint8_t[]){0x40}, 1, 0},
    {0x6C, (uint8_t[]){0x3D}, 1, 0},
    {0x6D, (uint8_t[]){0x31}, 1, 0},
    {0x6E, (uint8_t[]){0x20}, 1, 0},
    {0x6F, (uint8_t[]){0x0F}, 1, 0},

    {0x70, (uint8_t[]){0x7F}, 1, 0},
    {0x71, (uint8_t[]){0x56}, 1, 0},
    {0x72, (uint8_t[]){0x43}, 1, 0},
    {0x73, (uint8_t[]){0x37}, 1, 0},
    {0x74, (uint8_t[]){0x33}, 1, 0},
    {0x75, (uint8_t[]){0x25}, 1, 0},
    {0x76, (uint8_t[]){0x2A}, 1, 0},
    {0x77, (uint8_t[]){0x16}, 1, 0},
    {0x78, (uint8_t[]){0x30}, 1, 0},
    {0x79, (uint8_t[]){0x2F}, 1, 0},
    {0x7A, (uint8_t[]){0x32}, 1, 0},
    {0x7B, (uint8_t[]){0x53}, 1, 0},
    {0x7C, (uint8_t[]){0x43}, 1, 0},
    {0x7D, (uint8_t[]){0x4C}, 1, 0},
    {0x7E, (uint8_t[]){0x40}, 1, 0},
    {0x7F, (uint8_t[]){0x3D}, 1, 0},
    {0x80, (uint8_t[]){0x31}, 1, 0},
    {0x81, (uint8_t[]){0x20}, 1, 0},
    {0x82, (uint8_t[]){0x0F}, 1, 0},

    {0xE0, (uint8_t[]){0x02}, 1, 0},
    {0x00, (uint8_t[]){0x5F}, 1, 0},
    {0x01, (uint8_t[]){0x5F}, 1, 0},
    {0x02, (uint8_t[]){0x5E}, 1, 0},
    {0x03, (uint8_t[]){0x5E}, 1, 0},
    {0x04, (uint8_t[]){0x50}, 1, 0},
    {0x05, (uint8_t[]){0x48}, 1, 0},
    {0x06, (uint8_t[]){0x48}, 1, 0},
    {0x07, (uint8_t[]){0x4A}, 1, 0},
    {0x08, (uint8_t[]){0x4A}, 1, 0},
    {0x09, (uint8_t[]){0x44}, 1, 0},
    {0x0A, (uint8_t[]){0x44}, 1, 0},
    {0x0B, (uint8_t[]){0x46}, 1, 0},
    {0x0C, (uint8_t[]){0x46}, 1, 0},
    {0x0D, (uint8_t[]){0x5F}, 1, 0},
    {0x0E, (uint8_t[]){0x5F}, 1, 0},
    {0x0F, (uint8_t[]){0x57}, 1, 0},
    {0x10, (uint8_t[]){0x57}, 1, 0},
    {0x11, (uint8_t[]){0x77}, 1, 0},
    {0x12, (uint8_t[]){0x77}, 1, 0},
    {0x13, (uint8_t[]){0x40}, 1, 0},
    {0x14, (uint8_t[]){0x42}, 1, 0},
    {0x15, (uint8_t[]){0x5F}, 1, 0},

    {0x16, (uint8_t[]){0x5F}, 1, 0},
    {0x17, (uint8_t[]){0x5F}, 1, 0},
    {0x18, (uint8_t[]){0x5E}, 1, 0},
    {0x19, (uint8_t[]){0x5E}, 1, 0},
    {0x1A, (uint8_t[]){0x50}, 1, 0},
    {0x1B, (uint8_t[]){0x49}, 1, 0},
    {0x1C, (uint8_t[]){0x49}, 1, 0},
    {0x1D, (uint8_t[]){0x4B}, 1, 0},
    {0x1E, (uint8_t[]){0x4B}, 1, 0},
    {0x1F, (uint8_t[]){0x45}, 1, 0},
    {0x20, (uint8_t[]){0x45}, 1, 0},
    {0x21, (uint8_t[]){0x47}, 1, 0},
    {0x22, (uint8_t[]){0x47}, 1, 0},
    {0x23, (uint8_t[]){0x5F}, 1, 0},
    {0x24, (uint8_t[]){0x5F}, 1, 0},
    {0x25, (uint8_t[]){0x57}, 1, 0},
    {0x26, (uint8_t[]){0x57}, 1, 0},
    {0x27, (uint8_t[]){0x77}, 1, 0},
    {0x28, (uint8_t[]){0x77}, 1, 0},
    {0x29, (uint8_t[]){0x41}, 1, 0},
    {0x2A, (uint8_t[]){0x43}, 1, 0},
    {0x2B, (uint8_t[]){0x5F}, 1, 0},

    {0x2C, (uint8_t[]){0x1E}, 1, 0},
    {0x2D, (uint8_t[]){0x1E}, 1, 0},
    {0x2E, (uint8_t[]){0x1F}, 1, 0},
    {0x2F, (uint8_t[]){0x1F}, 1, 0},
    {0x30, (uint8_t[]){0x10}, 1, 0},
    {0x31, (uint8_t[]){0x07}, 1, 0},
    {0x32, (uint8_t[]){0x07}, 1, 0},
    {0x33, (uint8_t[]){0x05}, 1, 0},
    {0x34, (uint8_t[]){0x05}, 1, 0},
    {0x35, (uint8_t[]){0x0B}, 1, 0},
    {0x36, (uint8_t[]){0x0B}, 1, 0},
    {0x37, (uint8_t[]){0x09}, 1, 0},
    {0x38, (uint8_t[]){0x09}, 1, 0},
    {0x39, (uint8_t[]){0x1F}, 1, 0},
    {0x3A, (uint8_t[]){0x1F}, 1, 0},
    {0x3B, (uint8_t[]){0x17}, 1, 0},
    {0x3C, (uint8_t[]){0x17}, 1, 0},
    {0x3D, (uint8_t[]){0x17}, 1, 0},
    {0x3E, (uint8_t[]){0x17}, 1, 0},
    {0x3F, (uint8_t[]){0x03}, 1, 0},
    {0x40, (uint8_t[]){0x01}, 1, 0},
    {0x41, (uint8_t[]){0x1F}, 1, 0},

    {0x42, (uint8_t[]){0x1E}, 1, 0},
    {0x43, (uint8_t[]){0x1E}, 1, 0},
    {0x44, (uint8_t[]){0x1F}, 1, 0},
    {0x45, (uint8_t[]){0x1F}, 1, 0},
    {0x46, (uint8_t[]){0x10}, 1, 0},
    {0x47, (uint8_t[]){0x06}, 1, 0},
    {0x48, (uint8_t[]){0x06}, 1, 0},
    {0x49, (uint8_t[]){0x04}, 1, 0},
    {0x4A, (uint8_t[]){0x04}, 1, 0},
    {0x4B, (uint8_t[]){0x0A}, 1, 0},
    {0x4C, (uint8_t[]){0x0A}, 1, 0},
    {0x4D, (uint8_t[]){0x08}, 1, 0},
    {0x4E, (uint8_t[]){0x08}, 1, 0},
    {0x4F, (uint8_t[]){0x1F}, 1, 0},
    {0x50, (uint8_t[]){0x1F}, 1, 0},
    {0x51, (uint8_t[]){0x17}, 1, 0},
    {0x52, (uint8_t[]){0x17}, 1, 0},
    {0x53, (uint8_t[]){0x17}, 1, 0},
    {0x54, (uint8_t[]){0x17}, 1, 0},
    {0x55, (uint8_t[]){0x02}, 1, 0},
    {0x56, (uint8_t[]){0x00}, 1, 0},
    {0x57, (uint8_t[]){0x1F}, 1, 0},

    {0xE0, (uint8_t[]){0x02}, 1, 0},
    {0x58, (uint8_t[]){0x40}, 1, 0},
    {0x59, (uint8_t[]){0x00}, 1, 0},
    {0x5A, (uint8_t[]){0x00}, 1, 0},
    {0x5B, (uint8_t[]){0x30}, 1, 0},
    {0x5C, (uint8_t[]){0x01}, 1, 0},
    {0x5D, (uint8_t[]){0x30}, 1, 0},
    {0x5E, (uint8_t[]){0x01}, 1, 0},
    {0x5F, (uint8_t[]){0x02}, 1, 0},
    {0x60, (uint8_t[]){0x30}, 1, 0},
    {0x61, (uint8_t[]){0x03}, 1, 0},
    {0x62, (uint8_t[]){0x04}, 1, 0},
    {0x63, (uint8_t[]){0x04}, 1, 0},
    {0x64, (uint8_t[]){0xA6}, 1, 0},
    {0x65, (uint8_t[]){0x43}, 1, 0},
    {0x66, (uint8_t[]){0x30}, 1, 0},
    {0x67, (uint8_t[]){0x73}, 1, 0},
    {0x68, (uint8_t[]){0x05}, 1, 0},
    {0x69, (uint8_t[]){0x04}, 1, 0},
    {0x6A, (uint8_t[]){0x7F}, 1, 0},
    {0x6B, (uint8_t[]){0x08}, 1, 0},
    {0x6C, (uint8_t[]){0x00}, 1, 0},
    {0x6D, (uint8_t[]){0x04}, 1, 0},
    {0x6E, (uint8_t[]){0x04}, 1, 0},
    {0x6F, (uint8_t[]){0x88}, 1, 0},

    {0x75, (uint8_t[]){0xD9}, 1, 0},
    {0x76, (uint8_t[]){0x00}, 1, 0},
    {0x77, (uint8_t[]){0x33}, 1, 0},
    {0x78, (uint8_t[]){0x43}, 1, 0},

    {0xE0, (uint8_t[]){0x00}, 1, 0},

    {0x11, (uint8_t[]){0x00}, 1, 120},

    {0x29, (uint8_t[]){0x00}, 1, 20},
    {0x35, (uint8_t[]){0x00}, 1, 0},
};

const DisplayConfig SCREEN_3_4_INCH_CONFIG = {
    .name = "3.4INCH-DSI",
    .hsync_pulse_width = 20,
    .hsync_back_porch = 20,
    .hsync_front_porch = 40,
    .vsync_pulse_width = 4,
    .vsync_back_porch = 12,
    .vsync_front_porch = 24,
    .prefer_speed = 80000000,
    .lane_bit_rate = 1500,
    .width = 800,
    .height = 800,
    .rotation = 2,
    .auto_flush = false,
    .rst_pin = -1,
    .init_cmds = vendor_specific_init_3_4,
    .init_cmds_size = sizeof(vendor_specific_init_3_4) / sizeof(lcd_init_cmd_t),
    .i2c_sda_pin = 7,
    .i2c_scl_pin = 8,
    .i2c_clock_speed = 100000,
    .lcd_rst = 27,
    // Default values for new fields (no LDO or special touch pins)
    .ldo_channel_id = -1,
    .ldo_voltage_mv = 0,
    .touch_rst_pin = -1,
    .touch_int_pin = -1,
};

// Define 4.0" display init commands
static const lcd_init_cmd_t vendor_specific_init_4_0[] = {
    {0xE0, (uint8_t[]){0x00}, 1, 0},

    {0xE1, (uint8_t[]){0x93}, 1, 0},
    {0xE2, (uint8_t[]){0x65}, 1, 0},
    {0xE3, (uint8_t[]){0xF8}, 1, 0},
    {0x80, (uint8_t[]){0x01}, 1, 0},

    {0xE0, (uint8_t[]){0x01}, 1, 0},

    {0x00, (uint8_t[]){0x00}, 1, 0},
    {0x01, (uint8_t[]){0x41}, 1, 0},
    {0x03, (uint8_t[]){0x10}, 1, 0},
    {0x04, (uint8_t[]){0x44}, 1, 0},

    {0x17, (uint8_t[]){0x00}, 1, 0},
    {0x18, (uint8_t[]){0xD0}, 1, 0},
    {0x19, (uint8_t[]){0x00}, 1, 0},
    {0x1A, (uint8_t[]){0x00}, 1, 0},
    {0x1B, (uint8_t[]){0xD0}, 1, 0},
    {0x1C, (uint8_t[]){0x00}, 1, 0},

    {0x24, (uint8_t[]){0xFE}, 1, 0},
    {0x35, (uint8_t[]){0x26}, 1, 0},

    {0x37, (uint8_t[]){0x09}, 1, 0},

    {0x38, (uint8_t[]){0x04}, 1, 0},
    {0x39, (uint8_t[]){0x08}, 1, 0},
    {0x3A, (uint8_t[]){0x0A}, 1, 0},
    {0x3C, (uint8_t[]){0x78}, 1, 0},
    {0x3D, (uint8_t[]){0xFF}, 1, 0},
    {0x3E, (uint8_t[]){0xFF}, 1, 0},
    {0x3F, (uint8_t[]){0xFF}, 1, 0},

    {0x40, (uint8_t[]){0x04}, 1, 0},
    {0x41, (uint8_t[]){0x64}, 1, 0},
    {0x42, (uint8_t[]){0xC7}, 1, 0},
    {0x43, (uint8_t[]){0x18}, 1, 0},
    {0x44, (uint8_t[]){0x0B}, 1, 0},
    {0x45, (uint8_t[]){0x14}, 1, 0},

    {0x55, (uint8_t[]){0x02}, 1, 0},
    {0x57, (uint8_t[]){0x49}, 1, 0},
    {0x59, (uint8_t[]){0x0A}, 1, 0},
    {0x5A, (uint8_t[]){0x1B}, 1, 0},
    {0x5B, (uint8_t[]){0x19}, 1, 0},

    {0x5D, (uint8_t[]){0x7F}, 1, 0},
    {0x5E, (uint8_t[]){0x56}, 1, 0},
    {0x5F, (uint8_t[]){0x43}, 1, 0},
    {0x60, (uint8_t[]){0x37}, 1, 0},
    {0x61, (uint8_t[]){0x33}, 1, 0},
    {0x62, (uint8_t[]){0x25}, 1, 0},
    {0x63, (uint8_t[]){0x2A}, 1, 0},
    {0x64, (uint8_t[]){0x16}, 1, 0},
    {0x65, (uint8_t[]){0x30}, 1, 0},
    {0x66, (uint8_t[]){0x2F}, 1, 0},
    {0x67, (uint8_t[]){0x32}, 1, 0},
    {0x68, (uint8_t[]){0x53}, 1, 0},
    {0x69, (uint8_t[]){0x43}, 1, 0},
    {0x6A, (uint8_t[]){0x4C}, 1, 0},
    {0x6B, (uint8_t[]){0x40}, 1, 0},
    {0x6C, (uint8_t[]){0x3D}, 1, 0},
    {0x6D, (uint8_t[]){0x31}, 1, 0},
    {0x6E, (uint8_t[]){0x20}, 1, 0},
    {0x6F, (uint8_t[]){0x0F}, 1, 0},

    {0x70, (uint8_t[]){0x7F}, 1, 0},
    {0x71, (uint8_t[]){0x56}, 1, 0},
    {0x72, (uint8_t[]){0x43}, 1, 0},
    {0x73, (uint8_t[]){0x37}, 1, 0},
    {0x74, (uint8_t[]){0x33}, 1, 0},
    {0x75, (uint8_t[]){0x25}, 1, 0},
    {0x76, (uint8_t[]){0x2A}, 1, 0},
    {0x77, (uint8_t[]){0x16}, 1, 0},
    {0x78, (uint8_t[]){0x30}, 1, 0},
    {0x79, (uint8_t[]){0x2F}, 1, 0},
    {0x7A, (uint8_t[]){0x32}, 1, 0},
    {0x7B, (uint8_t[]){0x53}, 1, 0},
    {0x7C, (uint8_t[]){0x43}, 1, 0},
    {0x7D, (uint8_t[]){0x4C}, 1, 0},
    {0x7E, (uint8_t[]){0x40}, 1, 0},
    {0x7F, (uint8_t[]){0x3D}, 1, 0},
    {0x80, (uint8_t[]){0x31}, 1, 0},
    {0x81, (uint8_t[]){0x20}, 1, 0},
    {0x82, (uint8_t[]){0x0F}, 1, 0},

    {0xE0, (uint8_t[]){0x02}, 1, 0},
    {0x00, (uint8_t[]){0x5F}, 1, 0},
    {0x01, (uint8_t[]){0x5F}, 1, 0},
    {0x02, (uint8_t[]){0x5E}, 1, 0},
    {0x03, (uint8_t[]){0x5E}, 1, 0},
    {0x04, (uint8_t[]){0x50}, 1, 0},
    {0x05, (uint8_t[]){0x48}, 1, 0},
    {0x06, (uint8_t[]){0x48}, 1, 0},
    {0x07, (uint8_t[]){0x4A}, 1, 0},
    {0x08, (uint8_t[]){0x4A}, 1, 0},
    {0x09, (uint8_t[]){0x44}, 1, 0},
    {0x0A, (uint8_t[]){0x44}, 1, 0},
    {0x0B, (uint8_t[]){0x46}, 1, 0},
    {0x0C, (uint8_t[]){0x46}, 1, 0},
    {0x0D, (uint8_t[]){0x5F}, 1, 0},
    {0x0E, (uint8_t[]){0x5F}, 1, 0},
    {0x0F, (uint8_t[]){0x57}, 1, 0},
    {0x10, (uint8_t[]){0x57}, 1, 0},
    {0x11, (uint8_t[]){0x77}, 1, 0},
    {0x12, (uint8_t[]){0x77}, 1, 0},
    {0x13, (uint8_t[]){0x40}, 1, 0},
    {0x14, (uint8_t[]){0x42}, 1, 0},
    {0x15, (uint8_t[]){0x5F}, 1, 0},

    {0x16, (uint8_t[]){0x5F}, 1, 0},
    {0x17, (uint8_t[]){0x5F}, 1, 0},
    {0x18, (uint8_t[]){0x5E}, 1, 0},
    {0x19, (uint8_t[]){0x5E}, 1, 0},
    {0x1A, (uint8_t[]){0x50}, 1, 0},
    {0x1B, (uint8_t[]){0x49}, 1, 0},
    {0x1C, (uint8_t[]){0x49}, 1, 0},
    {0x1D, (uint8_t[]){0x4B}, 1, 0},
    {0x1E, (uint8_t[]){0x4B}, 1, 0},
    {0x1F, (uint8_t[]){0x45}, 1, 0},
    {0x20, (uint8_t[]){0x45}, 1, 0},
    {0x21, (uint8_t[]){0x47}, 1, 0},
    {0x22, (uint8_t[]){0x47}, 1, 0},
    {0x23, (uint8_t[]){0x5F}, 1, 0},
    {0x24, (uint8_t[]){0x5F}, 1, 0},
    {0x25, (uint8_t[]){0x57}, 1, 0},
    {0x26, (uint8_t[]){0x57}, 1, 0},
    {0x27, (uint8_t[]){0x77}, 1, 0},
    {0x28, (uint8_t[]){0x77}, 1, 0},
    {0x29, (uint8_t[]){0x41}, 1, 0},
    {0x2A, (uint8_t[]){0x43}, 1, 0},
    {0x2B, (uint8_t[]){0x5F}, 1, 0},

    {0x2C, (uint8_t[]){0x1E}, 1, 0},
    {0x2D, (uint8_t[]){0x1E}, 1, 0},
    {0x2E, (uint8_t[]){0x1F}, 1, 0},
    {0x2F, (uint8_t[]){0x1F}, 1, 0},
    {0x30, (uint8_t[]){0x10}, 1, 0},
    {0x31, (uint8_t[]){0x07}, 1, 0},
    {0x32, (uint8_t[]){0x07}, 1, 0},
    {0x33, (uint8_t[]){0x05}, 1, 0},
    {0x34, (uint8_t[]){0x05}, 1, 0},
    {0x35, (uint8_t[]){0x0B}, 1, 0},
    {0x36, (uint8_t[]){0x0B}, 1, 0},
    {0x37, (uint8_t[]){0x09}, 1, 0},
    {0x38, (uint8_t[]){0x09}, 1, 0},
    {0x39, (uint8_t[]){0x1F}, 1, 0},
    {0x3A, (uint8_t[]){0x1F}, 1, 0},
    {0x3B, (uint8_t[]){0x17}, 1, 0},
    {0x3C, (uint8_t[]){0x17}, 1, 0},
    {0x3D, (uint8_t[]){0x17}, 1, 0},
    {0x3E, (uint8_t[]){0x17}, 1, 0},
    {0x3F, (uint8_t[]){0x03}, 1, 0},
    {0x40, (uint8_t[]){0x01}, 1, 0},
    {0x41, (uint8_t[]){0x1F}, 1, 0},

    {0x42, (uint8_t[]){0x1E}, 1, 0},
    {0x43, (uint8_t[]){0x1E}, 1, 0},
    {0x44, (uint8_t[]){0x1F}, 1, 0},
    {0x45, (uint8_t[]){0x1F}, 1, 0},
    {0x46, (uint8_t[]){0x10}, 1, 0},
    {0x47, (uint8_t[]){0x06}, 1, 0},
    {0x48, (uint8_t[]){0x06}, 1, 0},
    {0x49, (uint8_t[]){0x04}, 1, 0},
    {0x4A, (uint8_t[]){0x04}, 1, 0},
    {0x4B, (uint8_t[]){0x0A}, 1, 0},
    {0x4C, (uint8_t[]){0x0A}, 1, 0},
    {0x4D, (uint8_t[]){0x08}, 1, 0},
    {0x4E, (uint8_t[]){0x08}, 1, 0},
    {0x4F, (uint8_t[]){0x1F}, 1, 0},
    {0x50, (uint8_t[]){0x1F}, 1, 0},
    {0x51, (uint8_t[]){0x17}, 1, 0},
    {0x52, (uint8_t[]){0x17}, 1, 0},
    {0x53, (uint8_t[]){0x17}, 1, 0},
    {0x54, (uint8_t[]){0x17}, 1, 0},
    {0x55, (uint8_t[]){0x02}, 1, 0},
    {0x56, (uint8_t[]){0x00}, 1, 0},
    {0x57, (uint8_t[]){0x1F}, 1, 0},

    {0xE0, (uint8_t[]){0x02}, 1, 0},
    {0x58, (uint8_t[]){0x40}, 1, 0},
    {0x59, (uint8_t[]){0x00}, 1, 0},
    {0x5A, (uint8_t[]){0x00}, 1, 0},
    {0x5B, (uint8_t[]){0x30}, 1, 0},
    {0x5C, (uint8_t[]){0x01}, 1, 0},
    {0x5D, (uint8_t[]){0x30}, 1, 0},
    {0x5E, (uint8_t[]){0x01}, 1, 0},
    {0x5F, (uint8_t[]){0x02}, 1, 0},
    {0x60, (uint8_t[]){0x30}, 1, 0},
    {0x61, (uint8_t[]){0x03}, 1, 0},
    {0x62, (uint8_t[]){0x04}, 1, 0},
    {0x63, (uint8_t[]){0x04}, 1, 0},
    {0x64, (uint8_t[]){0xA6}, 1, 0},
    {0x65, (uint8_t[]){0x43}, 1, 0},
    {0x66, (uint8_t[]){0x30}, 1, 0},
    {0x67, (uint8_t[]){0x73}, 1, 0},
    {0x68, (uint8_t[]){0x05}, 1, 0},
    {0x69, (uint8_t[]){0x04}, 1, 0},
    {0x6A, (uint8_t[]){0x7F}, 1, 0},
    {0x6B, (uint8_t[]){0x08}, 1, 0},
    {0x6C, (uint8_t[]){0x00}, 1, 0},
    {0x6D, (uint8_t[]){0x04}, 1, 0},
    {0x6E, (uint8_t[]){0x04}, 1, 0},
    {0x6F, (uint8_t[]){0x88}, 1, 0},

    {0x75, (uint8_t[]){0xD9}, 1, 0},
    {0x76, (uint8_t[]){0x00}, 1, 0},
    {0x77, (uint8_t[]){0x33}, 1, 0},
    {0x78, (uint8_t[]){0x43}, 1, 0},

    {0xE0, (uint8_t[]){0x00}, 1, 0},
    {0x11, (uint8_t[]){0x00}, 1, 120},

    {0x29, (uint8_t[]){0x00}, 1, 20},
    {0x35, (uint8_t[]){0x00}, 1, 0},
};

const DisplayConfig SCREEN_4_INCH_CONFIG = {
    .name = "4INCH-DSI",
    .hsync_pulse_width = 20,
    .hsync_back_porch = 20,
    .hsync_front_porch = 40,
    .vsync_pulse_width = 4,
    .vsync_back_porch = 12,
    .vsync_front_porch = 24,
    .prefer_speed = 80000000,
    .lane_bit_rate = 1500,
    .width = 720,
    .height = 720,
    .rotation = 2,
    .auto_flush = true,
    .rst_pin = -1,
    .init_cmds = vendor_specific_init_4_0,
    .init_cmds_size = sizeof(vendor_specific_init_4_0) / sizeof(lcd_init_cmd_t),
    .i2c_sda_pin = 7,
    .i2c_scl_pin = 8,
    .i2c_clock_speed = 100000,
    .lcd_rst = 27,
    // Default values for new fields (no LDO or special touch pins)
    .ldo_channel_id = -1,
    .ldo_voltage_mv = 0,
    .touch_rst_pin = -1,
    .touch_int_pin = -1,
};

// ===== Waveshare ESP32-P4 Smart 86 Box Vendor Init Commands =====
// Based on WORKING Waveshare Arduino examples
// Source: docs/waveshare/Arduino/libraries/displays/displays_config.h
static const lcd_init_cmd_t waveshare_p4_86_init[] = {
    {0xB9, (uint8_t[]){0xF1, 0x12, 0x83}, 3, 0},

    {0xB1, (uint8_t[]){0x00, 0x00, 0x00, 0xDA, 0x80}, 5, 0},

    {0xB2, (uint8_t[]){0x3C, 0x12, 0x30}, 3, 0},

    {0xB3, (uint8_t[]){0x10, 0x10, 0x28, 0x28, 0x03, 0xFF, 0x00, 0x00, 0x00, 0x00}, 10, 0},

    {0xB4, (uint8_t[]){0x80}, 1, 0},

    {0xB5, (uint8_t[]){0x0A, 0x0A}, 2, 0},

    {0xB6, (uint8_t[]){0x97, 0x97}, 2, 0},

    {0xB8, (uint8_t[]){0x26, 0x22, 0xF0, 0x13}, 4, 0},

    {0xBA, (uint8_t[]){0x31, 0x81, 0x0F, 0xF9, 0x0E, 0x06, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x25, 0x00, 0x90, 0x0A, 0x00, 0x00, 0x01, 0x4F, 0x01, 0x00, 0x00, 0x37}, 27, 0},

    {0xBC, (uint8_t[]){0x47}, 1, 0},

    {0xBF, (uint8_t[]){0x02, 0x11, 0x00}, 3, 0},

    {0xC0, (uint8_t[]){0x73, 0x73, 0x50, 0x50, 0x00, 0x00, 0x12, 0x70, 0x00}, 9, 0},

    {0xC1, (uint8_t[]){0x25, 0x00, 0x32, 0x32, 0x77, 0xE4, 0xFF, 0xFF, 0xCC, 0xCC, 0x77, 0x77}, 12, 0},

    {0xC6, (uint8_t[]){0x82, 0x00, 0xBF, 0xFF, 0x00, 0xFF}, 6, 0},

    {0xC7, (uint8_t[]){0xB8, 0x00, 0x0A, 0x10, 0x01, 0x09}, 6, 0},

    {0xC8, (uint8_t[]){0x10, 0x40, 0x1E, 0x02}, 4, 0},

    {0xCC, (uint8_t[]){0x0B}, 1, 0},

    {0xE0, (uint8_t[]){0x00, 0x0B, 0x10, 0x2C, 0x3D, 0x3F, 0x42, 0x3A, 0x07, 0x0D, 0x0F, 0x13, 0x15, 0x13, 0x14, 0x0F, 0x16, 0x00, 0x0B, 0x10, 0x2C, 0x3D, 0x3F, 0x42, 0x3A, 0x07, 0x0D, 0x0F, 0x13, 0x15, 0x13, 0x14, 0x0F, 0x16}, 34, 0},

    {0xE3, (uint8_t[]){0x07, 0x07, 0x0B, 0x0B, 0x0B, 0x0B, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xC0, 0x10}, 14, 0},

    {0xE9, (uint8_t[]){0xC8, 0x10, 0x0A, 0x00, 0x00, 0x80, 0x81, 0x12, 0x31, 0x23, 0x4F, 0x86, 0xA0, 0x00, 0x47, 0x08, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x98, 0x02, 0x8B, 0xAF, 0x46, 0x02, 0x88, 0x88, 0x88, 0x88, 0x88, 0x98, 0x13, 0x8B, 0xAF, 0x57, 0x13, 0x88, 0x88, 0x88, 0x88, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 63, 0},

    {0xEA, (uint8_t[]){0x97, 0x0C, 0x09, 0x09, 0x09, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9F, 0x31, 0x8B, 0xA8, 0x31, 0x75, 0x88, 0x88, 0x88, 0x88, 0x88, 0x9F, 0x20, 0x8B, 0xA8, 0x20, 0x64, 0x88, 0x88, 0x88, 0x88, 0x88, 0x23, 0x00, 0x00, 0x02, 0x71, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x80, 0x81, 0x00, 0x00, 0x00, 0x00}, 61, 0},

    {0xEF, (uint8_t[]){0xFF, 0xFF, 0x01}, 3, 0},

    {0x11, (uint8_t[]){0x00}, 1, 250},
    {0x29, (uint8_t[]){0x00}, 1, 50},
};


// Waveshare ESP32-P4 Smart 86 Box Configuration
const DisplayConfig SCREEN_WAVESHARE_CONFIG = {
    .name = "WS-P4-BOX-720x720",
    // Timing values from working Waveshare Arduino examples
    .hsync_pulse_width = 20,
    .hsync_back_porch = 50,
    .hsync_front_porch = 50,
    .vsync_pulse_width = 4,
    .vsync_back_porch = 20,
    .vsync_front_porch = 20,
    .prefer_speed = 38000000,  // 38MHz pixel clock
    .lane_bit_rate = 480,      // 480Mbps per lane
    .width = 720,
    .height = 720,
    .rotation = 0,  // No rotation for Waveshare
    .auto_flush = true,
    .rst_pin = -1,  // No dedicated reset pin in working example
    .init_cmds = waveshare_p4_86_init,
    .init_cmds_size = sizeof(waveshare_p4_86_init) / sizeof(lcd_init_cmd_t),
    .i2c_sda_pin = 7,
    .i2c_scl_pin = 8,
    .i2c_clock_speed = 100000,  // 100kHz I2C (from working example)
    .lcd_rst = 27,  // LCD reset pin
    // --- Waveshare P4 Box Specifics ---
    .ldo_channel_id = 3,      // Enable LDO Channel 3
    .ldo_voltage_mv = 2500,   // 2.5V output
    .touch_rst_pin = -1,      // Software Reset (no physical pin)
    .touch_int_pin = -1,      // Polling Mode (no interrupt pin)
};

// Compile-time default selection (actual runtime selection happens in display_manager.cpp setup())
#if CURRENT_SCREEN == SCREEN_WAVESHARE_P4_BOX
inline const DisplayConfig& display_cfg = SCREEN_WAVESHARE_CONFIG;
#elif CURRENT_SCREEN == SCREEN_4INCH_DSI
inline const DisplayConfig& display_cfg = SCREEN_4_INCH_CONFIG;
#else
inline const DisplayConfig& display_cfg = SCREEN_3_4_INCH_CONFIG;
#endif

#endif
