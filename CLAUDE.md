# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESPHome configuration for the Louder-ESP32S3 as a **Sendspin multi-room audio target**. This turns Sonocotta's Louder-ESP32S3 board into a network-connected speaker endpoint for the Sendspin streaming system, with optional room correction via the TAS5805M's built-in DSP.

**Primary Purpose:** Sendspin streaming endpoint with Home Assistant integration
**Secondary Feature:** Room correction / parametric EQ via TAS5805M biquad filters

**Hardware Stack:**
- Sonocotta Louder-ESP32S3 (ESP32-S3 + TAS5805M Class-D amplifier)
- W5500 SPI Ethernet module (recommended for streaming reliability)
- SSD1306/SH1106 OLED display (shows now playing info)
- Passive bookshelf speakers

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
- Sendspin media source and group media player
- TAS5805M DAC via `mrtoy-me/esphome-tas5805m@beta`
- Audio pipeline: Sendspin → Resampler → Mixer → I2S → TAS5805M
- Announcement support with automatic ducking
- OLED display showing track title, artist, album
- 15-band graphic EQ exposed as Home Assistant number entities
- W5500 ethernet networking

### Audio Pipeline
```
Sendspin Source → Resampler (48kHz) → Mixer → I2S Speaker → TAS5805M → Speakers
                                        ↑
                   Announcement → Resampler → (ducking applied)
```

### External Components (from ESPHome PRs)
Components pulled from unmerged ESPHome PRs for Sendspin beta:
- `sendspin`, `mdns` - PR #12284
- `mixer` - PR #12253
- `resampler` - PR #12254
- `audio` - PR #12256
- `media_player` - PR #12258
- `file`, `http_request`, `media_source`, `speaker_source` - PR #12429
- `tas5805m` - `mrtoy-me/esphome-tas5805m@beta`

Use `refresh: 0s` to cache components and avoid re-fetching on each compile.

### Room Correction Package (Optional Feature)
Included via `packages: room_correction: !include room_correction_services.yaml`

| File | Purpose |
|------|---------|
| `room_correction_services.yaml` | Home Assistant services for EQ/biquad programming |
| `tas5805m_biquad_i2c.h` | Low-level I2C biquad coefficient writing |
| `tas5805m_profile_manager.h` | Save/load EQ profiles to NVS |
| `calibrate.html` | Phone-based room measurement web UI |
| `index.html` | Room correction management interface |

### Room Correction Services
Home Assistant services exposed when room correction package is included:
- `set_parametric_eq` - Frequency/gain/Q parametric filter
- `set_low_shelf`, `set_high_shelf` - Shelf filters
- `set_highpass`, `set_lowpass` - HP/LP filters
- `set_notch` - Notch filter
- `set_biquad` - Raw coefficient programming
- `reset_biquad`, `reset_all_biquads` - Reset filters
- `save_profile`, `load_profile`, `delete_profile` - Profile management
- `set_active_profile`, `clear_active_profile` - Boot profile selection

### Profile Management
- Up to 5 named profiles stored in NVS
- Profiles auto-load on boot if set as active
- Each profile stores 30 biquads (15 per channel)
- CRC32 validation for data integrity

### TAS5805M DSP Details
- 15 biquad filters per channel (30 total)
- Book 0xAA contains coefficients
- Left channel: Pages 0x24-0x27, Right channel: Pages 0x32-0x35
- 9.23 fixed-point format (coefficients × 2^23)
- a1/a2 coefficients are sign-inverted when written

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
| Ethernet INT | 6 |
| Ethernet RST | 5 |
| OLED CS | 47 |
| OLED DC | 38 |
| OLED RST | 48 |

## Project Structure

```
├── louder-s3-sendspin-ethernet-oled.yaml  # Main ESPHome config
├── room_correction_services.yaml          # HA services package
├── tas5805m_biquad_i2c.h                  # I2C biquad implementation
├── tas5805m_profile_manager.h             # Profile storage/management
├── calibrate.html                         # Phone calibration web UI
├── index.html                             # Room correction management UI
├── secrets.yaml.example                   # Example secrets file
├── ARCHITECTURE.md                        # Technical deep-dive
├── GETTING_STARTED.md                     # Setup guide
├── PROFILE_USAGE.md                       # Profile management guide
└── TESTING_CHECKLIST.md                   # Testing procedures
```

## Key Patterns

### Substitutions for Multi-Zone Deployment
Change these per zone:
```yaml
substitutions:
  name: "louder-s3-kitchen"
  friendly_name: "Kitchen Speaker"
```

### Announcement Ducking
Media audio ducks 40dB during announcements, then restores with 1s fade.

### PSRAM Configuration
ESP32-S3 octal PSRAM at 80MHz with optimized sdkconfig for audio streaming.
