/*
 * ESP32-C6 E-Paper Photo Frame
 * 13.3" Waveshare Spectra 6 Display with SD Card Storage
 * 
 * Features:
 * - WiFi setup via captive portal on first boot
 * - Web interface for image upload and selection
 * - SD card storage for images
 * - Daily automatic refresh
 * - Power-efficient operation
 * 
 * Hardware:
 * - SparkFun Thing Plus ESP32-C6
 * - Waveshare 13.3" e-Paper HAT+ (E) - Dual IC Display
 * - MicroSD card
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <DNSServer.h>
#include <time.h>
#include <stdarg.h>
#if defined(__has_include)
#if __has_include(<soc/soc_caps.h>)
#include <soc/soc_caps.h>
#endif
#endif

#ifndef SOC_SPI_PERIPH_NUM
#define SOC_SPI_PERIPH_NUM 2
#endif

#if !defined(HSPI)
#define HSPI FSPI
#endif

// ==================== PIN DEFINITIONS ====================
// SD Card pins (onboard - hardware SPI)
#define SD_MISO     21
#define SD_MOSI     20
#define SD_SCK      19
#define SD_CS       18

// E-Paper Display pins (secondary SPI)
#define EPD_MOSI    3
#define EPD_SCK     9
#define EPD_CS_M    10  // Master area chip select
#define EPD_CS_S    5   // Slave area chip select
#define EPD_DC      4   // Data/Command
#define EPD_RST     2   // Reset
#define EPD_BUSY    1   // Busy status
#define EPD_PWR     11  // Display power enable

// Display specifications
#define EPD_WIDTH   1600
#define EPD_HEIGHT  1200
#define EPD_HALF_WIDTH 800  // Each IC controls half the screen

// ==================== GLOBAL OBJECTS ====================
WebServer server(80);
DNSServer dnsServer;
Preferences prefs;
#if SOC_SPI_PERIPH_NUM > 1
// Use the second hardware SPI peripheral for the e-paper so the primary bus
// remains available for the onboard SD card slot on the ESP32-C6 Thing Plus.
SPIClass epdSPI(HSPI);
#else
// Some targets only expose a single general purpose SPI peripheral.
SPIClass epdSPI(FSPI);
#endif

// State variables
String ssid_ap = "EPaper-Setup";
String current_image = "";
bool wifi_configured = false;
bool display_needs_refresh = true;
unsigned long last_refresh_time = 0;
const unsigned long REFRESH_INTERVAL = 24UL * 60UL * 60UL * 1000UL; // 24 hours
bool access_point_active = false;

enum LogLevel : uint8_t {
    LOG_SILENT = 1,
    LOG_STANDARD = 2,
    LOG_VERBOSE = 3
};

LogLevel currentLogLevel = LOG_STANDARD;

enum NetworkMode : uint8_t {
    NETWORK_MODE_STA,
    NETWORK_MODE_AP
};

void logVPrintf(LogLevel level, const char* format, va_list args) {
    if (currentLogLevel < level) {
        return;
    }
    Serial.vprintf(format, args);
}

void logPrintf(LogLevel level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    logVPrintf(level, format, args);
    va_end(args);
}

void logPrintln(LogLevel level, const String& message) {
    if (currentLogLevel < level) {
        return;
    }
    Serial.println(message);
}

void logPrintln(LogLevel level, const char* message) {
    if (currentLogLevel < level) {
        return;
    }
    Serial.println(message);
}

void setLogLevel(LogLevel level, bool persist = false) {
    LogLevel clamped = level;
    if (clamped < LOG_SILENT) {
        clamped = LOG_SILENT;
    } else if (clamped > LOG_VERBOSE) {
        clamped = LOG_VERBOSE;
    }
    currentLogLevel = clamped;

    if (persist) {
        prefs.begin("photoframe", false);
        prefs.putUChar("log_level", static_cast<uint8_t>(currentLogLevel));
        prefs.end();
    }

    const char* levelName = currentLogLevel == LOG_VERBOSE ? "VERBOSE" :
                            currentLogLevel == LOG_STANDARD ? "STANDARD" : "SILENT";
    Serial.printf("[LOG] Logging level set to %s (%d)\n", levelName, static_cast<int>(currentLogLevel));
}

#define LOGE(format, ...) logPrintf(LOG_SILENT, format, ##__VA_ARGS__)
#define LOGI(format, ...) logPrintf(LOG_STANDARD, format, ##__VA_ARGS__)
#define LOGV(format, ...) logPrintf(LOG_VERBOSE, format, ##__VA_ARGS__)
#define LOGELN(message) logPrintln(LOG_SILENT, message)
#define LOGILN(message) logPrintln(LOG_STANDARD, message)
#define LOGVLN(message) logPrintln(LOG_VERBOSE, message)

// ==================== E-PAPER DISPLAY FUNCTIONS ====================

class EPaperDisplay {
private:
    void spiTransfer(uint8_t data) {
        epdSPI.transfer(data);
    }

    void sendCommand(uint8_t cs_pin, uint8_t command) {
        digitalWrite(EPD_DC, LOW);
        digitalWrite(cs_pin, LOW);
        spiTransfer(command);
        digitalWrite(cs_pin, HIGH);
    }

    void sendData(uint8_t cs_pin, uint8_t data) {
        digitalWrite(EPD_DC, HIGH);
        digitalWrite(cs_pin, LOW);
        spiTransfer(data);
        digitalWrite(cs_pin, HIGH);
    }

    void waitUntilIdle() {
        LOGVLN("[EPD] waitUntilIdle() -> monitoring BUSY pin (LOW = busy). Expect the display to resume within 30 seconds.");
        unsigned long startTime = millis();
        int busyState = digitalRead(EPD_BUSY);
        LOGV("[EPD] Initial BUSY state: %d\n", busyState);
        unsigned long lastLog = 0;
        while (busyState == LOW) {
            delay(10);
            busyState = digitalRead(EPD_BUSY);
            unsigned long elapsed = millis() - startTime;
            if (elapsed - lastLog >= 1000) {
                LOGV("[EPD] Display controller still busy (BUSY=%d). Waiting for refresh to finish... %lu ms elapsed\n", busyState, elapsed);
                lastLog = elapsed;
            }
            if (elapsed > 30000) { // 30 second timeout
                LOGELN("[EPD] Display timed out after 30 seconds. Check power and BUSY wiring.");
                break;
            }
        }
        if (busyState != LOW) {
            LOGV("[EPD] BUSY pin released after %lu ms. Continuing.\n", millis() - startTime);
        }
        delay(200);
    }

    void initializeIC(uint8_t cs_pin) {
        LOGV("[EPD] Configuring controller on CS pin %u\n", cs_pin);
        // Software reset
        sendCommand(cs_pin, 0x12);
        delay(10);

        // Set display settings
        sendCommand(cs_pin, 0x00); // Panel setting
        sendData(cs_pin, 0x1F);    // KW-3f, KWR-2F, BWROTP 0f, BWOTP 1f

        sendCommand(cs_pin, 0x61); // Resolution setting
        sendData(cs_pin, (EPD_HALF_WIDTH >> 8) & 0xFF);
        sendData(cs_pin, EPD_HALF_WIDTH & 0xFF);
        sendData(cs_pin, (EPD_HEIGHT >> 8) & 0xFF);
        sendData(cs_pin, EPD_HEIGHT & 0xFF);

        sendCommand(cs_pin, 0x50); // VCOM and data interval
        sendData(cs_pin, 0x97);
        LOGV("[EPD] Controller on CS %u configured\n", cs_pin);
    }

public:
    void begin() {
        LOGILN("[EPD] Powering up e-paper interface");
        LOGVLN("[EPD] Configuring GPIO directions for display control pins");
        pinMode(EPD_PWR, OUTPUT);
        pinMode(EPD_CS_M, OUTPUT);
        pinMode(EPD_CS_S, OUTPUT);
        pinMode(EPD_DC, OUTPUT);
        pinMode(EPD_RST, OUTPUT);
        pinMode(EPD_BUSY, INPUT);

        digitalWrite(EPD_PWR, LOW);
        digitalWrite(EPD_CS_M, HIGH);
        digitalWrite(EPD_CS_S, HIGH);

        LOGV("[EPD] BUSY pin initial state: %d\n", digitalRead(EPD_BUSY));
        LOGV("[EPD] RST pin initial state: %d\n", digitalRead(EPD_RST));

        LOGVLN("[EPD] Attaching HSPI bus for display communication (4MHz)");
        epdSPI.begin(EPD_SCK, -1, EPD_MOSI, -1);
        epdSPI.setFrequency(4000000); // 4MHz

        LOGILN("[EPD] Display interface ready. Expect BUSY pin to toggle during refresh operations.");
    }

    void reset() {
        LOGILN("[EPD] Performing hardware reset on both controllers");
        digitalWrite(EPD_PWR, HIGH);
        delay(10);
        digitalWrite(EPD_RST, HIGH);
        delay(20);
        digitalWrite(EPD_RST, LOW);
        delay(2);
        digitalWrite(EPD_RST, HIGH);
        delay(20);
        LOGVLN("[EPD] Hardware reset complete");
    }

    void init() {
        LOGILN("[EPD] Initializing display controllers (expect the panel to clear and flash)");
        reset();
        waitUntilIdle();

        // Initialize both ICs
        LOGVLN("[EPD] Initializing master controller");
        initializeIC(EPD_CS_M);
        LOGVLN("[EPD] Initializing slave controller");
        initializeIC(EPD_CS_S);

        LOGILN("[EPD] Display initialization complete");
    }

    void clear() {
        LOGILN("[EPD] Clearing display to white. Expect a full screen flash.");
        
        // Clear master area
        sendCommand(EPD_CS_M, 0x10); // Start transmission
        for(int i = 0; i < (EPD_HALF_WIDTH * EPD_HEIGHT) / 8; i++) {
            sendData(EPD_CS_M, 0xFF);
        }
        
        // Clear slave area
        sendCommand(EPD_CS_S, 0x10); // Start transmission
        for(int i = 0; i < (EPD_HALF_WIDTH * EPD_HEIGHT) / 8; i++) {
            sendData(EPD_CS_S, 0xFF);
        }
        
        refresh();
    }

    void refresh() {
        LOGILN("[EPD] Triggering display refresh. Panel should update shortly.");
        
        // Refresh both ICs
        sendCommand(EPD_CS_M, 0x04); // Power on
        waitUntilIdle();
        sendCommand(EPD_CS_S, 0x04);
        waitUntilIdle();
        
        sendCommand(EPD_CS_M, 0x12); // Display refresh
        sendCommand(EPD_CS_S, 0x12);
        delay(100);
        waitUntilIdle();
        
        LOGILN("[EPD] Display refresh complete");
    }

    void sleep() {
        LOGILN("[EPD] Entering deep sleep to save power");

        sendCommand(EPD_CS_M, 0x02); // Power off
        waitUntilIdle();
        sendCommand(EPD_CS_S, 0x02);
        waitUntilIdle();

        sendCommand(EPD_CS_M, 0x07); // Deep sleep
        sendData(EPD_CS_M, 0xA5);
        sendCommand(EPD_CS_S, 0x07);
        sendData(EPD_CS_S, 0xA5);

        digitalWrite(EPD_PWR, LOW);

        LOGILN("[EPD] Display is now in deep sleep");
    }

    void displayBitmap(uint8_t* imageData, int dataSize) {
        if(imageData == NULL || dataSize == 0) {
            LOGELN("[EPD] Invalid image data passed to displayBitmap");
            return;
        }
        
        LOGILN("[EPD] Displaying bitmap. Expect a two-stage refresh (left then right controller).");
        init();
        
        int halfDataSize = dataSize / 2;
        
        // Send to master area (left half)
        sendCommand(EPD_CS_M, 0x10);
        for(int i = 0; i < halfDataSize; i++) {
            sendData(EPD_CS_M, imageData[i]);
            if(i % 10000 == 0) {
                LOGV("[EPD] Master load progress: %d/%d bytes\n", i, halfDataSize);
            }
        }
        
        // Send to slave area (right half)
        sendCommand(EPD_CS_S, 0x10);
        for(int i = halfDataSize; i < dataSize; i++) {
            sendData(EPD_CS_S, imageData[i]);
            if((i - halfDataSize) % 10000 == 0) {
                LOGV("[EPD] Slave load progress: %d/%d bytes\n", i - halfDataSize, halfDataSize);
            }
        }
        
        refresh();
        sleep();
    }

    void displayText(const char* text, int x, int y) {
        // Simple text display for WiFi info
        // This is a simplified version - you'd want a proper font library
        init();
        clear();
        
        // For now, just display something visible
        // In production, use GFX library with fonts
        LOGI("[EPD] Text placeholder -> '%s' at (%d,%d). Expect a future font renderer here.\n", text, x, y);
        
        refresh();
        sleep();
    }
};

EPaperDisplay epd;

// ==================== SD CARD FUNCTIONS ====================

bool initSDCard() {
    LOGILN("[SD] Initializing SD card interface");
    LOGVLN("[SD] Preparing chip select pin and shared SPI bus");
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);

    // Explicitly initialise the default FSPI bus with the onboard slot's pins
    // before handing it over to the SD library. Some Arduino core versions do
    // not automatically attach the default SPI instance to the board specific
    // pin mapping on first use, which can leave the SD card inaccessible.
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    // Use the default FSPI bus (shared with the onboard microSD slot) at a
    // conservative clock rate. 80MHz was unreliable with the level shifter on
    // the SparkFun board and prevented the card from initialising.
    LOGVLN("[SD] Calling SD.begin() at 20MHz. Expect a short pause during card detection.");
    if (!SD.begin(SD_CS, SPI, 20000000)) {
        LOGELN("[SD] SD Card initialization failed! Insert or reseat the card and reset the device.");
        return false;
    }
    
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        LOGELN("[SD] No SD card detected. The display cannot load images.");
        return false;
    }
    
    LOGILN("[SD] SD card initialized successfully");
    LOGI("[SD] SD Card Type: %s\n", 
                  cardType == CARD_MMC ? "MMC" :
                  cardType == CARD_SD ? "SD" :
                  cardType == CARD_SDHC ? "SDHC" : "UNKNOWN");
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    LOGI("[SD] SD Card Size: %lluMB\n", cardSize);
    
    // Create images directory if it doesn't exist
    if(!SD.exists("/images")) {
        SD.mkdir("/images");
        LOGILN("[SD] Created /images directory for uploaded assets");
    }
    
    return true;
}

String listImages() {
    String imageList = "[";
    File root = SD.open("/images");
    if(!root) {
        return "[]";
    }
    
    File file = root.openNextFile();
    bool first = true;
    while(file) {
        if(!file.isDirectory()) {
            String filename = String(file.name());
            if(filename.endsWith(".bin") || filename.endsWith(".BIN")) {
                if(!first) imageList += ",";
                imageList += "\"" + filename + "\"";
                first = false;
            }
        }
        file = root.openNextFile();
    }
    imageList += "]";
    return imageList;
}

bool displayImageFromSD(String filename) {
    LOGVLN("[IMG] displayImageFromSD() invoked");
    String path = "/images/" + filename;
    File file = SD.open(path);
    
    if(!file) {
        LOGELN("[IMG] Failed to open image file: " + path);
        return false;
    }
    
    size_t fileSize = file.size();
    LOGI("[IMG] Loading image: %s (%d bytes). Expect the panel to refresh after transfer.\n", filename.c_str(), fileSize);
    
    // Allocate buffer for image data
    uint8_t* imageBuffer = (uint8_t*)malloc(fileSize);
    if(imageBuffer == NULL) {
        LOGELN("[IMG] Failed to allocate memory for image");
        file.close();
        return false;
    }
    
    // Read file into buffer
    LOGVLN("[IMG] Reading file into RAM buffer");
    size_t bytesRead = file.read(imageBuffer, fileSize);
    file.close();

    if(bytesRead != fileSize) {
        LOGELN("[IMG] Failed to read complete image file");
        free(imageBuffer);
        return false;
    }

    // Display image
    LOGVLN("[IMG] Sending buffered image to display controller");
    epd.displayBitmap(imageBuffer, fileSize);
    free(imageBuffer);
    
    // Save current image to preferences
    prefs.begin("photoframe", false);
    prefs.putString("current_img", filename);
    prefs.end();
    current_image = filename;
    
    LOGILN("[IMG] Image displayed successfully");
    return true;
}

// ==================== WIFI FUNCTIONS ====================

IPAddress startAccessPointNetwork() {
    LOGILN("[WIFI] Entering Access Point mode for first-time setup");
    access_point_active = true;
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid_ap.c_str());
    
    IPAddress IP = WiFi.softAPIP();
    LOGI("[WIFI] Access Point IP address: %s\n", IP.toString().c_str());
    LOGILN("[WIFI] Expect to connect your laptop/phone to the '" + ssid_ap + "' network and browse to http://" + IP.toString());
    
    // Start DNS server for captive portal
    dnsServer.start(53, "*", IP);
    LOGILN("[WIFI] Captive portal DNS server running");

    LOGILN("[WIFI] Access Point network ready for configuration");
    return IP;
}

bool connectToWiFi(String ssid, String password, int timeout_s = 20) {
    LOGI("[WIFI] Attempting to connect to WiFi SSID '%s'\n", ssid.c_str());
    LOGILN("[WIFI] Expect the status LED (if connected) and logs to update within ~" + String(timeout_s) + " seconds.");
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    
    int elapsed = 0;
    while (WiFi.status() != WL_CONNECTED && elapsed < timeout_s) {
        delay(1000);
        LOGV("[WIFI] Waiting for connection... %d/%d seconds elapsed\n", elapsed + 1, timeout_s);
        elapsed++;
    }
    
    if(WiFi.status() == WL_CONNECTED) {
        LOGILN("[WIFI] Connected to WiFi successfully");
        LOGI("[WIFI] Device IP Address: %s\n", WiFi.localIP().toString().c_str());
        return true;
    } else {
        LOGELN("[WIFI] Connection failed. Falling back to Access Point mode.");
        return false;
    }
}

void displayAccessPointInstructions(const IPAddress& ip) {
    LOGVLN("[WIFI] Rendering Access Point instructions on e-paper display");
    epd.init();
    epd.clear();
    String displayText = "Connect to WiFi:\n" + ssid_ap + "\nThen go to:\n" + ip.toString();
    epd.displayText(displayText.c_str(), 100, 400);
    LOGILN("[WIFI] Display updated with Access Point connection steps");
}

void displayStationConnectionInfo(const IPAddress& ip) {
    LOGVLN("[WIFI] Rendering station-mode status on e-paper display");
    epd.init();
    epd.clear();
    String displayText = "Connected!\nUpload images to:\nhttp://" + ip.toString();
    epd.displayText(displayText.c_str(), 100, 400);
    LOGILN("[WIFI] Display updated with WiFi connection details");
}

NetworkMode establishNetwork(IPAddress& networkIP) {
    LOGILN("[WIFI] Checking saved WiFi credentials");
    prefs.begin("photoframe", true);
    String saved_ssid = prefs.getString("wifi_ssid", "");
    String saved_pass = prefs.getString("wifi_pass", "");
    prefs.end();

    if(saved_ssid.length() > 0) {
        wifi_configured = true;
        LOGILN("[WIFI] Saved credentials found. Attempting to reconnect automatically.");

        if(connectToWiFi(saved_ssid, saved_pass)) {
            access_point_active = false;
            networkIP = WiFi.localIP();
            configTime(0, 0, "pool.ntp.org");
            LOGILN("[WIFI] WiFi connected. Use the reported IP for the web interface.");
            return NETWORK_MODE_STA;
        }

        LOGELN("[WIFI] Unable to connect using stored credentials. Switching to Access Point mode.");
        wifi_configured = false;
    } else {
        LOGILN("[WIFI] No stored WiFi credentials found");
    }

    networkIP = startAccessPointNetwork();
    wifi_configured = false;
    return NETWORK_MODE_AP;
}

// ==================== WEB SERVER HANDLERS ====================

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>E-Paper Photo Frame</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; margin: 20px; background: #f0f0f0; }
        .container { max-width: 800px; margin: auto; background: white; padding: 20px; border-radius: 10px; }
        h1 { color: #333; }
        .section { margin: 20px 0; padding: 15px; border: 1px solid #ddd; border-radius: 5px; }
        input[type="text"], input[type="password"] { width: 100%; padding: 10px; margin: 5px 0; }
        button { background: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; margin: 5px; }
        button:hover { background: #45a049; }
        .image-list { display: flex; flex-wrap: wrap; gap: 10px; }
        .image-item { padding: 10px; background: #e0e0e0; border-radius: 5px; cursor: pointer; }
        .image-item:hover { background: #d0d0d0; }
        .current { background: #4CAF50 !important; color: white; }
        .status { padding: 10px; margin: 10px 0; border-radius: 5px; }
        .success { background: #d4edda; color: #155724; }
        .error { background: #f8d7da; color: #721c24; }
    </style>
</head>
<body>
    <div class="container">
        <h1>📷 E-Paper Photo Frame</h1>
        
        <div class="section" id="wifiSetup" style="display:none;">
            <h2>WiFi Setup</h2>
            <input type="text" id="ssid" placeholder="WiFi SSID">
            <input type="password" id="password" placeholder="WiFi Password">
            <button onclick="saveWiFi()">Connect</button>
            <div id="wifiStatus"></div>
        </div>
        
        <div class="section">
            <h2>Upload Image</h2>
            <p>Upload a 1600x1200 pixel image in raw binary format (.bin)</p>
            <input type="file" id="fileInput" accept=".bin">
            <button onclick="uploadImage()">Upload</button>
            <div id="uploadStatus"></div>
        </div>
        
        <div class="section">
            <h2>Stored Images</h2>
            <button onclick="loadImages()">Refresh List</button>
            <div id="imageList" class="image-list"></div>
        </div>
        
        <div class="section">
            <h2>System Info</h2>
            <div id="systemInfo"></div>
        </div>
    </div>
    
    <script>
        let currentImage = '';
        
        function showWiFiSetup() {
            if(window.location.hostname === '192.168.4.1') {
                document.getElementById('wifiSetup').style.display = 'block';
            }
        }
        
        async function saveWiFi() {
            const ssid = document.getElementById('ssid').value;
            const password = document.getElementById('password').value;
            
            const response = await fetch('/save-wifi', {
                method: 'POST',
                headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                body: `ssid=${encodeURIComponent(ssid)}&password=${encodeURIComponent(password)}`
            });
            
            const result = await response.text();
            const statusDiv = document.getElementById('wifiStatus');
            statusDiv.className = 'status ' + (result.includes('success') ? 'success' : 'error');
            statusDiv.innerHTML = result;
        }
        
        async function uploadImage() {
            const fileInput = document.getElementById('fileInput');
            const file = fileInput.files[0];
            
            if(!file) {
                alert('Please select a file');
                return;
            }
            
            const statusDiv = document.getElementById('uploadStatus');
            statusDiv.className = 'status';
            statusDiv.innerHTML = 'Uploading...';
            
            const formData = new FormData();
            formData.append('file', file);
            
            try {
                const response = await fetch('/upload', {
                    method: 'POST',
                    body: formData
                });
                
                const result = await response.text();
                statusDiv.className = 'status ' + (result.includes('success') ? 'success' : 'error');
                statusDiv.innerHTML = result;
                
                if(result.includes('success')) {
                    loadImages();
                }
            } catch(error) {
                statusDiv.className = 'status error';
                statusDiv.innerHTML = 'Upload failed: ' + error;
            }
        }
        
        async function loadImages() {
            const response = await fetch('/list-images');
            const images = await response.json();
            
            const imageList = document.getElementById('imageList');
            imageList.innerHTML = '';
            
            if(images.length === 0) {
                imageList.innerHTML = '<p>No images stored</p>';
                return;
            }
            
            images.forEach(img => {
                const div = document.createElement('div');
                div.className = 'image-item' + (img === currentImage ? ' current' : '');
                div.textContent = img;
                div.onclick = () => displayImage(img);
                imageList.appendChild(div);
            });
        }
        
        async function displayImage(filename) {
            const response = await fetch('/display?image=' + encodeURIComponent(filename));
            const result = await response.text();
            
            if(result.includes('success')) {
                currentImage = filename;
                loadImages();
            }
            alert(result);
        }
        
        async function loadSystemInfo() {
            const response = await fetch('/system-info');
            const info = await response.json();
            
            document.getElementById('systemInfo').innerHTML = `
                <p><strong>Current Image:</strong> ${info.current_image || 'None'}</p>
                <p><strong>Free Heap:</strong> ${info.free_heap} bytes</p>
                <p><strong>SD Card:</strong> ${info.sd_size}MB (${info.sd_used}MB used)</p>
                <p><strong>WiFi:</strong> ${info.wifi_ssid} (${info.wifi_rssi} dBm)</p>
                <p><strong>IP Address:</strong> ${info.ip_address}</p>
            `;
        }
        
        window.onload = () => {
            showWiFiSetup();
            loadImages();
            loadSystemInfo();
            setInterval(loadSystemInfo, 10000);
        };
    </script>
</body>
</html>
)rawliteral";

void handleRoot() {
    LOGVLN("[HTTP] Serving root UI page");
    server.send(200, "text/html", index_html);
}

void handleSaveWiFi() {
    LOGILN("[HTTP] Received /save-wifi request");
    if(server.hasArg("ssid") && server.hasArg("password")) {
        String ssid = server.arg("ssid");
        String password = server.arg("password");
        
        prefs.begin("photoframe", false);
        prefs.putString("wifi_ssid", ssid);
        prefs.putString("wifi_pass", password);
        prefs.end();

        LOGI("[HTTP] Stored WiFi credentials for SSID '%s'. Device will restart to apply changes.\n", ssid.c_str());
        
        server.send(200, "text/plain", "WiFi credentials saved! Device will restart...");
        
        delay(1000);
        ESP.restart();
    } else {
        LOGELN("[HTTP] Missing parameters for /save-wifi");
        server.send(400, "text/plain", "Missing parameters");
    }
}

void handleUpload() {
    HTTPUpload& upload = server.upload();
    static File uploadFile;

    if(upload.status == UPLOAD_FILE_START) {
        LOGILN("[HTTP] Upload initiated");
        String filename = upload.filename;
        if(!filename.endsWith(".bin")) {
            filename += ".bin";
        }
        String path = "/images/" + filename;

        LOGI("[HTTP] Upload started: %s\n", path.c_str());
        uploadFile = SD.open(path, FILE_WRITE);
    }
    else if(upload.status == UPLOAD_FILE_WRITE) {
        if(uploadFile) {
            uploadFile.write(upload.buf, upload.currentSize);
        }
    }
    else if(upload.status == UPLOAD_FILE_END) {
        LOGVLN("[HTTP] Upload finished streaming to storage");
        if(uploadFile) {
            uploadFile.close();
            LOGI("[HTTP] Upload complete: %s (%d bytes)\n", upload.filename.c_str(), upload.totalSize);
            server.send(200, "text/plain", "Upload success!");
        } else {
            LOGELN("[HTTP] Upload failed to open file for writing");
            server.send(500, "text/plain", "Upload failed!");
        }
    }
}

void handleListImages() {
    LOGVLN("[HTTP] Listing stored images");
    String imageList = listImages();
    server.send(200, "application/json", imageList);
}

void handleDisplay() {
    LOGILN("[HTTP] Display image request received");
    if(server.hasArg("image")) {
        String filename = server.arg("image");

        if(displayImageFromSD(filename)) {
            server.send(200, "text/plain", "Image displayed successfully!");
        } else {
            LOGELN("[HTTP] Failed to display requested image");
            server.send(500, "text/plain", "Failed to display image");
        }
    } else {
        LOGELN("[HTTP] Missing image parameter for /display");
        server.send(400, "text/plain", "Missing image parameter");
    }
}

void handleSystemInfo() {
    LOGVLN("[HTTP] Reporting system information");
    prefs.begin("photoframe", true);
    String current_img = prefs.getString("current_img", "None");
    String wifi_ssid = prefs.getString("wifi_ssid", "Not configured");
    prefs.end();
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    uint64_t cardUsed = SD.usedBytes() / (1024 * 1024);
    
    String json = "{";
    json += "\"current_image\":\"" + current_img + "\",";
    json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"sd_size\":" + String((uint32_t)cardSize) + ",";
    json += "\"sd_used\":" + String((uint32_t)cardUsed) + ",";
    IPAddress reportIP = access_point_active ? WiFi.softAPIP() : WiFi.localIP();
    json += "\"wifi_ssid\":\"" + wifi_ssid + "\",";
    json += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"ip_address\":\"" + reportIP.toString() + "\",";
    json += "\"log_level\":" + String(static_cast<int>(currentLogLevel)) + ",";
    json += "\"mode\":\"" + String(access_point_active ? "AP" : "STA") + "\"";
    json += "}";
    
    server.send(200, "application/json", json);
}

void handleSetLogLevel() {
    LOGILN("[HTTP] Log level change requested");
    if(server.hasArg("level")) {
        int requested = server.arg("level").toInt();
        setLogLevel(static_cast<LogLevel>(requested), true);
        String response = "Log level updated to " + String(static_cast<int>(currentLogLevel));
        server.send(200, "text/plain", response);
    } else {
        LOGELN("[HTTP] Missing level parameter for /set-log-level");
        server.send(400, "text/plain", "Missing level parameter");
    }
}

void setupWebServer() {
    LOGILN("[HTTP] Configuring HTTP routes");
    server.on("/", handleRoot);
    server.on("/save-wifi", HTTP_POST, handleSaveWiFi);
    server.on("/upload", HTTP_POST, []() {
        server.send(200);
    }, handleUpload);
    server.on("/list-images", handleListImages);
    server.on("/display", handleDisplay);
    server.on("/system-info", handleSystemInfo);
    server.on("/set-log-level", handleSetLogLevel);
    
    server.begin();
    LOGILN("[HTTP] Web server started and ready for requests");
}

// ==================== MAIN SETUP ====================

void setup() {
    Serial.begin(115200);
    delay(1000);

    LOGELN("[SETUP] Booting..."); // ensure visibility even at low log level
    LOGILN("[SETUP] Serial interface ready");

    LOGILN("\n\n=================================\nESP32-C6 E-Paper Photo Frame\n=================================");

    // Initialize preferences and log level
    LOGILN("[SETUP] Loading preferences");
    prefs.begin("photoframe", true);
    current_image = prefs.getString("current_img", "");
    uint8_t storedLevel = prefs.getUChar("log_level", static_cast<uint8_t>(LOG_STANDARD));
    prefs.end();
    setLogLevel(static_cast<LogLevel>(storedLevel));

    if(current_image.length() == 0) {
        LOGILN("[SETUP] No previously displayed image recorded");
    } else {
        LOGILN("[SETUP] Last displayed image: " + current_image);
    }

    // Initialize hardware
    LOGILN("[SETUP] Initializing e-paper interface");
    epd.begin();

    LOGILN("[SETUP] Initializing SD card");
    if(!initSDCard()) {
        LOGELN("[SETUP] SD Card required! Halting.");
        epd.init();
        epd.clear();
        epd.displayText("ERROR: No SD Card", 100, 500);
        while(1) delay(1000);
    }

    LOGILN("[SETUP] Establishing network connectivity");
    IPAddress networkIP;
    NetworkMode mode = establishNetwork(networkIP);

    LOGILN("[SETUP] Starting web server");
    setupWebServer();

    if(mode == NETWORK_MODE_STA) {
        LOGILN("[SETUP] Web interface available at http://" + networkIP.toString());
        displayStationConnectionInfo(networkIP);
    } else {
        LOGILN("[SETUP] Captive portal available at http://" + networkIP.toString());
        displayAccessPointInstructions(networkIP);
    }

    // If we have a saved image and WiFi is working, display it
    if(current_image.length() > 0 && wifi_configured) {
        LOGILN("[SETUP] Displaying last selected image: " + current_image + ". Expect the panel to refresh shortly.");
        epd.init();
        epd.clear();
        delay(1000);
        displayImageFromSD(current_image);
        last_refresh_time = millis();
    } else if(current_image.length() == 0) {
        LOGILN("[SETUP] No saved image to display at boot");
    } else if(!wifi_configured) {
        LOGILN("[SETUP] WiFi not configured; skipping auto display");
    }
    
    LOGILN("\nSetup complete! System is ready.");
}

// ==================== MAIN LOOP ====================

void loop() {
    // Handle DNS for captive portal (AP mode only)
    if(!wifi_configured) {
        dnsServer.processNextRequest();
    }

    // Handle web server
    server.handleClient();

    // Check for daily refresh
    if(wifi_configured && current_image.length() > 0) {
        unsigned long currentTime = millis();

        if(currentTime - last_refresh_time >= REFRESH_INTERVAL) {
            LOGILN("[LOOP] Performing scheduled daily refresh of the display");
            epd.init();
            epd.clear();
            delay(1000);
            displayImageFromSD(current_image);
            last_refresh_time = currentTime;
        }
    }

    static unsigned long lastStatusLog = 0;
    unsigned long now = millis();
    if(now - lastStatusLog >= 5000) {
        LOGV("[LOOP] wifi_configured=%d current_image='%s' free_heap=%u\n",
             wifi_configured,
             current_image.c_str(),
             ESP.getFreeHeap());
        lastStatusLog = now;
    }

    delay(100);
}
