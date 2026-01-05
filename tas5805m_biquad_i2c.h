/**
 * TAS5805M Biquad I2C Programming for ESPHome
 *
 * This header provides functions to program the TAS5805M's DSP biquad filters
 * directly via I2C using ESPHome's I2C bus abstraction.
 *
 * Usage in ESPHome lambda:
 *   #include "tas5805m_biquad_i2c.h"
 *   tas5805m_biquad::write_biquad(id(i2c_bus), 0x2C, 0, 0, {1.0, 0, 0, 0, 0});
 */

#pragma once

#include "esphome/components/i2c/i2c.h"
#include <cstdint>
#include <cmath>

namespace tas5805m_biquad {

// =============================================================================
// CONSTANTS
// =============================================================================

constexpr uint8_t TAS5805M_ADDR = 0x2C;  // Default I2C address

// Book/Page selection registers
constexpr uint8_t REG_PAGE_SELECT = 0x00;
constexpr uint8_t REG_BOOK_SELECT = 0x7F;

// Biquad coefficient book
constexpr uint8_t BOOK_COEFF = 0xAA;

// Page addresses for left channel biquads (0-14)
constexpr uint8_t PAGE_LEFT_BQ[15] = {
    0x24, 0x24, 0x24, 0x24,  // BQ0-BQ3
    0x25, 0x25, 0x25, 0x25,  // BQ4-BQ7
    0x26, 0x26, 0x26, 0x26,  // BQ8-BQ11
    0x27, 0x27, 0x27         // BQ12-BQ14
};

// Offset within page for each biquad (20 bytes per biquad)
constexpr uint8_t OFFSET_BQ[15] = {
    0x08, 0x1C, 0x30, 0x44,  // Positions 0-3 on each page
    0x08, 0x1C, 0x30, 0x44,
    0x08, 0x1C, 0x30, 0x44,
    0x08, 0x1C, 0x30
};

// Page addresses for right channel biquads (0-14)
constexpr uint8_t PAGE_RIGHT_BQ[15] = {
    0x32, 0x32, 0x32, 0x32,
    0x33, 0x33, 0x33, 0x33,
    0x34, 0x34, 0x34, 0x34,
    0x35, 0x35, 0x35
};

// =============================================================================
// COEFFICIENT CONVERSION
// =============================================================================

/**
 * Convert float to TAS5805M 9.23 fixed-point format
 */
inline int32_t float_to_9_23(float value) {
    // Check for invalid values (NaN, Infinity)
    if (!std::isfinite(value)) {
        ESP_LOGE("tas5805m_bq", "Invalid coefficient: %f (NaN or Inf), using bypass", value);
        return 0;  // Return bypass coefficient
    }

    // Clamp to valid range for 9.23 format
    if (value > 255.999999f) value = 255.999999f;
    if (value < -256.0f) value = -256.0f;

    return static_cast<int32_t>(value * (1 << 23));
}

/**
 * Pack 32-bit value into big-endian byte buffer
 */
inline void pack_be32(int32_t value, uint8_t* buffer) {
    buffer[0] = (value >> 24) & 0xFF;
    buffer[1] = (value >> 16) & 0xFF;
    buffer[2] = (value >> 8) & 0xFF;
    buffer[3] = value & 0xFF;
}

// =============================================================================
// I2C HELPER CLASS
// =============================================================================

/**
 * Helper class to communicate with TAS5805M via ESPHome I2C bus
 */
class TAS5805M_I2C {
public:
    TAS5805M_I2C(esphome::i2c::I2CBus* bus, uint8_t address = TAS5805M_ADDR)
        : bus_(bus), address_(address) {}

    /**
     * Write a single byte to a register (with retry logic)
     */
    bool write_byte(uint8_t reg, uint8_t value) {
        const int MAX_RETRIES = 3;

        for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
            uint8_t data[2] = {reg, value};
            auto err = bus_->write(address_, data, 2, true);

            if (err == esphome::i2c::ERROR_OK) {
                return true;  // Success
            }

            ESP_LOGW("tas5805m_bq", "I2C write failed (attempt %d/%d): reg=0x%02X val=0x%02X err=%d",
                     attempt + 1, MAX_RETRIES, reg, value, (int)err);

            if (attempt < MAX_RETRIES - 1) {
                delay(5);  // Wait 5ms before retry
            }
        }

        ESP_LOGE("tas5805m_bq", "I2C write failed after %d attempts: reg=0x%02X val=0x%02X",
                 MAX_RETRIES, reg, value);
        return false;
    }

