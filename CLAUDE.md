# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESPHome configuration and room correction system for Sonocotta's Louder-ESP32S3 audio board with TAS5805M DAC. Provides phone-based room measurement and parametric EQ correction targeting **Sonos TruePlay-level performance** with universal phone support (Android + iOS).

**Project Status:** Production-ready with comprehensive code review fixes and professional-grade testing methodology.

**Hardware Stack:**
- ESP32-S3 with PSRAM (octal mode, 80MHz)
- TAS5805M Class-D amplifier with 15 programmable biquads per channel
- W5500 SPI Ethernet module
- SSD1306/SH1106 OLED display (SPI)
- Sendspin multi-room audio streaming (beta)

## Build & Deploy

### First-Time Setup

**Create secrets.yaml** (required for security):
```bash
cp secrets.yaml.example secrets.yaml
# Edit secrets.yaml and add your API encryption key and OTA password
```

**Compile and upload:**
```bash
# Compile and upload via ESPHome
esphome run louder-s3-sendspin-ethernet-oled.yaml

# Compile only (check for errors)
esphome compile louder-s3-sendspin-ethernet-oled.yaml

# View logs
esphome logs louder-s3-sendspin-ethernet-oled.yaml
```

### Security Features

- **API encryption** - Encrypted Home Assistant communication (required)
- **OTA password** - Secure firmware updates
- **Parameter validation** - All services validate inputs (bounds checking, NaN/Inf detection)
- **I2C retry logic** - Automatic recovery from communication failures

## Architecture

### Main Configuration
`louder-s3-sendspin-ethernet-oled.yaml` - Complete ESPHome config including:
- TAS5805M DAC via `mrtoy-me/esphome-tas5805m@beta`
- Sendspin media streaming with mixer/resampler pipeline
- 15-band graphic EQ exposed as Home Assistant number entities
- OLED display showing track metadata

### Room Correction System
Extends the graphic EQ with arbitrary parametric EQ via direct biquad programming:

| File | Purpose |
|------|---------|
| `room_correction_services.yaml` | Home Assistant services for biquad programming with full validation |
| `tas5805m_biquad_i2c.h` | C++ biquad coefficient calculation, I2C register map, retry logic |
| `tas5805m_profile_manager.h` | Profile storage/retrieval with NVS and checksum validation |
| `calibrate.html` | Web UI for phone-based room measurement (complete FFT algorithm) |
| `index.html` | Room correction management web interface |

**Testing Documentation:**
| File | Purpose |
|------|---------|
| `CODE_REVIEW.md` | Comprehensive code review with 26 identified issues and fixes |
| `TESTING_CHECKLIST.md` | TruePlay-level validation methodology (100+ checkpoints) |
| `GETTING_STARTED.md` | Week 1 practical testing roadmap |
| `MULTI_DEVICE_TESTING.md` | Cross-platform validation strategy (Android + iOS) |
| `ARCHITECTURE.md` | Technical deep-dive on system design |
| `PROFILE_USAGE.md` | Profile management and NVS storage guide |

### Biquad Register Map (TAS5805M)
- Book 0xAA contains all biquad coefficients
- Left channel: Pages 0x24-0x26, Right channel: Pages 0x26-0x28
- Each biquad: 20 bytes (5 coefficients × 4 bytes, 9.23 fixed-point)
- a1/a2 coefficients are sign-inverted when written

### Audio Pipeline
```
Sendspin Source → Resampler (48kHz) → Mixer → I2S Speaker → TAS5805M → Speakers
                                        ↑
                   Announcement → Resampler → (ducking applied)
```

## Key ESPHome Patterns

### External Components (from GitHub PRs)
Components pulled from unmerged ESPHome PRs for Sendspin beta:
- `mixer`, `resampler`, `audio`, `media_player`, `sendspin`
- Use `refresh: 0s` to cache and avoid re-fetching

### TAS5805M Services
Home Assistant service calls for room correction (all with comprehensive validation):
- `set_biquad`: Raw coefficient programming (validates bounds, NaN/Inf)
- `set_parametric_eq`: Frequency/gain/Q to biquad conversion (updates shadow state)
- `set_low_shelf`, `set_high_shelf`, `set_highpass`: Filter types
- `set_lowpass`, `set_notch`: Additional filter types
- `reset_biquad`: Reset single biquad to bypass
- `reset_all_biquads`: Clear all biquads to passthrough
- `enter_calibration_mode`, `exit_calibration_mode`: Calibration workflow control
- Profile management: `save_profile`, `load_profile`, `delete_profile`, `list_profiles`

