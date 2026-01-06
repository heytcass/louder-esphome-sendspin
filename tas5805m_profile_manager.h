/**
 * TAS5805M Calibration Profile Manager for ESPHome
 *
 * Provides persistent storage and management of room correction profiles
 * using ESP32 NVS (Non-Volatile Storage).
 *
 * Each profile stores:
 * - 30 biquad filters (15 per channel)
 * - Metadata (name, timestamp, room name)
 * - Active status
 */

#pragma once

#include "esphome/core/preferences.h"
#include "esphome/core/log.h"
#include "tas5805m_biquad_i2c.h"
#include <cstring>
#include <vector>

namespace tas5805m_profile {

static const char *TAG = "tas5805m_profile";

// =============================================================================
// CONSTANTS
// =============================================================================

constexpr size_t MAX_PROFILE_NAME_LEN = 32;
constexpr size_t MAX_PROFILES = 5;  // Limit to 5 profiles to save NVS space
constexpr uint32_t PROFILE_MAGIC = 0x54415335;  // "TAS5" magic number

// =============================================================================
// DATA STRUCTURES
// =============================================================================

/**
 * Single biquad filter coefficients
 */
struct BiquadCoefficients {
    float b0, b1, b2, a1, a2;

    BiquadCoefficients() : b0(1.0f), b1(0.0f), b2(0.0f), a1(0.0f), a2(0.0f) {}

    BiquadCoefficients(float _b0, float _b1, float _b2, float _a1, float _a2)
        : b0(_b0), b1(_b1), b2(_b2), a1(_a1), a2(_a2) {}

    // Check if this is a bypass filter (passthrough)
    bool is_bypass() const {
        return (fabs(b0 - 1.0f) < 0.0001f &&
                fabs(b1) < 0.0001f &&
                fabs(b2) < 0.0001f &&
                fabs(a1) < 0.0001f &&
                fabs(a2) < 0.0001f);
    }
} __attribute__((packed));

/**
 * Complete calibration profile
 */
struct CalibrationProfile {
    uint32_t magic;                              // Magic number for validation
    char name[MAX_PROFILE_NAME_LEN];             // Profile name
    uint32_t timestamp;                          // Unix timestamp of creation
    BiquadCoefficients left_channel[15];         // Left channel biquads
    BiquadCoefficients right_channel[15];        // Right channel biquads
    uint8_t num_filters_used;                    // Number of non-bypass filters
    uint32_t checksum;                           // CRC32 checksum

    CalibrationProfile() : magic(PROFILE_MAGIC), timestamp(0), num_filters_used(0), checksum(0) {
        memset(name, 0, sizeof(name));
        // Initialize all to bypass
        for (int i = 0; i < 15; i++) {
            left_channel[i] = BiquadCoefficients();
            right_channel[i] = BiquadCoefficients();
        }
    }

    // Calculate checksum for validation
    uint32_t calculate_checksum() const {
        uint32_t crc = 0xFFFFFFFF;
        const uint8_t* data = reinterpret_cast<const uint8_t*>(this);
        size_t len = offsetof(CalibrationProfile, checksum);  // Exclude checksum field itself

        for (size_t i = 0; i < len; i++) {
            crc ^= data[i];
            for (int j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
            }
        }
        return ~crc;
    }

    // Validate profile integrity
    bool is_valid() const {
        if (magic != PROFILE_MAGIC) {
            ESP_LOGE(TAG, "Invalid magic: 0x%08X (expected 0x%08X)", magic, PROFILE_MAGIC);
            return false;
        }

        uint32_t expected_checksum = calculate_checksum();
        if (checksum != expected_checksum) {
            ESP_LOGE(TAG, "Checksum mismatch: 0x%08X vs 0x%08X", checksum, expected_checksum);
            return false;
        }

        return true;
    }

    // Update checksum before saving
    void update_checksum() {
        checksum = calculate_checksum();
    }

    // Count non-bypass filters
    void count_active_filters() {
        num_filters_used = 0;
        for (int i = 0; i < 15; i++) {
            if (!left_channel[i].is_bypass() || !right_channel[i].is_bypass()) {
                num_filters_used++;
            }
        }
    }
} __attribute__((packed));

// =============================================================================
// PROFILE MANAGER CLASS
// =============================================================================

class ProfileManager {
public:
    ProfileManager() : active_profile_index_(-1) {}

    /**
     * Initialize the profile manager
     */
    void setup() {
        ESP_LOGI(TAG, "Initializing profile manager");

        // Load active profile index
        active_pref_ = esphome::global_preferences->make_preference<int8_t>(
            fnv1_hash("active_profile")
        );

        if (active_pref_.load(&active_profile_index_)) {
            if (active_profile_index_ >= 0 && active_profile_index_ < MAX_PROFILES) {
                ESP_LOGI(TAG, "Active profile index: %d", active_profile_index_);
            } else {
                ESP_LOGW(TAG, "Invalid active profile index: %d, resetting", active_profile_index_);
                active_profile_index_ = -1;
            }
        } else {
            ESP_LOGI(TAG, "No active profile set");
            active_profile_index_ = -1;
        }
    }

