# Chamber Master - Klipper Integration Guide

> **Hardware Tested & Verified**: Contributed by [Richard Kennett](https://github.com/richard-kennett) ([GitHub Issue #3](https://github.com/jayanttyson/Chamber-Master/issues/3#issuecomment-5619889047)).

This guide explains how to connect your **Chamber Master** enclosure controller to **Klipper 3D printer firmware**, allowing your slicer or filament profiles to automatically set enclosure modes and custom target temperatures, or trigger adaptive cooldown when a print finishes.

---

## Prerequisites

1. **mDNS Resolution on Klipper Host (Raspberry Pi / Linux):**
   Linux hosts often fail to resolve `.local` domains in subshells without mDNS packages installed. Run:
   ```bash
   sudo apt update
   sudo apt install -y mdns avahi-daemon
   ```

2. **Gcode Shell Command Extension:**
   Ensure `gcode_shell_command` is installed in Klipper (e.g. via **KIAUH** -> *Advanced* -> *G-Code Shell Command*).

---

## Installation Steps

### Step 1: Copy the Trigger Script
Copy [`chamber_trigger.sh`](chamber_trigger.sh) to your home directory (`$HOME`):
```bash
cp chamber_trigger.sh ~/chamber_trigger.sh
chmod +x ~/chamber_trigger.sh
```

> **Why this script is needed:**
> Linux curl subshells can hang or fail resolving `.local` mDNS domains. The script performs a fast IPv4 ping lookup (`ping -4 -c 1 enclosure-monitor.local`) to grab the IP address directly from the network cache before issuing the HTTP request to Chamber Master.

### Step 2: Include Configuration in Klipper
Copy [`chamber_master.cfg`](chamber_master.cfg) to your Klipper config directory (usually `~/printer_data/config/`):
```bash
cp chamber_master.cfg ~/printer_data/config/
```

Add the following line to your `printer.cfg`:
```ini
[include chamber_master.cfg]
```

Or copy the macros from [`chamber_master.cfg`](chamber_master.cfg) directly into your `printer.cfg`.

Restart Klipper (`FIRMWARE_RESTART`).

---

## Macros Reference

| Macro | Parameters | Example | Description |
| :--- | :--- | :--- | :--- |
| `SET_CHAMBER` | `MATERIAL` *(string)*, `TEMPERATURE` *(int/float, optional)* | `SET_CHAMBER MATERIAL=ABS` | Sets enclosure preset mode (`PLA`, `ASA`, `ABS`, `TPU`, `PETG`) |
| `SET_CHAMBER` | `MATERIAL=CUSTOM`, `TEMPERATURE=<target>` | `SET_CHAMBER MATERIAL=CUSTOM TEMPERATURE=55` | Sets custom target enclosure temperature |
| `SET_CHAMBER` | `MATERIAL=COOLDOWN` | `SET_CHAMBER MATERIAL=COOLDOWN` | Initiates active adaptive cooldown routine |
| `START_CHAMBER_COOLDOWN` | *(none)* | `START_CHAMBER_COOLDOWN` | Convenience alias for cooldown |

---

## Slicer Configuration

### Filament Start G-Code
In OrcaSlicer, PrusaSlicer, or Bambu Studio, add to your **Filament Start G-Code**:
```gcode
; Set Chamber Master mode based on active filament
SET_CHAMBER MATERIAL=[filament_type]
```
For filaments requiring a specific custom temperature:
```gcode
SET_CHAMBER MATERIAL=CUSTOM TEMPERATURE=55
```

### Machine End G-Code
In your slicer's **Machine End G-Code** or `PRINT_END` macro:
```gcode
; Start adaptive enclosure cooldown routine
START_CHAMBER_COOLDOWN
```

---

## Testing & Verification

You can test the trigger script directly in your terminal:
```bash
# Test material preset
bash ~/chamber_trigger.sh enclosure-monitor.local PETG

# Test custom temperature
bash ~/chamber_trigger.sh enclosure-monitor.local CUSTOM 60

# Test cooldown trigger
bash ~/chamber_trigger.sh enclosure-monitor.local COOLDOWN
```

Or from the Klipper console in Mainsail / Fluidd:
```gcode
SET_CHAMBER MATERIAL=PLA
SET_CHAMBER MATERIAL=CUSTOM TEMPERATURE=45
START_CHAMBER_COOLDOWN
```
