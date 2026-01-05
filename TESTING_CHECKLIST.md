# TruePlay-Level Room Correction Testing Checklist

This comprehensive testing checklist ensures the room correction system achieves professional-grade performance comparable to Sonos TruePlay.

## Overview

**Goal**: Validate that the ESP32 + TAS5805M room correction system provides:
- Accurate frequency response measurement (±1.5 dB from 80 Hz to 8 kHz)
- Effective room mode correction (±3 dB target across critical range)
- Repeatable results across multiple calibration runs (±1 dB variance)
- Stable, audibly transparent correction (no ringing or phase issues)

**Required Equipment**:
- ✅ Reference measurement microphone (UMIK-1, miniDSP UMIK-2, or calibrated USB mic)
- ✅ Room EQ Wizard (REW) software
- ✅ Multiple test phones (iPhone, Android flagship, Android mid-range)
- ✅ SPL meter or calibrated iOS/Android SPL app
- ✅ Audio analyzer (optional: Audio Precision, QuantAsylum, or software-based)

---

## Phase 1: Baseline System Validation

### 1.1 Hardware Verification

- [ ] **TAS5805M Frequency Response (No Correction)**
  - Measure analog output with REW using reference mic
  - Verify ±0.5 dB flatness from 20 Hz to 20 kHz into resistive load
  - Document any roll-off above 18 kHz
  - Verify channel balance within ±0.2 dB across frequency range
  - Test file: `tests/01_baseline_frequency_response.mdat` (REW measurement)

- [ ] **TAS5805M THD+N Baseline**
  - Measure at -10 dBFS, 1 kHz sine wave
  - Verify THD+N < 0.01% (target: < 0.005%)
  - Test at 20 Hz, 100 Hz, 1 kHz, 10 kHz
  - Test file: `tests/01_baseline_thd.mdat`

- [ ] **Biquad Filter Accuracy**
  - Program known test filters (peaking EQ at 1 kHz, +6 dB, Q=1.0)
  - Measure actual response vs theoretical
  - Verify ±0.1 dB accuracy in passband
  - Verify ±0.2 dB accuracy at peak frequency
  - Test all 15 biquad slots independently
  - Test file: `tests/01_biquad_accuracy.mdat`

- [ ] **Fixed-Point Conversion Accuracy**
  - Test edge cases: very small Q values (0.1), large Q (10.0)
  - Test extreme gains: +12 dB, -12 dB
  - Test extreme frequencies: 20 Hz, 20 kHz
  - Verify no clipping or overflow in coefficient conversion
  - Log all coefficients and verify against Python reference implementation

### 1.2 I2C Communication Reliability

- [ ] **Write Success Rate**
  - Program all 30 biquads (15 per channel) 100 times consecutively
  - Verify 100% success rate with current retry logic
  - Monitor ESP logs for any retry warnings
  - Test under different conditions: cold boot, after WiFi activity, during streaming

- [ ] **Coefficient Readback Verification** (if TAS5805M supports readback)
  - Write known coefficients
  - Read back and verify bit-exact match
  - If readback not available, verify with acoustic measurement instead

- [ ] **Timing Validation**
  - Measure total time to program all 30 biquads
  - Target: < 500 ms total (acceptable: < 1 second)
  - Verify delays don't cause audio dropouts during streaming

---

## Phase 2: Microphone & Calibration Algorithm Validation

### 2.1 Phone Microphone Characterization

- [ ] **Reference Microphone Comparison**
  - Play pink noise through system in test room
  - Record simultaneously with:
    - Phone microphone (calibrate.html)
    - Reference microphone (REW)
  - Compare frequency responses
  - Document phone mic deviations vs reference
  - Create phone-specific calibration curve if needed
  - Test devices:
    - [ ] iPhone 14/15 Pro (known good mic)
    - [ ] Samsung Galaxy S22/S23
    - [ ] Google Pixel 7/8
    - [ ] Mid-range Android device