    /**
     * Write multiple bytes starting at a register (with retry logic)
     */
    bool write_bytes(uint8_t reg, const uint8_t* data, size_t len) {
        const int MAX_RETRIES = 3;

        for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
            // Allocate buffer for register + data
            std::vector<uint8_t> buffer(len + 1);
            buffer[0] = reg;
            memcpy(&buffer[1], data, len);

            auto err = bus_->write(address_, buffer.data(), buffer.size(), true);

            if (err == esphome::i2c::ERROR_OK) {
                return true;  // Success
            }

            ESP_LOGW("tas5805m_bq", "I2C write_bytes failed (attempt %d/%d): reg=0x%02X len=%d err=%d",
                     attempt + 1, MAX_RETRIES, reg, (int)len, (int)err);

            if (attempt < MAX_RETRIES - 1) {
                delay(5);  // Wait 5ms before retry
            }
        }

        ESP_LOGE("tas5805m_bq", "I2C write_bytes failed after %d attempts: reg=0x%02X len=%d",
                 MAX_RETRIES, reg, (int)len);
        return false;
    }

    /**
     * Select book and page for coefficient access
     */
    bool select_book_page(uint8_t book, uint8_t page) {
        // First go to page 0 to access book register
        if (!write_byte(REG_PAGE_SELECT, 0x00)) return false;
        delay(2);  // Wait for page select to take effect

        // Select book
        if (!write_byte(REG_BOOK_SELECT, book)) return false;
        delay(2);  // Wait for book select to take effect

        // Select page within book
        if (!write_byte(REG_PAGE_SELECT, page)) return false;
        delay(2);  // Wait for page select to take effect

        return true;
    }

    /**
     * Return to normal operation (book 0, page 0)
     */
    bool return_to_normal() {
        if (!write_byte(REG_PAGE_SELECT, 0x00)) return false;
        if (!write_byte(REG_BOOK_SELECT, 0x00)) return false;
        return true;
    }

private:
    esphome::i2c::I2CBus* bus_;
    uint8_t address_;
};

// =============================================================================
// BIQUAD PROGRAMMING FUNCTIONS
// =============================================================================

/**
 * Write biquad coefficients to TAS5805M
 *
 * @param bus ESPHome I2C bus pointer (use id(i2c_bus))
 * @param address I2C address (typically 0x2C)
 * @param channel 0=left, 1=right, 2=both
 * @param index Biquad index 0-14
 * @param b0, b1, b2, a1, a2 Float coefficients (normalized, a0=1)
 * @return true on success
 */
inline bool write_biquad(esphome::i2c::I2CBus* bus, uint8_t address,
                         int channel, int index,
                         float b0, float b1, float b2, float a1, float a2) {

    if (index < 0 || index >= 15) {
        ESP_LOGE("tas5805m_bq", "Invalid biquad index: %d (must be 0-14)", index);
        return false;
    }

    TAS5805M_I2C dev(bus, address);

    // Convert to 9.23 fixed-point
    // Note: TAS5805M expects a1 and a2 with inverted signs!
    int32_t b0_fp = float_to_9_23(b0);
    int32_t b1_fp = float_to_9_23(b1);
    int32_t b2_fp = float_to_9_23(b2);
    int32_t a1_fp = float_to_9_23(-a1);  // Sign inverted!
    int32_t a2_fp = float_to_9_23(-a2);  // Sign inverted!

    // Pack coefficients into 20-byte buffer
    uint8_t coeff_buf[20];
    pack_be32(b0_fp, &coeff_buf[0]);
    pack_be32(b1_fp, &coeff_buf[4]);
    pack_be32(b2_fp, &coeff_buf[8]);
    pack_be32(a1_fp, &coeff_buf[12]);
    pack_be32(a2_fp, &coeff_buf[16]);

    ESP_LOGD("tas5805m_bq", "Writing biquad ch=%d idx=%d", channel, index);
    ESP_LOGD("tas5805m_bq", "  b0=%.6f b1=%.6f b2=%.6f a1=%.6f a2=%.6f", b0, b1, b2, a1, a2);
    ESP_LOGD("tas5805m_bq", "  FP: b0=0x%08X b1=0x%08X b2=0x%08X a1=0x%08X a2=0x%08X",
             b0_fp, b1_fp, b2_fp, a1_fp, a2_fp);

    bool success = true;

    // Write to left channel if requested
    if (channel == 0 || channel == 2) {
        uint8_t page = PAGE_LEFT_BQ[index];
        uint8_t offset = OFFSET_BQ[index];

        if (!dev.select_book_page(BOOK_COEFF, page)) {
            ESP_LOGE("tas5805m_bq", "Failed to select book/page for left channel");
            return false;
        }

        if (!dev.write_bytes(offset, coeff_buf, 20)) {
            ESP_LOGE("tas5805m_bq", "Failed to write left channel coefficients");
            success = false;
        } else {
            ESP_LOGI("tas5805m_bq", "Left channel BQ%d written (page=0x%02X offset=0x%02X)",
                     index, page, offset);
        }
        delay(5);  // Wait for TAS5805M to process coefficient write
    }

    // Write to right channel if requested
    if (channel == 1 || channel == 2) {
        uint8_t page = PAGE_RIGHT_BQ[index];
        uint8_t offset = OFFSET_BQ[index];

        if (!dev.select_book_page(BOOK_COEFF, page)) {
            ESP_LOGE("tas5805m_bq", "Failed to select book/page for right channel");
            return false;
        }

        if (!dev.write_bytes(offset, coeff_buf, 20)) {
            ESP_LOGE("tas5805m_bq", "Failed to write right channel coefficients");
            success = false;
        } else {
            ESP_LOGI("tas5805m_bq", "Right channel BQ%d written (page=0x%02X offset=0x%02X)",
                     index, page, offset);
        }
        delay(5);  // Wait for TAS5805M to process coefficient write
    }

    // Return to normal operation
    dev.return_to_normal();

    return success;
}

