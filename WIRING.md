# Wiring Diagram - ESP32-C6 E-Paper Photo Frame

## Overview

This document provides detailed wiring instructions for connecting the Waveshare 13.3" e-Paper display to the SparkFun ESP32-C6 Thing Plus.

## Pin Summary

### E-Paper Display → ESP32-C6

```
┌─────────────────────────────────────────────────┐
│         Waveshare 13.3" e-Paper HAT+ (E)        │
│                                                  │
│  VCC  GND  DIN  CLK  CS_M CS_S  DC  RST  BUSY   │
└───┬───┬────┬────┬────┬────┬────┬───┬────┬──────┘
    │   │    │    │    │    │    │   │    │
    │   │    │    │    │    │    │   │    │
┌───┴───┴────┴────┴────┴────┴────┴───┴────┴──────┐
│  3V3 GND   3   9   10    5    4   2    1   11  │
│                                                  │
│         SparkFun ESP32-C6 Thing Plus            │
└─────────────────────────────────────────────────┘
```

## Detailed Pin Connections

| # | E-Paper Pin | Wire Color* | ESP32-C6 Pin | Function |
|---|-------------|-------------|--------------|----------|
| 1 | VCC         | Red         | 3V3          | Power (+3.3V) |
| 2 | GND         | Black       | GND          | Ground |
| 3 | DIN (MOSI)  | Orange      | GPIO 3       | Data In |
| 4 | CLK (SCK)   | Yellow      | GPIO 9       | SPI Clock |
| 5 | CS_M        | Green       | GPIO 10      | Chip Select (Master IC) |
| 6 | CS_S        | Blue        | GPIO 5       | Chip Select (Slave IC) |
| 7 | DC          | Purple      | GPIO 4       | Data/Command |
| 8 | RST         | Gray        | GPIO 2       | Reset |
| 9 | BUSY        | White       | GPIO 1       | Busy Status |
|10 | PWR         | Pink        | GPIO 11      | Display Power Enable |

*Suggested wire colors for easy identification

## Visual Connection Diagram

```
╔═══════════════════════════════════════════════════════════════╗
║                  ESP32-C6 THING PLUS (TOP VIEW)               ║
╠═══════════════════════════════════════════════════════════════╣
║                                                               ║
║  USB-C                                                        ║
║  ┌──┐                                                         ║
║  └──┘                                                         ║
║                                                               ║
║  [Reset]  [Boot]                    [BAT]  [PWR]  [RGB]      ║
║                                                               ║
║   Left Header              Center              Right Header   ║
║   ┌───────┐                                    ┌───────┐     ║
║   │ 3V3 ●─┼──── VCC (Red)                      │       │     ║
║   │ GND ●─┼──── GND (Black)                    │       │     ║
║   │  23 ○ │                                    │   0 ○ │     ║
║   │  22 ○ │                                    │   1 ●─┼──── BUSY (White)
║   │  21 ○ │ [SD Card Slot]                     │   2 ●─┼──── RST (Gray)
║   │  20 ○ │                                    │   3 ●─┼──── DIN (Orange)
║   │  19 ○ │                                    │   4 ●─┼──── DC (Purple)
║   │  18 ○ │                                    │   5 ●─┼──── CS_S (Blue)
║   │   A ○ │                                    │   6 ○ │     ║
║   │  15 ○ │                                    │   7 ○ │     ║
║   │  14 ○ │                                    │   8 ○ │     ║
║   │  13 ○ │                                    │   9 ●─┼──── CLK (Yellow)
║   │  12 ○ │                                    │  10 ●─┼──── CS_M (Green)
║   │  11 ○ │                                    │  11 ●─┼──── PWR (Pink)
║   │ USB ○ │                                    │  12 ○ │     ║
║   │ BAT ○ │                                    │  13 ○ │     ║
║   └───────┘                                    └───────┘     ║
║                                                               ║
╚═══════════════════════════════════════════════════════════════╝

● = Connected    ○ = Not used
```

