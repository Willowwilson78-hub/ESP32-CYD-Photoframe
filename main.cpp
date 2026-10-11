#include <Arduino.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <vector>
#include <algorithm>
#include <FS.h>
#include <SD.h>
#include "SDCard.h"

TFT_eSPI tft;

static constexpr uint32_t SERIAL_BAUD = 115200;
static constexpr uint32_t DEFAULT_INTERVAL_MS = 5000;
static constexpr uint32_t DEFAULT_ANIM_INTERVAL_MS = 100; // Tốc độ mặc định cho animation (ms)
static constexpr uint8_t DEFAULT_BRIGHTNESS = 20;
static constexpr size_t MAX_COMMAND_LENGTH = 127;

constexpr uint8_t BUTTON_PIN = 0;

enum DeviceMode : uint8_t {
    MODE_PHOTO = 0,
    MODE_GIF = 1
};

struct Config {
    uint32_t intervalMs = DEFAULT_INTERVAL_MS;
    uint32_t animIntervalMs = DEFAULT_ANIM_INTERVAL_MS; // Thêm cấu hình tốc độ animation riêng
    uint8_t brightness = DEFAULT_BRIGHTNESS;
    uint8_t red = 255;
    uint8_t green = 0;
    uint8_t blue = 128;
    bool rgbEnabled = true;
    DeviceMode mode = MODE_PHOTO;
    uint8_t rotation = 3;
};

Config config;

constexpr uint8_t LED_R_PIN = 17;
constexpr uint8_t LED_G_PIN = 4;
constexpr uint8_t LED_B_PIN = 16;

constexpr uint32_t RGB_PWM_FREQ = 5000;
constexpr uint8_t RGB_PWM_BITS = 8;
constexpr uint8_t RGB_R_CH = 0;
constexpr uint8_t RGB_G_CH = 1;
constexpr uint8_t RGB_B_CH = 2;

bool rgbReady = false;

std::vector<String> jpgFiles;
std::vector<String> animFolders;        // Danh sách các thư mục chứa chuỗi frame (như anim_XiaomiSu7)
std::vector<String> currentAnimFrames;   // Danh sách các ảnh bên trong thư mục animation hiện tại

size_t fileIndex = 0;
size_t animFrameIndex = 0;
uint32_t lastImageTime = 0;
bool fileNeedsUpdate = true;

unsigned long lastDebounceTime = 0;
bool lastButtonState = HIGH;

char commandBuffer[MAX_COMMAND_LENGTH + 1];
size_t commandLength = 0;

void applyRgb() {
    if (!rgbReady) return;
    uint8_t r = config.rgbEnabled ? config.red : 0;
    uint8_t g = config.rgbEnabled ? config.green : 0;
    uint8_t b = config.rgbEnabled ? config.blue : 0;
    ledcWrite(RGB_R_CH, r);
    ledcWrite(RGB_G_CH, g);
    ledcWrite(RGB_B_CH, b);
}

bool ledcAttachCustom(uint8_t pin, uint32_t freq, uint8_t bits) {
    uint8_t channel = 0;
    if (pin == LED_R_PIN) channel = RGB_R_CH;
    else if (pin == LED_G_PIN) channel = RGB_G_CH;
    else if (pin == LED_B_PIN) channel = RGB_B_CH;
    else return false;

    ledcSetup(channel, freq, bits);
    ledcAttachPin(pin, channel);
    return true;
}

void initRGB() {
    rgbReady = ledcAttachCustom(LED_R_PIN, RGB_PWM_FREQ, RGB_PWM_BITS) &&
               ledcAttachCustom(LED_G_PIN, RGB_PWM_FREQ, RGB_PWM_BITS) &&
               ledcAttachCustom(LED_B_PIN, RGB_PWM_FREQ, RGB_PWM_BITS);
    if (rgbReady) applyRgb();
}

void saveConfigToSD() {
    File f = SD.open("/config.txt", FILE_WRITE);
    if (!f) {
        Serial.println("ERR: Failed to write config file to SD");
        return;
    }
    f.printf("interval=%u\n", config.intervalMs);
    f.printf("animInterval=%u\n", config.animIntervalMs); // Lưu tốc độ animation vào thẻ SD
    f.printf("brightness=%u\n", config.brightness);
    f.printf("mode=%u\n", (uint8_t)config.mode);
    f.printf("red=%u\n", config.red);
    f.printf("green=%u\n", config.green);
    f.printf("blue=%u\n", config.blue);
    f.printf("rgbEnabled=%d\n", config.rgbEnabled ? 1 : 0);
    f.printf("rotation=%u\n", config.rotation);
    f.close();
}