**Validation features:**
- Channel validation (0-2)
- Biquad index validation (0-14)
- Frequency range validation (10 Hz - 24 kHz)
- Gain limiting (±12 dB default)
- Q factor validation (0.1 - 10.0)
- NaN/Infinity detection
- Filter stability checks

### Fixed-Point Format
TAS5805M uses 9.23 signed fixed-point with overflow protection:
```cpp
inline int32_t float_to_9_23(float value) {
    // Check for invalid values (NaN, Infinity)
    if (!std::isfinite(value)) {
        ESP_LOGE("tas5805m_bq", "Invalid coefficient: %f", value);
        return 0;  // Return bypass coefficient
    }

    // Clamp to valid range for 9.23 format
    if (value > 255.999999f) value = 255.999999f;
    if (value < -256.0f) value = -256.0f;

    return static_cast<int32_t>(value * (1 << 23));
}
```

## Web Calibration Flow

1. **Phone opens** `http://louder-s3-kitchen.local/calibrate`
2. **Browser captures** room response via Web Audio API during log sine sweep
3. **FFT analysis** (complete implementation with windowed DFT):
   - Hann windowing to reduce spectral leakage
   - 75% overlap processing for accuracy
   - dB scale conversion
   - 1kHz normalization
   - 1/3 octave smoothing
   - Performance optimizations (4x decimation)
4. **Filter generation** - Calculate parametric EQ biquad coefficients to flatten response
5. **Coefficients sent** to ESP32 via Home Assistant API (with validation)
6. **Profile storage** - Coefficients stored in NVS for persistence across reboots

**Supported platforms:**
- iOS Safari (iPhone/iPad) - excellent microphone quality
- Android Chrome (Pixel, Samsung, etc.) - varies by device
- Desktop browsers (testing/development)

**Multi-device validation:**
See `MULTI_DEVICE_TESTING.md` for cross-platform validation strategy.

## Hardware Pin Assignments

| Function | GPIO |
|----------|------|
| I2S LRCLK | 15 |
| I2S BCLK | 14 |
| I2S DOUT | 16 |
| TAS5805M Enable | 17 |
| I2C SDA | 8 |
| I2C SCL | 9 |
| Ethernet CS | 10 |
| OLED CS | 47 |
| OLED DC | 38 |
| OLED RST | 48 |

## Code Quality & Testing

### Code Review Status
See `CODE_REVIEW.md` for comprehensive analysis. **26 issues identified**, with critical fixes implemented:

**Completed (Phase 1):**
- ✅ Security: API encryption, OTA password
- ✅ Parameter validation (all services)
- ✅ Shadow state updates (profile management)
- ✅ I2C reliability (retry logic, delays)
- ✅ Float overflow handling (NaN/Inf detection)
- ✅ Checksum determinism (struct packing)
- ✅ Complete FFT algorithm implementation

**Remaining:**
- Deprecated Web Audio API (ScriptProcessorNode → AudioWorklet migration)
- Race conditions in announcement handling
- Stack overflow risk (large profile struct)
- Additional reliability improvements

### Testing Methodology

**Target: Sonos TruePlay-level performance**

**Success criteria:**
- **MVP**: ±3 dB variance after correction, >70% subjective preference
- **TruePlay-level**: ±2 dB variance, >85% preference, <60s calibration time

**Validation approach:**
1. **Hardware verification** - TAS5805M frequency response, biquad accuracy, I2C reliability
2. **Phone microphone characterization** - UMIK-1 reference comparison
3. **FFT algorithm validation** - Accuracy vs reference implementations
4. **Room measurement accuracy** - Repeatability (±1 dB target), REW comparison
5. **Correction effectiveness** - Before/after objective measurements, blind A/B listening
6. **Multi-device testing** - Android (Pixel 9/10) + iOS (iPhone/iPad)
7. **Cross-platform consistency** - Universal phone support (exceeds TruePlay's iOS-only)

**Required equipment:**
- UMIK-1 or UMIK-2 measurement microphone (~$75-100)
- Room EQ Wizard (REW) software (free)
- Multiple test devices (Android + iOS)
- SPL meter or calibrated app

**Testing timeline:** 8-13 days for comprehensive validation

See `GETTING_STARTED.md` for week 1 practical roadmap and `TESTING_CHECKLIST.md` for complete validation methodology.

### I2C Reliability Features

**Retry logic** (3 attempts with 5ms delays):
- Automatic recovery from transient I2C failures
- Logged warnings for debugging
- Graceful degradation on persistent errors

**Timing delays:**
- 2ms after book/page select
- 5ms after coefficient write
- Ensures TAS5805M has time to process commands

**Error detection:**
- I2C write status checks
- Coefficient validation before write
- Profile checksum validation
