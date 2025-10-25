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
SPIClass sdSPI(HSPI);
SPIClass epdSPI(FSPI);

// State variables
String ssid_ap = "EPaper-Setup";
String current_image = "";
bool wifi_configured = false;
bool display_needs_refresh = true;
unsigned long last_refresh_time = 0;
const unsigned long REFRESH_INTERVAL = 24UL * 60UL * 60UL * 1000UL; // 24 hours

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
        Serial.println("Waiting for display...");
        unsigned long startTime = millis();
        while(digitalRead(EPD_BUSY) == LOW) {
            delay(10);
            if(millis() - startTime > 30000) { // 30 second timeout
                Serial.println("Display timeout!");
                break;
            }
        }
        delay(200);
    }

    void initializeIC(uint8_t cs_pin) {
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
    }

public:
    void begin() {
        // Initialize pins
        pinMode(EPD_PWR, OUTPUT);
        pinMode(EPD_CS_M, OUTPUT);
        pinMode(EPD_CS_S, OUTPUT);
        pinMode(EPD_DC, OUTPUT);
        pinMode(EPD_RST, OUTPUT);
        pinMode(EPD_BUSY, INPUT);

        digitalWrite(EPD_PWR, LOW);
        digitalWrite(EPD_CS_M, HIGH);
        digitalWrite(EPD_CS_S, HIGH);

        // Initialize SPI
        epdSPI.begin(EPD_SCK, -1, EPD_MOSI, -1);
        epdSPI.setFrequency(4000000); // 4MHz

        Serial.println("E-Paper initialized");
    }

    void reset() {
        digitalWrite(EPD_PWR, HIGH);
        delay(10);
        digitalWrite(EPD_RST, HIGH);
        delay(20);
        digitalWrite(EPD_RST, LOW);
        delay(2);
        digitalWrite(EPD_RST, HIGH);
        delay(20);
    }

    void init() {
        reset();
        waitUntilIdle();
        
        // Initialize both ICs
        initializeIC(EPD_CS_M);
        initializeIC(EPD_CS_S);
        
        Serial.println("Display initialized");
    }

    void clear() {
        Serial.println("Clearing display...");
        
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
        Serial.println("Refreshing display...");
        
        // Refresh both ICs
        sendCommand(EPD_CS_M, 0x04); // Power on
        waitUntilIdle();
        sendCommand(EPD_CS_S, 0x04);
        waitUntilIdle();
        
        sendCommand(EPD_CS_M, 0x12); // Display refresh
        sendCommand(EPD_CS_S, 0x12);
        delay(100);
        waitUntilIdle();
        
        Serial.println("Display refresh complete");
    }

    void sleep() {
        Serial.println("Putting display to sleep...");

        sendCommand(EPD_CS_M, 0x02); // Power off
        waitUntilIdle();
        sendCommand(EPD_CS_S, 0x02);
        waitUntilIdle();

        sendCommand(EPD_CS_M, 0x07); // Deep sleep
        sendData(EPD_CS_M, 0xA5);
        sendCommand(EPD_CS_S, 0x07);
        sendData(EPD_CS_S, 0xA5);

        digitalWrite(EPD_PWR, LOW);

        Serial.println("Display in deep sleep");
    }

    void displayBitmap(uint8_t* imageData, int dataSize) {
        if(imageData == NULL || dataSize == 0) {
            Serial.println("Invalid image data");
            return;
        }
        
        Serial.println("Displaying bitmap...");
        init();
        
        int halfDataSize = dataSize / 2;
        
        // Send to master area (left half)
        sendCommand(EPD_CS_M, 0x10);
        for(int i = 0; i < halfDataSize; i++) {
            sendData(EPD_CS_M, imageData[i]);
            if(i % 10000 == 0) {
                Serial.printf("Master: %d/%d\n", i, halfDataSize);
            }
        }
        
        // Send to slave area (right half)
        sendCommand(EPD_CS_S, 0x10);
        for(int i = halfDataSize; i < dataSize; i++) {
            sendData(EPD_CS_S, imageData[i]);
            if((i - halfDataSize) % 10000 == 0) {
                Serial.printf("Slave: %d/%d\n", i - halfDataSize, halfDataSize);
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
        Serial.printf("Would display: %s at (%d, %d)\n", text, x, y);
        
        refresh();
        sleep();
    }
};

EPaperDisplay epd;

// ==================== SD CARD FUNCTIONS ====================

bool initSDCard() {
    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    
    if (!SD.begin(SD_CS, sdSPI, 80000000)) {
        Serial.println("SD Card initialization failed!");
        return false;
    }
    
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("No SD card attached");
        return false;
    }
    
    Serial.println("SD Card initialized");
    Serial.printf("SD Card Type: %s\n", 
                  cardType == CARD_MMC ? "MMC" :
                  cardType == CARD_SD ? "SD" :
                  cardType == CARD_SDHC ? "SDHC" : "UNKNOWN");
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);
    
    // Create images directory if it doesn't exist
    if(!SD.exists("/images")) {
        SD.mkdir("/images");
        Serial.println("Created /images directory");
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
    String path = "/images/" + filename;
    File file = SD.open(path);
    
    if(!file) {
        Serial.println("Failed to open image file: " + path);
        return false;
    }
    
    size_t fileSize = file.size();
    Serial.printf("Loading image: %s (%d bytes)\n", filename.c_str(), fileSize);
    
    // Allocate buffer for image data
    uint8_t* imageBuffer = (uint8_t*)malloc(fileSize);
    if(imageBuffer == NULL) {
        Serial.println("Failed to allocate memory for image");
        file.close();
        return false;
    }
    
    // Read file into buffer
    size_t bytesRead = file.read(imageBuffer, fileSize);
    file.close();
    
    if(bytesRead != fileSize) {
        Serial.println("Failed to read complete image file");
        free(imageBuffer);
        return false;
    }
    
    // Display image
    epd.displayBitmap(imageBuffer, fileSize);
    free(imageBuffer);
    
    // Save current image to preferences
    prefs.begin("photoframe", false);
    prefs.putString("current_img", filename);
    prefs.end();
    current_image = filename;
    
    Serial.println("Image displayed successfully");
    return true;
}