void loadConfigFromSD() {
    if (!SD.exists("/config.txt")) {
        saveConfigToSD();
        return;
    }

    File f = SD.open("/config.txt", FILE_READ);
    if (!f) return;

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        int eq = line.indexOf('=');
        if (eq == -1) continue;
        String key = line.substring(0, eq);
        String val = line.substring(eq + 1);
        val.trim();

        if (key == "interval") config.intervalMs = val.toInt();
        else if (key == "animInterval") config.animIntervalMs = val.toInt(); // Đọc tốc độ animation từ thẻ SD
        else if (key == "brightness") config.brightness = val.toInt();
        else if (key == "mode") config.mode = (DeviceMode)val.toInt();
        else if (key == "red") config.red = val.toInt();
        else if (key == "green") config.green = val.toInt();
        else if (key == "blue") config.blue = val.toInt();
        else if (key == "rgbEnabled") config.rgbEnabled = (val.toInt() == 1);
        else if (key == "rotation") config.rotation = val.toInt();
    }
    f.close();
    Serial.println("OK: Configuration loaded from SD card");
}

// Hàm tải danh sách các frame ảnh bên trong thư mục animation
void loadAnimFrames(const String& folderPath) {
    currentAnimFrames.clear();
    File dir = SD.open(folderPath.c_str());
    if (!dir || !dir.isDirectory()) return;

    File file = dir.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String fileName = String(file.name());
            String lowerName = fileName;
            lowerName.toLowerCase();
            if (lowerName.endsWith(".jpg") || lowerName.endsWith(".jpeg") || lowerName.endsWith(".png")) {
                String fullPath = folderPath + "/" + fileName;
                currentAnimFrames.push_back(fullPath);
            }
        }
        File nextFile = dir.openNextFile();
        file.close();
        file = nextFile;
    }
    dir.close();
    std::sort(currentAnimFrames.begin(), currentAnimFrames.end());
    animFrameIndex = 0;
}

void scanImageFiles() {
    jpgFiles.clear();
    animFolders.clear();
    
    File root = SD.open("/");
    if (!root || !root.isDirectory()) {
        Serial.println("ERR: Failed to open SD card root directory!");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        String fileName = String(file.name());
        if (file.isDirectory()) {
            String lowerDir = fileName;
            lowerDir.toLowerCase();
            if (lowerDir.startsWith("anim") || lowerDir.startsWith("gif")) {
                animFolders.push_back("/" + fileName);
            }
        } else {
            String lowerName = fileName;
            lowerName.toLowerCase();
            if (!fileName.startsWith("/")) {
                fileName = "/" + fileName;
            }
            if (lowerName.endsWith(".jpg") || lowerName.endsWith(".jpeg")) {
                jpgFiles.push_back(fileName);
            }
        }
        File nextFile = root.openNextFile();
        file.close();
        file = nextFile;
    }
    root.close();

    std::sort(jpgFiles.begin(), jpgFiles.end());
    std::sort(animFolders.begin(), animFolders.end());
    
    Serial.printf("Total files -> JPG: %u, Animation Folders: %u\n", (unsigned)jpgFiles.size(), (unsigned)animFolders.size());

    if (config.mode == MODE_GIF && !animFolders.empty()) {
        if (fileIndex >= animFolders.size()) fileIndex = 0;
        loadAnimFrames(animFolders[fileIndex]);
    } else {
        if (fileIndex >= jpgFiles.size()) fileIndex = 0;
    }
}

void setBrightness(uint8_t percent) {
    config.brightness = percent;
    analogWrite(TFT_BL, (uint32_t)percent * 255 / 100);
}

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (x >= tft.width() || y >= tft.height()) return false;
    tft.pushImage(x, y, w, h, bitmap);
    return true;
}

void printHelp() {
    Serial.println("\n--- CYD PhotoFrame Commands ---");
    Serial.println("   help");
    Serial.println("   status");
    Serial.println("   scan");
    Serial.println("   interval <milliseconds>       (Time between photo slides)");
    Serial.println("   animinterval <milliseconds>   (Speed/delay between animation frames, e.g., 50)");
    Serial.println("   mode <photo|gif>");
    Serial.println("   rotation <0-3>");
    Serial.println("   rgb <HEX> or <0-255> <0-255> <0-255>");
    Serial.println("   rgb off");
    Serial.println("   next");
    Serial.println("   brightness <0-100>");
    Serial.println("   save");
    Serial.println("--------------------------------");
}

