# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESPHome configuration and room correction system for Sonocotta's Louder-ESP32S3 audio board with TAS5805M DAC. Provides phone-based room measurement and parametric EQ correction similar to Sonos Trueplay.

**Hardware Stack:**
- ESP32-S3 with PSRAM (octal mode, 80MHz)
- TAS5805M Class-D amplifier with 15 programmable biquads per channel
- W5500 SPI Ethernet module
- SSD1306/SH1106 OLED display (SPI)
- Sendspin multi-room audio streaming (beta)

## Build & Deploy

```bash
# Compile and upload via ESPHome
esphome run louder-s3-sendspin-ethernet-oled.yaml

# Compile only (check for errors)
esphome compile louder-s3-sendspin-ethernet-oled.yaml

# View logs
esphome logs louder-s3-sendspin-ethernet-oled.yaml
```

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
| `room_correction_services.yaml` | Home Assistant services for biquad programming |
| `tas5805m_biquad.hpp` | C++ biquad coefficient calculation and I2C register map |
| `calibrate.html` | Web UI for phone-based room measurement |
| `index.html` | Room correction management web interface |

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
Home Assistant service calls for room correction:
- `set_biquad`: Raw coefficient programming
- `set_parametric_eq`: Frequency/gain/Q to biquad conversion
- `set_low_shelf`, `set_high_shelf`, `set_highpass`: Filter types
- `reset_eq`: Clear all biquads to passthrough

### Fixed-Point Format
TAS5805M uses 9.23 signed fixed-point:
```cpp
int32_t float_to_9_23(float value) {
    return static_cast<int32_t>(value * 8388608.0f);  // 2^23
}
```

## Web Calibration Flow

1. Phone opens `http://louder-s3-kitchen.local/calibrate`
2. Browser captures room response via Web Audio API during sine sweep
3. FFT analysis calculates inverse EQ filters
4. Biquad coefficients sent to ESP32 via Home Assistant API
5. Coefficients stored in NVS for persistence across reboots

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