/**
 * Reset a single biquad to bypass (b0=1, all others=0)
 */
inline bool reset_biquad(esphome::i2c::I2CBus* bus, uint8_t address,
                         int channel, int index) {
    return write_biquad(bus, address, channel, index, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}

/**
 * Reset all 30 biquads to bypass
 */
inline bool reset_all_biquads(esphome::i2c::I2CBus* bus, uint8_t address) {
    ESP_LOGI("tas5805m_bq", "Resetting all 30 biquads to bypass");

    bool success = true;
    for (int bq = 0; bq < 15; bq++) {
        // Write bypass to both channels
        if (!reset_biquad(bus, address, 2, bq)) {
            ESP_LOGE("tas5805m_bq", "Failed to reset biquad %d", bq);
            success = false;
        }
    }

    if (success) {
        ESP_LOGI("tas5805m_bq", "All biquads reset to bypass");
    }
    return success;
}

// =============================================================================
// FILTER COEFFICIENT CALCULATORS
// =============================================================================

/**
 * Calculate and write a parametric EQ filter
 */
inline bool write_parametric_eq(esphome::i2c::I2CBus* bus, uint8_t address,
                                int channel, int index,
                                float frequency, float gain_db, float q,
                                float fs = 48000.0f) {

    const float A = std::pow(10.0f, gain_db / 40.0f);
    const float omega = 2.0f * M_PI * frequency / fs;
    const float sin_omega = std::sin(omega);
    const float cos_omega = std::cos(omega);
    const float alpha = sin_omega / (2.0f * q);

    float b0 = 1.0f + alpha * A;
    float b1 = -2.0f * cos_omega;
    float b2 = 1.0f - alpha * A;
    float a0 = 1.0f + alpha / A;
    float a1 = -2.0f * cos_omega;
    float a2 = 1.0f - alpha / A;

    // Normalize by a0
    b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;

    ESP_LOGI("tas5805m_bq", "PEQ: fc=%.1fHz gain=%.1fdB Q=%.2f", frequency, gain_db, q);

    return write_biquad(bus, address, channel, index, b0, b1, b2, a1, a2);
}

/**
 * Calculate and write a low shelf filter
 */
inline bool write_low_shelf(esphome::i2c::I2CBus* bus, uint8_t address,
                            int channel, int index,
                            float frequency, float gain_db, float slope = 1.0f,
                            float fs = 48000.0f) {

    const float A = std::pow(10.0f, gain_db / 40.0f);
    const float omega = 2.0f * M_PI * frequency / fs;
    const float sin_omega = std::sin(omega);
    const float cos_omega = std::cos(omega);
    const float alpha = sin_omega / 2.0f * std::sqrt((A + 1.0f/A) * (1.0f/slope - 1.0f) + 2.0f);
    const float two_sqrt_A_alpha = 2.0f * std::sqrt(A) * alpha;

    float b0 = A * ((A + 1.0f) - (A - 1.0f) * cos_omega + two_sqrt_A_alpha);
    float b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cos_omega);
    float b2 = A * ((A + 1.0f) - (A - 1.0f) * cos_omega - two_sqrt_A_alpha);
    float a0 = (A + 1.0f) + (A - 1.0f) * cos_omega + two_sqrt_A_alpha;
    float a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cos_omega);
    float a2 = (A + 1.0f) + (A - 1.0f) * cos_omega - two_sqrt_A_alpha;

    b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;

    ESP_LOGI("tas5805m_bq", "Low shelf: fc=%.1fHz gain=%.1fdB slope=%.2f", frequency, gain_db, slope);

    return write_biquad(bus, address, channel, index, b0, b1, b2, a1, a2);
}