void printStatus() {
    Serial.printf("Mode: %s\n", config.mode == MODE_PHOTO ? "Photo (JPG)" : "Animation Folders");
    Serial.printf("Rotation: %u\n", config.rotation);
    Serial.printf("Photo Interval: %u ms\n", config.intervalMs);
    Serial.printf("Animation Interval (Speed): %u ms\n", config.animIntervalMs);
    Serial.printf("RGB: R=%u, G=%u, B=%u (%s)\n", config.red, config.green, config.blue, config.rgbEnabled ? "On" : "Off");
    Serial.printf("Files -> JPG: %u, Anim Folders: %u (Current Index: %u)\n", (unsigned)jpgFiles.size(), (unsigned)animFolders.size(), (unsigned)fileIndex);
}

bool parseLong(const char* s, long& result) {
    if (!s || !*s) return false;
    char* end = nullptr;
    result = strtol(s, &end, 10);
    return end != s && *end == '\0';
}

void requestNextFile(int direction) {
    if (config.mode == MODE_PHOTO) {
        if (jpgFiles.empty()) return;
        fileIndex = (direction > 0) ? (fileIndex + 1) % jpgFiles.size() : (fileIndex + jpgFiles.size() - 1) % jpgFiles.size();
    } else {
        if (animFolders.empty()) return;
        fileIndex = (direction > 0) ? (fileIndex + 1) % animFolders.size() : (fileIndex + animFolders.size() - 1) % animFolders.size();
        loadAnimFrames(animFolders[fileIndex]);
    }
    fileNeedsUpdate = true;
    lastImageTime = millis();
}

void handleCommand(char* line) {
    char* argv[5] = {};
    int argc = 0;
    char* token = strtok(line, " \t");

    while (token && argc < 5) {
        argv[argc++] = token;
        token = strtok(nullptr, " \t");
    }

    if (argc == 0) return;

    if (!strcmp(argv[0], "help")) {
        printHelp();
    } else if (!strcmp(argv[0], "status")) {
        printStatus();
    } else if (!strcmp(argv[0], "scan") && argc == 1) {
        scanImageFiles();
        fileNeedsUpdate = true;
        Serial.println("OK: SD card rescanned successfully");
    } else if (!strcmp(argv[0], "save")) {
        saveConfigToSD();
        Serial.println("OK: Configuration saved to SD card");
    } else if (!strcmp(argv[0], "mode") && argc == 2) {
        if (!strcmp(argv[1], "photo")) {
            config.mode = MODE_PHOTO;
            fileIndex = 0;
            fileNeedsUpdate = true;
            saveConfigToSD();
            Serial.println("OK: Switched to Photo mode (saved)");
        } else if (!strcmp(argv[1], "gif")) {
            config.mode = MODE_GIF;
            fileIndex = 0;
            if (!animFolders.empty()) loadAnimFrames(animFolders[fileIndex]);
            fileNeedsUpdate = true;
            saveConfigToSD();
            Serial.println("OK: Switched to Animation mode (saved)");
        }
    } else if (!strcmp(argv[0], "rotation") && argc == 2) {
        long val;
        if (parseLong(argv[1], val) && val >= 0 && val <= 3) {
            config.rotation = (uint8_t)val;
            tft.setRotation(config.rotation);
            fileNeedsUpdate = true;
            saveConfigToSD();
            Serial.printf("OK: Screen rotation set to %u (saved)\n", config.rotation);
        } else {
            Serial.println("ERR: Invalid rotation value, use 0 to 3");
        }
    } else if (!strcmp(argv[0], "rgb") && argc == 2) {
        if (!strcmp(argv[1], "off")) {
            config.rgbEnabled = false;
            applyRgb();
            saveConfigToSD();
            Serial.println("OK: RGB LED turned off (saved)");
        } else {
            const char* hexStr = argv[1];
            if (hexStr[0] == '#') hexStr++;
            
            if (strlen(hexStr) == 6) {
                long number = strtol(hexStr, NULL, 16);
                config.red = (number >> 16) & 0xFF;
                config.green = (number >> 8) & 0xFF;
                config.blue = number & 0xFF;
                config.rgbEnabled = true;
                applyRgb();
                saveConfigToSD();
                Serial.printf("OK: RGB color changed to HEX #%s (saved)\n", hexStr);
            } else {
                Serial.println("ERR: Invalid HEX code");
            }
        }
    } else if (!strcmp(argv[0], "rgb") && argc == 4) {
        long r, g, b;
        if (parseLong(argv[1], r) && parseLong(argv[2], g) && parseLong(argv[3], b) &&
            r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
            config.red = (uint8_t)r;
            config.green = (uint8_t)g;
            config.blue = (uint8_t)b;
            config.rgbEnabled = true;
            applyRgb();
            saveConfigToSD();
            Serial.println("OK: RGB color updated (saved)");
        }
    } else if (!strcmp(argv[0], "next") && argc == 1) {
        requestNextFile(1);
        Serial.printf("OK: Next item index -> %u\n", (unsigned)fileIndex);
    } else if (!strcmp(argv[0], "brightness") && argc == 2) {
        long val;
        if (parseLong(argv[1], val) && val >= 0 && val <= 100) {
            setBrightness(val);
            saveConfigToSD();
            Serial.println("OK: Brightness updated (saved)");
        }
    } else if (!strcmp(argv[0], "interval") && argc == 2) {
        long val;
        if (parseLong(argv[1], val) && val >= 100) {
            config.intervalMs = (uint32_t)val;
            saveConfigToSD();
            Serial.printf("OK: Photo interval changed to %u ms (saved)\n", config.intervalMs);
        } else {
            Serial.println("ERR: Minimum photo interval is 100 ms");
        }
    } else if (!strcmp(argv[0], "animinterval") && argc == 2) {
        long val;
        if (parseLong(argv[1], val) && val >= 10) { // Cho phép tốc độ khung hình từ 10ms trở lên
            config.animIntervalMs = (uint32_t)val;
            saveConfigToSD();
            Serial.printf("OK: Animation frame interval changed to %u ms (saved)\n", config.animIntervalMs);
        } else {
            Serial.println("ERR: Minimum animation interval is 10 ms");
        }
    }
}