- [ ] **Phone Microphone Frequency Response**
  - Measure each phone's mic response in anechoic or quiet room
  - Target: ±3 dB from 100 Hz to 10 kHz (acceptable for room correction)
  - Document roll-off below 100 Hz (common for phone mics)
  - Note any resonances or peaks (often around 3-5 kHz for phone mics)
  - Test file: `tests/02_phone_mic_calibration_{device}.mdat`

- [ ] **Phone Microphone SPL Linearity**
  - Test at 70 dB, 80 dB, 90 dB SPL (measured with reference SPL meter)
  - Verify linear response (no AGC activation)
  - Document clipping threshold (should be > 95 dB SPL for music)
  - Note: May need to disable AGC in browser (check Web Audio API settings)

- [ ] **Background Noise Floor**
  - Measure noise floor with system playing silence
  - Target: < 40 dB SPL in test room during calibration
  - Verify SNR > 50 dB during sweep playback

### 2.2 Sweep Signal Validation

- [ ] **Sweep Signal Quality**
  - Verify sine sweep is logarithmic (equal energy per octave)
  - Check for digital clipping or distortion in generated sweep
  - Measure THD during sweep playback (should be < 0.1%)
  - Verify sweep level is consistent (±1 dB) across full range
  - Test file: `tests/02_sweep_signal_analysis.mdat`

- [ ] **Sweep Frequency Range**
  - Verify sweep covers 20 Hz to 20 kHz (or adjust to phone mic limits)
  - Check that sweep timing is accurate (5-10 second duration typical)
  - Verify no aliasing artifacts above Nyquist frequency

### 2.3 FFT Analysis Validation

- [ ] **FFT Implementation Accuracy**
  - Generate known test signal (pure sine at 1 kHz)
  - Feed through analyzeResponse() function
  - Verify:
    - Peak appears at exactly 1000 Hz bin
    - Magnitude is correct (within ±0.5 dB)
    - No spectral leakage > -40 dB from peak
  - Test file: `tests/02_fft_validation.json`

- [ ] **Windowing Function**
  - Verify Hann window is applied correctly
  - Test with rectangular window (no windowing) for comparison
  - Measure reduction in spectral leakage (should be > 30 dB improvement)

- [ ] **Frequency Resolution**
  - Verify bin spacing matches expected value (sampleRate / fftSize)
  - For fftSize=8192, sampleRate=48000: resolution = 5.86 Hz
  - Confirm sufficient resolution for room modes (typically 20-50 Hz spacing)

- [ ] **Smoothing Algorithm**
  - Validate 1/3 octave smoothing implementation
  - Compare against REW's smoothing of same raw data
  - Verify smoothing doesn't over-blur narrow room modes
  - Test file: `tests/02_smoothing_comparison.mdat`

- [ ] **Normalization**
  - Verify 1 kHz reference normalization works correctly
  - Test with input signals at different levels (-20 dBFS to -6 dBFS)
  - Verify normalized response is consistent regardless of input level
  - Acceptable variance: ±0.5 dB

---

## Phase 3: Room Measurement Accuracy

### 3.1 Measurement Repeatability

- [ ] **Single Position Repeatability**
  - Run calibration 5 times at same listening position (without moving phone)
  - Compare frequency responses
  - Target: ±1 dB variance across critical range (100 Hz - 4 kHz)
  - Acceptable: ±2 dB variance
  - Document variance at each frequency band
  - Test file: `tests/03_repeatability_single_position.csv`

- [ ] **Multi-Position Consistency**
  - Run calibration at 3 different listening positions (20cm apart)
  - Verify responses show expected room mode behavior
  - Average the responses and compare to single-position measurement
  - Document which position gives most representative results
  - Test file: `tests/03_multi_position_comparison.csv`

### 3.2 Room Response Accuracy

