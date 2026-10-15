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
static constexpr uint8_t DEFAULT_BRIGHTNESS = 20;
static constexpr size_t MAX_COMMAND_LENGTH = 127;

static constexpr size_t MJ_BUF_SIZE = 64 * 1024;
static constexpr size_t MJ_BUF_MIN  = 24 * 1024;
static constexpr size_t NPOS = (size_t)-1;

static const char* VIDEO_FOLDER = "/mjpeg";

constexpr uint8_t BUTTON_PIN = 0;

enum DeviceMode : uint8_t {
    MODE_PHOTO = 0,
    MODE_VIDEO = 1
};

struct Config {
    uint32_t intervalMs = DEFAULT_INTERVAL_MS;
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
std::vector<String> videoFiles;

size_t fileIndex = 0;
uint32_t lastImageTime = 0;
bool fileNeedsUpdate = true;

unsigned long lastDebounceTime = 0;
bool lastButtonState = HIGH;

char commandBuffer[MAX_COMMAND_LENGTH + 1];
size_t commandLength = 0;

File videoFile;
bool videoOpen = false;
bool videoGeomReady = false;
int16_t videoX = 0, videoY = 0;
uint32_t videoFrames = 0;
uint32_t videoStartMs = 0;

uint8_t* mjBuf = nullptr;
size_t mjCap = 0;
size_t mjLen = 0;
size_t mjPos = 0;

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

static String baseNameOf(const char* path) {
    String s(path);
    int i = s.lastIndexOf('/');
    return (i >= 0) ? s.substring(i + 1) : s;
}

static void scanDir(const char* dirPath, bool collectPhotos) {
    File dir = SD.open(dirPath);
    if (!dir || !dir.isDirectory()) {
        if (dir) dir.close();
        return;
    }

    String prefix(dirPath);
    if (!prefix.endsWith("/")) prefix += "/";

    File file = dir.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String name = baseNameOf(file.name());
            String lower = name;
            lower.toLowerCase();

            if (!name.startsWith(".")) {
                if (lower.endsWith(".mjpeg") || lower.endsWith(".mjpg")) {
                    if (file.size() > 0) videoFiles.push_back(prefix + name);
                } else if (collectPhotos && (lower.endsWith(".jpg") || lower.endsWith(".jpeg"))) {
                    jpgFiles.push_back(prefix + name);
                }
            }
        }
        File nextFile = dir.openNextFile();
        file.close();
        file = nextFile;
    }
    dir.close();
}

void closeVideo();

