/**
 * Unit Tests for TAS5805M Biquad and Profile Manager
 *
 * These tests verify both pure calculation logic AND I2C/NVS operations
 * using enhanced mocks that track calls and inject failures.
 *
 * Build and run:
 *   g++ -std=c++17 -o test_tas5805m test_tas5805m.cpp -lm && ./test_tas5805m
 */

#include <cstdio>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <limits>
#include <functional>

// =============================================================================
// ENHANCED MOCK ESPHOME DEPENDENCIES
// =============================================================================

// Mock ESP logging macros (silent by default, can enable for debugging)
#ifdef TEST_VERBOSE
#define ESP_LOGE(tag, fmt, ...) printf("[E][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) printf("[W][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) printf("[I][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) printf("[D][%s] " fmt "\n", tag, ##__VA_ARGS__)
#else
#define ESP_LOGE(tag, fmt, ...) ((void)0)
#define ESP_LOGW(tag, fmt, ...) ((void)0)
#define ESP_LOGI(tag, fmt, ...) ((void)0)
#define ESP_LOGD(tag, fmt, ...) ((void)0)
#endif

// Track delay calls for timing verification
static std::vector<uint32_t> g_delay_calls;
static uint32_t g_millis_value = 0;

inline void delay(uint32_t ms) {
    g_delay_calls.push_back(ms);
}

inline uint32_t millis() {
    return g_millis_value;
}

// ESPHome expects millis in its namespace
namespace esphome {
inline uint32_t millis() {
    return g_millis_value;
}
}  // namespace esphome

// =============================================================================
// ENHANCED MOCK I2C BUS
// =============================================================================

namespace esphome {
namespace i2c {
    enum ErrorCode {
        ERROR_OK = 0,
        ERROR_TIMEOUT = 1,
        ERROR_NOT_ACKNOWLEDGED = 2,
        ERROR_UNKNOWN = 3
    };

    // I2C call record for verification
    struct I2CCall {
        uint8_t address;
        std::vector<uint8_t> data;
        bool stop;
    };

    // Global state for I2C mock
    static std::vector<I2CCall> g_i2c_calls;
    static ErrorCode g_i2c_error = ERROR_OK;
    static int g_i2c_fail_after_calls = -1;  // -1 = don't fail
    static int g_i2c_fail_count = 0;         // How many times to fail
    static int g_i2c_call_count = 0;

    // Reset mock state
    inline void mock_reset() {
        g_i2c_calls.clear();
        g_i2c_error = ERROR_OK;
        g_i2c_fail_after_calls = -1;
        g_i2c_fail_count = 0;
        g_i2c_call_count = 0;
        g_delay_calls.clear();
        g_millis_value = 0;
    }

    // Configure failure injection
    inline void mock_set_error(ErrorCode err) {
        g_i2c_error = err;
    }

    // Configure failure after N successful calls
    inline void mock_fail_after(int n, int fail_count = 1) {
        g_i2c_fail_after_calls = n;
        g_i2c_fail_count = fail_count;
    }

    class I2CBus {
    public:
        ErrorCode write(uint8_t addr, const uint8_t* data, size_t len, bool stop) {
            // Record the call
            I2CCall call;
            call.address = addr;
            call.data.assign(data, data + len);
            call.stop = stop;
            g_i2c_calls.push_back(call);

            g_i2c_call_count++;

            // Check for programmed failure
            if (g_i2c_fail_after_calls >= 0 &&
                g_i2c_call_count > g_i2c_fail_after_calls &&
                g_i2c_fail_count > 0) {
                g_i2c_fail_count--;
                return g_i2c_error != ERROR_OK ? g_i2c_error : ERROR_TIMEOUT;
            }

            return g_i2c_error;
        }
    };
}  // namespace i2c
}  // namespace esphome

// =============================================================================
// ENHANCED MOCK NVS/PREFERENCES
// =============================================================================

// Storage for mock preferences (key -> raw bytes) - at global scope
static std::map<uint32_t, std::vector<uint8_t>> g_nvs_storage;
static bool g_nvs_load_fails = false;
static bool g_nvs_save_fails = false;

// Global namespace mock functions
void mock_nvs_reset() {
    g_nvs_storage.clear();
    g_nvs_load_fails = false;
    g_nvs_save_fails = false;
}

void mock_nvs_set_load_fails(bool fails) {
    g_nvs_load_fails = fails;
}

void mock_nvs_set_save_fails(bool fails) {
    g_nvs_save_fails = fails;
}

// ESPHome namespace for preferences
namespace esphome {

class ESPPreferenceObject {
public:
    ESPPreferenceObject() : hash_(0) {}
    ESPPreferenceObject(uint32_t hash) : hash_(hash) {}

    template<typename T>
    bool load(T* value) {
        if (g_nvs_load_fails) return false;

        auto it = g_nvs_storage.find(hash_);
        if (it == g_nvs_storage.end()) return false;
        if (it->second.size() != sizeof(T)) return false;

        memcpy(value, it->second.data(), sizeof(T));
        return true;
    }

    template<typename T>
    bool save(const T* value) {
        if (g_nvs_save_fails) return false;

        std::vector<uint8_t> data(sizeof(T));
        memcpy(data.data(), value, sizeof(T));
        g_nvs_storage[hash_] = data;
        return true;
    }

private:
    uint32_t hash_;
};

class ESPPreferences {
public:
    template<typename T>
    ESPPreferenceObject make_preference(uint32_t hash) {
        return ESPPreferenceObject(hash);
    }
};

static ESPPreferences g_mock_preferences;
ESPPreferences* global_preferences = &g_mock_preferences;

}  // namespace esphome

// =============================================================================
// INCLUDE ACTUAL HEADERS UNDER TEST (after mocks are defined)
// =============================================================================

// Now include the actual implementation headers - they will use our mocks
#include "tas5805m_biquad_i2c.h"
#include "tas5805m_profile_manager.h"

// =============================================================================
// TEST FRAMEWORK
// =============================================================================

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// Reset all mock state before each test
inline void test_setup() {
    esphome::i2c::mock_reset();
    mock_nvs_reset();
}

#define TEST(name) \
    void test_##name(); \
    struct TestRegistrar_##name { \
        TestRegistrar_##name() { \
            printf("  Running: %s ... ", #name); \
            fflush(stdout); \
            tests_run++; \
            try { \
                test_setup(); \
                test_##name(); \
                printf("PASSED\n"); \
                tests_passed++; \
            } catch (const char* msg) { \
                printf("FAILED: %s\n", msg); \
                tests_failed++; \
            } \
        } \
    } test_registrar_##name; \
    void test_##name()

#define ASSERT_TRUE(expr) \
    if (!(expr)) throw "ASSERT_TRUE failed: " #expr

#define ASSERT_FALSE(expr) \
    if (expr) throw "ASSERT_FALSE failed: " #expr

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) throw "ASSERT_EQ failed: " #a " != " #b

#define ASSERT_NEAR(a, b, eps) \
    if (std::fabs((a) - (b)) > (eps)) throw "ASSERT_NEAR failed: " #a " != " #b

#define ASSERT_GT(a, b) \
    if (!((a) > (b))) throw "ASSERT_GT failed: " #a " <= " #b

#define ASSERT_GE(a, b) \
    if (!((a) >= (b))) throw "ASSERT_GE failed: " #a " < " #b

#define ASSERT_LT(a, b) \
    if (!((a) < (b))) throw "ASSERT_LT failed: " #a " >= " #b

#define ASSERT_LE(a, b) \
    if (!((a) <= (b))) throw "ASSERT_LE failed: " #a " > " #b

#define ASSERT_NE(a, b) \
    if ((a) == (b)) throw "ASSERT_NE failed: " #a " == " #b

// =============================================================================
// LOCAL TEST HELPERS FOR FILTER CALCULATION TESTS
// =============================================================================

