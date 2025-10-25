# Quick Start Guide - ESP32-C6 E-Paper Photo Frame

Get your photo frame running in 10 minutes!

## What You Need

- ✅ SparkFun ESP32-C6 Thing Plus
- ✅ Waveshare 13.3" e-Paper HAT+ (E)
- ✅ MicroSD card (FAT32 formatted)
- ✅ USB-C cable
- ✅ Jumper wires for connections

## 1. Wire It Up (5 min)

Connect the e-paper display to ESP32-C6:

```
E-Paper → ESP32-C6
-----------------
VCC     → 3.3V
GND     → GND
DIN     → GPIO 10
CLK     → GPIO 9
CS_M    → GPIO 8
CS_S    → GPIO 5
DC      → GPIO 4
RST     → GPIO 3
BUSY    → GPIO 2
```

Insert your microSD card into the onboard slot.

## 2. Upload Code (2 min)

### Arduino IDE
1. Install ESP32 board support: https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html
2. Select Board: **ESP32C6 Dev Module**
3. Open `epaper_photoframe.ino`
4. Click Upload

### PlatformIO
```bash
pio run --target upload
```

## 3. First Boot - WiFi Setup (2 min)

1. Power on the device
2. Display shows: **"Connect to WiFi: EPaper-Setup"**
3. On your phone, connect to WiFi: **EPaper-Setup**
4. Browser opens automatically (or go to `192.168.4.1`)
5. Enter your WiFi credentials
6. Click "Connect"
7. Device restarts and displays its IP address

## 4. Prepare Your Images (5 min)

### Install Python Requirements
```bash
pip install Pillow
```

### Convert Your Photo
```bash
python convert_image.py vacation.jpg vacation.bin
```

This creates:
- `vacation.bin` - Upload this to your photo frame
- `vacation_preview.png` - Preview of how it will look

### Tips for Best Results
- Use high-contrast images
- Black and white photos work great
- Avoid complex gradients
- Test with simple images first

## 5. Upload and Display (1 min)

1. Go to the IP address shown on display (e.g., `http://192.168.1.100`)
2. Click **"Upload Image"**
3. Select your `.bin` file
4. After upload, click the image name to display it
5. Wait 20-30 seconds for display update

## Done! 🎉

Your photo frame is now running! It will:
- ✅ Automatically refresh once per day
- ✅ Remember last image on reboot
- ✅ Reconnect to WiFi automatically
- ✅ Save power by sleeping between updates

## Common Commands

### Convert image with adjustments
```bash
# Increase contrast for better clarity
python convert_image.py photo.jpg photo.bin --contrast 1.5

# Use dithering for gradients
python convert_image.py photo.jpg photo.bin --dither

# Adjust threshold (lower = darker)
python convert_image.py photo.jpg photo.bin --threshold 100
```

### Batch convert folder
```bash
python convert_image.py ./my_photos/ ./converted/ --batch
```

## Troubleshooting

**Display not updating?**
- Check BUSY wire connection
- Verify both CS pins (CS_M and CS_S) are connected
- Check serial monitor for errors (115200 baud)

**Can't connect to WiFi?**
- Make sure you're using 2.4GHz WiFi (not 5GHz)
- Check password is correct
- Try resetting preferences by re-uploading code

**SD card not detected?**
- Format as FAT32
- Try different card
- Check card is fully inserted

**Image looks wrong?**
- Verify file is exactly 240,000 bytes
- Check resolution is 1600×1200
- Try the preview image to see what it should look like

## Advanced Features

### Change Refresh Interval
Edit line 56 in code:
```cpp
const unsigned long REFRESH_INTERVAL = 24UL * 60UL * 60UL * 1000UL; // 24 hours
```

### Reset WiFi Settings
1. Upload code again, or
2. Clear preferences in code:
```cpp
prefs.begin("photoframe", false);
prefs.clear();
prefs.end();
```

## Web Interface Features

Access at the device's IP address:

- 📤 **Upload** - Add new images
- 🖼️ **Gallery** - View and select stored images  
- ℹ️ **System Info** - SD card space, WiFi signal, memory
- 🔄 **Refresh List** - Update image list after manual SD card changes

## Power Consumption

- **Updating display**: ~500mA for 20-30 sec
- **WiFi active**: ~50mA
- **Display sleeping**: <0.01mA

For battery operation, the display uses minimal power!

## Next Steps

1. ✅ Upload multiple images
2. ✅ Set up scheduled refreshes
3. ✅ Create a custom enclosure
4. ✅ Add battery for portability
5. ✅ Share with friends!

## Need Help?

Check the full **README.md** for:
- Detailed wiring diagrams
- Complete troubleshooting guide
- Advanced configuration options
- Image preparation best practices

---

**Happy framing!** 📷✨