void scanImageFiles() {
    closeVideo();
    jpgFiles.clear();
    videoFiles.clear();

    scanDir("/", true);
    scanDir(VIDEO_FOLDER, false);

    std::sort(jpgFiles.begin(), jpgFiles.end());
    std::sort(videoFiles.begin(), videoFiles.end());

    Serial.printf("Total files -> JPG: %u, MJPEG videos: %u\n",
                  (unsigned)jpgFiles.size(), (unsigned)videoFiles.size());

    size_t count = (config.mode == MODE_VIDEO) ? videoFiles.size() : jpgFiles.size();
    if (fileIndex >= count) fileIndex = 0;
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

static size_t findMarker(size_t from, uint8_t code) {
    while (from + 1 < mjLen) {
        const uint8_t* p = (const uint8_t*)memchr(mjBuf + from, 0xFF, mjLen - from - 1);
        if (!p) return NPOS;
        size_t i = (size_t)(p - mjBuf);
        if (mjBuf[i + 1] == code) return i;
        from = i + 1;
    }
    return NPOS;
}

enum FillResult { FILL_OK, FILL_EOF, FILL_FULL };

static FillResult mjFill(File& f) {
    if (mjPos > 0) {
        mjLen -= mjPos;
        memmove(mjBuf, mjBuf + mjPos, mjLen);
        mjPos = 0;
    }
    if (mjLen >= mjCap) return FILL_FULL;
    int n = f.read(mjBuf + mjLen, mjCap - mjLen);
    if (n <= 0) return FILL_EOF;
    mjLen += (size_t)n;
    return FILL_OK;
}

static bool mjpegNextFrame(File& f, const uint8_t*& frame, size_t& size) {
    size_t searchFrom = mjPos;
    bool haveSoi = false;

    for (;;) {
        if (!haveSoi) {
            size_t s = findMarker(searchFrom, 0xD8);
            if (s != NPOS) {
                haveSoi = true;
                mjPos = s;
                searchFrom = s + 2;
            } else {

                mjPos = (mjLen > 0) ? mjLen - 1 : 0;
                searchFrom = mjPos;
            }
        }

        if (haveSoi) {
            size_t e = findMarker(searchFrom, 0xD9);
            if (e != NPOS) {
                frame = mjBuf + mjPos;
                size = e + 2 - mjPos;
                mjPos = e + 2;
                return true;
            }

            size_t resume = (mjLen > 0) ? mjLen - 1 : 0;
            searchFrom = std::max(resume, mjPos + 2);
        }

        size_t shift = mjPos;
        FillResult r = mjFill(f);
        searchFrom -= shift;

        if (r == FILL_EOF) return false;
        if (r == FILL_FULL) {

            mjPos = std::min<size_t>(2, mjLen);
            searchFrom = mjPos;
            haveSoi = false;
        }
    }
}

void closeVideo() {
    if (videoOpen) {
        videoFile.close();
        videoOpen = false;
        uint32_t ms = millis() - videoStartMs;
        if (videoFrames > 0 && ms > 0) {
            Serial.printf("Video done: %u frames, %.1f fps\n", (unsigned)videoFrames, 1000.0f * videoFrames / ms);
        }
    }
    mjLen = 0;
    mjPos = 0;
}

bool openVideo() {
    closeVideo();
    if (videoFiles.empty() || !mjBuf) return false;
    if (fileIndex >= videoFiles.size()) fileIndex = 0;

    const char* path = videoFiles[fileIndex].c_str();
    videoFile = SD.open(path, FILE_READ);
    if (!videoFile || videoFile.isDirectory()) {
        Serial.printf("ERR: Cannot open video %s\n", path);
        if (videoFile) videoFile.close();
        return false;
    }

    Serial.printf("Playing %s (%u bytes)\n", path, (unsigned)videoFile.size());
    mjLen = 0;
    mjPos = 0;
    videoOpen = true;
    videoGeomReady = false;
    videoFrames = 0;
    videoStartMs = millis();
    tft.fillScreen(TFT_BLACK);
    return true;
}

static void setupVideoGeometry(const uint8_t* frame, size_t size) {
    videoGeomReady = true;
    videoX = 0;
    videoY = 0;
    TJpgDec.setJpgScale(1);

    uint16_t w = 0, h = 0;
    if (TJpgDec.getJpgSize(&w, &h, frame, (uint32_t)size) != JDR_OK) return;

    uint8_t scale = 1;
    while (scale < 8 && (w / scale > tft.width() || h / scale > tft.height())) scale <<= 1;
    TJpgDec.setJpgScale(scale);

    int sw = w / scale;
    int sh = h / scale;
    videoX = std::max(0, (tft.width() - sw) / 2);
    videoY = std::max(0, (tft.height() - sh) / 2);
    Serial.printf("Video frame %ux%u, scale 1/%u\n", w, h, scale);
}

void playVideoStep() {
    if (videoFiles.empty()) return;

    if (fileNeedsUpdate || !videoOpen) {
        fileNeedsUpdate = false;
        if (!openVideo()) {
            fileIndex = (fileIndex + 1) % videoFiles.size();
            delay(200);
            return;
        }
    }

    const uint8_t* frame = nullptr;
    size_t size = 0;
    if (!mjpegNextFrame(videoFile, frame, size)) {
        videoFile.seek(0);
        mjLen = 0;
        mjPos = 0;
        return;
    }

    if (!videoGeomReady) setupVideoGeometry(frame, size);

    tft.startWrite();
    TJpgDec.drawJpg(videoX, videoY, frame, (uint32_t)size);
    tft.endWrite();

    videoFrames++;
}

void printHelp() {
    Serial.println("\n--- CYD PhotoFrame Commands ---");
    Serial.println("   help");
    Serial.println("   status");
    Serial.println("   scan");
    Serial.println("   interval <milliseconds>       (Time between photo slides)");
    Serial.println("   mode <photo|video>            ('gif' is an alias of 'video')");
    Serial.println("   rotation <0-3>");
    Serial.println("   rgb <HEX> or <0-255> <0-255> <0-255>");
    Serial.println("   rgb off");
    Serial.println("   next");
    Serial.println("   brightness <0-100>");
    Serial.println("   save");
    Serial.println("--------------------------------");
}

void printStatus() {
    Serial.printf("Mode: %s\n", config.mode == MODE_PHOTO ? "Photo (JPG)" : "Video (MJPEG)");
    Serial.printf("Rotation: %u\n", config.rotation);
    Serial.printf("Photo Interval: %u ms\n", config.intervalMs);
    Serial.printf("RGB: R=%u, G=%u, B=%u (%s)\n", config.red, config.green, config.blue, config.rgbEnabled ? "On" : "Off");
    Serial.printf("Files -> JPG: %u, MJPEG: %u (Current Index: %u)\n",
                  (unsigned)jpgFiles.size(), (unsigned)videoFiles.size(), (unsigned)fileIndex);
    Serial.printf("Free heap: %u, MJPEG buffer: %u bytes\n", (unsigned)ESP.getFreeHeap(), (unsigned)mjCap);
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
        if (videoFiles.empty()) return;
        fileIndex = (direction > 0) ? (fileIndex + 1) % videoFiles.size() : (fileIndex + videoFiles.size() - 1) % videoFiles.size();
        closeVideo();
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
            closeVideo();
            config.mode = MODE_PHOTO;
            fileIndex = 0;
            fileNeedsUpdate = true;
            saveConfigToSD();
            Serial.println("OK: Switched to Photo mode (saved)");
        } else if (!strcmp(argv[1], "video") || !strcmp(argv[1], "gif")) {
            closeVideo();
            config.mode = MODE_VIDEO;
            fileIndex = 0;
            fileNeedsUpdate = true;
            saveConfigToSD();
            Serial.println("OK: Switched to Video mode (saved)");
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

    for (size_t cap = MJ_BUF_SIZE; cap >= MJ_BUF_MIN && !mjBuf; cap /= 2) {
        mjBuf = (uint8_t*)malloc(cap);
        if (mjBuf) mjCap = cap;
    }
    if (!mjBuf) Serial.println("WARN: Cannot allocate MJPEG buffer, video mode disabled");

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
        playVideoStep();
    }
    yield();
}