- [ ] **Frequency Response vs REW Reference**
  - Play log sweep through system
  - Record simultaneously with:
    - Phone microphone (calibrate.html)
    - Reference microphone (REW measurement)
  - Compare measured frequency responses after smoothing
  - Target: ±1.5 dB agreement from 80 Hz to 8 kHz
  - Acceptable: ±3 dB agreement
  - Document any systematic errors (e.g., phone mic high-frequency boost)
  - Test file: `tests/03_phone_vs_reference_comparison.mdat`

- [ ] **Room Mode Detection**
  - Identify room modes with REW measurement
  - Verify calibrate.html detects same modes (within ±5 Hz)
  - Typical room modes:
    - Length mode: c / (2L) where c = 343 m/s, L = room length
    - Width mode: c / (2W)
    - Height mode: c / (2H)
  - Example: 5m x 4m x 2.7m room has modes at ~34 Hz, 43 Hz, 64 Hz
  - Test file: `tests/03_room_modes_detection.csv`

- [ ] **Phase Response** (Advanced)
  - Use REW to measure phase response before/after correction
  - Verify biquad filters maintain reasonable phase behavior
  - Target: Minimum-phase behavior (group delay = derivative of phase)
  - Ensure no phase wrapping issues or large group delay peaks

### 3.3 Environmental Robustness

- [ ] **Background Noise Handling**
  - Test with increasing background noise levels:
    - Silent room (< 40 dB SPL)
    - Quiet room (45-50 dB SPL) - normal household
    - Moderate noise (55-60 dB SPL) - HVAC running
  - Verify calibration still produces valid results up to 50 dB SPL
  - Document accuracy degradation with noise level

- [ ] **Temperature Stability**
  - Test after cold boot vs. after system running for 30 minutes
  - Verify I2C communication remains stable
  - Check for any thermal drift in TAS5805M response

---

## Phase 4: Correction Filter Generation

### 4.1 Filter Design Validation

- [ ] **Biquad Coefficient Calculation**
  - Compare generated biquad coefficients against reference implementation:
    - Robert Bristow-Johnson's Audio EQ Cookbook formulas
    - MATLAB Signal Processing Toolbox
    - SciPy signal.iirfilter()
  - Test cases:
    - Peaking EQ: 100 Hz, +6 dB, Q=1.0
    - Peaking EQ: 1 kHz, -3 dB, Q=0.5
    - Low shelf: 80 Hz, +4 dB, Q=0.7
    - High shelf: 10 kHz, -2 dB, Q=0.7
  - Verify coefficient accuracy to 0.01% (32-bit float precision)
  - Test file: `tests/04_biquad_coefficient_validation.py`

- [ ] **Filter Stability**
  - Verify all generated biquad filters are stable:
    - Check poles are inside unit circle: |pole| < 1
    - Use `b0 + b1 + b2 < 1 + a1 + a2` stability criterion
  - Test extreme filter settings:
    - Very high Q (Q=10) at low frequencies
    - Maximum boost (+12 dB)
    - Maximum cut (-12 dB)
  - Verify no unstable filters are generated
  - Test file: `tests/04_filter_stability_check.py`

- [ ] **Gain Limiting**
  - Verify maximum boost is limited to safe levels (suggest +6 to +9 dB max)
  - Verify maximum cut is limited (suggest -12 dB max)
  - Test what happens with very deep room nulls (> -20 dB)
  - Ensure system doesn't try to boost nulls excessively

- [ ] **Filter Count Optimization**
  - Verify system uses minimum number of biquads needed
  - Test complex room responses:
    - How many filters for typical room? (target: 5-10)
    - Does it use all 15 filters in worst-case room?
    - Are bypass filters correctly identified?

### 4.2 Target Curve