// ==================== WIFI FUNCTIONS ====================

void setupAccessPoint() {
    Serial.println("Setting up Access Point...");
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid_ap.c_str());
    
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
    
    // Start DNS server for captive portal
    dnsServer.start(53, "*", IP);
    
    // Display WiFi info on e-paper
    epd.init();
    epd.clear();
    
    String displayText = "Connect to WiFi:\n" + ssid_ap + "\nThen go to:\n192.168.4.1";
    epd.displayText(displayText.c_str(), 100, 400);
    
    Serial.println("Access Point ready");
    Serial.println("Connect to: " + ssid_ap);
    Serial.println("Then navigate to: http://192.168.4.1");
}

bool connectToWiFi(String ssid, String password, int timeout_s = 20) {
    Serial.println("Connecting to WiFi: " + ssid);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    
    int elapsed = 0;
    while (WiFi.status() != WL_CONNECTED && elapsed < timeout_s) {
        delay(1000);
        Serial.print(".");
        elapsed++;
    }
    Serial.println();
    
    if(WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        return true;
    } else {
        Serial.println("Connection failed");
        return false;
    }
}

void checkWiFiStatus() {
    prefs.begin("photoframe", true);
    String saved_ssid = prefs.getString("wifi_ssid", "");
    String saved_pass = prefs.getString("wifi_pass", "");
    prefs.end();
    
    if(saved_ssid.length() > 0) {
        wifi_configured = true;
        Serial.println("Found saved WiFi credentials");
        
        if(connectToWiFi(saved_ssid, saved_pass)) {
            // Successfully connected - display IP
            epd.init();
            epd.clear();
            
            String displayText = "Connected!\nUpload images to:\nhttp://" + WiFi.localIP().toString();
            epd.displayText(displayText.c_str(), 100, 400);
            
            // Configure time
            configTime(0, 0, "pool.ntp.org");
            
            return;
        } else {
            // Can't connect - go to AP mode
            Serial.println("Cannot connect to saved WiFi");
            wifi_configured = false;
        }
    }
    
    // No saved WiFi or connection failed - setup AP
    setupAccessPoint();
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
    server.send(200, "text/html", index_html);
}

