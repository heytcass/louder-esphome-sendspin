/**
 * Unit Tests for TAS5805M Biquad and Profile Manager
 *
 * These tests verify the pure calculation logic without requiring
 * actual hardware or ESPHome dependencies.
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
#include <limits>

// =============================================================================
// MOCK ESPHOME DEPENDENCIES
// =============================================================================

// Mock ESP logging macros
#define ESP_LOGE(tag, fmt, ...) ((void)0)
#define ESP_LOGW(tag, fmt, ...) ((void)0)
#define ESP_LOGI(tag, fmt, ...) ((void)0)
#define ESP_LOGD(tag, fmt, ...) ((void)0)

// Mock delay function
inline void delay(uint32_t ms) { (void)ms; }
inline uint32_t millis() { return 0; }

// Mock I2C bus (minimal interface for compilation)
namespace esphome {
namespace i2c {
    enum ErrorCode { ERROR_OK = 0 };
    class I2CBus {
    public:
        ErrorCode write(uint8_t addr, const uint8_t* data, size_t len, bool stop) {
            (void)addr; (void)data; (void)len; (void)stop;
            return ERROR_OK;
        }
    };
}

// Mock preferences
class ESPPreferenceObject {
public:
    template<typename T>
    bool load(T* value) { (void)value; return false; }
    template<typename T>
    bool save(const T* value) { (void)value; return true; }
};

class ESPPreferences {
public:
    template<typename T>
    ESPPreferenceObject make_preference(uint32_t hash) { (void)hash; return ESPPreferenceObject(); }
};

ESPPreferences* global_preferences = nullptr;
}  // namespace esphome

// =============================================================================
// TEST FRAMEWORK
// =============================================================================

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    void test_##name(); \
    struct TestRegistrar_##name { \
        TestRegistrar_##name() { \
            printf("  Running: %s ... ", #name); \
            fflush(stdout); \
            tests_run++; \
            try { \
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

// =============================================================================
// INCLUDE THE CODE UNDER TEST (after mocks are defined)
// =============================================================================

// We need to extract just the testable functions without the I2C-dependent ones
// For this test, we'll copy the pure functions inline

namespace tas5805m_biquad {

// Validation functions
inline bool validate_channel(int channel) {
    if (channel < 0 || channel > 2) {
        return false;
    }
    return true;
}

inline bool validate_index(int index) {
    if (index < 0 || index >= 15) {
        return false;
    }
    return true;
}

inline bool validate_frequency(float frequency, float min_freq = 10.0f, float max_freq = 24000.0f) {
    if (!std::isfinite(frequency) || frequency < min_freq || frequency > max_freq) {
        return false;
    }
    return true;
}

inline bool validate_gain(float gain_db, float min_gain = -20.0f, float max_gain = 20.0f) {
    if (!std::isfinite(gain_db) || gain_db < min_gain || gain_db > max_gain) {
        return false;
    }
    return true;
}

inline bool validate_q(float q, float min_q = 0.1f, float max_q = 20.0f) {
    if (!std::isfinite(q) || q < min_q || q > max_q) {
        return false;
    }
    return true;
}

inline bool validate_slope(float slope, float min_slope = 0.1f, float max_slope = 5.0f) {
    if (!std::isfinite(slope) || slope < min_slope || slope > max_slope) {
        return false;
    }
    return true;
}

inline bool validate_coefficients(float b0, float b1, float b2, float a1, float a2) {
    if (!std::isfinite(b0) || !std::isfinite(b1) || !std::isfinite(b2) ||
        !std::isfinite(a1) || !std::isfinite(a2)) {
        return false;
    }
    return true;
}

// Conversion functions
inline int32_t float_to_9_23(float value) {
    if (!std::isfinite(value)) {
        return 0;
    }
    if (value > 255.999999f) value = 255.999999f;
    if (value < -256.0f) value = -256.0f;
    return static_cast<int32_t>(value * (1 << 23));
}

inline void pack_be32(int32_t value, uint8_t* buffer) {
    buffer[0] = (value >> 24) & 0xFF;
    buffer[1] = (value >> 16) & 0xFF;
    buffer[2] = (value >> 8) & 0xFF;
    buffer[3] = value & 0xFF;
}

// Filter coefficient calculators (pure math, returning coefficients)
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

}  // namespace tas5805m_biquad

// Profile structures
namespace tas5805m_profile {

constexpr size_t MAX_PROFILE_NAME_LEN = 32;
constexpr uint32_t PROFILE_MAGIC = 0x54415335;

struct BiquadCoefficients {
    float b0, b1, b2, a1, a2;

    BiquadCoefficients() : b0(1.0f), b1(0.0f), b2(0.0f), a1(0.0f), a2(0.0f) {}

    BiquadCoefficients(float _b0, float _b1, float _b2, float _a1, float _a2)
        : b0(_b0), b1(_b1), b2(_b2), a1(_a1), a2(_a2) {}

    bool is_bypass() const {
        return (fabs(b0 - 1.0f) < 0.0001f &&
                fabs(b1) < 0.0001f &&
                fabs(b2) < 0.0001f &&
                fabs(a1) < 0.0001f &&
                fabs(a2) < 0.0001f);
    }
} __attribute__((packed));

struct CalibrationProfile {
    uint32_t magic;
    char name[MAX_PROFILE_NAME_LEN];
    uint32_t timestamp;
    BiquadCoefficients left_channel[15];
    BiquadCoefficients right_channel[15];
    uint8_t num_filters_used;
    uint32_t checksum;

    CalibrationProfile() : magic(PROFILE_MAGIC), timestamp(0), num_filters_used(0), checksum(0) {
        memset(name, 0, sizeof(name));
        for (int i = 0; i < 15; i++) {
            left_channel[i] = BiquadCoefficients();
            right_channel[i] = BiquadCoefficients();
        }
    }

    uint32_t calculate_checksum() const {
        uint32_t crc = 0xFFFFFFFF;
        const uint8_t* data = reinterpret_cast<const uint8_t*>(this);
        size_t len = offsetof(CalibrationProfile, checksum);

        for (size_t i = 0; i < len; i++) {
            crc ^= data[i];
            for (int j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
            }
        }
        return ~crc;
    }

    bool is_valid() const {
        if (magic != PROFILE_MAGIC) {
            return false;
        }
        uint32_t expected_checksum = calculate_checksum();
        if (checksum != expected_checksum) {
            return false;
        }
        return true;
    }

    void update_checksum() {
        checksum = calculate_checksum();
    }

    void count_active_filters() {
        num_filters_used = 0;
        for (int i = 0; i < 15; i++) {
            if (!left_channel[i].is_bypass() || !right_channel[i].is_bypass()) {
                num_filters_used++;
            }
        }
    }
} __attribute__((packed));

// FNV-1a hash (copied from profile manager)
inline uint32_t fnv1_hash(const char* str) {
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= static_cast<uint8_t>(*str++);
        hash *= 16777619u;
    }
    return hash;
}

}  // namespace tas5805m_profile

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
bool coeffs_are_stable(const tas5805m_biquad::FilterCoeffs& c) {
    return std::isfinite(c.b0) && std::isfinite(c.b1) && std::isfinite(c.b2) &&
           std::isfinite(c.a1) && std::isfinite(c.a2);
}

TEST(parametric_eq_zero_gain_is_bypass) {
    // With 0dB gain, parametric EQ should be near-bypass (b0≈1, others≈0)
    auto c = tas5805m_biquad::calc_parametric_eq(1000.0f, 0.0f, 1.0f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
    ASSERT_NEAR(c.b0, 1.0f, 0.0001f);
    ASSERT_NEAR(c.b1, c.a1, 0.0001f);  // b1 and a1 should match
    ASSERT_NEAR(c.b2, c.a2, 0.0001f);  // b2 and a2 should match
}

TEST(parametric_eq_positive_gain) {
    auto c = tas5805m_biquad::calc_parametric_eq(1000.0f, 6.0f, 1.0f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
    // At resonance, b0 should be > 1 for boost
    ASSERT_TRUE(c.b0 > 1.0f);
}

TEST(parametric_eq_negative_gain) {
    auto c = tas5805m_biquad::calc_parametric_eq(1000.0f, -6.0f, 1.0f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
    // For cut, b0 should be < 1
    ASSERT_TRUE(c.b0 < 1.0f);
}

TEST(parametric_eq_high_q) {
    auto c = tas5805m_biquad::calc_parametric_eq(1000.0f, 6.0f, 10.0f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
}

TEST(low_shelf_zero_gain) {
    auto c = tas5805m_biquad::calc_low_shelf(200.0f, 0.0f, 1.0f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
    // Should be approximately bypass
    ASSERT_NEAR(c.b0, 1.0f, 0.001f);
}

TEST(low_shelf_boost) {
    auto c = tas5805m_biquad::calc_low_shelf(200.0f, 6.0f, 1.0f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
}

TEST(low_shelf_cut) {
    auto c = tas5805m_biquad::calc_low_shelf(200.0f, -6.0f, 1.0f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
}

TEST(high_shelf_zero_gain) {
    auto c = tas5805m_biquad::calc_high_shelf(8000.0f, 0.0f, 1.0f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
    ASSERT_NEAR(c.b0, 1.0f, 0.001f);
}

TEST(high_shelf_boost) {
    auto c = tas5805m_biquad::calc_high_shelf(8000.0f, 6.0f, 1.0f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
}

TEST(highpass_butterworth) {
    // Q = 0.707 is Butterworth (maximally flat)
    auto c = tas5805m_biquad::calc_highpass(100.0f, 0.707f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
    // b1 should be -2*b0 for HP
    ASSERT_NEAR(c.b1, -2.0f * c.b0, 0.0001f);
    ASSERT_NEAR(c.b2, c.b0, 0.0001f);
}

TEST(highpass_low_frequency) {
    auto c = tas5805m_biquad::calc_highpass(20.0f, 0.707f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
}

TEST(highpass_high_frequency) {
    auto c = tas5805m_biquad::calc_highpass(10000.0f, 0.707f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
}

TEST(lowpass_butterworth) {
    auto c = tas5805m_biquad::calc_lowpass(10000.0f, 0.707f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
    // b1 should be 2*b0 for LP
    ASSERT_NEAR(c.b1, 2.0f * c.b0, 0.0001f);
    ASSERT_NEAR(c.b2, c.b0, 0.0001f);
}

TEST(lowpass_near_nyquist) {
    // Test near Nyquist frequency (fs/2)
    auto c = tas5805m_biquad::calc_lowpass(23000.0f, 0.707f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
}

TEST(notch_stability) {
    auto c = tas5805m_biquad::calc_notch(1000.0f, 10.0f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
    // For notch: b0 and b2 should equal 1/a0 (after normalization)
    // and b0 == b2
    ASSERT_NEAR(c.b0, c.b2, 0.0001f);
}

TEST(notch_high_q) {
    // High Q = narrow notch
    auto c = tas5805m_biquad::calc_notch(60.0f, 20.0f, 48000.0f);  // 60Hz hum filter
    ASSERT_TRUE(coeffs_are_stable(c));
}

TEST(notch_low_q) {
    auto c = tas5805m_biquad::calc_notch(1000.0f, 0.5f, 48000.0f);  // Wide notch
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
    uint32_t hash = tas5805m_profile::fnv1_hash("");
    ASSERT_EQ(hash, 2166136261u);  // FNV offset basis
}

TEST(fnv1_hash_known_values) {
    // These are deterministic - same input always gives same output
    uint32_t hash1 = tas5805m_profile::fnv1_hash("test");
    uint32_t hash2 = tas5805m_profile::fnv1_hash("test");
    ASSERT_EQ(hash1, hash2);

    // Different strings should produce different hashes (with high probability)
    uint32_t hash3 = tas5805m_profile::fnv1_hash("profile_0");
    uint32_t hash4 = tas5805m_profile::fnv1_hash("profile_1");
    ASSERT_TRUE(hash3 != hash4);
}

TEST(fnv1_hash_profile_keys) {
    // Test typical profile keys
    uint32_t h0 = tas5805m_profile::fnv1_hash("profile_0");
    uint32_t h1 = tas5805m_profile::fnv1_hash("profile_1");
    uint32_t h2 = tas5805m_profile::fnv1_hash("profile_2");
    uint32_t h3 = tas5805m_profile::fnv1_hash("profile_3");
    uint32_t h4 = tas5805m_profile::fnv1_hash("profile_4");

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
    auto c = tas5805m_biquad::calc_lowpass(nyquist - 100.0f, 0.707f, fs);
    ASSERT_TRUE(coeffs_are_stable(c));
}

TEST(filter_very_low_frequency) {
    // Very low frequency filter
    auto c = tas5805m_biquad::calc_highpass(10.0f, 0.707f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));
}

TEST(filter_extreme_q) {
    // Very high Q = very narrow filter
    auto c = tas5805m_biquad::calc_parametric_eq(1000.0f, 6.0f, 20.0f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c));

    // Very low Q = very broad filter
    auto c2 = tas5805m_biquad::calc_parametric_eq(1000.0f, 6.0f, 0.1f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(c2));
}

TEST(filter_extreme_gain) {
    auto boost = tas5805m_biquad::calc_parametric_eq(1000.0f, 20.0f, 1.0f, 48000.0f);
    ASSERT_TRUE(coeffs_are_stable(boost));

    auto cut = tas5805m_biquad::calc_parametric_eq(1000.0f, -20.0f, 1.0f, 48000.0f);
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
