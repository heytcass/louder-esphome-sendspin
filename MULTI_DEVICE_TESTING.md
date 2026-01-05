# Multi-Device Microphone Testing Strategy

You have access to multiple devices - this is a major advantage! Room correction must work reliably across different phone microphones to be consumer-ready.

## Available Test Devices

**Week 1 (Immediate):**
- ✅ Google Pixel 10 Pro (yours) - Flagship Android
- ✅ Google Pixel 9 (wife's) - Current-gen Android

**Week 3-4 (Parents visiting):**
- ✅ iPhone (parents) - iOS reference (likely iPhone 13-15 range)
- ✅ iPad (parents) - Larger device form factor

## Why Multi-Device Testing Matters

**Sonos TruePlay iOS-only limitation:**
- TruePlay only works with iPhone/iPad due to microphone calibration complexity
- Android microphones vary widely between manufacturers
- You're attempting something more ambitious: **universal phone support**

**Your validation proves:**
1. Algorithm works with different microphone characteristics
2. Phone mic variations don't break calibration
3. System is robust enough for consumer deployment
4. You can identify problem devices early

---

## Testing Strategy

### Phase 1: Week 1 - Android Baseline (Pixel 10 Pro)

**Primary device: Pixel 10 Pro**
- This is your reference Android device
- Complete all Day 1-7 testing with this device
- Establish baseline performance metrics
- Validate against UMIK-1 measurements

**Why start here:**
- Flagship microphone (best-case Android scenario)
- Your daily device (always available)
- Sets performance ceiling for Android devices

### Phase 2: Week 1 - Android Consistency (Pixel 9)

**Secondary device: Pixel 9**

**Test on Day 3-4** (after Pixel 10 Pro baseline established):

#### Test 2.1: Side-by-Side Measurement
**Setup:**
- Same room, same speaker position
- Run calibration with Pixel 10 Pro → save "Pixel10_Room1"
- **Immediately after**, run calibration with Pixel 9 → save "Pixel9_Room1"
- Keep all conditions identical

**Compare:**
1. **Visual comparison:**
   - Do frequency response graphs look similar?
   - Are room modes detected at same frequencies?
   - Are magnitudes comparable (±2-3 dB acceptable)?

2. **UMIK-1 validation (if available):**
   - Measure room with UMIK-1 → "UMIK_Room1_Reference.mdat"
   - Compare both phones vs UMIK-1:
     - Pixel 10 Pro accuracy: ±X dB
     - Pixel 9 accuracy: ±X dB
   - Which is more accurate?

3. **Filter comparison:**
   - Load both profiles in Home Assistant
   - Check generated biquad coefficients
   - Are they similar? (should be if phones measure similarly)

**Expected result:**
- Pixel 9 and 10 Pro should agree within ±2-3 dB (same generation microphones)
- Both should produce similar correction filters
- Any systematic differences indicate microphone variation

#### Test 2.2: Cross-Calibration Test

**Test if one phone's calibration works for another:**

1. Calibrate with Pixel 10 Pro → apply filters
2. Measure result with UMIK-1 → "Corrected_by_Pixel10.mdat"
3. Reset EQ
4. Calibrate with Pixel 9 → apply filters
5. Measure result with UMIK-1 → "Corrected_by_Pixel9.mdat"

**Compare in REW:**
- Are final corrected responses similar? (target: ±1 dB)
- Do both achieve target curve?
- Which phone produces better final result?

**This tells you:** Can different phones achieve same correction quality?

#### Test 2.3: Blind Listening (Pixel 9 vs Pixel 10 Pro)

**Setup:**
1. Apply Pixel 10 Pro calibration → listen
2. Apply Pixel 9 calibration → listen
3. Can you hear a difference?
4. Which sounds better? (or same?)

**Expected result:**
- Should sound very similar (both Pixel devices)
- If one is clearly better, investigate why (check REW measurements)

---

### Phase 3: Week 3-4 - iOS Validation (iPhone/iPad)

**When parents arrive with iOS devices:**

#### Test 3.1: iOS vs Android Comparison

**Primary question:** Does calibrate.html work correctly on iOS Safari?

**Known iOS Safari differences:**
- Different Web Audio API implementation
- Potential microphone access restrictions
- May handle requestAnimationFrame differently
- ScriptProcessorNode behavior may vary

**Test procedure:**
1. Open calibrate.html on iPhone in Safari
2. Grant microphone permission
3. Run calibration in same location as Pixel tests
4. Compare frequency response vs Pixel measurements

**Check for:**
- ✅ UI loads correctly (no JavaScript errors)
- ✅ Microphone access works
- ✅ Sweep playback is clean (no distortion)
- ✅ FFT analysis completes successfully
- ✅ Frequency response looks reasonable

**UMIK-1 validation:**
- Measure with iPhone → save "iPhone_Room1"
- Compare to Pixel 10 Pro measurement → "Pixel10_Room1"
- Compare to UMIK-1 reference → "UMIK_Room1_Reference.mdat"

**Expected accuracy:**
- iPhone vs UMIK-1: ±2 dB (iOS mics are typically excellent)
- iPhone vs Pixel 10 Pro: ±2-3 dB (should be comparable)

#### Test 3.2: iPhone Microphone Characterization

**iPhone models have well-documented mic characteristics:**
- Generally flat from 100 Hz - 8 kHz (±2 dB)
- High-pass filter around 80-100 Hz (bass roll-off)
- Slight presence boost around 3-5 kHz (common for phone mics)

**Measure iPhone mic response:**
1. Play pink noise through system
2. Record with iPhone (calibrate.html)
3. Record with UMIK-1 simultaneously
4. Compare frequency responses in REW

**Document:**
- Roll-off below 100 Hz (expected)
- Any peaks in midrange/treble
- Overall accuracy vs reference
- Compare to Pixel 10 Pro mic response

**This data enables:** Phone-specific calibration curves (future enhancement)

#### Test 3.3: iPad Testing

**iPad is interesting because:**
- Different microphone position (landscape/portrait)
- Potentially different mic capsule than iPhone
- Larger form factor (different handling)

**Test procedure:**
1. Run calibration with iPad
2. Compare to iPhone from same family (if same generation)
3. Compare to Pixel measurements

**Expected result:**
- iPad should be similar to iPhone (if same generation)
- May have slightly different mic characteristics
- Should still produce valid calibration

#### Test 3.4: iOS Safari Compatibility Issues

**Test for known Safari issues:**

1. **ScriptProcessorNode deprecation:**
   - Does it work in current iOS Safari? (should, but deprecated)
   - Check browser console for warnings
   - Document iOS version tested

2. **Memory leaks:**
   - Run calibration 5 times in a row on iPhone
   - Monitor Safari memory usage (Settings → Safari → Advanced → Web Inspector)
   - Does it crash or slow down?

3. **Background tab handling:**
   - Start calibration
   - Switch to another tab mid-sweep
   - Does calibration continue or fail?
   - Should prevent tab switching during calibration

**Document issues for future AudioWorklet migration**

---

## Multi-Device Test Matrix

| Device | Android/iOS | Priority | Week | Key Tests |
|--------|-------------|----------|------|-----------|
| Pixel 10 Pro | Android 15 | Primary | 1 | Full validation vs UMIK-1, baseline metrics |
| Pixel 9 | Android 14 | High | 1 | Android consistency, cross-calibration |
| iPhone (parent) | iOS 17+ | High | 3-4 | iOS Safari compatibility, mic accuracy |
| iPad (parent) | iOS 17+ | Medium | 3-4 | Form factor test, Safari validation |

---

## Week 1: Android Device Testing Plan

### Day 1 (Boards arrive)
- Primary device: **Pixel 10 Pro**
- Complete initial calibration and validation
- Establish baseline performance

### Day 3-4 (UMIK-1 arrives)
- Validate Pixel 10 Pro accuracy vs UMIK-1
- **Add Pixel 9 testing:**
  - Side-by-side comparison (same room, same time)
  - Cross-calibration test
  - Blind A/B listening
- Document any differences

### Day 5-7
- Continue with Pixel 10 Pro as primary
- Use Pixel 9 for second room testing
- Compare: Do different devices work better in different rooms?

---

## Week 3-4: iOS Device Testing Plan

### When Parents Arrive

**Day 1 (iOS introduction):**
- [ ] Test calibrate.html on iPhone (Safari)
- [ ] Verify microphone permission flow
- [ ] Run first iPhone calibration
- [ ] Compare to Pixel 10 Pro baseline

**Day 2 (iOS validation):**
- [ ] iPhone vs UMIK-1 accuracy test
- [ ] iPhone microphone characterization (pink noise test)
- [ ] Cross-platform comparison (iOS vs Android)
- [ ] iPad testing (if available)

**Day 3 (Safari compatibility):**
- [ ] Test for Safari-specific issues
- [ ] Memory leak testing
- [ ] Background tab handling
- [ ] Document iOS version and Safari version

**Day 4 (Real-world scenario):**
- [ ] Have parent run calibration (without your help)
- [ ] Observe user experience issues
- [ ] Document confusion points
- [ ] Test "typical user" success rate

---

## Data Organization (Multi-Device)

```
tests/
├── pixel10_pro/
│   ├── day1_initial/
│   ├── day4_umik_validation/
│   ├── day5_before_after/
│   └── microphone_characterization.mdat
├── pixel9/
│   ├── day3_side_by_side/
│   │   ├── pixel9_room1.json
│   │   ├── pixel10_room1.json (copy for comparison)
│   │   └── comparison_notes.txt
│   ├── day4_cross_calibration/
│   └── microphone_characterization.mdat
├── iphone/
│   ├── week3_initial/
│   ├── week3_umik_validation/
│   ├── week3_safari_testing/
│   └── microphone_characterization.mdat
├── ipad/
│   └── week3_validation/
└── device_comparison/
    ├── all_devices_room1.csv
    ├── accuracy_matrix.xlsx
    └── recommendation_by_device.md
```

---

## Key Questions to Answer

### Week 1 (Android)
1. **How accurate is Pixel 10 Pro?** → Compare to UMIK-1 (target: ±2 dB)
2. **Is Pixel 9 as accurate?** → Side-by-side test (target: similar)
3. **Do different Pixels produce same final result?** → Cross-calibration test
4. **Can we trust consumer Android devices?** → If Pixels work, most flagships should

### Week 3-4 (iOS)
1. **Does calibrate.html work on iOS Safari?** → Compatibility test
2. **Are iPhones more accurate than Android?** → Compare to UMIK-1
3. **Should we recommend iPhone over Android?** → Accuracy comparison
4. **What's the "minimum viable phone"?** → Document oldest/lowest-spec device that works

---

## Expected Results & Interpretation

### Scenario 1: All Devices Similar (±2 dB)
**Great news!**
- Algorithm is robust across platforms
- Can support universal phone calibration
- You've exceeded Sonos TruePlay (iOS-only)

**Next steps:**
- Test older/cheaper phones (mid-range Android)
- Document minimum device requirements
- Consider public beta

### Scenario 2: iOS More Accurate (±1.5 dB) vs Android (±3 dB)
**Still good, but nuanced:**
- May need platform-specific tuning
- Consider showing accuracy estimate in UI
- Document recommended devices

**Next steps:**
- Investigate Android mic variation causes
- Consider per-device calibration curves
- May need to limit Android support to flagships

### Scenario 3: Large Variations (>±3 dB between devices)
**Needs work:**
- Algorithm may need phone-specific tuning
- Web Audio API may be unreliable
- Consider requiring mic calibration step

**Next steps:**
- Deep-dive into FFT implementation
- Check for browser-specific bugs
- May need to add mic calibration database (like Sonarworks)

---

## Advanced: Phone Microphone Database

**If you find systematic device differences**, consider creating calibration curves:

### Calibration Curve Format
```json
{
  "device": "Google Pixel 10 Pro",
  "calibration": [
    {"frequency": 100, "correction_dB": 0.0},
    {"frequency": 200, "correction_dB": +0.5},
    {"frequency": 500, "correction_dB": +1.0},
    {"frequency": 1000, "correction_dB": 0.0},
    {"frequency": 3000, "correction_dB": +2.0},
    {"frequency": 8000, "correction_dB": -1.5}
  ]
}
```

**Auto-detect device:**
```javascript
// In calibrate.html
const userAgent = navigator.userAgent;
if (userAgent.includes('Pixel 10')) {
  applyMicCalibration('pixel10_pro');
}
```

**This is how Sonarworks Reference handles headphone/speaker calibration**

---

## Parent Involvement (Real User Testing)

**When parents visit, leverage them for UX testing:**

### Test 1: Unassisted Calibration
1. Give parent the URL (no other instructions)
2. Observe them complete calibration
3. Note where they get confused
4. Time how long it takes
5. Ask: "Was that easy or hard?"

**Document:**
- Did they grant mic permission correctly?
- Did they understand phone placement?
- Did they wait for sweep to finish?
- Did they apply correction successfully?

### Test 2: Perceived Improvement
1. Parent listens before calibration
2. You run calibration while they're out of room
3. Parent listens after calibration
4. Ask: "Do you notice any difference?"
5. If yes: "Better or worse?"

**This is valuable because:**
- Parents aren't audiophiles (typical users)
- Unbiased (don't know what to listen for)
- If they notice improvement, algorithm is working well

### Test 3: Repeatability (Multiple Family Members)
1. Have each family member run calibration independently
2. Compare generated profiles
3. Are they similar?
4. Which produces best final result?

**This simulates:** Multiple users calibrating same system

---

## Success Criteria (Multi-Device)

### Minimum Viable Product
- ✅ Works on both Android and iOS
- ✅ All tested devices within ±3 dB of UMIK-1
- ✅ Cross-calibration test: devices produce similar final results (±2 dB)
- ✅ No browser crashes or errors
- ✅ Parents can complete calibration unassisted

### TruePlay-Level (Cross-Platform!)
- ✅ Flagship devices (Pixel 10 Pro, iPhone 15) within ±1.5 dB of UMIK-1
- ✅ Mid-range devices (Pixel 9) within ±2 dB of UMIK-1
- ✅ Cross-calibration test: devices produce nearly identical results (±1 dB)
- ✅ Works reliably across Safari and Chrome
- ✅ >90% of test users complete calibration successfully
- ✅ **Exceeds Sonos TruePlay**: Works on Android AND iOS

---

## Documentation Deliverable

**Create device compatibility matrix after testing:**

| Device | Accuracy vs UMIK-1 | Repeatability | Recommended? | Notes |
|--------|-------------------|---------------|--------------|-------|
| Pixel 10 Pro | ±1.5 dB | ±1.0 dB | ✅ Excellent | Primary test device |
| Pixel 9 | ±2.0 dB | ±1.2 dB | ✅ Good | Very close to Pixel 10 |
| iPhone 15 Pro | ±1.3 dB | ±0.8 dB | ✅ Excellent | Best tested device |
| iPad Pro | ±1.8 dB | ±1.0 dB | ✅ Good | Landscape orientation best |

**User-facing guidance:**
- "Works best with iPhone 12 or newer"
- "Android: Flagship devices from 2022+ recommended"
- "Older devices may work but with reduced accuracy"

---

## Timeline Addition

**Updated timeline with multi-device testing:**

- **Week 1, Day 1**: Pixel 10 Pro baseline
- **Week 1, Day 3-4**: Add Pixel 9 comparison
- **Week 1, Day 5-7**: Both Pixels in different rooms
- **Week 3-4, Day 1**: iOS introduction (iPhone)
- **Week 3-4, Day 2**: iOS validation vs UMIK-1
- **Week 3-4, Day 3**: Safari compatibility testing
- **Week 3-4, Day 4**: Parent user testing (UX)

**Total testing coverage:**
- 2 Android devices (same manufacturer)
- 2 iOS devices (iPhone + iPad)
- Multiple rooms (2 boards)
- Multiple users (you, wife, parents)
- Ground truth validation (UMIK-1)

This is **professional-grade testing** 🎯