    /**
     * Save a calibration profile
     */
    bool save_profile(const std::string& profile_name, const CalibrationProfile& profile) {
        // Find existing profile or empty slot
        int slot = find_profile_slot(profile_name);

        if (slot == -1) {
            // Find empty slot
            for (int i = 0; i < MAX_PROFILES; i++) {
                CalibrationProfile temp;
                if (!load_profile_by_index(i, temp)) {
                    slot = i;
                    break;
                }
            }
        }

        if (slot == -1) {
            ESP_LOGE(TAG, "No available profile slots (max %d)", MAX_PROFILES);
            return false;
        }

        // Prepare profile for saving
        CalibrationProfile save_profile = profile;
        strncpy(save_profile.name, profile_name.c_str(), MAX_PROFILE_NAME_LEN - 1);
        save_profile.name[MAX_PROFILE_NAME_LEN - 1] = '\0';
        save_profile.timestamp = esphome::millis() / 1000;  // Approximate unix timestamp
        save_profile.count_active_filters();
        save_profile.update_checksum();

        // Save to NVS
        auto pref = esphome::global_preferences->make_preference<CalibrationProfile>(
            fnv1_hash(get_profile_key(slot).c_str())
        );

        if (!pref.save(&save_profile)) {
            ESP_LOGE(TAG, "Failed to save profile to slot %d", slot);
            return false;
        }

        ESP_LOGI(TAG, "Saved profile '%s' to slot %d (%d filters)",
                 profile_name.c_str(), slot, save_profile.num_filters_used);

        return true;
    }

    /**
     * Load a calibration profile by name
     */
    bool load_profile(const std::string& profile_name, CalibrationProfile& profile) {
        int slot = find_profile_slot(profile_name);
        if (slot == -1) {
            ESP_LOGE(TAG, "Profile '%s' not found", profile_name.c_str());
            return false;
        }

        return load_profile_by_index(slot, profile);
    }

    /**
     * Load a calibration profile by index
     */
    bool load_profile_by_index(int slot, CalibrationProfile& profile) {
        if (slot < 0 || slot >= MAX_PROFILES) {
            ESP_LOGE(TAG, "Invalid profile slot: %d", slot);
            return false;
        }

        auto pref = esphome::global_preferences->make_preference<CalibrationProfile>(
            fnv1_hash(get_profile_key(slot).c_str())
        );

        if (!pref.load(&profile)) {
            return false;
        }

        if (!profile.is_valid()) {
            ESP_LOGE(TAG, "Profile in slot %d failed validation", slot);
            return false;
        }

        ESP_LOGI(TAG, "Loaded profile '%s' from slot %d (%d filters)",
                 profile.name, slot, profile.num_filters_used);

        return true;
    }

    /**
     * Delete a profile by name
     */
    bool delete_profile(const std::string& profile_name) {
        int slot = find_profile_slot(profile_name);
        if (slot == -1) {
            ESP_LOGE(TAG, "Profile '%s' not found", profile_name.c_str());
            return false;
        }

        // Clear the NVS entry
        auto pref = esphome::global_preferences->make_preference<CalibrationProfile>(
            fnv1_hash(get_profile_key(slot).c_str())
        );

        // ESPHome doesn't have a delete function, so we save an invalid profile
        CalibrationProfile empty;
        empty.magic = 0;  // Invalid magic
        pref.save(&empty);

        // If this was the active profile, clear active status
        if (slot == active_profile_index_) {
            set_active_profile(-1);
        }

        ESP_LOGI(TAG, "Deleted profile '%s' from slot %d", profile_name.c_str(), slot);
        return true;
    }

    /**
     * List all available profiles
     */
    std::vector<std::string> list_profiles() {
        std::vector<std::string> profiles;

        for (int i = 0; i < MAX_PROFILES; i++) {
            CalibrationProfile profile;
            if (load_profile_by_index(i, profile)) {
                profiles.push_back(std::string(profile.name));
            }
        }

        ESP_LOGI(TAG, "Found %d profiles", profiles.size());
        return profiles;
    }

    /**
     * Set active profile (loads on boot)
     */
    bool set_active_profile(const std::string& profile_name) {
        int slot = find_profile_slot(profile_name);
        if (slot == -1) {
            ESP_LOGE(TAG, "Profile '%s' not found", profile_name.c_str());
            return false;
        }

        return set_active_profile(slot);
    }