- [ ] **House Curve Selection**
  - Document default target curve (flat, Harman, B&K, custom?)
  - Test different target curves:
    - Flat (0 dB reference)
    - Gentle high-frequency roll-off (-3 dB at 10 kHz)
    - Harman target curve (slight bass boost, HF roll-off)
  - Allow user to select target curve in UI
  - Test file: `tests/04_target_curves.json`

- [ ] **Frequency Range for Correction**
  - Define correction range (suggest 40 Hz to 8 kHz)
  - Don't correct below room modes (< 40 Hz may cause over-excursion)
  - Don't correct extreme HF (> 8 kHz may be phone mic artifacts)
  - Verify filters outside correction range are set to bypass

---

## Phase 5: Correction Effectiveness

### 5.1 Objective Measurements (Before/After)

- [ ] **Frequency Response Improvement**
  - Measure room response before correction (with REW)
  - Apply correction filters
  - Measure room response after correction (with REW)
  - Calculate metrics:
    - Standard deviation of SPL (should decrease)
    - Peak-to-peak variation (should decrease)
    - Mean absolute error from target curve (should decrease)
  - Target improvement:
    - Before: ±6 dB variance typical
    - After: ±3 dB variance (good), ±2 dB variance (excellent)
  - Test file: `tests/05_before_after_comparison.mdat`

- [ ] **Room Mode Correction**
  - Identify 3 worst room modes in uncorrected response
  - Measure attenuation of each mode after correction
  - Target: Reduce peaks by 50-70% (6-12 dB reduction)
  - Example: 55 Hz room mode at +8 dB should reduce to +3 dB or less
  - Verify no new peaks are introduced elsewhere

- [ ] **Waterfall Plot Analysis** (Advanced)
  - Generate waterfall plot (time-frequency decay) in REW
  - Verify room mode decay is improved after correction
  - Target: Faster decay time (reduced ringing)
  - Compare RT60 (reverberation time) before/after

### 5.2 Subjective Listening Tests

- [ ] **A/B Comparison Test**
  - Set up instant switching between corrected/uncorrected
  - Use variety of music genres:
    - Bass-heavy (electronic, hip-hop)
    - Vocal-focused (jazz, acoustic)
    - Complex orchestral
  - Test with 5+ listeners (some trained, some untrained)
  - Questions:
    - Which sounds more balanced?
    - Which has clearer vocals?
    - Which has tighter bass?
    - Which sounds more natural?
  - Target: >80% preference for corrected version
  - Test file: `tests/05_listening_test_results.csv`

- [ ] **Artifact Detection**
  - Listen for audible artifacts:
    - Ringing or resonance (indicates over-Q or instability)
    - Harshness or sibilance (excessive HF boost)
    - Boomy or muddy bass (excessive LF boost)
    - Phasey or hollow sound (phase issues)
  - Test with demanding tracks:
    - Close-miked vocals (for sibilance)
    - Kick drum (for bass tightness)
    - Cymbal crashes (for HF artifacts)
  - Zero tolerance: Any audible artifacts require fixing

- [ ] **Blind Test (Gold Standard)**
  - Set up true blind A/B test (assistant switches for listener)
  - Use ABX testing methodology
  - Test if listeners can reliably distinguish corrected vs uncorrected
  - If corrected is preferred, they should score >70% in ABX test
  - Test file: `tests/05_blind_test_protocol.md`

### 5.3 Different Room Scenarios

- [ ] **Small Room (< 20 m³)**
  - Test in bedroom or small office
  - Expect strong room modes (large spacing)
  - Verify correction handles deep nulls gracefully
  - Test file: `tests/05_small_room_results.mdat`

- [ ] **Medium Room (20-60 m³)**
  - Test in living room or home theater
  - Typical residential use case
  - Expect moderate room modes
  - Test file: `tests/05_medium_room_results.mdat`

- [ ] **Large Room (> 60 m³)**
  - Test in large open-plan space
  - Expect weaker room modes (smaller peaks)
  - Verify correction doesn't over-process
  - Test file: `tests/05_large_room_results.mdat`

