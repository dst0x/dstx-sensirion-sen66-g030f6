# Last Known Good Value Implementation

## Overview

This firmware now implements a **"last known good value"** mechanism to handle transient sensor glitches. If the SEN66 sensor returns a `0` reading for any parameter, the system will automatically substitute it with the last valid non-zero reading before sending it to Modbus clients.

## Problem Solved

Previously, if the sensor momentarily returned `0` for any measurement (due to communication glitches, sensor warm-up, or transient errors), this invalid data would be sent to Modbus clients. This could cause:
- False alarms in monitoring systems
- Incorrect data logging
- Confusion about actual sensor failures vs. transient glitches

## Implementation Details

### Sensor Driver Layer (`drv_sen66.c`)

The implementation consists of three key components:

#### 1. Data Storage Structure
```c
typedef struct {
    Sen66_Data_t  data;              // Current sensor readings
    Sen66_Data_t  last_valid_data;   // Last known-good non-zero reading
    bool          has_valid_data;    // True once at least one valid reading stored
    // ... other fields
} Sen66_Ctx_t;
```

#### 2. Fallback Logic (`apply_last_valid` function)
When sensor data is read in `DRV_SEN66_Poll()`:
- Parse raw sensor data into `ctx->data`
- **If** `ctx->has_valid_data` is true (we have prior good data):
  - For each field (PM1.0, PM2.5, PM4.0, PM10, RH, Temp, VOC, NOx, CO2, NC values):
    - If new value is `0` **AND** last valid value was non-zero
    - Replace new value with last valid value
- After substitution, update `last_valid_data` with current readings (only if non-zero)

#### 3. Validation Logic
The system only updates `last_valid_data` when:
- At least one sensor category (PM, environmental, gas, or CO2) has valid flags set
- **AND** the values are non-zero

This prevents storing all-zero readings as "valid" data.

### Modbus Layer (`mb_slave.c`)

The Modbus layer simply reads from `sensor_ctx->data`, which already contains the substituted values. No changes needed here—the fallback is transparent to Modbus clients.

```c
static void build_register_map(const Sen66_Ctx_t *s, uint16_t *regs)
{
    const Sen66_Data_t *d = &s->data;  // Already contains fallback values
    regs[0x00U] = d->pm1_0;
    regs[0x01U] = d->pm2_5;
    // ... etc
}
```

## Behavior Examples

### Scenario 1: Normal Operation
| Time | Sensor Reading | Last Valid | Sent to Modbus |
|------|---------------|------------|----------------|
| T1   | PM2.5 = 25    | (none)     | 25             |
| T2   | PM2.5 = 28    | 25         | 28             |
| T3   | PM2.5 = 30    | 28         | 30             |

### Scenario 2: Transient Glitch
| Time | Sensor Reading | Last Valid | Sent to Modbus |
|------|---------------|------------|----------------|
| T1   | PM2.5 = 25    | (none)     | 25             |
| T2   | PM2.5 = 0     | 25         | **25** (substituted) |
| T3   | PM2.5 = 28    | 25         | 28             |
| T4   | PM2.5 = 30    | 28         | 30             |

### Scenario 3: Startup (No Prior Data)
| Time | Sensor Reading | Last Valid | Sent to Modbus |
|------|---------------|------------|----------------|
| T1   | PM2.5 = 0     | (none)     | 0              |
| T2   | PM2.5 = 15    | (none)     | 15             |
| T3   | PM2.5 = 0     | 15         | **15** (substituted) |

### Scenario 4: Persistent Failure
If the sensor continuously returns `0`, the system will:
- Keep using the last valid value indefinitely
- The error counter will eventually trigger `SEN66_STATE_ERROR`
- LED will blink rapidly (100ms) to indicate error state
- Debug logs will show "SEN66 offline"
- Modbus still reports last known good values with error status flag

## Configuration

No configuration is needed—the mechanism is always active. To disable it, you would need to comment out the `apply_last_valid()` call in `drv_sen66.c`:

```c
// apply_last_valid(&ctx->data, &ctx->last_valid_data);  // Disable fallback
```

## Status Monitoring

Modbus register `0x0E` contains status flags:
- Bit 0: PM measurements valid
- Bit 1: Environmental (RH/Temp) valid
- Bit 2: Gas (VOC/NOx) valid  
- Bit 3: CO2 valid

Register `0x0F` contains sensor state:
- 0 = UNINIT
- 1 = IDLE
- 2 = WARMING_UP
- 3 = RUNNING
- 4 = ERROR

Register `0x10` contains error count.

These status values allow Modbus clients to distinguish between:
- Fresh data (error_count=0, state=RUNNING)
- Fallback data (error_count>0, may be using last known good values)
- Complete failure (state=ERROR)

## Testing Recommendations

1. **Normal Operation Test**: Verify sensor readings update normally
2. **Glitch Simulation**: Temporarily disconnect I2C to simulate communication failure
3. **Recovery Test**: Reconnect I2C and verify system recovers with fresh data
4. **Cold Start Test**: Power cycle and verify system handles initial zeros correctly

## Code Changes Summary

### Modified Files:
- `drivers/drv_sen66.c`: Added `apply_last_valid()` function with complete substitution logic for all sensor fields
- `drivers/drv_sen66.h`: Already had `last_valid_data` and `has_valid_data` fields in structure
- `modbus/mb_slave.c`: Added documentation comment explaining the fallback mechanism

### Build Output:
- Flash usage: 7676 bytes (11.71% of 64KB)
- RAM usage: 2352 bytes (28.71% of 8KB)
- No increase in memory usage (logic optimization)

---

**Last Updated**: 2026-07-12  
**Firmware Version**: v1.2 with Last Known Good Value mechanism
