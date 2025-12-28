# TAS5805M Room Correction System

## Overview

A complete room correction solution for Louder-ESP32S3 that:
1. Uses phone microphone at listening position (no extra hardware)
2. Measures room frequency response via swept sine/pink noise
3. Calculates parametric EQ corrections
4. Programs TAS5805M biquad filters directly (not just the 15-band graphic EQ)
5. Stores calibration profiles persistently

## System Components

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              CALIBRATION FLOW                               │
└─────────────────────────────────────────────────────────────────────────────┘

    PHONE BROWSER                    ESP32-S3                    TAS5805M
    ─────────────                    ────────                    ────────
         │                               │                           │
         │  1. Open calibration URL      │                           │
         │──────────────────────────────>│                           │
         │                               │                           │
         │  2. WebSocket connection      │                           │
         │<─────────────────────────────>│                           │
         │                               │                           │
         │  3. Request test tone         │                           │
         │──────────────────────────────>│  4. Play sweep            │
         │                               │─────────────────────────> │
         │                               │                           │
         │  5. Capture via Web Audio API │                           │
         │  (mic at listening position)  │                           │
         │                               │                           │
         │  6. FFT analysis              │                           │
         │  7. Calculate response curve  │                           │
         │  8. Generate inverse EQ       │                           │
         │  9. Compute biquad coeffs     │                           │
         │                               │                           │
         │  10. Send coefficients        │                           │
         │──────────────────────────────>│  11. Program biquads      │
         │                               │─────────────────────────> │
         │                               │                           │
         │                               │  12. Save to NVS          │
         │                               │                           │
         │  13. Confirm applied          │                           │
         │<──────────────────────────────│                           │
         │                               │                           │
```

## TAS5805M Biquad Structure

The TAS5805M has 15 biquad filters per channel (30 total for stereo).
Each biquad implements a 2nd-order IIR filter:

```
        b0 + b1*z^-1 + b2*z^-2
H(z) = ────────────────────────
        1 + a1*z^-1 + a2*z^-2
```

### Coefficient Format: 9.23 Fixed-Point

TAS5805M uses 32-bit fixed-point with:
- 9 bits integer (signed)
- 23 bits fractional

Conversion from float:
```c
int32_t float_to_9_23(float value) {
    return (int32_t)(value * (1 << 23));
}
```

## Filter Types Supported

| Type | Use Case | Parameters |
|------|----------|------------|
| Parametric EQ | Surgical room mode correction | fc, gain (dB), Q |
| Low Shelf | Broad bass adjustment | fc, gain (dB), S (slope) |
| High Shelf | Broad treble adjustment | fc, gain (dB), S (slope) |
| High-Pass | Subsonic protection | fc, Q |
| Low-Pass | Tweeter protection | fc, Q |
| Notch | Kill specific resonance | fc, Q |

## Calibration Algorithm

### Phase 1: Measurement
1. Play logarithmic sine sweep (20Hz → 20kHz over 5 seconds)
2. Capture via phone mic using Web Audio API
3. Compute transfer function via FFT
4. Average multiple sweeps for noise reduction

### Phase 2: Analysis
1. Smooth frequency response (1/6 octave smoothing)
2. Identify peaks > 3dB from target curve
3. Target curve: Slight downward slope per Harman research
4. Focus on 20Hz-500Hz where room modes dominate

### Phase 3: Filter Generation (REW-style)
1. For each identified peak:
   - Calculate optimal fc, gain, Q
   - Generate biquad coefficients
2. Limit corrections to cuts only (don't boost into nulls)
3. Maximum 10 filters to leave headroom
4. Prioritize bass correction

### Phase 4: Application
1. Convert coefficients to 9.23 format
2. Program TAS5805M via I2C
3. Store in ESP32 NVS for persistence

## Safety Limits

- Maximum boost: +6dB
- Maximum cut: -15dB  
- Subsonic high-pass: 25Hz (always on)
- Q range: 0.5 to 10