- [ ] **Treated Room**
  - Test in room with acoustic treatment (panels, bass traps)
  - Should require less correction
  - Verify system doesn't introduce artifacts in already-good room
  - Test file: `tests/05_treated_room_results.mdat`

- [ ] **Difficult Room**
  - Test worst-case scenario:
    - Asymmetric room
    - Hard reflective surfaces
    - Off-center speaker placement
  - Document limitations of the system
  - Test file: `tests/05_difficult_room_results.mdat`

---

## Phase 6: System Integration & Reliability

### 6.1 Profile Management

- [ ] **Profile Save/Load**
  - Create calibration profile
  - Save to NVS
  - Reboot ESP32
  - Verify profile loads correctly on boot
  - Compare before/after frequency response (should be identical)
  - Test file: `tests/06_profile_persistence.csv`

- [ ] **Checksum Validation**
  - Save 10 different profiles
  - Load each profile 10 times
  - Verify checksum never fails (with packed structs fix)
  - Monitor logs for any validation errors

- [ ] **Multiple Profiles**
  - Create 5 different profiles (max capacity)
  - Verify all can be saved without issues
  - Test switching between profiles
  - Verify correct profile is applied
  - Test deletion of profiles

- [ ] **Profile Corruption Recovery**
  - Simulate corrupted profile (manual NVS edit if possible)
  - Verify system detects corruption (checksum fail)
  - Verify system falls back gracefully (no crash)
  - Verify logs provide clear error message

### 6.2 Web UI Reliability

- [ ] **Browser Compatibility**
  - Test calibrate.html on:
    - [ ] iOS Safari (iPhone 12+, iOS 15+)
    - [ ] iOS Safari (older iPhone, iOS 14)
    - [ ] Android Chrome (flagship device)
    - [ ] Android Chrome (mid-range device)
    - [ ] Desktop Chrome
    - [ ] Desktop Firefox
    - [ ] Desktop Safari
  - Document any compatibility issues
  - Note performance differences (FFT processing time)

- [ ] **Network Reliability**
  - Test calibration over WiFi
  - Test calibration over Ethernet (if device supports)
  - Simulate poor network conditions:
    - High latency (50+ ms)
    - Packet loss (1-5%)
  - Verify UI provides feedback on connection issues
  - Test recovery from temporary network dropout

- [ ] **Mobile Device Compatibility**
  - Test phones held in portrait vs landscape
  - Test with different phone cases (no case, thin case, thick case)
  - Verify microphone not blocked during calibration
  - Test with phone on tripod vs hand-held

### 6.3 Long-Term Stability

- [ ] **Extended Operation**
  - Run system continuously for 24 hours
  - Apply and remove corrections multiple times
  - Verify no memory leaks (monitor ESP32 free heap)
  - Verify I2C communication remains stable
  - Check logs for any errors or warnings

- [ ] **Power Cycle Testing**
  - Power cycle ESP32 20 times
  - Verify profile loads correctly each time
  - Monitor NVS wear (should be minimal with current design)
  - Check for any flash corruption

- [ ] **Concurrent Operations**
  - Test calibration while audio is streaming (should prevent this)
  - Test profile switching during streaming
  - Verify no audio dropouts or clicks
  - Verify system state remains consistent

---

## Phase 7: Performance Benchmarking (TruePlay Comparison)

### 7.1 Sonos TruePlay Comparison (If Available)

- [ ] **Setup Comparison Test**
  - Use Sonos speaker in same room as Louder-ESP32
  - Run TruePlay calibration on Sonos
  - Run calibrate.html on Louder-ESP32
  - Measure both systems with REW reference mic
  - Compare:
    - Before/after improvement magnitude
    - Frequency response variance
    - Room mode attenuation effectiveness
    - Subjective sound quality
  - Test file: `tests/07_trueplay_comparison.mdat`

