# Getting Started: Room Correction Testing

Quick-start guide for validating your Louder ESP32 room correction system.

## Equipment You Have

- ✅ **2x Louder ESP32-S3 boards** (arriving tomorrow)
- ✅ **Google Pixel 10 Pro** (excellent microphone - flagship quality)

## Equipment You Need to Order (Priority Order)

### Critical (Order Today)
1. **UMIK-1 or UMIK-2** USB measurement microphone
   - UMIK-1: ~$75 (https://www.minidsp.com/products/acoustic-measurement/umik-1)
   - UMIK-2: ~$100 (updated model with better low-frequency response)
   - **Why**: Your ground truth for validating phone measurements
   - **When**: Needed by Day 3-4 for Phase 2 testing

### Nice to Have
2. **Microphone stand/tripod** (~$20-40)
   - Consistent positioning for repeatability tests
   - Can use phone tripod adapter initially

3. **Room EQ Wizard (REW)** - FREE software
   - Download: https://www.roomeqwizard.com/
   - Works on Windows, Mac, Linux
   - **Install today** - you can start learning the interface

---

## Phase 1: Day 1 - Board Bring-Up (2-3 hours)

### 1.1 Flash Firmware
```bash
cd /home/user/louder-esphome-sendspin

# Flash board #1
esphome run louder-s3-sendspin-ethernet-oled.yaml

# Flash board #2 (update device name in YAML first)
# Edit: substitutions -> device_name: "louder-s3-living-room" (or similar)
esphome run louder-s3-sendspin-ethernet-oled.yaml
```

**Expected result**: Both boards boot, show IP on OLED display, connect to Home Assistant

### 1.2 Verify Basic Operation

**Test on both boards:**
- [ ] OLED display shows "Louder ESP32" and IP address
- [ ] Home Assistant discovers device (check Integrations page)
- [ ] All services appear in Developer Tools → Services:
  - `esphome.louder_s3_kitchen_set_biquad`
  - `esphome.louder_s3_kitchen_set_parametric_eq`
  - `esphome.louder_s3_kitchen_enter_calibration_mode`
  - etc.
- [ ] I2C communication working (check ESPHome logs - should be no I2C errors)

**Check logs:**
```bash
esphome logs louder-s3-sendspin-ethernet-oled.yaml
```
Look for:
- ✅ "TAS5805M initialized successfully"
- ✅ "Profile manager initialized"
- ❌ NO I2C errors or warnings

### 1.3 First Audio Test

**Connect speakers and play audio:**
- Use Home Assistant media player entity
- Play music from Sendspin or local file
- Verify both channels work
- Listen for any obvious distortion or issues

**Expected result**: Clean audio, no clicks, pops, or distortion

---

## Phase 2: Day 1 - First Calibration (30 minutes)

### 2.1 Prepare Room

**Room setup:**
- Place speaker in final location (away from walls ideally, but anywhere is fine for testing)
- Turn off HVAC, fans, appliances (reduce background noise)
- Close windows if street noise present
- Quiet the room as much as possible

**Expected noise floor**: < 40 dB SPL (use free SPL meter app on Pixel to check)

### 2.2 Run Calibration

**On Pixel 10 Pro:**
1. Open Chrome browser
2. Navigate to: `http://louder-s3-kitchen.local/calibrate`
   - (Replace with your actual device hostname)
3. Grant microphone permission when prompted
4. Position phone at listening position:
   - Seated ear height (~100-120 cm from floor)
   - 2-3 meters from speaker
   - Point microphone toward speaker (usually bottom of phone)
   - Hold steady or place on stable surface
5. Click "Start Calibration"
6. **Stay silent** during sweep playback (~10 seconds)
7. Wait for analysis (~5-10 seconds)
8. Review frequency response graph

**What to look for:**
- Graph should show room response with peaks and dips
- Typical pattern: bass peaks (room modes), mid-range dips, HF roll-off
- No error messages about clipping or background noise

### 2.3 Apply Correction

1. Click "Apply Correction" button
2. Monitor ESP32 logs for filter programming
3. Play music again
4. **Listen for difference** - should sound more balanced

**Expected result:**
- Filters upload successfully (check logs for "Biquad X written successfully")
- Audible difference (usually tighter bass, clearer vocals)
- No artifacts (ringing, harshness, etc.)

### 2.4 Save Profile

1. In web UI, name profile (e.g., "Living Room - Pixel 10")
2. Click "Save Profile"
3. Verify save success message

**Test persistence:**
```bash
# In Home Assistant Developer Tools → Services
service: esphome.louder_s3_kitchen_reset_eq
# Reboot ESP32
# Check if profile auto-loads on boot
```

---

## Phase 3: Day 1 - Quick Validation (30 minutes)

### 3.1 Repeatability Test

**Run calibration 3 times in a row:**
1. Keep phone in **exact same position**
2. Run calibration #1 → save as "Test_1"
3. Run calibration #2 → save as "Test_2"
4. Run calibration #3 → save as "Test_3"

**Manual check:**
- Do the frequency response graphs look similar?
- Are major peaks/dips in same locations?
- Acceptable variance: ±2-3 dB eyeball test

**This tells you:** Is the measurement system stable enough for testing?

### 3.2 A/B Listening Test (Yourself)

**Set up quick switching:**
```yaml
# In Home Assistant, create automation for quick A/B:
# Button press → toggle between corrected and bypass
```

Or manually:
- Corrected: Apply profile
- Bypass: Call `reset_eq` service

**Test with 3 music tracks:**
1. **Bass-heavy** (electronic, hip-hop) - listen for bass tightness
2. **Vocals** (acoustic, jazz) - listen for clarity
3. **Complex** (orchestral, rock) - listen for balance

**Rate each:**
- Which sounds more natural?
- Which has clearer vocals?
- Which has tighter bass?
- Any artifacts (ringing, harshness)?

**Expected result**: Preference for corrected version, no obvious artifacts

---

## Phase 4: Days 2-3 - Phone Mic Validation (Wait for UMIK-1)

### 4.1 Install REW (Do This While Waiting)

**Download and setup:**
1. Download from https://www.roomeqwizard.com/
2. Install Java runtime if needed
3. Run REW and complete initial setup wizard
4. **Tutorial**: Watch "REW Basics" on YouTube (Gene DellaSala's videos are excellent)

**Practice measurements:**
- Set up UMIK-1 when it arrives (USB connection)
- Run SPL meter calibration
- Practice taking measurements
- Learn to generate frequency response graphs

### 4.2 Phone vs Reference Comparison (When UMIK-1 Arrives)

**Setup:**
- Place Pixel 10 Pro at listening position
- Place UMIK-1 **right next to phone** (within 5-10 cm)
- Both pointed at speaker

**Dual measurement:**
1. **Phone measurement**: Run calibrate.html on Pixel
2. **Reference measurement**:
   - In REW, use "Measure" → "Log sweep"
   - Start frequency: 20 Hz
   - End frequency: 20,000 Hz
   - Duration: 10 seconds
   - Level: -12 dBFS
   - Take measurement

**Compare in REW:**
1. Import phone measurement (if possible - may need to export data from calibrate.html)
2. Overlay both graphs
3. Apply 1/3 octave smoothing to both
4. **Look for**:
   - Overall shape agreement (peaks and dips in same places)
   - Target: ±3 dB agreement from 100 Hz to 8 kHz
   - Document any systematic error (e.g., Pixel 10 has +2 dB boost at 4 kHz)

**This tells you:** Can you trust your phone measurements?

---

## Phase 5: Days 3-5 - Correction Effectiveness

### 5.1 Before/After Measurement (REW + UMIK-1)

**Workflow:**

**Before correction:**
1. Call `reset_eq` service to bypass all filters
2. Take REW measurement → save as "Before_Correction.mdat"

**Apply correction:**
3. Run phone calibration
4. Apply generated filters

**After correction:**
5. Take REW measurement → save as "After_Correction.mdat"

**Analysis in REW:**
1. Overlay both measurements
2. Apply 1/3 octave smoothing
3. **Calculate metrics**:
   - Standard deviation of SPL (Before vs After)
   - Peak-to-peak variation
   - Visual inspection: Are room modes attenuated?

**Success criteria:**
- Before: Typical room has ±5-8 dB variance
- After (MVP): ±3 dB variance
- After (TruePlay-level): ±2 dB variance

### 5.2 Room Mode Analysis

**In REW, identify room modes:**
- Look for peaks in 30-200 Hz region
- Typical spacing: 20-50 Hz between modes
- Document frequency and magnitude of 3 worst modes

**Measure attenuation:**
- Compare mode magnitude before vs after
- Target: 50-70% reduction (6-12 dB)
- Example: 55 Hz mode at +8 dB → after correction +3 dB = 5 dB reduction (good!)

### 5.3 Listening Tests (Blind A/B)

**Set up blind test:**
- Get a friend/family member to help
- They switch between corrected/bypass without telling you
- You identify which sounds better

**Test protocol:**
1. 10 trials, randomized order
2. You must identify which is corrected
3. Rate confidence (1-5 scale)

**Success criteria:**
- Score > 7/10 correct (statistically significant)
- High confidence ratings when correct
- Consistent preference for corrected version

---

## Phase 6: Days 5-7 - Optimization & Second Board

### 6.1 Multi-Room Testing

**You have 2 boards - test in different rooms!**

**Room 1** (e.g., living room):
- Full testing as above
- Document room size, speaker placement
- Save all REW measurements

**Room 2** (e.g., bedroom):
- Repeat same tests
- Compare correction effectiveness
- Does it work in different acoustic environments?

**This tells you:** Is the system robust across different rooms?

### 6.2 Algorithm Tuning

**Based on test results, adjust:**

**If correction is too aggressive:**
- Reduce max boost (currently allows +12 dB, try +6 dB)
- Increase minimum Q (avoid very narrow filters)
- Limit correction to 80 Hz - 5 kHz range

**If correction is not effective enough:**
- Check FFT implementation accuracy (Phase 4.1 in TESTING_CHECKLIST.md)
- Verify biquad coefficient calculation
- Compare calculated filters to REW's AutoEQ

**Document all changes and re-test!**

---

## Quick Reference: Daily Tasks

### Day 1 (Boards Arrive)
- [ ] Flash firmware on both boards
- [ ] Verify Home Assistant integration
- [ ] Run first calibration with Pixel 10 Pro
- [ ] Repeatability test (3x same position)
- [ ] Initial listening tests (self)
- [ ] **Order UMIK-1 microphone**

### Day 2-3 (Learning & Preparation)
- [ ] Install and learn REW software
- [ ] Practice with REW interface
- [ ] Run more phone calibrations (build familiarity)
- [ ] Test different listening positions
- [ ] Document room dimensions and speaker placement
- [ ] **Wait for UMIK-1 delivery**

### Day 4-5 (UMIK-1 Arrives)
- [ ] Calibrate UMIK-1 in REW (load cal file from miniDSP)
- [ ] Phone vs UMIK-1 comparison test
- [ ] Before/after REW measurements
- [ ] Room mode analysis
- [ ] Calculate objective metrics (std dev, variance)

### Day 6-7 (Validation)
- [ ] Blind A/B listening tests
- [ ] Second room testing (board #2)
- [ ] Algorithm tuning based on results
- [ ] Document findings

---

## What Success Looks Like (End of Week 1)

**Minimum Viable Product:**
- ✅ Both boards running reliably
- ✅ Phone measurements repeatable within ±2 dB
- ✅ REW comparison shows ±3 dB agreement
- ✅ Before/after shows audible improvement
- ✅ Correction reduces room modes by >50%
- ✅ You prefer corrected version in blind tests

**TruePlay-Level Indicators:**
- ✅ Phone measurements repeatable within ±1 dB
- ✅ REW comparison shows ±1.5 dB agreement
- ✅ After correction: ±2 dB variance or better
- ✅ Room modes reduced by >60%
- ✅ Blind test score >8/10
- ✅ Works well in both test rooms

---

## Common Issues & Troubleshooting

### Issue: High background noise during calibration
**Solution:**
- Turn off HVAC, fans
- Close windows
- Test at quiet time of day (late night)
- Check SPL meter app: aim for <40 dB SPL

### Issue: Phone measurements not repeatable
**Solution:**
- Use phone stand/tripod (not handheld)
- Verify microphone not blocked by case
- Check for AGC (automatic gain control) in Chrome
- Try landscape vs portrait orientation

### Issue: Correction makes sound worse
**Solution:**
- Check for artifacts (ringing, harshness)
- Reduce max boost limit
- Verify biquad coefficients are stable
- Check ESP logs for I2C errors

### Issue: I2C write failures in logs
**Solution:**
- Already implemented retry logic (should self-recover)
- If persistent, check I2C wiring/connections
- Verify TAS5805M initialization succeeds on boot

---

## Data Organization

Create this folder structure for your test data:

```
tests/
├── day1_initial/
│   ├── pixel10_repeatability_1.json
│   ├── pixel10_repeatability_2.json
│   ├── pixel10_repeatability_3.json
│   └── notes.txt
├── day4_umik_validation/
│   ├── phone_measurement.mdat
│   ├── umik_reference.mdat
│   └── comparison_notes.txt
├── day5_before_after/
│   ├── living_room_before.mdat
│   ├── living_room_after.mdat
│   ├── bedroom_before.mdat
│   ├── bedroom_after.mdat
│   └── analysis.txt
└── listening_tests/
    └── blind_ab_results.csv
```

---

## Next Steps After Week 1

If tests are successful:
1. Address remaining code review issues (deprecated Web Audio API, etc.)
2. Expand testing to more rooms and phones
3. Beta test with friends/family
4. Compare to Sonos TruePlay (if you can borrow a Sonos speaker)
5. Consider publishing results and making project public

If tests show issues:
1. Debug algorithm (FFT, smoothing, filter design)
2. Improve phone mic calibration
3. Tune filter parameters (max boost, Q range, frequency limits)
4. Consult TESTING_CHECKLIST.md for detailed validation steps

---

## Questions to Answer This Week

1. **Is the Pixel 10 Pro accurate enough?** (Day 4 - UMIK comparison)
2. **Is correction effective?** (Day 5 - before/after metrics)
3. **Is it audibly better?** (Day 6 - blind listening tests)
4. **Does it work in different rooms?** (Day 6 - second board)
5. **Are you TruePlay-level?** (Day 7 - compare to success criteria)

Good luck! Report back with results and we can iterate on the algorithm.
