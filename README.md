# 🖼️ ESP32 CYD Custom Photo & Animation Frame

<img width="9346" height="5451" alt="1791710886704" src="https://github.com/user-attachments/assets/657ef804-0049-4a1a-9e7d-ef228b0ccdc2" />

High-performance electronic photo frame and animation (GIF/Video) player optimized for the **ESP32-2432S028 (CYD)** board featuring an ILI9341 display, XPT2046 touch controller, and an SD card slot. This project replaces the traditional, unstable `AnimatedGIF` library with a robust folder-based JPG frame runner, eliminating screen artifacts and providing smooth playback speeds.

### 🛠️ How to Install & Setup
Choose your preferred development environment:

#### Option A: PlatformIO (Recommended - Easiest)
1. Clone or download this repository.
2. Open the project folder in **Visual Studio Code** with the PlatformIO extension installed.
3. PlatformIO will automatically fetch all required dependencies (`TFT_eSPI`, `TJpg_Decoder`) and board configurations via `platformio.ini`.
4. Connect your ESP32 CYD via USB, compile, and upload.

#### Option B: Arduino IDE
1. Open **Arduino IDE**, go to **File > Preferences**, and add the ESP32 board manager URL:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
2. Go to **Tools > Board > Boards Manager**, search for `esp32` by Espressif Systems, and install version `2.x.x` or higher.
3. Select your board (usually **ESP32 Dev Module** or equivalent ESP32-WROOM-32).
4. Install the required libraries via **Library Manager**:
   * `TFT_eSPI` (Bodmer)
   * `TJpg_Decoder` (Bodmer)
5. **Important TFT_eSPI Configuration:** You must configure `User_Setup.h` inside your `TFT_eSPI` library folder to match the CYD hardware pins (ILI9341 display, XPT2046 touch, and SD card SPI pins).
6. Compile and upload the firmware to your board.

### 🚀 1. Quick Conversion Scripts (Just Drop & Run)

You don't need complex setups on your PC. Simply drop the conversion scripts into your MicroSD card alongside your media files:

1. **`convert_image.sh`**: Automatically resizes, crops, and optimizes all loose images (`.jpg`, `.png`, etc.) in the directory to the exact 320x240 resolution required for single photo mode.
2. **`convert_media2frame.sh`**: Automatically extracts and converts videos (`.mp4`, `.mov`, etc.) and GIFs into individual sequential JPG frames structured inside separate `anim_*` folders (automatically skips already processed files).

Run them directly via terminal/shell or just double click:
```bash
./convert_image.sh
./convert_media2frame.sh
```

### 🎛️ 2. Using the BOOT Button & Serial Monitor (SM)

* **Physical BOOT Button (Single Click - GPIO 0):** Advances to the next image (in Photo Mode) or switches to the next animation folder (in Animation Mode). Built-in debounce logic prevents accidental double-triggers.
* **Serial Monitor (SM) Commands:** Connect via Serial Monitor (Arduino IDE or PlatformIO) at **Baudrate: `115200`**. Type the commands below and press Enter:

| Command | Detailed Description |
| :--- | :--- |
| `help` | Displays the complete list of supported commands |
| `status` | Checks current system status (Active Mode, Rotation, Speeds, Total Files) |
| `scan` | Rescans the SD card to index newly added images and animation folders |
| `mode photo` | Switches to single static photo playback mode (`.jpg` files in the root directory) |
| `mode gif` | Switches to folder-based animation playback mode (`anim_*` folders) |
| `next` | Skips to the next item (acts the same as pressing the physical BOOT button) |
| `interval <ms>` | Sets the delay time between static photos (minimum 100ms) |
| `animinterval <ms>` | Sets the animation frame speed/delay (e.g., `animinterval 50` for fast playback, `30` for ultra-fast) |
| `brightness <0-100>` | Adjusts screen backlight brightness (TFT_BL) |
| `rotation <0-3>` | Changes hardware display orientation (0 to 3) |
| `rgb <HEX>` | Changes status RGB LED color (e.g., `rgb #FF00FF` or use three RGB values `r g b`) |
| `save` | Manually saves current settings to `/config.txt` on the SD card |

### 📂 SD Card Directory Structure
Your MicroSD card layout should look like this:
```text
📂 MicroSD Card (FAT32)
├── 📂 anim_XiaomiSu7/         # Animation folder extracted from video
├── 📂 converted_landscape/    # Processed single images folder (if applicable)
├── config.txt                 # Auto-saved system configuration file
├── convert_image.sh           # Image optimization script
└── convert_media2frame.sh     # Video/GIF conversion script
```