- [ ] **TruePlay Feature Parity**
  - Document TruePlay features:
    - Multi-point measurement (waves phone around)
    - Automatic gain adjustment
    - Voice guidance
  - Compare to Louder-ESP32 implementation
  - Document feature gaps and improvement opportunities

### 7.2 Reference Correction System Comparison

- [ ] **REW + miniDSP Comparison**
  - Use same room and speaker position
  - Calibrate with REW + UMIK-1
  - Generate correction filters in REW
  - Apply to miniDSP hardware (or simulate)
  - Measure final response
  - Compare to Louder-ESP32 correction
  - Metrics:
    - Final frequency response variance
    - Subjective preference in blind test
  - Test file: `tests/07_rew_comparison.mdat`

### 7.3 Performance Metrics

- [ ] **Processing Speed**
  - Measure total calibration time from start to finish
  - Target: < 60 seconds total
  - Breakdown:
    - Sweep playback: ~10 seconds
    - FFT analysis: < 5 seconds
    - Filter calculation: < 1 second
    - Filter upload: < 1 second
  - Test on different phones (document slowest acceptable device)

- [ ] **Memory Usage**
  - Monitor ESP32 free heap during calibration
  - Verify no leaks after calibration completes
  - Document peak memory usage
  - Target: > 50 KB free heap remaining at all times

- [ ] **Accuracy Summary**
  - Final frequency response variance: ±X dB (100 Hz - 4 kHz)
  - Repeatability: ±X dB across 5 runs
  - Agreement with reference: ±X dB vs REW measurement
  - Subjective preference: X% prefer corrected version
  - Compare to TruePlay benchmarks (if known)

---

## Phase 8: User Experience Testing

### 8.1 First-Time User Experience

- [ ] **Onboarding Flow**
  - Test with users who have never used room correction before
  - Provide minimal instructions (simulate help documentation)
  - Observe where users get confused
  - Document common mistakes:
    - Phone placement
    - Background noise
    - Calibration volume setting
  - Improve UI based on feedback

- [ ] **Error Handling**
  - Test error scenarios:
    - Microphone permission denied
    - Network connection lost during calibration
    - Background noise too high
    - Sweep signal clipping
  - Verify clear error messages are shown
  - Verify user can recover without reloading page

- [ ] **Help Documentation**
  - Write user guide for calibration process
  - Include photos/diagrams of correct phone placement
  - Document optimal room conditions
  - Provide troubleshooting section
  - Test file: `docs/USER_GUIDE.md`

### 8.2 Advanced User Features

- [ ] **Manual EQ Adjustment**
  - Allow users to manually tweak correction filters
  - Provide graphical EQ view
  - Allow saving custom profiles
  - Test undo/redo functionality

- [ ] **Multi-Listener Optimization**
  - Test averaging multiple listening positions
  - Compare single-position vs averaged correction
  - Document which approach gives better results

---

## Phase 9: Production Readiness

### 9.1 Code Quality

- [ ] **Address Remaining Code Review Issues**
  - Fix deprecated Web Audio API (ScriptProcessorNode → AudioWorklet)
  - Implement thread safety for I2C access
  - Add rollback on failed profile load
  - Add profile format version field
  - Complete remaining items from CODE_REVIEW.md

- [ ] **Error Logging**
  - Ensure all error paths have appropriate ESP_LOGE messages
  - Add telemetry for common issues (I2C failures, checksum errors)
  - Consider adding remote logging for field issues

- [ ] **Performance Optimization**
  - Profile FFT performance on slow phones
  - Consider WebAssembly for FFT if needed
  - Optimize I2C writes (batch where possible)
  - Minimize delays to reduce calibration time

### 9.2 Documentation

- [ ] **User Documentation**
  - Complete USER_GUIDE.md
  - Add FAQ section
  - Include troubleshooting flowchart
  - Add photos/videos of calibration process

