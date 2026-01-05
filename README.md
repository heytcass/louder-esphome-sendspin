# Louder-ESP32S3 Sendspin Speaker

ESPHome configuration for the Sonocotta Louder-ESP32S3 as a Sendspin multi-room audio endpoint. Features W5500 ethernet for reliable streaming, OLED now-playing display, and optional room correction via the TAS5805M's built-in DSP.

## Overview

This project turns the Louder-ESP32S3 into a network speaker for the Sendspin multi-room audio system (ESPHome beta). It integrates with Home Assistant as a media player with full playback control, metadata display, and announcement support.

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  Sendspin       │────►│  Louder-ESP32S3 │────►│    Speakers     │
│  Server         │     │  (this config)  │     │                 │
└─────────────────┘     └─────────────────┘     └─────────────────┘
   Multi-room              W5500 Ethernet          Passive
   streaming               TAS5805M DAC            bookshelf
                           OLED display
```

## Features

- **Sendspin streaming** - Synchronized multi-room audio playback
- **Home Assistant integration** - Media player entity with full control
- **Announcement support** - TTS/announcements with automatic ducking
- **Now playing display** - OLED shows track, artist, album
- **Ethernet connectivity** - W5500 for reliable, low-latency streaming
- **15-band graphic EQ** - Adjustable via Home Assistant
- **Room correction** - Optional parametric EQ via TAS5805M biquads

## Hardware Requirements

- **Sonocotta Louder-ESP32S3** board
- **W5500 SPI Ethernet module** (recommended)
- **128x64 SSD1306/SH1106 OLED** (SPI)
- **Passive speakers** (bookshelf, etc.)
- **65W USB-C PD power supply**

## Quick Start

### 1. Clone and Configure

```bash
git clone https://github.com/yourusername/louder-esphome-sendspin.git
cd louder-esphome-sendspin

# Copy and edit secrets
cp secrets.yaml.example secrets.yaml
# Edit secrets.yaml with your API key and OTA password
```

### 2. Customize for Your Zone

Edit `louder-s3-sendspin-ethernet-oled.yaml`:

```yaml
substitutions:
  name: "louder-s3-kitchen"      # Change per zone
  friendly_name: "Kitchen Speaker"  # Change per zone
```

### 3. Flash

```bash
esphome run louder-s3-sendspin-ethernet-oled.yaml
```

### 4. Add to Home Assistant

The device will be auto-discovered. Add it via the ESPHome integration.

## Project Structure

```
├── louder-s3-sendspin-ethernet-oled.yaml  # Main ESPHome config
├── room_correction_services.yaml          # Room correction HA services
├── tas5805m_biquad_i2c.h                  # Biquad I2C implementation
├── tas5805m_profile_manager.h             # EQ profile storage
├── calibrate.html                         # Phone calibration UI
├── index.html                             # Room correction UI
├── secrets.yaml.example                   # Example secrets
└── docs/
    ├── GETTING_STARTED.md                 # Detailed setup guide
    ├── PROFILE_USAGE.md                   # Profile management
    └── TESTING_CHECKLIST.md               # Testing procedures
```

## Pin Configuration

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

## Room Correction (Optional)

The configuration includes optional room correction via the TAS5805M's 30 programmable biquad filters (15 per channel). This is included as a package and can be removed if not needed.

### Home Assistant Services

When room correction is enabled, these services are available:

| Service | Description |
|---------|-------------|
| `set_parametric_eq` | Parametric EQ (frequency, gain, Q) |
| `set_low_shelf` / `set_high_shelf` | Shelf filters |
| `set_highpass` / `set_lowpass` | HP/LP filters |
| `set_notch` | Notch filter |
| `reset_all_biquads` | Reset to flat response |
| `save_profile` / `load_profile` | Manage EQ profiles |

### Example: Cut a Room Mode

```yaml
service: esphome.louder_s3_kitchen_set_parametric_eq
data:
  channel: 2      # 0=left, 1=right, 2=both
  index: 0        # Biquad slot 0-14
  frequency: 80   # Hz
  gain_db: -6     # dB (negative = cut)
  q: 2            # Q factor
```

### Phone-Based Calibration

For automated measurement and correction:

1. Open `http://louder-s3-kitchen.local/calibrate` on your phone
2. Position phone at listening position
3. Allow microphone access
4. Follow measurement wizard
5. Review and apply calculated filters

## Removing Room Correction

To use as a simple Sendspin speaker without room correction, remove this from the main config:

```yaml
packages:
  room_correction: !include room_correction_services.yaml
```

## Troubleshooting

### No audio playback
- Check ethernet connection (link LED on W5500)
- Verify Sendspin server is running
- Check ESPHome logs for errors

### Audio dropouts
- Use ethernet instead of WiFi for reliability
- Check network congestion
- Verify PSRAM is detected in logs

### OLED not displaying
- Check SPI wiring (CS=47, DC=38, RST=48)
- Try changing model from `SSD1306` to `SH1106`

### Room correction not applying
- Check I2C bus logs for errors
- Verify TAS5805M address (0x2C)
- Ensure DAC is enabled

## Credits

- **mrtoy-me** - ESPHome TAS5805M component
- **Sonocotta** - Louder-ESP32S3 hardware
- **ESPHome Sendspin team** - Multi-room audio streaming

## License

MIT License
