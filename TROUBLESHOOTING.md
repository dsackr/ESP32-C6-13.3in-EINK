# Troubleshooting Checklist

Quick diagnostic guide for common issues.

## 🔧 Hardware Check

### Display Not Responding

**Symptoms**: Display shows nothing, or stays blank after commands

**Checklist**:
- [ ] Check all 10 wire connections (VCC, GND, MOSI, SCK, CS_M, CS_S, DC, RST, BUSY, PWR)
- [ ] Verify BUSY pin is connected (GPIO 1) - this is critical!
- [ ] Ensure both CS pins are connected (GPIO 10 and GPIO 5)
- [ ] Check 3.3V power supply can deliver 500mA
- [ ] Verify ground connection is solid
- [ ] Try swapping jumper wires (they can be faulty)
- [ ] Check for shorts between adjacent pins

**Test**: 
```cpp
// Add to setup() to test BUSY pin
pinMode(EPD_BUSY, INPUT);
Serial.println(digitalRead(EPD_BUSY)); // Should toggle during display updates
```

### SD Card Not Detected

**Symptoms**: "SD Card initialization failed!" in serial monitor

**Checklist**:
- [ ] SD card is FAT32 formatted (not exFAT or NTFS)
- [ ] Card is fully inserted into slot
- [ ] Try different SD card (some cards are incompatible)
- [ ] Card is 32GB or smaller (larger may have issues)
- [ ] Card detect pin works (GPIO 22)
- [ ] SPI pins not shorted to ground

**Test Commands**:
```bash
# On computer, verify card format:
# Windows: Right-click drive → Properties
# Mac: diskutil info /dev/disk2
# Linux: sudo fdisk -l
```

## 📡 WiFi Issues

### Can't Connect to Setup Network

**Symptoms**: "EPaper-Setup" network not visible

**Checklist**:
- [ ] Device has power (check PWR LED)
- [ ] Wait 10-15 seconds after power-on
- [ ] Check serial monitor shows "Setting up Access Point"
- [ ] Try scanning for 2.4GHz networks only
- [ ] Restart device and try again
- [ ] Other devices can see the network

**Fix**: Add delay after WiFi.softAP():
```cpp
WiFi.softAP(ssid_ap.c_str());
delay(1000); // Give time to start
```

### Can't Connect to Home WiFi

**Symptoms**: Device creates AP mode after entering credentials

