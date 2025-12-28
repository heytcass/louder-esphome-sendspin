# TAS5805M Room Correction for Louder-ESP32S3

Phone-based room measurement and parametric EQ correction that leverages the TAS5805M's full DSP capabilities. Designed for Sonocotta's Louder-ESP32S3 with Sendspin multi-room audio.

## Overview

This system provides Sonos Trueplay-like room correction for Sonocotta's Louder-ESP32S3 boards, using your phone as the measurement microphone at the listening position.

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌──────────┐
│   Phone     │────►│   ESP32-S3  │────►│  TAS5805M   │────►│ Speakers │
│  (mic+UI)   │◄────│ (ESPHome)   │◄────│    DSP      │     │          │
└─────────────┘     └─────────────┘     └─────────────┘     └──────────┘
     Web Audio API      Home Assistant      15 biquads/ch      Bookshelf
     FFT analysis       service calls       48kHz DSP          speakers
```

## Features

- **Phone-based measurement** - No special equipment needed
- **Full DSP utilization** - 15 programmable biquad filters per channel (30 total)
- **Arbitrary parametric EQ** - Any frequency, any Q, any gain (not fixed bands)
- **Home Assistant integration** - Services exposed for automation and manual control
- **Multiple filter types** - Parametric EQ, shelves, high/low pass, notch
- **Sendspin compatible** - Works with multi-room audio streaming

## Project Structure

```
├── louder-s3-sendspin-ethernet-oled.yaml  # Main ESPHome configuration
├── room_correction_services.yaml           # HA services package (biquad programming)
├── tas5805m_biquad_i2c.h                   # I2C implementation for ESPHome
├── calibrate.html                          # Phone-based measurement web UI
├── index.html                              # Room correction management interface
├── ARCHITECTURE.md                         # Technical deep-dive on system design
└── CLAUDE.md                               # Claude Code project instructions
```

## Quick Start

### 1. Flash the ESPHome Configuration

The main config already includes room correction services via packages:

```yaml
packages:
  room_correction: !include room_correction_services.yaml
```

Flash to your Louder-ESP32S3:
```bash
esphome run louder-s3-sendspin-ethernet-oled.yaml
```

### 2. Use Home Assistant Services

After flashing, these services are available in Home Assistant:

| Service | Description |
|---------|-------------|
| `set_parametric_eq` | Add a parametric EQ filter (frequency, gain, Q) |
| `set_low_shelf` | Add a low shelf filter (bass adjustment) |
| `set_high_shelf` | Add a high shelf filter (treble adjustment) |
| `set_highpass` | Add a high-pass filter (subsonic protection) |
| `set_lowpass` | Add a low-pass filter (tweeter protection) |
| `set_notch` | Add a notch filter (kill resonances) |
| `set_biquad` | Program raw biquad coefficients |
| `reset_biquad` | Reset single biquad to bypass |
| `reset_all_biquads` | Reset all filters to flat response |

### 3. Apply Room Correction

Example: Cut a room mode at 80Hz:
```yaml
service: esphome.louder_s3_kitchen_set_parametric_eq
data:
  channel: 2      # 0=left, 1=right, 2=both
  index: 0        # Biquad slot 0-14
  frequency: 80   # Hz
  gain_db: -6     # dB (negative = cut)
  q: 2            # Q factor
```

Reset to flat:
```yaml
service: esphome.louder_s3_kitchen_reset_all_biquads
```

### 4. Phone-Based Calibration (Optional)

For automated measurement and correction:

1. Host `calibrate.html` on your network (ESPHome web server, Home Assistant, etc.)
2. Open on your phone at the listening position
3. Allow microphone access
4. Follow the measurement wizard
5. Review and apply calculated filters

## Hardware Requirements

- **Sonocotta Louder-ESP32S3** board
- **TAS5805M DAC** (integrated on Louder board)
- **W5500 Ethernet module** (recommended for Sendspin reliability)
- **Passive speakers** (e.g., bookshelf speakers)

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
| OLED CS | 47 |
| OLED DC | 38 |
| OLED RST | 48 |

## TAS5805M DSP Details

The TAS5805M has 15 biquad filters per channel, each implementing:

```
        b0 + b1*z^-1 + b2*z^-2
H(z) = ────────────────────────
        1 + a1*z^-1 + a2*z^-2
```

Coefficients are stored in 9.23 fixed-point format (9 bits integer, 23 bits fractional).

### Register Map

- **Book 0xAA**: DSP coefficient pages
- **Left channel**: Pages 0x24-0x27
- **Right channel**: Pages 0x32-0x35
- **20 bytes per biquad**: b0, b1, b2, a1, a2 (big-endian, a1/a2 sign-inverted)

## Limitations

- **Phone mic accuracy** - Good for room modes (±6dB), not laboratory precision
- **Bass response** - Phone mics roll off below ~80Hz
- **High frequencies** - Limited accuracy above ~12kHz
- **Single position** - Optimizes for one listening spot

## Troubleshooting

### "Filters not applying"
- Check ESPHome logs for I2C errors
- Verify TAS5805M I2C address (0x2C)
- Ensure the DAC is enabled

### "Sound quality degraded"
- Reduce boost amounts (cuts are safer than boosts)
- Check for filter instability (very high Q with boost)
- Try fewer filters

### "I2C errors in logs"
- Check I2C wiring (SDA=GPIO8, SCL=GPIO9)
- Verify 400kHz bus speed
- Ensure no bus contention with other devices

## Credits

- **mrtoy-me** - ESPHome TAS5805M component
- **Sonocotta** - Louder-ESP32S3 hardware
- **ESPHome Sendspin team** - Multi-room audio streaming

## License

MIT License - Use freely, attribution appreciated.