/**
 * Calculate and write a high shelf filter
 */
inline bool write_high_shelf(esphome::i2c::I2CBus* bus, uint8_t address,
                             int channel, int index,
                             float frequency, float gain_db, float slope = 1.0f,
                             float fs = 48000.0f) {

    const float A = std::pow(10.0f, gain_db / 40.0f);
    const float omega = 2.0f * M_PI * frequency / fs;
    const float sin_omega = std::sin(omega);
    const float cos_omega = std::cos(omega);
    const float alpha = sin_omega / 2.0f * std::sqrt((A + 1.0f/A) * (1.0f/slope - 1.0f) + 2.0f);
    const float two_sqrt_A_alpha = 2.0f * std::sqrt(A) * alpha;

    float b0 = A * ((A + 1.0f) + (A - 1.0f) * cos_omega + two_sqrt_A_alpha);
    float b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos_omega);
    float b2 = A * ((A + 1.0f) + (A - 1.0f) * cos_omega - two_sqrt_A_alpha);
    float a0 = (A + 1.0f) - (A - 1.0f) * cos_omega + two_sqrt_A_alpha;
    float a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cos_omega);
    float a2 = (A + 1.0f) - (A - 1.0f) * cos_omega - two_sqrt_A_alpha;

    b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;

    ESP_LOGI("tas5805m_bq", "High shelf: fc=%.1fHz gain=%.1fdB slope=%.2f", frequency, gain_db, slope);

    return write_biquad(bus, address, channel, index, b0, b1, b2, a1, a2);
}

/**
 * Calculate and write a high-pass filter
 */
inline bool write_highpass(esphome::i2c::I2CBus* bus, uint8_t address,
                           int channel, int index,
                           float frequency, float q,
                           float fs = 48000.0f) {

    const float omega = 2.0f * M_PI * frequency / fs;
    const float sin_omega = std::sin(omega);
    const float cos_omega = std::cos(omega);
    const float alpha = sin_omega / (2.0f * q);

    float b0 = (1.0f + cos_omega) / 2.0f;
    float b1 = -(1.0f + cos_omega);
    float b2 = (1.0f + cos_omega) / 2.0f;
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cos_omega;
    float a2 = 1.0f - alpha;

    b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;

    ESP_LOGI("tas5805m_bq", "High-pass: fc=%.1fHz Q=%.2f", frequency, q);

    return write_biquad(bus, address, channel, index, b0, b1, b2, a1, a2);
}

/**
 * Calculate and write a low-pass filter
 */
inline bool write_lowpass(esphome::i2c::I2CBus* bus, uint8_t address,
                          int channel, int index,
                          float frequency, float q,
                          float fs = 48000.0f) {

    const float omega = 2.0f * M_PI * frequency / fs;
    const float sin_omega = std::sin(omega);
    const float cos_omega = std::cos(omega);
    const float alpha = sin_omega / (2.0f * q);

    float b0 = (1.0f - cos_omega) / 2.0f;
    float b1 = 1.0f - cos_omega;
    float b2 = (1.0f - cos_omega) / 2.0f;
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cos_omega;
    float a2 = 1.0f - alpha;

    b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;

    ESP_LOGI("tas5805m_bq", "Low-pass: fc=%.1fHz Q=%.2f", frequency, q);

    return write_biquad(bus, address, channel, index, b0, b1, b2, a1, a2);
}

/**
 * Calculate and write a notch filter
 */
inline bool write_notch(esphome::i2c::I2CBus* bus, uint8_t address,
                        int channel, int index,
                        float frequency, float q,
                        float fs = 48000.0f) {

    const float omega = 2.0f * M_PI * frequency / fs;
    const float sin_omega = std::sin(omega);
    const float cos_omega = std::cos(omega);
    const float alpha = sin_omega / (2.0f * q);

    float b0 = 1.0f;
    float b1 = -2.0f * cos_omega;
    float b2 = 1.0f;
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cos_omega;
    float a2 = 1.0f - alpha;

    b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;

    ESP_LOGI("tas5805m_bq", "Notch: fc=%.1fHz Q=%.2f", frequency, q);

    return write_biquad(bus, address, channel, index, b0, b1, b2, a1, a2);
}

}  // namespace tas5805m_biquad
