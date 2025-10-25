# ESP32-C6 E-Paper Photo Frame

A smart photo frame using the SparkFun ESP32-C6 Thing Plus and Waveshare 13.3" Spectra 6 e-Paper display with WiFi setup, web interface, and SD card storage.

## Features

- ✅ **WiFi Setup Portal**: First-boot captive portal for easy WiFi configuration
- ✅ **Web Interface**: Upload and manage images through a browser
- ✅ **SD Card Storage**: Store multiple images locally
- ✅ **Power Efficient**: Display sleeps between updates
- ✅ **Auto Refresh**: Daily screen refresh to prevent ghosting
- ✅ **Persistent State**: Remembers last displayed image

## Hardware Requirements

- SparkFun Thing Plus - ESP32-C6
- Waveshare 13.3" e-Paper HAT+ (E) - Spectra 6
- MicroSD card (formatted as FAT32, 8GB+ recommended)
- USB-C cable for programming

## Wiring Connections

### E-Paper Display Connections

| E-Paper Pin | ESP32-C6 GPIO | Description |
|-------------|---------------|-------------|
| MOSI/DIN    | GPIO 3        | Data In     |
| SCK/CLK     | GPIO 9        | Clock       |
| CS_M        | GPIO 10       | Chip Select (Master) |
| CS_S        | GPIO 5        | Chip Select (Slave)  |
| DC          | GPIO 4        | Data/Command |
| RST         | GPIO 2        | Reset       |
| BUSY        | GPIO 1        | Busy Status |
| PWR         | GPIO 11       | Display Power Enable |
| VCC         | 3.3V          | Power       |
| GND         | GND           | Ground      |

### SD Card (Onboard - Pre-wired)

The onboard microSD slot uses:
- GPIO 21 (MISO)
- GPIO 20 (MOSI)  
- GPIO 19 (SCK)
- GPIO 18 (CS)
- GPIO 22 (Card Detect)

## Software Setup

### 1. Install Arduino IDE

Download from: https://www.arduino.cc/en/software

### 2. Install ESP32 Board Support

1. Open Arduino IDE
2. Go to **File → Preferences**
3. Add to "Additional Board Manager URLs":
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Go to **Tools → Board → Board Manager**
5. Search for "esp32" and install "esp32 by Espressif Systems"

### 3. Install Required Libraries

Go to **Tools → Manage Libraries** and install:
- None required! All libraries are built-in with ESP32 core

### 4. Board Settings

1. **Board**: "ESP32C6 Dev Module"
2. **USB CDC On Boot**: "Enabled"
3. **Flash Size**: "16MB"
4. **Partition Scheme**: "Default 4MB with spiffs"
5. **PSRAM**: "Disabled"
6. **Upload Speed**: "921600"

### 5. Upload the Code

1. Connect ESP32-C6 via USB-C
2. Select the correct **Port** in Tools menu
3. Click **Upload** (Ctrl+U)

## Image Preparation

The display requires images in a specific raw binary format.

### Image Specifications

- **Resolution**: 1600 × 1200 pixels
- **Format**: Raw binary (.bin)
- **Color Depth**: 1-bit per pixel (black/white)
- **File Size**: 240,000 bytes (1600 × 1200 / 8)
- **Byte Order**: Row-major, MSB first

### Converting Images to .bin Format

#### Using Python Script

Create a file called `convert_image.py`:

```python
#!/usr/bin/env python3
from PIL import Image
import sys

def convert_to_bin(input_path, output_path):
    # Open and convert image
    img = Image.open(input_path)
    
    # Resize to 1600x1200
    img = img.resize((1600, 1200), Image.LANCZOS)
    
    # Convert to grayscale then to black and white
    img = img.convert('L')
    img = img.point(lambda x: 0 if x < 128 else 255, '1')
    
    # Convert to binary format
    width, height = img.size
    output_bytes = bytearray()
    
    for y in range(height):
        for x in range(0, width, 8):
            byte = 0
            for bit in range(8):
                if x + bit < width:
                    pixel = img.getpixel((x + bit, y))
                    if pixel == 0:  # Black pixel
                        byte |= (1 << (7 - bit))
            output_bytes.append(byte)
    
    # Write to file
    with open(output_path, 'wb') as f:
        f.write(output_bytes)
    
    print(f"Converted {input_path} to {output_path}")
    print(f"Output size: {len(output_bytes)} bytes")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python convert_image.py input.jpg output.bin")
        sys.exit(1)
    
    convert_to_bin(sys.argv[1], sys.argv[2])
```

**Usage:**
```bash
pip install Pillow
python convert_image.py photo.jpg photo.bin
```

#### Using ImageMagick (Command Line)

```bash
# Install ImageMagick
sudo apt-get install imagemagick  # Linux
brew install imagemagick          # macOS

# Convert image
convert input.jpg -resize 1600x1200! -monochrome -depth 1 gray:output.bin
```

### Tips for Best Results

1. **High Contrast**: E-paper displays work best with high-contrast images
2. **Avoid Gradients**: Gradients will dither and may not look great
3. **Black & White Photography**: Works better than color photos
4. **Simple Graphics**: Line art and text display beautifully
5. **Test Small**: Try with a small test image first

## First-Time Setup

### 1. Power On

When you first power on the device:

1. The display will show: "Connect to WiFi: EPaper-Setup"
2. The device creates a WiFi access point

### 2. Connect to WiFi Setup

1. On your phone/computer, connect to WiFi network: **EPaper-Setup**
2. Open a browser and go to: **http://192.168.4.1**
3. Enter your home WiFi credentials
4. Click "Connect"
5. The device will restart and connect to your WiFi

### 3. Upload Images

1. On the web interface, note the IP address displayed
2. On your main WiFi, go to that IP address in a browser
3. Click "Upload Image" and select your .bin file
4. After upload, click on the image name to display it

## Web Interface

Access the web interface at the device's IP address.

### Main Features

1. **WiFi Setup** (only in AP mode)
   - Configure WiFi credentials
   - Automatic restart after saving

2. **Upload Image**
   - Drag and drop or select .bin files
   - Automatic storage to SD card
   - Progress indication

3. **Stored Images**
   - View all images on SD card
   - Click any image to display
   - Current image highlighted in green

4. **System Info**
   - Current displayed image
   - Free memory
   - SD card usage
   - WiFi signal strength
   - IP address

## Operation Modes

### Normal Operation

- Device connects to configured WiFi
- Web server runs on local network
- Display updates when new image selected
- Daily automatic refresh at midnight

### Setup Mode (AP Mode)

Triggers when:
- First boot (no WiFi configured)
- Saved WiFi credentials fail to connect
- Manual reset

In this mode:
- Creates "EPaper-Setup" WiFi network
- Runs captive portal on 192.168.4.1
- Display shows connection instructions

### Recovery Mode

If display becomes unresponsive:
1. Power cycle the device
2. Check serial monitor for errors
3. Verify SD card is properly inserted
4. Check wiring connections

## Power Management

The e-paper display is put into deep sleep after each update to save power:

- **Active Display Update**: ~500mA for 20-30 seconds
- **Deep Sleep**: <0.01mA
- **ESP32 Idle**: ~30-50mA (WiFi on)
- **Daily Refresh**: Automatically at 24-hour intervals

To maximize battery life (if using LiPo):
- Reduce WiFi usage with ESP32 light sleep modes
- Only wake for image updates
- Consider adding deep sleep between updates

## Troubleshooting

### Display Not Updating

1. Check serial monitor for errors
2. Verify display wiring (especially CS_M and CS_S)
3. Ensure BUSY pin is connected
4. Try power cycling the display

### SD Card Not Detected

1. Verify SD card is FAT32 formatted
2. Try a different SD card
3. Check that card is fully inserted
4. Look for card detect pin (GPIO 22) status

### WiFi Connection Fails

1. Check SSID and password are correct
2. Ensure 2.4GHz WiFi (ESP32 doesn't support 5GHz)
3. Check router allows new devices
4. Try erasing preferences: `prefs.clear()`

### Image Displays Incorrectly

1. Verify image is exactly 240,000 bytes
2. Check image conversion script
3. Ensure correct resolution (1600×1200)
4. Try a simpler test image

### Web Interface Not Loading

1. Find IP address from serial monitor
2. Check that device is on same network
3. Try accessing from different browser
4. Disable VPN if active

## Serial Monitor Commands

While connected via USB, monitor at **115200 baud** for:
- Boot messages
- WiFi connection status
- Image upload progress
- Display refresh status
- Error messages

## File Structure on SD Card

```
/
├── images/
│   ├── photo1.bin
│   ├── photo2.bin
│   └── vacation.bin
```

All images are stored in `/images/` directory as .bin files.

## Advanced Configuration

### Changing Daily Refresh Time

Edit this line in the code:
```cpp
const unsigned long REFRESH_INTERVAL = 24UL * 60UL * 60UL * 1000UL; // milliseconds
```

### Changing AP Name

Edit this line:
```cpp
String ssid_ap = "EPaper-Setup";
```

### Adjusting SPI Speed

If display has artifacts, reduce SPI speed:
```cpp
epdSPI.setFrequency(4000000); // Try 2000000 (2MHz) if issues occur
```

## Known Limitations

1. **Memory**: Large image buffering uses significant RAM
2. **Refresh Time**: Full display refresh takes 20-30 seconds
3. **Color**: Display is black/white only (not color E6)
4. **WiFi**: 2.4GHz only (no 5GHz support)
5. **Upload Size**: Web upload limited by ESP32 memory (~1MB)

## Future Enhancements

Possible improvements:
- [ ] OTA firmware updates
- [ ] Slideshow mode
- [ ] Battery level indicator
- [ ] Sleep mode between uploads
- [ ] Image rotation/flip options
- [ ] Direct JPEG support (convert on device)
- [ ] Cloud image sync (Google Photos, etc.)

## Credits

- **Hardware**: SparkFun, Waveshare, Espressif
- **E-Paper Library**: Based on Waveshare examples
- **Web Interface**: Custom HTML/JavaScript

## License

MIT License - Feel free to modify and distribute

## Support

For issues:
1. Check serial monitor output
2. Verify wiring with multimeter
3. Test with simple display patterns
4. Check SD card functionality independently

---

**Version**: 1.0  
**Last Updated**: 2025-10-24  
**Tested On**: ESP32-C6, Arduino IDE 2.3.0