## Physical Layout

### Step-by-Step Wiring

1. **Power Connections First**
   ```
   ESP32-C6 3V3 → E-Paper VCC (Red wire)
   ESP32-C6 GND → E-Paper GND (Black wire)
   ```

2. **SPI Data Lines**
   ```
   ESP32-C6 GPIO 3 → E-Paper DIN/MOSI (Orange wire)
   ESP32-C6 GPIO 9  → E-Paper CLK/SCK  (Yellow wire)
   ```

3. **Chip Select Pins** (Critical for dual-IC display!)
   ```
   ESP32-C6 GPIO 10 → E-Paper CS_M (Green wire)
   ESP32-C6 GPIO 5 → E-Paper CS_S (Blue wire)
   ```

4. **Control Signals**
   ```
   ESP32-C6 GPIO 4 → E-Paper DC   (Purple wire)
   ESP32-C6 GPIO 2 → E-Paper RST  (Gray wire)
   ESP32-C6 GPIO 1 → E-Paper BUSY (White wire)
   ESP32-C6 GPIO 11 → E-Paper PWR (Pink wire)
   ```

## Connection Tips

### Wire Length
- Keep wires as short as practical (< 15cm recommended)
- Longer wires can cause signal integrity issues
- Use twisted pairs for noisy environments

### Wire Gauge
- 22-26 AWG solid or stranded wire works well
- Thicker wire (22 AWG) better for power lines
- Thinner wire (26 AWG) fine for signal lines

### Connectors
- Female-to-male jumper wires are easiest
- Consider soldering for permanent installation
- Use header pins for easy disconnect

### Testing Continuity
Before powering on, verify connections with multimeter:
```bash
Continuity Test:
1. ESP32 GND ←→ E-Paper GND  ✓
2. ESP32 GPIO3 ←→ E-Paper DIN ✓
3. No shorts between adjacent pins ✓
```

## Common Wiring Mistakes

### ❌ Wrong CS Connections
```
WRONG:  CS_M and CS_S connected to same pin
RIGHT:  CS_M → GPIO 10, CS_S → GPIO 5
```
The 13.3" display has TWO ICs (Master and Slave), each needs its own CS pin!

### ❌ Swapped MOSI/MISO
```
WRONG:  DIN connected to MISO pin
RIGHT:  DIN connected to MOSI (GPIO 3)
```
E-paper only receives data, so only MOSI is used (no MISO connection needed)

### ❌ Missing BUSY Pin
```
WRONG:  BUSY pin not connected
RIGHT:  BUSY → GPIO 1
```
The BUSY pin is essential - code will hang without it!

### ❌ Wrong Power Voltage
```
WRONG:  5V power to e-paper
RIGHT:  3.3V power only!
```
E-paper operates at 3.3V. The HAT has voltage translation, but use 3.3V for safety.

## SD Card Connections

The SD card slot is onboard the ESP32-C6 Thing Plus - **no wiring needed!**

### SD Card Pinout (For Reference)
```
Internal connections:
GPIO 21 → SD_MISO
GPIO 20 → SD_MOSI  
GPIO 19 → SD_SCK
GPIO 18 → SD_CS
GPIO 22 → SD_DET (Card Detect)
```

## Alternative Pin Configurations

If you need to use different pins (e.g., GPIO 8/9 are boot strapping pins):

### Alternative Configuration
```cpp
// Change these in the code:
#define EPD_MOSI    12
#define EPD_SCK     13
#define EPD_CS_M    14
#define EPD_CS_S    16
#define EPD_DC      17
#define EPD_RST     10
#define EPD_BUSY    0
#define EPD_PWR     11
```

## Power Considerations

### Current Requirements
```
Display refresh: ~500mA peak (20-30 seconds)
Idle (WiFi on):  ~50mA
Deep sleep:      <1mA
```

### Power Supply Options
1. **USB-C**: Most convenient, 500mA+ available
2. **LiPo Battery**: Use onboard JST connector, 1000mAh+ recommended
3. **External 3.3V**: Connect to 3V3 pin, bypass regulator