- [ ] **Developer Documentation**
  - Document API for adding custom filter types
  - Explain biquad coefficient calculation
  - Document TAS5805M register map usage
  - Add examples of common operations

- [ ] **Test Results Publication**
  - Publish all test results from this checklist
  - Include comparison charts and graphs
  - Document accuracy vs TruePlay (if tested)
  - Be transparent about limitations

### 9.3 Final Validation

- [ ] **Beta Testing Program**
  - Deploy to 5-10 beta testers in different environments
  - Collect feedback on reliability and sound quality
  - Document any new issues discovered
  - Iterate based on feedback

- [ ] **Regression Testing**
  - Re-run critical tests after any code changes:
    - Frequency response accuracy (Phase 3.2)
    - Correction effectiveness (Phase 5.1)
    - Profile persistence (Phase 6.1)
  - Maintain test result history to catch regressions

---

## Success Criteria

### Minimum Viable Product (MVP)
- ✅ Frequency response variance < ±3 dB (100 Hz - 4 kHz) after correction
- ✅ Repeatability within ±2 dB across multiple calibration runs
- ✅ > 70% subjective preference for corrected version in listening tests
- ✅ Zero crashes or audio artifacts during normal operation
- ✅ Profile persistence across reboots (100% success rate)

### TruePlay-Level Performance
- ✅ Frequency response variance < ±2 dB (80 Hz - 8 kHz) after correction
- ✅ Repeatability within ±1 dB across multiple calibration runs
- ✅ > 85% subjective preference for corrected version
- ✅ Within ±1.5 dB agreement with REW reference measurement
- ✅ Room mode attenuation > 60% (dB reduction)
- ✅ Total calibration time < 60 seconds
- ✅ Works reliably on mid-range phones (3+ years old)
- ✅ Comparable results to Sonos TruePlay in direct testing (if available)

---

## Test Data Organization

All test data should be stored in the `tests/` directory:

```
tests/
├── 01_baseline_frequency_response.mdat       # REW measurement file
├── 01_baseline_thd.mdat
├── 01_biquad_accuracy.mdat
├── 02_phone_mic_calibration_iphone14.mdat
├── 02_phone_mic_calibration_galaxys23.mdat
├── 02_sweep_signal_analysis.mdat
├── 02_fft_validation.json
├── 02_smoothing_comparison.mdat
├── 03_repeatability_single_position.csv
├── 03_multi_position_comparison.csv
├── 03_phone_vs_reference_comparison.mdat
├── 03_room_modes_detection.csv
├── 04_biquad_coefficient_validation.py       # Python validation script
├── 04_filter_stability_check.py
├── 04_target_curves.json
├── 05_before_after_comparison.mdat
├── 05_listening_test_results.csv
├── 05_blind_test_protocol.md
├── 05_small_room_results.mdat
├── 05_medium_room_results.mdat
├── 05_large_room_results.mdat
├── 05_treated_room_results.mdat
├── 05_difficult_room_results.mdat
├── 06_profile_persistence.csv
├── 07_trueplay_comparison.mdat
└── 07_rew_comparison.mdat
```

---

## Estimated Testing Timeline

- **Phase 1-2** (Baseline & Algorithm): 1-2 days
- **Phase 3** (Room Measurements): 2-3 days (multiple rooms, multiple runs)
- **Phase 4-5** (Correction & Effectiveness): 2-3 days
- **Phase 6-7** (Integration & Benchmarking): 1-2 days
- **Phase 8-9** (UX & Production): 2-3 days

**Total**: 8-13 days for comprehensive testing (with dedicated hardware and test rooms)

---

## Notes

- REW (Room EQ Wizard) is free software: https://www.roomeqwizard.com/
- UMIK-1 microphone: ~$75, UMIK-2: ~$100
- TruePlay comparison requires access to Sonos speaker with TruePlay support
- Consider creating automated test harness for regression testing
- Document all deviations from this checklist with justification