void handleSaveWiFi() {
    if(server.hasArg("ssid") && server.hasArg("password")) {
        String ssid = server.arg("ssid");
        String password = server.arg("password");
        
        prefs.begin("photoframe", false);
        prefs.putString("wifi_ssid", ssid);
        prefs.putString("wifi_pass", password);
        prefs.end();
        
        server.send(200, "text/plain", "WiFi credentials saved! Device will restart...");
        
        delay(1000);
        ESP.restart();
    } else {
        server.send(400, "text/plain", "Missing parameters");
    }
}

void handleUpload() {
    HTTPUpload& upload = server.upload();
    static File uploadFile;
    
    if(upload.status == UPLOAD_FILE_START) {
        String filename = upload.filename;
        if(!filename.endsWith(".bin")) {
            filename += ".bin";
        }
        String path = "/images/" + filename;
        
        Serial.println("Upload started: " + path);
        uploadFile = SD.open(path, FILE_WRITE);
    } 
    else if(upload.status == UPLOAD_FILE_WRITE) {
        if(uploadFile) {
            uploadFile.write(upload.buf, upload.currentSize);
        }
    } 
    else if(upload.status == UPLOAD_FILE_END) {
        if(uploadFile) {
            uploadFile.close();
            Serial.printf("Upload complete: %s (%d bytes)\n", upload.filename.c_str(), upload.totalSize);
            server.send(200, "text/plain", "Upload success!");
        } else {
            server.send(500, "text/plain", "Upload failed!");
        }
    }
}

void handleListImages() {
    String imageList = listImages();
    server.send(200, "application/json", imageList);
}

void handleDisplay() {
    if(server.hasArg("image")) {
        String filename = server.arg("image");
        
        if(displayImageFromSD(filename)) {
            server.send(200, "text/plain", "Image displayed successfully!");
        } else {
            server.send(500, "text/plain", "Failed to display image");
        }
    } else {
        server.send(400, "text/plain", "Missing image parameter");
    }
}

void handleSystemInfo() {
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
    json += "\"wifi_ssid\":\"" + wifi_ssid + "\",";
    json += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"ip_address\":\"" + WiFi.localIP().toString() + "\"";
    json += "}";
    
    server.send(200, "application/json", json);
}

void setupWebServer() {
    server.on("/", handleRoot);
    server.on("/save-wifi", HTTP_POST, handleSaveWiFi);
    server.on("/upload", HTTP_POST, []() {
        server.send(200);
    }, handleUpload);
    server.on("/list-images", handleListImages);
    server.on("/display", handleDisplay);
    server.on("/system-info", handleSystemInfo);
    
    server.begin();
    Serial.println("Web server started");
}

// ==================== MAIN SETUP ====================

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n=================================");
    Serial.println("ESP32-C6 E-Paper Photo Frame");
    Serial.println("=================================\n");
    
    // Initialize preferences
    prefs.begin("photoframe", true);
    current_image = prefs.getString("current_img", "");
    prefs.end();
    
    // Initialize hardware
    epd.begin();
    
    if(!initSDCard()) {
        Serial.println("SD Card required! Halting.");
        epd.init();
        epd.clear();
        epd.displayText("ERROR: No SD Card", 100, 500);
        while(1) delay(1000);
    }
    
    // Check WiFi status and configure
    checkWiFiStatus();
    
    // Setup web server
    setupWebServer();
    
    // If we have a saved image and WiFi is working, display it
    if(current_image.length() > 0 && wifi_configured) {
        Serial.println("Displaying saved image: " + current_image);
        epd.init();
        epd.clear();
        delay(1000);
        displayImageFromSD(current_image);
        last_refresh_time = millis();
    }
    
    Serial.println("\nSetup complete!");
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
            Serial.println("Performing daily refresh...");
            epd.init();
            epd.clear();
            delay(1000);
            displayImageFromSD(current_image);
            last_refresh_time = currentTime;
        }
    }
    
    delay(100);
}
