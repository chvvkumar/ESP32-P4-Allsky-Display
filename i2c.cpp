/*****************************************************************************
 * | File         :   i2c.c
 * | Author       :   Waveshare team
 * | Function     :   Hardware underlying interface
 * | Info         :
 * |                 I2C driver code for I2C communication.
 * ----------------
 * | This version :   V1.0
 * | Date         :   2024-11-26
 * | Info         :   Basic version
 *
 ******************************************************************************/

#include "i2c.h"  // Include I2C driver header for I2C functions
static const char *TAG = "i2c";  // Define a tag for logging

// I2C timeout in milliseconds for transmit/receive operations
#define I2C_TIMEOUT_MS 20

// Global handle for the I2C master bus
// i2c_master_bus_handle_t bus_handle = NULL;
DEV_I2C_Port handle;
/**
 * @brief Initialize the I2C master interface.
 *
 * This function configures the I2C master bus and adds a device with the specified address.
 * The I2C clock source, frequency, SCL/SDA pins, and other settings are configured here.
 *
 * @param Addr The I2C address of the device to be initialized.
 * @return The device handle if initialization is successful, NULL otherwise.
 */
DEV_I2C_Port DEV_I2C_Init()
{
    // Define I2C bus configuration parameters
    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = EXAMPLE_I2C_MASTER_NUM,     // I2C master port number
        .sda_io_num = EXAMPLE_I2C_MASTER_SDA,   // I2C SDA (data) pin
        .scl_io_num = EXAMPLE_I2C_MASTER_SCL,   // I2C SCL (clock) pin
        .clk_source = I2C_CLK_SRC_DEFAULT,       // Default clock source for I2C
        .glitch_ignore_cnt = 7,                  // Ignore glitches in the I2C signal
    };

    // Create a new I2C master bus with the above configuration
    esp_err_t ret = i2c_new_master_bus(&i2c_bus_config, &handle.bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C master bus creation failed: %s", esp_err_to_name(ret));
        handle.bus = NULL;
        handle.dev = NULL;
        return handle;
    }

    // Configure the device's I2C parameters
    i2c_device_config_t i2c_dev_conf = {
        .scl_speed_hz = EXAMPLE_I2C_MASTER_FREQUENCY,  // Set I2C communication speed
    };

    // Add the I2C device to the bus
    // i2c_master_dev_handle_t dev_handle = NULL;
    if (i2c_master_bus_add_device(handle.bus, &i2c_dev_conf, &handle.dev) != ESP_OK) {
        ESP_LOGE(TAG, "I2C device creation failed");  // Log error if device creation fails
    }

    return handle;  // Return the device handle if successful
}

/**
 * @brief Set a new I2C slave address for the device.
 *
 * This function allows changing the I2C slave address for the specified device.
 * It removes the old device before adding the new one to prevent handle leaks.
 *
 * @param dev_handle The handle to the I2C device.
 * @param Addr The new I2C address for the device.
 */
void DEV_I2C_Set_Slave_Addr(i2c_master_dev_handle_t *dev_handle, uint8_t Addr)
{
    // Remove the old device to prevent handle leak
    if (*dev_handle != NULL) {
        esp_err_t rm_ret = i2c_master_bus_rm_device(*dev_handle);
        if (rm_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to remove old I2C device: %s", esp_err_to_name(rm_ret));
        }
        *dev_handle = NULL;
    }

    // Configure the new device address
    i2c_device_config_t i2c_dev_conf = {
        .device_address = Addr,                        // Set new device address
        .scl_speed_hz = EXAMPLE_I2C_MASTER_FREQUENCY,  // I2C frequency
    };

    // Update the device with the new address
    if (i2c_master_bus_add_device(handle.bus, &i2c_dev_conf, dev_handle) != ESP_OK) {
        ESP_LOGE(TAG, "I2C address modification failed");  // Log error if address modification fails
    }
}

/**
 * @brief Write a single byte to the I2C device.
 *
 * This function sends a command byte and a value byte to the I2C device.
 *
 * @param dev_handle The handle to the I2C device.
 * @param Cmd The command byte to send.
 * @param value The value byte to send.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t DEV_I2C_Write_Byte(i2c_master_dev_handle_t dev_handle, uint8_t Cmd, uint8_t value)
{
    uint8_t data[2] = {Cmd, value};  // Create an array with command and value
    esp_err_t ret = i2c_master_transmit(dev_handle, data, sizeof(data), I2C_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C write byte failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

/**
 * @brief Read a single byte from the I2C device.
 *
 * This function reads a byte of data from the I2C device.
 *
 * @param dev_handle The handle to the I2C device.
 * @param out Pointer to store the received byte.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t DEV_I2C_Read_Byte(i2c_master_dev_handle_t dev_handle, uint8_t *out)
{
    uint8_t data[1] = {0};  // Create a buffer to store the received byte
    esp_err_t ret = i2c_master_receive(dev_handle, data, 1, I2C_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C read byte failed: %s", esp_err_to_name(ret));
        *out = 0;
        return ret;
    }
    *out = data[0];
    return ESP_OK;
}

/**
 * @brief Read a word (2 bytes) from the I2C device.
 *
 * This function reads two bytes (a word) from the I2C device.
 * The data is received by sending a command byte and receiving the data.
 *
 * @param dev_handle The handle to the I2C device.
 * @param Cmd The command byte to send.
 * @param out Pointer to store the 16-bit word read from the device.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t DEV_I2C_Read_Word(i2c_master_dev_handle_t dev_handle, uint8_t Cmd, uint16_t *out)
{
    uint8_t data[2] = {Cmd};  // Create an array with the command byte
    esp_err_t ret = i2c_master_transmit_receive(dev_handle, data, 1, data, 2, I2C_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C read word failed: %s", esp_err_to_name(ret));
        *out = 0;
        return ret;
    }
    *out = data[1] << 8 | data[0];  // Combine the two bytes into a word (16-bit)
    return ESP_OK;
}

/**
 * @brief Write multiple bytes to the I2C device.
 *
 * This function sends a block of data to the I2C device.
 *
 * @param dev_handle The handle to the I2C device.
 * @param pdata Pointer to the data to send.
 * @param len The number of bytes to send.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t DEV_I2C_Write_Nbyte(i2c_master_dev_handle_t dev_handle, uint8_t *pdata, uint8_t len)
{
    esp_err_t ret = i2c_master_transmit(dev_handle, pdata, len, I2C_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C write %d bytes failed: %s", len, esp_err_to_name(ret));
    }
    return ret;
}

/**
 * @brief Read multiple bytes from the I2C device.
 *
 * This function reads multiple bytes from the I2C device.
 * The function sends a command byte and receives the specified number of bytes.
 *
 * @param dev_handle The handle to the I2C device.
 * @param Cmd The command byte to send.
 * @param pdata Pointer to the buffer where received data will be stored.
 * @param len The number of bytes to read.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t DEV_I2C_Read_Nbyte(i2c_master_dev_handle_t dev_handle, uint8_t Cmd, uint8_t *pdata, uint8_t len)
{
    esp_err_t ret = i2c_master_transmit_receive(dev_handle, &Cmd, 1, pdata, len, I2C_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C read %d bytes failed: %s", len, esp_err_to_name(ret));
    }
    return ret;
}