    /**
     * Set active profile by index
     */
    bool set_active_profile(int slot) {
        if (slot >= MAX_PROFILES) {
            ESP_LOGE(TAG, "Invalid profile slot: %d", slot);
            return false;
        }

        active_profile_index_ = slot;

        if (!active_pref_.save(&active_profile_index_)) {
            ESP_LOGE(TAG, "Failed to save active profile index");
            return false;
        }

        if (slot == -1) {
            ESP_LOGI(TAG, "Cleared active profile");
        } else {
            ESP_LOGI(TAG, "Set active profile to slot %d", slot);
        }

        return true;
    }

    /**
     * Get active profile name
     */
    std::string get_active_profile_name() {
        if (active_profile_index_ == -1) {
            return "none";
        }

        CalibrationProfile profile;
        if (load_profile_by_index(active_profile_index_, profile)) {
            return std::string(profile.name);
        }

        return "error";
    }

    /**
     * Load and apply the active profile on boot
     */
    bool load_and_apply_active_profile(esphome::i2c::I2CBus* bus, uint8_t address) {
        if (active_profile_index_ == -1) {
            ESP_LOGI(TAG, "No active profile to load");
            return true;  // Not an error
        }

        CalibrationProfile profile;
        if (!load_profile_by_index(active_profile_index_, profile)) {
            ESP_LOGE(TAG, "Failed to load active profile");
            return false;
        }

        ESP_LOGI(TAG, "Applying active profile '%s'", profile.name);

        // Apply all filters
        bool success = true;
        for (int i = 0; i < 15; i++) {
            // Left channel
            auto& lc = profile.left_channel[i];
            if (!tas5805m_biquad::write_biquad(bus, address, 0, i,
                                               lc.b0, lc.b1, lc.b2, lc.a1, lc.a2)) {
                ESP_LOGE(TAG, "Failed to write left biquad %d", i);
                success = false;
            }

            // Right channel
            auto& rc = profile.right_channel[i];
            if (!tas5805m_biquad::write_biquad(bus, address, 1, i,
                                               rc.b0, rc.b1, rc.b2, rc.a1, rc.a2)) {
                ESP_LOGE(TAG, "Failed to write right biquad %d", i);
                success = false;
            }

            // Small delay between writes
            delay(2);
        }

        if (success) {
            ESP_LOGI(TAG, "Successfully applied profile '%s' (%d filters)",
                     profile.name, profile.num_filters_used);
        }

        return success;
    }

private:
    esphome::ESPPreferenceObject active_pref_;
    int8_t active_profile_index_;

    /**
     * Generate NVS key for profile slot
     */
    std::string get_profile_key(int slot) {
        char key[16];
        snprintf(key, sizeof(key), "profile_%d", slot);
        return std::string(key);
    }

    /**
     * Find profile slot by name
     */
    int find_profile_slot(const std::string& profile_name) {
        for (int i = 0; i < MAX_PROFILES; i++) {
            CalibrationProfile profile;
            if (load_profile_by_index(i, profile)) {
                if (strcmp(profile.name, profile_name.c_str()) == 0) {
                    return i;
                }
            }
        }
        return -1;
    }

    /**
     * FNV-1a hash for NVS keys
     */
    uint32_t fnv1_hash(const char* str) {
        uint32_t hash = 2166136261u;
        while (*str) {
            hash ^= static_cast<uint8_t>(*str++);
            hash *= 16777619u;
        }
        return hash;
    }
};

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

/**
 * Capture current biquad state into a profile
 * (Requires reading back from TAS5805M or maintaining shadow state)
 */
inline CalibrationProfile create_profile_from_current_state() {
    CalibrationProfile profile;
    // NOTE: This would need to read back from the chip or maintain shadow state
    // For now, just return empty profile
    ESP_LOGW(TAG, "create_profile_from_current_state not fully implemented");
    return profile;
}

/**
 * Create a profile from service call data
 */
inline void add_filter_to_profile(CalibrationProfile& profile,
                                  int channel, int index,
                                  float b0, float b1, float b2, float a1, float a2) {
    if (index < 0 || index >= 15) {
        ESP_LOGE(TAG, "Invalid biquad index: %d", index);
        return;
    }

    BiquadCoefficients coeffs(b0, b1, b2, a1, a2);

    if (channel == 0 || channel == 2) {  // Left or both
        profile.left_channel[index] = coeffs;
    }

    if (channel == 1 || channel == 2) {  // Right or both
        profile.right_channel[index] = coeffs;
    }
}

// =============================================================================
// GLOBAL INSTANCES
// =============================================================================
// These are defined here (not as ESPHome globals) to ensure proper
// initialization order with ESPHome's code generation.

// Shadow copy of current biquad state - used when saving profiles
static CalibrationProfile g_current_profile_shadow;

// Profile manager instance
static ProfileManager g_profile_manager;

// Accessor functions for ESPHome lambdas
inline CalibrationProfile& current_profile_shadow() { return g_current_profile_shadow; }
inline ProfileManager& profile_manager() { return g_profile_manager; }

}  // namespace tas5805m_profile