// These are test-only pure calculation functions used by existing tests.
// They mirror the production code but don't depend on I2C.
namespace test_helpers {

struct FilterCoeffs {
    float b0, b1, b2, a1, a2;
};

inline FilterCoeffs calc_parametric_eq(float frequency, float gain_db, float q, float fs = 48000.0f) {
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

    b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;

    return {b0, b1, b2, a1, a2};
}

inline FilterCoeffs calc_low_shelf(float frequency, float gain_db, float slope = 1.0f, float fs = 48000.0f) {
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

    return {b0, b1, b2, a1, a2};
}

inline FilterCoeffs calc_high_shelf(float frequency, float gain_db, float slope = 1.0f, float fs = 48000.0f) {
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

    return {b0, b1, b2, a1, a2};
}

inline FilterCoeffs calc_highpass(float frequency, float q, float fs = 48000.0f) {
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

    return {b0, b1, b2, a1, a2};
}

inline FilterCoeffs calc_lowpass(float frequency, float q, float fs = 48000.0f) {
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

    return {b0, b1, b2, a1, a2};
}

inline FilterCoeffs calc_notch(float frequency, float q, float fs = 48000.0f) {
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

    return {b0, b1, b2, a1, a2};
}

// FNV-1a hash for testing (mirrors the private function in ProfileManager)
inline uint32_t fnv1_hash(const char* str) {
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= static_cast<uint8_t>(*str++);
        hash *= 16777619u;
    }
    return hash;
}

}  // namespace test_helpers

// =============================================================================
// TESTS: VALIDATION FUNCTIONS
// =============================================================================

TEST(validate_channel_valid) {
    ASSERT_TRUE(tas5805m_biquad::validate_channel(0));   // Left
    ASSERT_TRUE(tas5805m_biquad::validate_channel(1));   // Right
    ASSERT_TRUE(tas5805m_biquad::validate_channel(2));   // Both
}

TEST(validate_channel_invalid) {
    ASSERT_FALSE(tas5805m_biquad::validate_channel(-1));
    ASSERT_FALSE(tas5805m_biquad::validate_channel(3));
    ASSERT_FALSE(tas5805m_biquad::validate_channel(100));
}

TEST(validate_index_valid) {
    for (int i = 0; i < 15; i++) {
        ASSERT_TRUE(tas5805m_biquad::validate_index(i));
    }
}

TEST(validate_index_invalid) {
    ASSERT_FALSE(tas5805m_biquad::validate_index(-1));
    ASSERT_FALSE(tas5805m_biquad::validate_index(15));
    ASSERT_FALSE(tas5805m_biquad::validate_index(100));
}

TEST(validate_frequency_valid) {
    ASSERT_TRUE(tas5805m_biquad::validate_frequency(10.0f));     // Min
    ASSERT_TRUE(tas5805m_biquad::validate_frequency(1000.0f));   // Mid
    ASSERT_TRUE(tas5805m_biquad::validate_frequency(24000.0f));  // Max
}

TEST(validate_frequency_invalid) {
    ASSERT_FALSE(tas5805m_biquad::validate_frequency(9.9f));      // Below min
    ASSERT_FALSE(tas5805m_biquad::validate_frequency(24001.0f)); // Above max
    ASSERT_FALSE(tas5805m_biquad::validate_frequency(NAN));
    ASSERT_FALSE(tas5805m_biquad::validate_frequency(INFINITY));
    ASSERT_FALSE(tas5805m_biquad::validate_frequency(-INFINITY));
}

TEST(validate_gain_valid) {
    ASSERT_TRUE(tas5805m_biquad::validate_gain(-20.0f));  // Min
    ASSERT_TRUE(tas5805m_biquad::validate_gain(0.0f));    // Unity
    ASSERT_TRUE(tas5805m_biquad::validate_gain(20.0f));   // Max
}

TEST(validate_gain_invalid) {
    ASSERT_FALSE(tas5805m_biquad::validate_gain(-21.0f));
    ASSERT_FALSE(tas5805m_biquad::validate_gain(21.0f));
    ASSERT_FALSE(tas5805m_biquad::validate_gain(NAN));
}

TEST(validate_q_valid) {
    ASSERT_TRUE(tas5805m_biquad::validate_q(0.1f));   // Min
    ASSERT_TRUE(tas5805m_biquad::validate_q(0.707f)); // Butterworth
    ASSERT_TRUE(tas5805m_biquad::validate_q(1.0f));   // Common
    ASSERT_TRUE(tas5805m_biquad::validate_q(20.0f));  // Max
}

TEST(validate_q_invalid) {
    ASSERT_FALSE(tas5805m_biquad::validate_q(0.09f));
    ASSERT_FALSE(tas5805m_biquad::validate_q(21.0f));
    ASSERT_FALSE(tas5805m_biquad::validate_q(NAN));
}

TEST(validate_slope_valid) {
    ASSERT_TRUE(tas5805m_biquad::validate_slope(0.1f));
    ASSERT_TRUE(tas5805m_biquad::validate_slope(1.0f));  // Default
    ASSERT_TRUE(tas5805m_biquad::validate_slope(5.0f));
}

TEST(validate_slope_invalid) {
    ASSERT_FALSE(tas5805m_biquad::validate_slope(0.05f));
    ASSERT_FALSE(tas5805m_biquad::validate_slope(6.0f));
    ASSERT_FALSE(tas5805m_biquad::validate_slope(NAN));
}

TEST(validate_coefficients_valid) {
    ASSERT_TRUE(tas5805m_biquad::validate_coefficients(1.0f, 0.0f, 0.0f, 0.0f, 0.0f));  // Bypass
    ASSERT_TRUE(tas5805m_biquad::validate_coefficients(1.5f, -2.0f, 0.5f, -1.9f, 0.95f));
}

TEST(validate_coefficients_invalid) {
    ASSERT_FALSE(tas5805m_biquad::validate_coefficients(NAN, 0.0f, 0.0f, 0.0f, 0.0f));
    ASSERT_FALSE(tas5805m_biquad::validate_coefficients(1.0f, INFINITY, 0.0f, 0.0f, 0.0f));
    ASSERT_FALSE(tas5805m_biquad::validate_coefficients(1.0f, 0.0f, -INFINITY, 0.0f, 0.0f));
}

// =============================================================================
// TESTS: CONVERSION FUNCTIONS
// =============================================================================

TEST(float_to_9_23_unity) {
    // 1.0 * 2^23 = 8388608
    int32_t result = tas5805m_biquad::float_to_9_23(1.0f);
    ASSERT_EQ(result, 8388608);
}

TEST(float_to_9_23_zero) {
    int32_t result = tas5805m_biquad::float_to_9_23(0.0f);
    ASSERT_EQ(result, 0);
}

TEST(float_to_9_23_negative) {
    // -1.0 * 2^23 = -8388608
    int32_t result = tas5805m_biquad::float_to_9_23(-1.0f);
    ASSERT_EQ(result, -8388608);
}

TEST(float_to_9_23_half) {
    // 0.5 * 2^23 = 4194304
    int32_t result = tas5805m_biquad::float_to_9_23(0.5f);
    ASSERT_EQ(result, 4194304);
}

TEST(float_to_9_23_clamping_high) {
    // Values > 255.999999 should be clamped
    int32_t max_val = tas5805m_biquad::float_to_9_23(255.999999f);
    int32_t over_val = tas5805m_biquad::float_to_9_23(500.0f);
    ASSERT_EQ(max_val, over_val);  // Both should be clamped to same value
}

TEST(float_to_9_23_clamping_low) {
    // Values < -256 should be clamped
    int32_t min_val = tas5805m_biquad::float_to_9_23(-256.0f);
    int32_t under_val = tas5805m_biquad::float_to_9_23(-500.0f);
    ASSERT_EQ(min_val, under_val);
}

TEST(float_to_9_23_nan) {
    // NaN should return 0 (bypass coefficient)
    int32_t result = tas5805m_biquad::float_to_9_23(NAN);
    ASSERT_EQ(result, 0);
}

TEST(float_to_9_23_infinity) {
    // Infinity should return 0 (bypass coefficient)
    int32_t result = tas5805m_biquad::float_to_9_23(INFINITY);
    ASSERT_EQ(result, 0);
}