**Checklist**:
- [ ] SSID and password are exactly correct (case-sensitive)
- [ ] WiFi is 2.4GHz (ESP32-C6 doesn't support 5GHz)
- [ ] Router allows new devices (MAC filtering?)
- [ ] Signal strength is adequate (>-75 dBm)
- [ ] No special characters causing issues
- [ ] Router is not in AP isolation mode

**Debug**:
```cpp
// Check connection status
Serial.print("Status: ");
Serial.println(WiFi.status()); // 3 = WL_CONNECTED
Serial.print("RSSI: ");
Serial.println(WiFi.RSSI()); // Should be > -75
```

### Web Interface Not Loading

**Symptoms**: Can't access IP address in browser

**Checklist**:
- [ ] Device and computer on same WiFi network
- [ ] Correct IP address (check serial monitor)
- [ ] Try http://192.168.4.1 in AP mode
- [ ] Firewall not blocking connection
- [ ] Try different browser
- [ ] Ping IP address works: `ping 192.168.1.100`

**Test**: Access from serial monitor IP:
```
WiFi connected!
IP Address: 192.168.1.100  <-- Try this in browser
```

## 🖼️ Image Display Issues

### Image Appears Scrambled

**Symptoms**: Image shows but looks wrong/shifted

**Checklist**:
- [ ] Image file is exactly 240,000 bytes
- [ ] Image is 1600×1200 resolution
- [ ] Used provided conversion script
- [ ] Check both CS pins are working
- [ ] Try test patterns first (checkerboard.bin)
- [ ] Reduce SPI clock speed if data corruption

**Fix**: Test with simple patterns:
```bash
python generate_test_patterns.py
# Upload white.bin, black.bin, checkerboard.bin
```

### Image Too Dark or Light

**Symptoms**: Image displays but contrast is wrong

**Fix**: Adjust conversion parameters:
```bash
# Lighter output
python convert_image.py photo.jpg photo.bin --threshold 150

# Darker output  
python convert_image.py photo.jpg photo.bin --threshold 100

# Increase contrast
python convert_image.py photo.jpg photo.bin --contrast 1.5
```

### Half Screen Works, Half Doesn't

**Symptoms**: Left or right half of display is blank/incorrect

**Checklist**:
- [ ] Both CS pins connected (CS_M = GPIO 10, CS_S = GPIO 5)
- [ ] Check which half is affected:
  - Left half blank → CS_M issue
  - Right half blank → CS_S issue
- [ ] Verify pullup on CS pins
- [ ] Check for shorts on CS pins

**Test**: Try split.bin pattern to verify both ICs

### Display Updates Very Slowly

**Symptoms**: Takes >60 seconds to update

**Checklist**:
- [ ] Normal: 20-30 seconds is expected
- [ ] Check SPI clock speed (4MHz default)
- [ ] Verify BUSY pin connected properly
- [ ] Large images need time to transfer
- [ ] Serial debug output slows down transfer

**Optimize**:
```cpp
epdSPI.setFrequency(8000000); // Try 8MHz if stable
```

## 💾 SD Card Issues

### Upload Fails

**Symptoms**: "Upload failed!" error message

**Checklist**:
- [ ] SD card has free space
- [ ] File isn't too large (>1MB may fail)
- [ ] SD card is writable (not locked)
- [ ] Correct file format (.bin)
- [ ] Try smaller test file first

**Test**: Upload small test pattern first

### Images Not Appearing in List

**Symptoms**: List shows "[]" or missing files

**Checklist**:
- [ ] Files are in /images/ directory
- [ ] Files end with .bin extension
- [ ] Refresh browser page
- [ ] Check SD card manually on computer
- [ ] Files not corrupted (check size)

**Fix**: Check SD card on computer:
```
/images/
  ├── photo1.bin (240,000 bytes)
  ├── photo2.bin (240,000 bytes)
  └── test.bin (240,000 bytes)
```

## ⚡ Power Issues

### Device Keeps Resetting

**Symptoms**: Restarts during display update

**Checklist**:
- [ ] Power supply provides enough current (500mA+)
- [ ] USB cable is good quality
- [ ] Brownout during display refresh
- [ ] Capacitor on power rails might help
- [ ] Try powered USB hub

**Fix**: Add capacitor (100-470µF) across 3.3V and GND

### Display Partially Updates

**Symptoms**: Update starts but doesn't complete

**Checklist**:
- [ ] Power supply stable during full update
- [ ] Check BUSY pin responds
- [ ] Timeout value too short (increase if needed)
- [ ] Display might be damaged

**Debug**:
```cpp
// Increase timeout
unsigned long startTime = millis();
while(digitalRead(EPD_BUSY) == LOW) {
    delay(10);
    if(millis() - startTime > 60000) { // Increase to 60 sec
        Serial.println("Display timeout!");
        break;
    }
}
```

## 🐛 Software Issues

### Code Won't Compile

**Common Errors**:

1. **"WebServer.h not found"**
   - Install ESP32 board support
   - Version 2.0.0 or newer required

2. **"SD.h not found"**  
   - Update ESP32 board package
   - Should be included by default

3. **"Preferences.h not found"**
   - Install latest ESP32 board package

### Upload Fails

**Symptoms**: Can't upload code to board

**Checklist**:
- [ ] Correct board selected: "ESP32C6 Dev Module"
- [ ] Correct port selected
- [ ] Try different USB cable
- [ ] Hold BOOT button during upload
- [ ] Driver installed for USB chip

**Fix**: Press and hold BOOT, click Upload, release BOOT

### Serial Monitor Shows Garbage

**Symptoms**: Random characters in serial monitor

**Fix**: Set baud rate to **115200**

## 🔍 Advanced Debugging

### Enable Verbose Logging

Add to setup():
```cpp
Serial.setDebugOutput(true);
```

### Check Memory Usage

```cpp
Serial.print("Free Heap: ");
Serial.println(ESP.getFreeHeap());
// Should be > 100,000 bytes
```

### Test Display Without Image

```cpp
void setup() {
    epd.begin();
    epd.init();
    epd.clear(); // Should clear to white
    delay(5000);
    // If this works, display hardware is OK
}
```

### Test SD Card Independently

```cpp
void setup() {
    if(SD.begin(SD_CS, sdSPI)) {
        Serial.println("SD OK!");
        File root = SD.open("/");
        File file = root.openNextFile();
        while(file) {
            Serial.println(file.name());
            file = root.openNextFile();
        }
    }
}
```

## 📝 Getting Help

If still having issues:

1. **Check Serial Monitor Output**
   - Connect USB and monitor at 115200 baud
   - Copy all output from startup
   
2. **Verify Hardware**
   - Use multimeter to check voltages
   - Test continuity on all wires
   
3. **Try Test Patterns**
   - Generate and test simple patterns
   - Narrow down which component is failing

4. **Document the Issue**
   - What you tried
   - Serial monitor output
   - Photos of wiring
   - Exact error messages

## ✅ Verification Tests

Run these in order:

1. ✅ **Power Test**: PWR LED lights up
2. ✅ **Serial Test**: Serial monitor shows boot messages
3. ✅ **SD Test**: "SD Card initialized" in serial monitor
4. ✅ **WiFi Test**: Can connect to EPaper-Setup network
5. ✅ **Display Test**: Clear screen command works
6. ✅ **Upload Test**: Can upload file via web interface
7. ✅ **Display Image**: Test pattern displays correctly
8. ✅ **Photo Test**: Real photo displays correctly

Work through each test systematically to identify issues.

---

**Still stuck?** Double-check all wiring, try a minimal test sketch, and verify each component individually.