### Adding Power Capacitor (Recommended)
For stable operation during display updates:
```
Add 100-470µF capacitor:
  Positive → 3V3
  Negative → GND
Place near ESP32-C6 module
```

## Breadboard Layout (Optional)

If prototyping on breadboard:

```
  +3.3V Rail                                   GND Rail
      ║                                            ║
  ┌───╨────────────────────────────────────────╨───┐
  │   ESP32-C6                                      │
  │   ┌──────────┐                                  │
  │   │          │                                  │
  │   │  Module  │           Jumper                │
  │   │          │           Wires                 │
  │   └──────────┘             │                   │
  │        ║                   │                   │
  │     Headers                ▼                   │
  │   ┌──────────┐      ┌──────────┐              │
  │   │ ● ● ● ●  │      │ Display  │              │
  │   │ ● ● ● ●  │──────│ Driver   │              │
  │   │ ● ● ● ●  │      │  HAT     │              │
  │   └──────────┘      └──────────┘              │
  └─────────────────────────────────────────────────┘
```

## Testing the Wiring

### Basic Connection Test
```cpp
void setup() {
    Serial.begin(115200);
    
    // Test each pin
    pinMode(EPD_CS_M, OUTPUT);
    pinMode(EPD_CS_S, OUTPUT);
    pinMode(EPD_DC, OUTPUT);
    pinMode(EPD_RST, OUTPUT);
    pinMode(EPD_BUSY, INPUT);
    
    digitalWrite(EPD_CS_M, HIGH);
    digitalWrite(EPD_CS_S, HIGH);
    digitalWrite(EPD_DC, LOW);
    digitalWrite(EPD_RST, HIGH);
    
    Serial.println("Pin test:");
    Serial.print("BUSY: ");
    Serial.println(digitalRead(EPD_BUSY)); // Should print 0 or 1
    
    // Toggle pins
    digitalWrite(EPD_CS_M, LOW);
    delay(100);
    digitalWrite(EPD_CS_M, HIGH);
    
    Serial.println("Pin test complete");
}
```

### Visual Inspection Checklist
- [ ] All 9 wires connected
- [ ] No loose connections
- [ ] No crossed wires
- [ ] Power wires (red/black) correct polarity
- [ ] No exposed wire causing shorts
- [ ] Wires away from moving parts
- [ ] SD card inserted properly

## Final Assembly Tips

1. **Test Before Enclosure**
   - Verify all functions work on bench first
   - Upload test patterns and verify display

2. **Strain Relief**
   - Hot glue wire bundle near connectors
   - Use cable ties for organization

3. **Labeling**
   - Label wires with tape and marker
   - Take photos for future reference

4. **Enclosure Considerations**
   - Allow ventilation for ESP32-C6
   - Cutout for USB-C programming port
   - Access to SD card slot
   - Space for optional battery

## Schematic Summary

```
              ┌─────────────────────┐
              │   E-Paper Display   │
              │    13.3" Spectra 6  │
              │                     │
  VCC (3.3V) ─┤ VCC            BUSY├─ BUSY (GPIO 1)
         GND ─┤ GND             RST├─ RST  (GPIO 2)
 MOSI (G3)  ─┤ DIN              DC├─ DC   (GPIO 4)
  SCK (G9)  ─┤ CLK            CS_S├─ CS_S (GPIO 5)
 CS_M (G10) ─┤ CS_M            PWR├─ PWR (GPIO 11)
              └─────────────────────┘
                      ║
              ┌───────╨─────────┐
              │   ESP32-C6      │
              │   Thing Plus    │
              │                 │
              │  [SD Card Slot] │
              │  GPIO 18-22     │
              └─────────────────┘
```

## Questions?

Refer to:
- **README.md** - Complete setup instructions
- **QUICKSTART.md** - Fast setup guide
- **TROUBLESHOOTING.md** - Common issues and fixes

---

**Double-check all connections before powering on!**