TEST(pack_be32_positive) {
    uint8_t buffer[4];
    tas5805m_biquad::pack_be32(0x12345678, buffer);
    ASSERT_EQ(buffer[0], 0x12);
    ASSERT_EQ(buffer[1], 0x34);
    ASSERT_EQ(buffer[2], 0x56);
    ASSERT_EQ(buffer[3], 0x78);
}

TEST(pack_be32_zero) {
    uint8_t buffer[4];
    tas5805m_biquad::pack_be32(0, buffer);
    ASSERT_EQ(buffer[0], 0x00);
    ASSERT_EQ(buffer[1], 0x00);
    ASSERT_EQ(buffer[2], 0x00);
    ASSERT_EQ(buffer[3], 0x00);
}

TEST(pack_be32_negative) {
    uint8_t buffer[4];
    tas5805m_biquad::pack_be32(-1, buffer);  // 0xFFFFFFFF
    ASSERT_EQ(buffer[0], 0xFF);
    ASSERT_EQ(buffer[1], 0xFF);
    ASSERT_EQ(buffer[2], 0xFF);
    ASSERT_EQ(buffer[3], 0xFF);
}

TEST(pack_be32_unity_9_23) {
    // 1.0 in 9.23 format = 0x00800000 = 8388608
    uint8_t buffer[4];
    tas5805m_biquad::pack_be32(8388608, buffer);
    ASSERT_EQ(buffer[0], 0x00);
    ASSERT_EQ(buffer[1], 0x80);
    ASSERT_EQ(buffer[2], 0x00);
    ASSERT_EQ(buffer[3], 0x00);
}

// =============================================================================
// TESTS: FILTER COEFFICIENT CALCULATORS
// =============================================================================

// Helper to check coefficient stability (all finite)
bool coeffs_are_stable(const test_helpers::FilterCoeffs& c) {
    return std::isfinite(c.b0) && std::isfinite(c.b1) && std::isfinite(c.b2) &&
           std::isfinite(c.a1) && std::isfinite(c.a2);
}

