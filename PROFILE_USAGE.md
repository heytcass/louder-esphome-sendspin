# Calibration Profile System - Usage Guide

The TAS5805M room correction system now supports persistent calibration profiles that can be saved, loaded, and automatically applied on boot.

## Features

- **Save/Load Profiles**: Store multiple calibration configurations
- **Auto-Load on Boot**: Set a profile to automatically load when ESP32 starts
- **Up to 5 Profiles**: Store different calibrations for different rooms or speaker positions
- **NVS Storage**: Profiles stored in ESP32 non-volatile storage (survives power cycles)
- **Profile Management**: List, delete, and switch between profiles

## Using the Web UI (calibrate.html)

### Saving a Profile

1. Complete the room measurement and apply correction filters
2. In **Step 5: Save Profile**, enter a descriptive name (e.g., "Living Room", "Sweet Spot")
3. Click **Save Profile**
4. Optionally, click **Set as Active Profile** to make it load automatically on boot

### Loading a Saved Profile

1. Select a profile from the dropdown menu in Step 5
2. Click **Load Profile**
3. The filters will be immediately applied to the TAS5805M

### Deleting a Profile

1. Select the profile from the dropdown
2. Click **Delete Profile**
3. Confirm the deletion (this cannot be undone)

## Using Home Assistant Services

### Save Current Configuration as a Profile

```yaml
service: esphome.louder_s3_kitchen_save_profile
data:
  profile_name: "My Living Room"
```

This saves whatever filters are currently loaded in the TAS5805M.

### Load a Profile

```yaml
service: esphome.louder_s3_kitchen_load_profile
data:
  profile_name: "My Living Room"
```

This loads the profile and immediately applies all filters to the hardware.

### Set Active Profile (Auto-Load on Boot)

```yaml
service: esphome.louder_s3_kitchen_set_active_profile
data:
  profile_name: "My Living Room"
```

After setting an active profile, it will automatically load every time the ESP32 boots.

### Clear Active Profile

```yaml
service: esphome.louder_s3_kitchen_clear_active_profile
```

After clearing, no profile will load on boot (flat response).

### Delete a Profile

```yaml
service: esphome.louder_s3_kitchen_delete_profile
data:
  profile_name: "My Living Room"
```

## Monitoring Profiles

Two text sensors are available in Home Assistant:

- **Active Calibration Profile**: Shows which profile is set to load on boot
- **Available Profiles**: Comma-separated list of all saved profiles

## Example Workflow

### Multi-Room Setup

If you have the same speakers in multiple rooms:

1. **Living Room**:
   - Measure with phone at couch position
   - Apply and save as "Living Room - Couch"
   - Set as active

2. **Bedroom**:
   - Measure with phone at bed position
   - Apply and save as "Bedroom - Bed"
   - Load profile when listening in bedroom

3. **Kitchen**:
   - Measure at breakfast bar
   - Apply and save as "Kitchen - Bar"
   - Set as active if this is the primary position

### Seasonal/Furniture Changes

Room acoustics change when you rearrange furniture or add/remove rugs:

1. Save your current calibration as "Summer 2024"
2. Make changes to room
3. Re-measure and save as "Winter 2024"
4. A/B test both profiles to see which sounds better
5. Set the better one as active

## Technical Details

### Profile Storage Format

Each profile contains:
- **Name**: Up to 32 characters
- **30 Biquad Filters**: 15 per channel (left/right)
- **Metadata**: Creation timestamp, filter count
- **Checksum**: CRC32 for data integrity

### Storage Limits

- **Max Profiles**: 5 (configurable in `tas5805m_profile_manager.h`)
- **Storage per Profile**: ~1.2 KB
- **Total NVS Usage**: ~6 KB for profiles

### How Shadow State Works

The system maintains a "shadow state" (`current_profile_shadow`) that tracks the current biquad configuration. Whenever you:

- Call `set_biquad`
- Call `set_parametric_eq`
- Call any filter service
- Call `load_profile`

...the shadow state is updated. When you save a profile, it saves this shadow state to NVS.

## Troubleshooting

### "Profile not found" Error

- Check the exact profile name (case-sensitive)
- Use the "Available Profiles" sensor to see all saved profiles
- The profile may have been deleted

### Profile Won't Load on Boot

- Verify it's set as active: check "Active Calibration Profile" sensor
- Check ESPHome logs for I2C errors during boot
- Try manually loading the profile first to verify it works

### Running Out of Profile Slots

- Delete old/unused profiles
- Or increase `MAX_PROFILES` in `tas5805m_profile_manager.h` and reflash
  - Note: More profiles = more NVS usage

### Profile Sounds Different Than Expected

- The shadow state may not match what you intended
- Manually apply the filters you want, then save as a new profile
- Check ESPHome logs to see what coefficients are being saved

## Advanced: Manual Profile Creation

You can create a profile programmatically:

```yaml
script:
  - id: create_custom_profile
    then:
      # Set specific filters
      - service: esphome.louder_s3_kitchen_set_parametric_eq
        data:
          channel: 2
          index: 0
          frequency: 80
          gain_db: -6
          q: 2

      - service: esphome.louder_s3_kitchen_set_highpass
        data:
          channel: 2
          index: 1
          frequency: 30
          q: 0.707

      # Save as profile
      - service: esphome.louder_s3_kitchen_save_profile
        data:
          profile_name: "Custom Bass Cut"

      # Set as active
      - service: esphome.louder_s3_kitchen_set_active_profile
        data:
          profile_name: "Custom Bass Cut"
```

## Profile Migration

To backup profiles:
1. Profiles are stored in ESP32 NVS (partition-based)
2. Use ESPHome's backup feature or manually export via services
3. To migrate to a new device, you'll need to recreate profiles

*Note: Future versions may add export/import functionality via JSON.*