void handleSerial() {
    while (Serial.available() > 0) {
        char c = Serial.read();
        if (c == '\r') continue;
        if (c == '\n') {
            commandBuffer[commandLength] = '\0';
            handleCommand(commandBuffer);
            commandLength = 0;
            Serial.print("cyd> ");
        } else if (c >= 32 && c <= 126) {
            if (commandLength < MAX_COMMAND_LENGTH) {
                commandBuffer[commandLength++] = c;
            } else {
                commandLength = 0;
            }
        }
    }
}

void handlePhysicalButton() {
    bool reading = digitalRead(BUTTON_PIN);
    unsigned long now = millis();

    if (reading == LOW && lastButtonState == HIGH && (now - lastDebounceTime > 250)) {
        lastDebounceTime = now;
        requestNextFile(1);
        Serial.printf("OK: BOOT button pressed -> Next index: %u\n", (unsigned)fileIndex);
    }
    lastButtonState = reading;
}

void setup() {
    Serial.begin(SERIAL_BAUD);

    pinMode(BUTTON_PIN, INPUT_PULLUP); 

    if (!initSDCard()) {
        Serial.println("ERR: SD card initialization failed");
        while (true) { yield(); }
    }

    loadConfigFromSD();

    tft.init();
    tft.setRotation(config.rotation);
    tft.setSwapBytes(true);

    setBrightness(config.brightness);
    initRGB();

    scanImageFiles();

    TJpgDec.setJpgScale(1);
    TJpgDec.setCallback(tft_output);

    Serial.println("\nCYD PhotoFrame ready.");
    printHelp();
    Serial.print("cyd> ");

    lastImageTime = millis();
    fileNeedsUpdate = true;
}

void loop() {
    handleSerial();
    handlePhysicalButton();

    uint32_t now = millis();

    if (config.mode == MODE_PHOTO) {
        if (!jpgFiles.empty()) {
            if (fileNeedsUpdate || (now - lastImageTime >= config.intervalMs)) {
                lastImageTime = now;
                fileNeedsUpdate = false;

                const char* path = jpgFiles[fileIndex].c_str();

                tft.fillScreen(TFT_BLACK);
                TJpgDec.setJpgScale(1);
                TJpgDec.drawSdJpg(0, 0, path);

                fileIndex = (fileIndex + 1) % jpgFiles.size();
            }
        }
    } else {
        // Chế độ Animation Folder: Chạy tuần tự các frame với tốc độ cấu hình động `config.animIntervalMs`
        if (!animFolders.empty() && !currentAnimFrames.empty()) {
            if (fileNeedsUpdate || (now - lastImageTime >= config.animIntervalMs)) {
                lastImageTime = now;
                fileNeedsUpdate = false;

                const char* path = currentAnimFrames[animFrameIndex].c_str();
                TJpgDec.drawSdJpg(0, 0, path);

                animFrameIndex = (animFrameIndex + 1) % currentAnimFrames.size();
            }
        }
    }
    yield();
}