TEST(parametric_eq_zero_gain_is_bypass) {
    // With 0dB gain, parametric EQ should be near-bypass (b0≈1, others≈0)
    auto c = test_helpers::calc_parametric_eq(1000.0f, 0.0f, 1.0f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
    ASSERT_NEAR(c.b0, 1.0f, 0.0001f);
    ASSERT_NEAR(c.b1, c.a1, 0.0001f);  // b1 and a1 should match
    ASSERT_NEAR(c.b2, c.a2, 0.0001f);  // b2 and a2 should match
}

TEST(parametric_eq_positive_gain) {
    auto c = test_helpers::calc_parametric_eq(1000.0f, 6.0f, 1.0f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
    // At resonance, b0 should be > 1 for boost
    ASSERT_TRUE(c.b0 > 1.0f);
}

TEST(parametric_eq_negative_gain) {
    auto c = test_helpers::calc_parametric_eq(1000.0f, -6.0f, 1.0f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
    // For cut, b0 should be < 1
    ASSERT_TRUE(c.b0 < 1.0f);
}

TEST(parametric_eq_high_q) {
    auto c = test_helpers::calc_parametric_eq(1000.0f, 6.0f, 10.0f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
}

TEST(low_shelf_zero_gain) {
    auto c = test_helpers::calc_low_shelf(200.0f, 0.0f, 1.0f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
    // Should be approximately bypass
    ASSERT_NEAR(c.b0, 1.0f, 0.001f);
}

TEST(low_shelf_boost) {
    auto c = test_helpers::calc_low_shelf(200.0f, 6.0f, 1.0f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
}

TEST(low_shelf_cut) {
    auto c = test_helpers::calc_low_shelf(200.0f, -6.0f, 1.0f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
}

TEST(high_shelf_zero_gain) {
    auto c = test_helpers::calc_high_shelf(8000.0f, 0.0f, 1.0f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
    ASSERT_NEAR(c.b0, 1.0f, 0.001f);
}

TEST(high_shelf_boost) {
    auto c = test_helpers::calc_high_shelf(8000.0f, 6.0f, 1.0f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
}

TEST(highpass_butterworth) {
    // Q = 0.707 is Butterworth (maximally flat)
    auto c = test_helpers::calc_highpass(100.0f, 0.707f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
    // b1 should be -2*b0 for HP
    ASSERT_NEAR(c.b1, -2.0f * c.b0, 0.0001f);
    ASSERT_NEAR(c.b2, c.b0, 0.0001f);
}

TEST(highpass_low_frequency) {
    auto c = test_helpers::calc_highpass(20.0f, 0.707f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
}

TEST(highpass_high_frequency) {
    auto c = test_helpers::calc_highpass(10000.0f, 0.707f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
}

TEST(lowpass_butterworth) {
    auto c = test_helpers::calc_lowpass(10000.0f, 0.707f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
    // b1 should be 2*b0 for LP
    ASSERT_NEAR(c.b1, 2.0f * c.b0, 0.0001f);
    ASSERT_NEAR(c.b2, c.b0, 0.0001f);
}

TEST(lowpass_near_nyquist) {
    // Test near Nyquist frequency (fs/2)
    auto c = test_helpers::calc_lowpass(23000.0f, 0.707f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
}

TEST(notch_stability) {
    auto c = test_helpers::calc_notch(1000.0f, 10.0f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
    // For notch: b0 and b2 should equal 1/a0 (after normalization)
    // and b0 == b2
    ASSERT_NEAR(c.b0, c.b2, 0.0001f);
}

TEST(notch_high_q) {
    // High Q = narrow notch
    auto c = test_helpers::calc_notch(60.0f, 20.0f, 48000.0f);  // 60Hz hum filter
    ASSERT_TRUE(coeffs_are_stable(c));
}

TEST(notch_low_q) {
    auto c = test_helpers::calc_notch(1000.0f, 0.5f, 48000.0f);  // Wide notch
    ASSERT_TRUE(coeffs_are_stable(c));
}

// =============================================================================
// TESTS: PROFILE MANAGER STRUCTURES
// =============================================================================

TEST(biquad_coefficients_default_is_bypass) {
    tas5805m_profile::BiquadCoefficients c;
    ASSERT_TRUE(c.is_bypass());
}

TEST(biquad_coefficients_non_bypass) {
    tas5805m_profile::BiquadCoefficients c(1.5f, -2.0f, 0.5f, -1.9f, 0.95f);
    ASSERT_FALSE(c.is_bypass());
}

TEST(biquad_coefficients_near_bypass) {
    // Very close to bypass but not quite - should still count as bypass
    tas5805m_profile::BiquadCoefficients c(1.00005f, 0.00001f, 0.0f, 0.0f, 0.0f);
    ASSERT_TRUE(c.is_bypass());  // Within tolerance
}

TEST(calibration_profile_default_valid) {
    tas5805m_profile::CalibrationProfile p;
    p.update_checksum();
    ASSERT_TRUE(p.is_valid());
}

TEST(calibration_profile_magic_check) {
    tas5805m_profile::CalibrationProfile p;
    p.magic = 0x12345678;  // Wrong magic
    p.update_checksum();
    ASSERT_FALSE(p.is_valid());  // Should fail due to wrong magic
}

TEST(calibration_profile_checksum_detects_corruption) {
    tas5805m_profile::CalibrationProfile p;
    p.update_checksum();
    ASSERT_TRUE(p.is_valid());

    // Corrupt data
    p.name[0] = 'X';
    ASSERT_FALSE(p.is_valid());  // Checksum mismatch
}

TEST(calibration_profile_count_filters_empty) {
    tas5805m_profile::CalibrationProfile p;
    p.count_active_filters();
    ASSERT_EQ(p.num_filters_used, 0);
}

TEST(calibration_profile_count_filters_some) {
    tas5805m_profile::CalibrationProfile p;
    // Set some non-bypass filters
    p.left_channel[0] = tas5805m_profile::BiquadCoefficients(1.5f, -2.0f, 0.5f, -1.9f, 0.95f);
    p.left_channel[5] = tas5805m_profile::BiquadCoefficients(1.2f, -1.8f, 0.6f, -1.7f, 0.85f);
    p.right_channel[0] = tas5805m_profile::BiquadCoefficients(1.5f, -2.0f, 0.5f, -1.9f, 0.95f);
    p.count_active_filters();
    ASSERT_EQ(p.num_filters_used, 2);  // Index 0 and 5 have non-bypass
}

TEST(calibration_profile_count_filters_all) {
    tas5805m_profile::CalibrationProfile p;
    // Set all filters to non-bypass
    for (int i = 0; i < 15; i++) {
        p.left_channel[i] = tas5805m_profile::BiquadCoefficients(1.5f, -2.0f, 0.5f, -1.9f, 0.95f);
        p.right_channel[i] = tas5805m_profile::BiquadCoefficients(1.5f, -2.0f, 0.5f, -1.9f, 0.95f);
    }
    p.count_active_filters();
    ASSERT_EQ(p.num_filters_used, 15);
}

TEST(fnv1_hash_empty) {
    uint32_t hash = test_helpers::fnv1_hash("");
    ASSERT_EQ(hash, 2166136261u);  // FNV offset basis
}

TEST(fnv1_hash_known_values) {
    // These are deterministic - same input always gives same output
    uint32_t hash1 = test_helpers::fnv1_hash("test");
    uint32_t hash2 = test_helpers::fnv1_hash("test");
    ASSERT_EQ(hash1, hash2);

    // Different strings should produce different hashes (with high probability)
    uint32_t hash3 = test_helpers::fnv1_hash("profile_0");
    uint32_t hash4 = test_helpers::fnv1_hash("profile_1");
    ASSERT_TRUE(hash3 != hash4);
}

TEST(fnv1_hash_profile_keys) {
    // Test typical profile keys
    uint32_t h0 = test_helpers::fnv1_hash("profile_0");
    uint32_t h1 = test_helpers::fnv1_hash("profile_1");
    uint32_t h2 = test_helpers::fnv1_hash("profile_2");
    uint32_t h3 = test_helpers::fnv1_hash("profile_3");
    uint32_t h4 = test_helpers::fnv1_hash("profile_4");

    // All should be unique
    ASSERT_TRUE(h0 != h1);
    ASSERT_TRUE(h1 != h2);
    ASSERT_TRUE(h2 != h3);
    ASSERT_TRUE(h3 != h4);
}

// =============================================================================
// TESTS: EDGE CASES AND NUMERICAL STABILITY
// =============================================================================

TEST(filter_at_nyquist) {
    // Filters at exactly Nyquist/2 can be unstable
    float fs = 48000.0f;
    float nyquist = fs / 2.0f;

    // Just below Nyquist should still work
    auto c = test_helpers::calc_lowpass(nyquist - 100.0f, 0.707f, fs);
    ASSERT_TRUE(coeffs_are_stable(c));
}

TEST(filter_very_low_frequency) {
    // Very low frequency filter
    auto c = test_helpers::calc_highpass(10.0f, 0.707f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
}

TEST(filter_extreme_q) {
    // Very high Q = very narrow filter
    auto c = test_helpers::calc_parametric_eq(1000.0f, 6.0f, 20.0f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));

    // Very low Q = very broad filter
    auto c2 = test_helpers::calc_parametric_eq(1000.0f, 6.0f, 0.1f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c2));
}

TEST(filter_extreme_gain) {
    auto boost = test_helpers::calc_parametric_eq(1000.0f, 20.0f, 1.0f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(boost));

    auto cut = test_helpers::calc_parametric_eq(1000.0f, -20.0f, 1.0f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(cut));
}

TEST(calibration_profile_name_truncation) {
    tas5805m_profile::CalibrationProfile p;
    // Name is 32 chars max, try setting a longer one
    const char* long_name = "This is a very long profile name that exceeds the maximum";
    strncpy(p.name, long_name, tas5805m_profile::MAX_PROFILE_NAME_LEN - 1);
    p.name[tas5805m_profile::MAX_PROFILE_NAME_LEN - 1] = '\0';

    // Should be truncated
    ASSERT_EQ(strlen(p.name), tas5805m_profile::MAX_PROFILE_NAME_LEN - 1);
}

// =============================================================================
// TIER 1 TESTS: I2C COMMUNICATION LAYER
// =============================================================================

// Note: tas5805m_biquad_i2c.h is included via tas5805m_profile_manager.h which
// was included earlier with the proper mock setup

TEST(i2c_write_byte_success) {
    esphome::i2c::I2CBus bus;
    tas5805m_biquad::TAS5805M_I2C dev(&bus, 0x2C);

    bool result = dev.write_byte(0x00, 0x55);
    ASSERT_TRUE(result);

    // Verify I2C call was made
    ASSERT_EQ(esphome::i2c::g_i2c_calls.size(), 1u);
    ASSERT_EQ(esphome::i2c::g_i2c_calls[0].address, 0x2C);
    ASSERT_EQ(esphome::i2c::g_i2c_calls[0].data.size(), 2u);
    ASSERT_EQ(esphome::i2c::g_i2c_calls[0].data[0], 0x00);  // Register
    ASSERT_EQ(esphome::i2c::g_i2c_calls[0].data[1], 0x55);  // Value
}

TEST(i2c_write_byte_retry_on_failure) {
    esphome::i2c::I2CBus bus;
    tas5805m_biquad::TAS5805M_I2C dev(&bus, 0x2C);

    // Fail first 2 attempts, succeed on 3rd
    esphome::i2c::mock_fail_after(0, 2);

    bool result = dev.write_byte(0x00, 0x55);
    ASSERT_TRUE(result);

    // Should have tried 3 times
    ASSERT_EQ(esphome::i2c::g_i2c_calls.size(), 3u);

    // Should have delay between retries (5ms each)
    ASSERT_EQ(g_delay_calls.size(), 2u);
    ASSERT_EQ(g_delay_calls[0], 5u);
    ASSERT_EQ(g_delay_calls[1], 5u);
}

TEST(i2c_write_byte_fails_after_max_retries) {
    esphome::i2c::I2CBus bus;
    tas5805m_biquad::TAS5805M_I2C dev(&bus, 0x2C);

    // Always fail
    esphome::i2c::mock_set_error(esphome::i2c::ERROR_TIMEOUT);

    bool result = dev.write_byte(0x00, 0x55);
    ASSERT_FALSE(result);

    // Should have tried 3 times (MAX_RETRIES)
    ASSERT_EQ(esphome::i2c::g_i2c_calls.size(), 3u);
}

TEST(i2c_write_bytes_success) {
    esphome::i2c::I2CBus bus;
    tas5805m_biquad::TAS5805M_I2C dev(&bus, 0x2C);

    uint8_t data[] = {0x11, 0x22, 0x33, 0x44};
    bool result = dev.write_bytes(0x08, data, 4);
    ASSERT_TRUE(result);

    // Verify I2C call
    ASSERT_EQ(esphome::i2c::g_i2c_calls.size(), 1u);
    ASSERT_EQ(esphome::i2c::g_i2c_calls[0].data.size(), 5u);  // reg + 4 bytes
    ASSERT_EQ(esphome::i2c::g_i2c_calls[0].data[0], 0x08);    // Register
    ASSERT_EQ(esphome::i2c::g_i2c_calls[0].data[1], 0x11);
    ASSERT_EQ(esphome::i2c::g_i2c_calls[0].data[4], 0x44);
}

TEST(i2c_write_bytes_retry_on_failure) {
    esphome::i2c::I2CBus bus;
    tas5805m_biquad::TAS5805M_I2C dev(&bus, 0x2C);

    // Fail first attempt
    esphome::i2c::mock_fail_after(0, 1);

    uint8_t data[] = {0xAA, 0xBB};
    bool result = dev.write_bytes(0x10, data, 2);
    ASSERT_TRUE(result);

    // Should have tried twice
    ASSERT_EQ(esphome::i2c::g_i2c_calls.size(), 2u);
}

TEST(i2c_select_book_page) {
    esphome::i2c::I2CBus bus;
    tas5805m_biquad::TAS5805M_I2C dev(&bus, 0x2C);

    bool result = dev.select_book_page(0xAA, 0x24);
    ASSERT_TRUE(result);

    // Should have 3 I2C writes: page=0, book=0xAA, page=0x24
    ASSERT_EQ(esphome::i2c::g_i2c_calls.size(), 3u);

    // First: select page 0
    ASSERT_EQ(esphome::i2c::g_i2c_calls[0].data[0], 0x00);  // REG_PAGE_SELECT
    ASSERT_EQ(esphome::i2c::g_i2c_calls[0].data[1], 0x00);  // Page 0

    // Second: select book
    ASSERT_EQ(esphome::i2c::g_i2c_calls[1].data[0], 0x7F);  // REG_BOOK_SELECT
    ASSERT_EQ(esphome::i2c::g_i2c_calls[1].data[1], 0xAA);  // Book 0xAA

    // Third: select page within book
    ASSERT_EQ(esphome::i2c::g_i2c_calls[2].data[0], 0x00);  // REG_PAGE_SELECT
    ASSERT_EQ(esphome::i2c::g_i2c_calls[2].data[1], 0x24);  // Page 0x24

    // Should have delays between operations
    ASSERT_GE(g_delay_calls.size(), 3u);
}

TEST(i2c_select_book_page_fails_on_first_write) {
    esphome::i2c::I2CBus bus;
    tas5805m_biquad::TAS5805M_I2C dev(&bus, 0x2C);

    // Always fail
    esphome::i2c::mock_set_error(esphome::i2c::ERROR_NOT_ACKNOWLEDGED);

    bool result = dev.select_book_page(0xAA, 0x24);
    ASSERT_FALSE(result);
}

TEST(i2c_return_to_normal) {
    esphome::i2c::I2CBus bus;
    tas5805m_biquad::TAS5805M_I2C dev(&bus, 0x2C);

    bool result = dev.return_to_normal();
    ASSERT_TRUE(result);

    // Should have 2 writes: page=0, book=0
    ASSERT_EQ(esphome::i2c::g_i2c_calls.size(), 2u);
    ASSERT_EQ(esphome::i2c::g_i2c_calls[0].data[1], 0x00);  // Page 0
    ASSERT_EQ(esphome::i2c::g_i2c_calls[1].data[1], 0x00);  // Book 0
}

// =============================================================================
// TIER 1 TESTS: BIQUAD WRITE OPERATIONS
// =============================================================================

TEST(write_biquad_success_left_channel) {
    esphome::i2c::I2CBus bus;

    bool result = tas5805m_biquad::write_biquad(&bus, 0x2C, 0, 0,
                                                 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    ASSERT_TRUE(result);

    // Should have: select_book_page (3 writes) + write coeffs (1 write) + return_to_normal (2 writes)
    ASSERT_GE(esphome::i2c::g_i2c_calls.size(), 5u);
}

TEST(write_biquad_success_right_channel) {
    esphome::i2c::I2CBus bus;

    bool result = tas5805m_biquad::write_biquad(&bus, 0x2C, 1, 5,
                                                 1.5f, -2.0f, 0.5f, -1.9f, 0.95f);
    ASSERT_TRUE(result);
    ASSERT_GE(esphome::i2c::g_i2c_calls.size(), 5u);
}

TEST(write_biquad_success_both_channels) {
    esphome::i2c::I2CBus bus;

    bool result = tas5805m_biquad::write_biquad(&bus, 0x2C, 2, 7,
                                                 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    ASSERT_TRUE(result);

    // Both channels = twice as many coefficient writes
    ASSERT_GE(esphome::i2c::g_i2c_calls.size(), 8u);
}

TEST(write_biquad_invalid_index) {
    esphome::i2c::I2CBus bus;

    // Index -1 should fail
    bool result = tas5805m_biquad::write_biquad(&bus, 0x2C, 0, -1,
                                                 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    ASSERT_FALSE(result);

    // Index 15 should fail
    result = tas5805m_biquad::write_biquad(&bus, 0x2C, 0, 15,
                                            1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    ASSERT_FALSE(result);
}

TEST(write_biquad_coefficients_packed_correctly) {
    esphome::i2c::I2CBus bus;

    // Write with b0=1.0 (which is 0x00800000 in 9.23 format)
    bool result = tas5805m_biquad::write_biquad(&bus, 0x2C, 0, 0,
                                                 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    ASSERT_TRUE(result);

    // Find the coefficient write (should be 21 bytes: reg + 20 coeff bytes)
    bool found_coeff_write = false;
    for (const auto& call : esphome::i2c::g_i2c_calls) {
        if (call.data.size() == 21) {
            found_coeff_write = true;
            // Check b0 = 1.0 = 0x00800000 in big-endian
            ASSERT_EQ(call.data[1], 0x00);
            ASSERT_EQ(call.data[2], 0x80);
            ASSERT_EQ(call.data[3], 0x00);
            ASSERT_EQ(call.data[4], 0x00);
            break;
        }
    }
    ASSERT_TRUE(found_coeff_write);
}

TEST(write_biquad_a1_a2_sign_inverted) {
    esphome::i2c::I2CBus bus;

    // Write with a1=1.0, a2=0.5 - these should be sign-inverted
    bool result = tas5805m_biquad::write_biquad(&bus, 0x2C, 0, 0,
                                                 1.0f, 0.0f, 0.0f, 1.0f, 0.5f);
    ASSERT_TRUE(result);

    // Find the coefficient write
    for (const auto& call : esphome::i2c::g_i2c_calls) {
        if (call.data.size() == 21) {
            // a1 at offset 13-16 should be -1.0 = 0xFF800000
            ASSERT_EQ(call.data[13], 0xFF);
            ASSERT_EQ(call.data[14], 0x80);
            ASSERT_EQ(call.data[15], 0x00);
            ASSERT_EQ(call.data[16], 0x00);
            // a2 at offset 17-20 should be -0.5 = 0xFFC00000
            ASSERT_EQ(call.data[17], 0xFF);
            ASSERT_EQ(call.data[18], 0xC0);
            ASSERT_EQ(call.data[19], 0x00);
            ASSERT_EQ(call.data[20], 0x00);
            break;
        }
    }
}

TEST(reset_biquad_writes_bypass) {
    esphome::i2c::I2CBus bus;

    bool result = tas5805m_biquad::reset_biquad(&bus, 0x2C, 0, 3);
    ASSERT_TRUE(result);

    // Should write bypass coefficients (b0=1, all others=0)
    bool found_coeff_write = false;
    for (const auto& call : esphome::i2c::g_i2c_calls) {
        if (call.data.size() == 21) {
            found_coeff_write = true;
            // b0 = 1.0 = 0x00800000
            ASSERT_EQ(call.data[1], 0x00);
            ASSERT_EQ(call.data[2], 0x80);
            // b1 = 0
            ASSERT_EQ(call.data[5], 0x00);
            ASSERT_EQ(call.data[6], 0x00);
            break;
        }
    }
    ASSERT_TRUE(found_coeff_write);
}

TEST(reset_all_biquads_resets_30_filters) {
    esphome::i2c::I2CBus bus;

    bool result = tas5805m_biquad::reset_all_biquads(&bus, 0x2C);
    ASSERT_TRUE(result);

    // Should write to all 15 biquads x 2 channels
    // Count coefficient writes (21-byte writes)
    int coeff_writes = 0;
    for (const auto& call : esphome::i2c::g_i2c_calls) {
        if (call.data.size() == 21) {
            coeff_writes++;
        }
    }
    ASSERT_EQ(coeff_writes, 30);  // 15 biquads x 2 channels
}

// =============================================================================
// TIER 1 TESTS: BATCHED I2C OPERATIONS
// =============================================================================

TEST(write_biquads_page_single_biquad) {
    esphome::i2c::I2CBus bus;
    tas5805m_biquad::TAS5805M_I2C dev(&bus, 0x2C);

    tas5805m_biquad::BiquadCoeffs coeffs[1] = {
        {1.0f, 0.0f, 0.0f, 0.0f, 0.0f}
    };

    bool result = tas5805m_biquad::write_biquads_page(dev, 0x24, coeffs, 1, 0);
    ASSERT_TRUE(result);
}

TEST(write_biquads_page_four_biquads) {
    esphome::i2c::I2CBus bus;
    tas5805m_biquad::TAS5805M_I2C dev(&bus, 0x2C);

    tas5805m_biquad::BiquadCoeffs coeffs[4] = {
        {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        {1.5f, -1.0f, 0.5f, -1.0f, 0.5f},
        {2.0f, -1.5f, 0.5f, -1.5f, 0.5f},
        {1.0f, 0.0f, 0.0f, 0.0f, 0.0f}
    };

    bool result = tas5805m_biquad::write_biquads_page(dev, 0x24, coeffs, 4, 0);
    ASSERT_TRUE(result);

    // Should write 4 coefficient sets
    int coeff_writes = 0;
    for (const auto& call : esphome::i2c::g_i2c_calls) {
        if (call.data.size() == 21) {
            coeff_writes++;
        }
    }
    ASSERT_EQ(coeff_writes, 4);
}

TEST(write_biquads_page_invalid_count) {
    esphome::i2c::I2CBus bus;
    tas5805m_biquad::TAS5805M_I2C dev(&bus, 0x2C);

    tas5805m_biquad::BiquadCoeffs coeffs[1];

    // count = 0 should fail
    ASSERT_FALSE(tas5805m_biquad::write_biquads_page(dev, 0x24, coeffs, 0, 0));

    // count > 4 should fail
    ASSERT_FALSE(tas5805m_biquad::write_biquads_page(dev, 0x24, coeffs, 5, 0));
}

TEST(write_channel_biquads_batched_left) {
    esphome::i2c::I2CBus bus;

    tas5805m_biquad::BiquadCoeffs coeffs[15];
    for (int i = 0; i < 15; i++) {
        coeffs[i] = tas5805m_biquad::BiquadCoeffs(1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    }

    bool result = tas5805m_biquad::write_channel_biquads_batched(&bus, 0x2C, 0, coeffs);
    ASSERT_TRUE(result);

    // Count coefficient writes
    int coeff_writes = 0;
    for (const auto& call : esphome::i2c::g_i2c_calls) {
        if (call.data.size() == 21) {
            coeff_writes++;
        }
    }
    ASSERT_EQ(coeff_writes, 15);
}

TEST(write_channel_biquads_batched_right) {
    esphome::i2c::I2CBus bus;

    tas5805m_biquad::BiquadCoeffs coeffs[15];
    for (int i = 0; i < 15; i++) {
        coeffs[i] = tas5805m_biquad::BiquadCoeffs(1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    }

    bool result = tas5805m_biquad::write_channel_biquads_batched(&bus, 0x2C, 1, coeffs);
    ASSERT_TRUE(result);

    // Verify right channel pages are used (0x32-0x35)
    bool found_right_page = false;
    for (const auto& call : esphome::i2c::g_i2c_calls) {
        if (call.data.size() == 2 && call.data[0] == 0x00) {  // Page select
            if (call.data[1] == 0x32 || call.data[1] == 0x33 ||
                call.data[1] == 0x34 || call.data[1] == 0x35) {
                found_right_page = true;
                break;
            }
        }
    }
    ASSERT_TRUE(found_right_page);
}

TEST(write_channel_biquads_batched_invalid_channel) {
    esphome::i2c::I2CBus bus;

    tas5805m_biquad::BiquadCoeffs coeffs[15];

    // Channel 2 (both) should fail for batched single-channel write
    bool result = tas5805m_biquad::write_channel_biquads_batched(&bus, 0x2C, 2, coeffs);
    ASSERT_FALSE(result);
}

TEST(write_all_biquads_batched_writes_30_filters) {
    esphome::i2c::I2CBus bus;

    tas5805m_biquad::BiquadCoeffs left[15], right[15];
    for (int i = 0; i < 15; i++) {
        left[i] = tas5805m_biquad::BiquadCoeffs(1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        right[i] = tas5805m_biquad::BiquadCoeffs(1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    }

    bool result = tas5805m_biquad::write_all_biquads_batched(&bus, 0x2C, left, right);
    ASSERT_TRUE(result);

    // Count coefficient writes (should be 30)
    int coeff_writes = 0;
    for (const auto& call : esphome::i2c::g_i2c_calls) {
        if (call.data.size() == 21) {
            coeff_writes++;
        }
    }
    ASSERT_EQ(coeff_writes, 30);
}

TEST(reset_all_biquads_batched) {
    esphome::i2c::I2CBus bus;

    bool result = tas5805m_biquad::reset_all_biquads_batched(&bus, 0x2C);
    ASSERT_TRUE(result);

    // Count coefficient writes
    int coeff_writes = 0;
    for (const auto& call : esphome::i2c::g_i2c_calls) {
        if (call.data.size() == 21) {
            coeff_writes++;
        }
    }
    ASSERT_EQ(coeff_writes, 30);
}

TEST(batched_uses_fewer_page_selects) {
    esphome::i2c::I2CBus bus;

    tas5805m_biquad::BiquadCoeffs coeffs[15];
    for (int i = 0; i < 15; i++) {
        coeffs[i] = tas5805m_biquad::BiquadCoeffs();
    }

    tas5805m_biquad::write_channel_biquads_batched(&bus, 0x2C, 0, coeffs);

    // Count page select operations (page select is 2-byte write to reg 0x00)
    int page_selects = 0;
    for (const auto& call : esphome::i2c::g_i2c_calls) {
        if (call.data.size() == 2 && call.data[0] == 0x00) {
            page_selects++;
        }
    }

    // Batched should use only 4 pages (0x24, 0x25, 0x26, 0x27) + initial page 0
    // So around 8-12 page selects (including book selection)
    // Non-batched would be 15 * 3 = 45 page-related writes
    ASSERT_LT(page_selects, 20);
}

// =============================================================================
// TIER 1 TESTS: PROFILE MANAGER
// =============================================================================

TEST(profile_manager_setup_no_active_profile) {
    tas5805m_profile::ProfileManager pm;
    pm.setup();

    ASSERT_EQ(pm.get_active_profile_name(), "none");
}

TEST(profile_manager_save_and_load_profile) {
    tas5805m_profile::ProfileManager pm;
    pm.setup();

    // Create a profile
    tas5805m_profile::CalibrationProfile profile;
    profile.left_channel[0] = tas5805m_profile::BiquadCoefficients(1.5f, -2.0f, 0.5f, -1.9f, 0.95f);
    profile.right_channel[0] = tas5805m_profile::BiquadCoefficients(1.5f, -2.0f, 0.5f, -1.9f, 0.95f);

    // Save
    bool saved = pm.save_profile("test_profile", profile);
    ASSERT_TRUE(saved);

    // Load
    tas5805m_profile::CalibrationProfile loaded;
    bool loaded_ok = pm.load_profile("test_profile", loaded);
    ASSERT_TRUE(loaded_ok);

    // Verify coefficients
    ASSERT_NEAR(loaded.left_channel[0].b0, 1.5f, 0.0001f);
    ASSERT_NEAR(loaded.left_channel[0].a1, -1.9f, 0.0001f);
}

TEST(profile_manager_load_nonexistent_profile) {
    tas5805m_profile::ProfileManager pm;
    pm.setup();

    tas5805m_profile::CalibrationProfile profile;
    bool loaded = pm.load_profile("nonexistent", profile);
    ASSERT_FALSE(loaded);
}

TEST(profile_manager_delete_profile) {
    tas5805m_profile::ProfileManager pm;
    pm.setup();

    // Save a profile
    tas5805m_profile::CalibrationProfile profile;
    pm.save_profile("to_delete", profile);

    // Verify it exists
    tas5805m_profile::CalibrationProfile loaded;
    ASSERT_TRUE(pm.load_profile("to_delete", loaded));

    // Delete
    bool deleted = pm.delete_profile("to_delete");
    ASSERT_TRUE(deleted);

    // Verify it's gone
    ASSERT_FALSE(pm.load_profile("to_delete", loaded));
}

TEST(profile_manager_list_profiles) {
    tas5805m_profile::ProfileManager pm;
    pm.setup();

    // Save a few profiles
    tas5805m_profile::CalibrationProfile p;
    pm.save_profile("profile_a", p);
    pm.save_profile("profile_b", p);
    pm.save_profile("profile_c", p);

    auto profiles = pm.list_profiles();
    ASSERT_EQ(profiles.size(), 3u);
}

TEST(profile_manager_set_active_profile) {
    tas5805m_profile::ProfileManager pm;
    pm.setup();

    // Save a profile
    tas5805m_profile::CalibrationProfile profile;
    pm.save_profile("active_test", profile);

    // Set as active
    bool set_ok = pm.set_active_profile("active_test");
    ASSERT_TRUE(set_ok);

    // Verify
    ASSERT_EQ(pm.get_active_profile_name(), "active_test");
}

TEST(profile_manager_clear_active_profile) {
    tas5805m_profile::ProfileManager pm;
    pm.setup();

    // Save and set active
    tas5805m_profile::CalibrationProfile profile;
    pm.save_profile("temp_active", profile);
    pm.set_active_profile("temp_active");

    // Clear
    bool cleared = pm.set_active_profile(-1);
    ASSERT_TRUE(cleared);
    ASSERT_EQ(pm.get_active_profile_name(), "none");
}

TEST(profile_manager_max_profiles_limit) {
    tas5805m_profile::ProfileManager pm;
    pm.setup();

    tas5805m_profile::CalibrationProfile profile;

    // Fill all 5 slots
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "profile_%d", i);
        ASSERT_TRUE(pm.save_profile(name, profile));
    }

    // 6th profile should fail
    bool result = pm.save_profile("overflow_profile", profile);
    ASSERT_FALSE(result);
}

TEST(profile_manager_overwrite_existing_profile) {
    tas5805m_profile::ProfileManager pm;
    pm.setup();

    // Save initial profile
    tas5805m_profile::CalibrationProfile p1;
    p1.left_channel[0] = tas5805m_profile::BiquadCoefficients(1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    pm.save_profile("overwrite_test", p1);

    // Save with same name but different data
    tas5805m_profile::CalibrationProfile p2;
    p2.left_channel[0] = tas5805m_profile::BiquadCoefficients(2.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    pm.save_profile("overwrite_test", p2);

    // Load and verify it has new data
    tas5805m_profile::CalibrationProfile loaded;
    pm.load_profile("overwrite_test", loaded);
    ASSERT_NEAR(loaded.left_channel[0].b0, 2.0f, 0.0001f);
}

TEST(profile_manager_load_and_apply_active_profile) {
    tas5805m_profile::ProfileManager pm;
    pm.setup();

    esphome::i2c::I2CBus bus;

    // Save a profile with non-bypass coefficients
    tas5805m_profile::CalibrationProfile profile;
    profile.left_channel[0] = tas5805m_profile::BiquadCoefficients(1.5f, -2.0f, 0.5f, -1.9f, 0.95f);
    pm.save_profile("apply_test", profile);
    pm.set_active_profile("apply_test");

    // Apply
    bool applied = pm.load_and_apply_active_profile(&bus, 0x2C);
    ASSERT_TRUE(applied);

    // Should have written biquads to I2C
    ASSERT_GT(esphome::i2c::g_i2c_calls.size(), 0u);
}

TEST(profile_manager_no_active_profile_apply_succeeds) {
    tas5805m_profile::ProfileManager pm;
    pm.setup();

    esphome::i2c::I2CBus bus;

    // No active profile set - should still return true (not an error)
    bool result = pm.load_and_apply_active_profile(&bus, 0x2C);
    ASSERT_TRUE(result);

    // But no I2C writes should have happened
    ASSERT_EQ(esphome::i2c::g_i2c_calls.size(), 0u);
}

TEST(profile_manager_load_by_index_valid) {
    tas5805m_profile::ProfileManager pm;
    pm.setup();

    tas5805m_profile::CalibrationProfile profile;
    pm.save_profile("index_test", profile);

    tas5805m_profile::CalibrationProfile loaded;
    bool result = pm.load_profile_by_index(0, loaded);
    ASSERT_TRUE(result);
}

TEST(profile_manager_load_by_index_invalid) {
    tas5805m_profile::ProfileManager pm;
    pm.setup();

    tas5805m_profile::CalibrationProfile loaded;

    // Negative index
    ASSERT_FALSE(pm.load_profile_by_index(-1, loaded));

    // Index >= MAX_PROFILES
    ASSERT_FALSE(pm.load_profile_by_index(5, loaded));
    ASSERT_FALSE(pm.load_profile_by_index(100, loaded));
}

TEST(profile_manager_delete_clears_active_if_same) {
    tas5805m_profile::ProfileManager pm;
    pm.setup();

    tas5805m_profile::CalibrationProfile profile;
    pm.save_profile("active_delete", profile);
    pm.set_active_profile("active_delete");

    ASSERT_EQ(pm.get_active_profile_name(), "active_delete");

    pm.delete_profile("active_delete");

    ASSERT_EQ(pm.get_active_profile_name(), "none");
}

// =============================================================================
// TIER 2 TESTS: FILTER WRITING FUNCTIONS
// =============================================================================

TEST(write_parametric_eq_success) {
    esphome::i2c::I2CBus bus;

    float coeffs[5];
    bool result = tas5805m_biquad::write_parametric_eq(&bus, 0x2C, 0, 0,
                                                        1000.0f, 6.0f, 1.0f,
                                                        48000.0f, coeffs);
    ASSERT_TRUE(result);

    // Verify coefficients were output
    ASSERT_TRUE(std::isfinite(coeffs[0]));  // b0
    ASSERT_TRUE(std::isfinite(coeffs[1]));  // b1
    ASSERT_TRUE(std::isfinite(coeffs[2]));  // b2
    ASSERT_TRUE(std::isfinite(coeffs[3]));  // a1
    ASSERT_TRUE(std::isfinite(coeffs[4]));  // a2

    // b0 should be > 1 for boost
    ASSERT_GT(coeffs[0], 1.0f);
}

TEST(write_parametric_eq_both_channels) {
    esphome::i2c::I2CBus bus;

    bool result = tas5805m_biquad::write_parametric_eq(&bus, 0x2C, 2, 5,
                                                        500.0f, -3.0f, 2.0f);
    ASSERT_TRUE(result);

    // Count coefficient writes
    int coeff_writes = 0;
    for (const auto& call : esphome::i2c::g_i2c_calls) {
        if (call.data.size() == 21) {
            coeff_writes++;
        }
    }
    ASSERT_EQ(coeff_writes, 2);  // Left and right
}

TEST(write_low_shelf_success) {
    esphome::i2c::I2CBus bus;

    float coeffs[5];
    bool result = tas5805m_biquad::write_low_shelf(&bus, 0x2C, 0, 0,
                                                    200.0f, 6.0f, 1.0f,
                                                    48000.0f, coeffs);
    ASSERT_TRUE(result);

    // Coefficients should be valid
    ASSERT_TRUE(std::isfinite(coeffs[0]));
    ASSERT_TRUE(std::isfinite(coeffs[1]));
}

TEST(write_high_shelf_success) {
    esphome::i2c::I2CBus bus;

    float coeffs[5];
    bool result = tas5805m_biquad::write_high_shelf(&bus, 0x2C, 1, 10,
                                                     8000.0f, -4.0f, 0.8f,
                                                     48000.0f, coeffs);
    ASSERT_TRUE(result);

    // Coefficients should be valid
    ASSERT_TRUE(std::isfinite(coeffs[0]));
}

TEST(write_highpass_success) {
    esphome::i2c::I2CBus bus;

    float coeffs[5];
    bool result = tas5805m_biquad::write_highpass(&bus, 0x2C, 2, 0,
                                                   80.0f, 0.707f,
                                                   48000.0f, coeffs);
    ASSERT_TRUE(result);

    // For highpass, b1 should be -2*b0
    ASSERT_NEAR(coeffs[1], -2.0f * coeffs[0], 0.0001f);
}

TEST(write_lowpass_success) {
    esphome::i2c::I2CBus bus;

    float coeffs[5];
    bool result = tas5805m_biquad::write_lowpass(&bus, 0x2C, 0, 14,
                                                  10000.0f, 0.707f,
                                                  48000.0f, coeffs);
    ASSERT_TRUE(result);

    // For lowpass, b1 should be 2*b0
    ASSERT_NEAR(coeffs[1], 2.0f * coeffs[0], 0.0001f);
}

TEST(write_notch_success) {
    esphome::i2c::I2CBus bus;

    float coeffs[5];
    bool result = tas5805m_biquad::write_notch(&bus, 0x2C, 2, 7,
                                                60.0f, 10.0f,  // 60Hz hum filter
                                                48000.0f, coeffs);
    ASSERT_TRUE(result);

    // For notch, b0 should equal b2
    ASSERT_NEAR(coeffs[0], coeffs[2], 0.0001f);
}

TEST(write_filter_null_out_coeffs) {
    esphome::i2c::I2CBus bus;

    // Should work without output coefficients
    bool result = tas5805m_biquad::write_parametric_eq(&bus, 0x2C, 0, 0,
                                                        1000.0f, 0.0f, 1.0f,
                                                        48000.0f, nullptr);
    ASSERT_TRUE(result);
}

// =============================================================================
// TIER 2 TESTS: ERROR HANDLING
// =============================================================================

TEST(i2c_persistent_failure_detected) {
    esphome::i2c::I2CBus bus;

    // Always fail
    esphome::i2c::mock_set_error(esphome::i2c::ERROR_NOT_ACKNOWLEDGED);

    bool result = tas5805m_biquad::write_biquad(&bus, 0x2C, 0, 0,
                                                 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    ASSERT_FALSE(result);
}

TEST(profile_checksum_corruption_detected) {
    tas5805m_profile::CalibrationProfile profile;
    profile.update_checksum();
    ASSERT_TRUE(profile.is_valid());

    // Corrupt data without updating checksum
    profile.left_channel[5].b0 = 99.0f;
    ASSERT_FALSE(profile.is_valid());
}

TEST(profile_magic_corruption_detected) {
    tas5805m_profile::CalibrationProfile profile;
    profile.magic = 0xDEADBEEF;  // Wrong magic
    profile.update_checksum();

    // Should still fail due to wrong magic
    ASSERT_FALSE(profile.is_valid());
}

TEST(nvs_save_failure_handled) {
    tas5805m_profile::ProfileManager pm;
    pm.setup();

    mock_nvs_set_save_fails(true);

    tas5805m_profile::CalibrationProfile profile;
    bool saved = pm.save_profile("fail_save", profile);
    ASSERT_FALSE(saved);
}

TEST(nvs_load_failure_handled) {
    tas5805m_profile::ProfileManager pm;
    pm.setup();

    // Save a profile first (with save working)
    tas5805m_profile::CalibrationProfile profile;
    pm.save_profile("fail_load", profile);

    // Now make loads fail
    mock_nvs_set_load_fails(true);

    tas5805m_profile::CalibrationProfile loaded;
    bool loaded_ok = pm.load_profile("fail_load", loaded);
    ASSERT_FALSE(loaded_ok);
}

TEST(add_filter_to_profile_left_channel) {
    tas5805m_profile::CalibrationProfile profile;

    tas5805m_profile::add_filter_to_profile(profile, 0, 5,
                                            1.5f, -2.0f, 0.5f, -1.9f, 0.95f);

    ASSERT_NEAR(profile.left_channel[5].b0, 1.5f, 0.0001f);
    ASSERT_TRUE(profile.right_channel[5].is_bypass());  // Right should be unchanged
}

TEST(add_filter_to_profile_right_channel) {
    tas5805m_profile::CalibrationProfile profile;

    tas5805m_profile::add_filter_to_profile(profile, 1, 10,
                                            2.0f, -1.5f, 0.5f, -1.5f, 0.5f);

    ASSERT_TRUE(profile.left_channel[10].is_bypass());  // Left should be unchanged
    ASSERT_NEAR(profile.right_channel[10].b0, 2.0f, 0.0001f);
}

TEST(add_filter_to_profile_both_channels) {
    tas5805m_profile::CalibrationProfile profile;

    tas5805m_profile::add_filter_to_profile(profile, 2, 0,
                                            3.0f, -2.5f, 0.5f, -2.5f, 0.5f);

    ASSERT_NEAR(profile.left_channel[0].b0, 3.0f, 0.0001f);
    ASSERT_NEAR(profile.right_channel[0].b0, 3.0f, 0.0001f);
}

TEST(add_filter_to_profile_invalid_index) {
    tas5805m_profile::CalibrationProfile profile;

    // Negative index - should not crash
    tas5805m_profile::add_filter_to_profile(profile, 0, -1,
                                            1.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    // Index 15 - should not crash
    tas5805m_profile::add_filter_to_profile(profile, 0, 15,
                                            1.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    // Profile should still be all bypass (invalid indices ignored)
    for (int i = 0; i < 15; i++) {
        ASSERT_TRUE(profile.left_channel[i].is_bypass());
    }
}

TEST(batched_write_partial_failure) {
    esphome::i2c::I2CBus bus;

    // Fail after writing first page
    esphome::i2c::mock_fail_after(10, 100);  // Fail after ~10 calls

    tas5805m_biquad::BiquadCoeffs coeffs[15];
    for (int i = 0; i < 15; i++) {
        coeffs[i] = tas5805m_biquad::BiquadCoeffs();
    }

    bool result = tas5805m_biquad::write_channel_biquads_batched(&bus, 0x2C, 0, coeffs);
    ASSERT_FALSE(result);
}

TEST(coefficient_nan_handled) {
    // float_to_9_23 should handle NaN
    int32_t result = tas5805m_biquad::float_to_9_23(NAN);
    ASSERT_EQ(result, 0);  // Should return bypass
}

TEST(coefficient_infinity_handled) {
    // float_to_9_23 should handle infinity
    int32_t result_pos = tas5805m_biquad::float_to_9_23(INFINITY);
    int32_t result_neg = tas5805m_biquad::float_to_9_23(-INFINITY);

    // Both should return 0 (bypass)
    ASSERT_EQ(result_pos, 0);
    ASSERT_EQ(result_neg, 0);
}

TEST(profile_active_filter_count_accuracy) {
    tas5805m_profile::CalibrationProfile profile;

    // Add 3 non-bypass filters
    profile.left_channel[0] = tas5805m_profile::BiquadCoefficients(1.5f, -1.0f, 0.5f, -1.0f, 0.5f);
    profile.left_channel[5] = tas5805m_profile::BiquadCoefficients(2.0f, -1.5f, 0.5f, -1.5f, 0.5f);
    profile.right_channel[10] = tas5805m_profile::BiquadCoefficients(1.2f, -0.8f, 0.3f, -0.8f, 0.3f);

    profile.count_active_filters();

    // Should count 3 indices with non-bypass filters (0, 5, 10)
    ASSERT_EQ(profile.num_filters_used, 3);
}

// =============================================================================
// MAIN
// =============================================================================

int main() {
    printf("\n=== TAS5805M Unit Tests ===\n\n");

    // Static initialization runs all tests

    printf("\n=== Results ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Passed:    %d\n", tests_passed);
    printf("Failed:    %d\n", tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
