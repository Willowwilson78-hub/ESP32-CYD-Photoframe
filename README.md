# 🖼️ ESP32 CYD Custom Photo & Video Frame

<img width="9346" height="5451" alt="1791710886704" src="https://github.com/user-attachments/assets/657ef804-0049-4a1a-9e7d-ef228b0ccdc2" />

High-performance electronic photo frame and video (GIF/Video) player optimized for the **ESP32-2432S028 (CYD)** board featuring an ILI9341 display, XPT2046 touch controller, and an SD card slot. This project replaces the traditional, unstable `AnimatedGIF` library, and the slow approach of storing thousands of separate JPG frames, with a streaming **MJPEG** player: each video is a single `.mjpeg` file that is read sequentially from the SD card and decoded straight from RAM. The result is smoother playback, no screen artifacts, and almost no loading time.

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
5. **Important TFT_eSPI Configuration:** You must configure `User_Setup.h` inside your `TFT_eSPI` library folder to match the CYD hardware pins (ILI9341 display, XPT2046 touch, and SD card SPI pins). A higher `SPI_FREQUENCY` (e.g. 40–55 MHz, as far as your display tolerates) noticeably improves video smoothness.
6. Compile and upload the firmware to your board.

### 🚀 1. Quick Conversion Scripts (Just Drop & Run)

Simply drop the conversion scripts into a folder alongside your media files and run them on your PC (**[ffmpeg](https://ffmpeg.org/)** must be installed), then copy the results to your MicroSD card:

1. **`convert_image.sh`**: Automatically resizes, crops, and optimizes all loose images (`.jpg`, `.png`, etc.) in the directory to the exact 320x240 resolution required for single photo mode.
2. **`convert_media2mjpeg.sh`**: Automatically converts videos (`.mp4`, `.mov`, etc.) and GIFs into a single 320x240 `.mjpeg` file per video, saved inside the `mjpeg/` folder (automatically skips already processed files).

Run them from a terminal that understands shell scripts (Linux, macOS, or **Git Bash / WSL** on Windows):
```bash
./convert_image.sh
./convert_media2mjpeg.sh
```

> **Windows users:** double-clicking a `.sh` file normally just opens it in an editor instead of running it. Install [Git for Windows](https://git-scm.com/) (it includes Git Bash) or WSL, open a terminal inside the folder that contains the script and your videos, and run the commands above. `ffmpeg` must also work in that terminal (`ffmpeg -version` should print a version).

#### Changing FPS and quality (optional)

The defaults (24 fps, quality 5) work fine, so you don't have to change anything. If you want different values, there are two ways:

**Option 1 – Type the values in front of the command.** This applies to this run only and changes nothing in the file:
```bash
./convert_media2mjpeg.sh                    # default: 24 fps, quality 5
FPS=30 ./convert_media2mjpeg.sh             # change only the fps
FPS=30 QUALITY=7 ./convert_media2mjpeg.sh   # change both
```

**Option 2 – Edit the script in any text editor** (Notepad, VS Code, ...). Open `convert_media2mjpeg.sh` and change the numbers after `:-` near the top of the file, then save:
```bash
FPS="${FPS:-24}"          # change 24 to the fps you want
QUALITY="${QUALITY:-5}"   # change 5 to the quality you want
```
Change only the number and keep the rest of the line (quotes, braces and `:-`) exactly as it is.

> **Troubleshooting:** if the script fails with an error such as `$'\r': command not found`, your editor (or Git) saved it with Windows line endings (CRLF). Convert it back to Unix line endings (LF): in VS Code click `CRLF` in the bottom-right corner and choose `LF`, or run `dos2unix convert_media2mjpeg.sh`.

What the two settings mean:

| Variable | Default | Description |
| :--- | :--- | :--- |
| `FPS` | `24` | Frames per second of the output video (integer). The player shows frames as fast as the ESP32 can decode them, so for natural-speed playback set this close to the average FPS printed in the Serial Monitor |
| `QUALITY` | `5` | ffmpeg JPEG quality from `2` (best, largest) to `31` (worst, smallest). Values around `5`–`9` work best; heavier frames decode more slowly |

### 🎛️ 2. Using the BOOT Button & Serial Monitor (SM)

* **Physical BOOT Button (Single Click - GPIO 0):** Advances to the next image (in Photo Mode) or switches to the next `.mjpeg` video (in Video Mode). Built-in debounce logic prevents accidental double-triggers.
* **Serial Monitor (SM) Commands:** Connect via Serial Monitor (Arduino IDE or PlatformIO) at **Baudrate: `115200`**. Type the commands below and press Enter:

| Command | Detailed Description |
| :--- | :--- |
| `help` | Displays the complete list of supported commands |
| `status` | Checks current system status (Active Mode, Rotation, Speeds, Total Files, free memory) |
| `scan` | Rescans the SD card to index newly added images and videos |
| `mode photo` | Switches to single static photo playback mode (`.jpg` files in the root directory) |
| `mode video` | Switches to video playback mode (`.mjpeg` files in the `mjpeg/` folder or the root directory). `mode gif` still works as an alias |
| `next` | Skips to the next item (acts the same as pressing the physical BOOT button) |
| `interval <ms>` | Sets the delay time between static photos (minimum 100ms) |
| `brightness <0-100>` | Adjusts screen backlight brightness (TFT_BL) |
| `rotation <0-3>` | Changes hardware display orientation (0 to 3) |
| `rgb <HEX>` | Changes status RGB LED color (e.g., `rgb #FF00FF` or use three RGB values `r g b`) |
| `rgb off` | Turns the RGB LED off |
| `save` | Manually saves current settings to `/config.txt` on the SD card |

When a video finishes, the real average frame rate is printed to the Serial Monitor, which helps when tuning `QUALITY` and `FPS`. With several videos, they play one after another; a single video loops.

### 📂 SD Card Directory Structure
Your MicroSD card layout should look like this:
```text
📂 MicroSD Card (FAT32)
├── 📂 mjpeg/                  # Videos converted to single .mjpeg files (e.g. XiaomiSu7.mjpeg)
├── 📂 converted_landscape/    # Processed single images folder (if applicable)
├── config.txt                 # Auto-saved system configuration file
├── convert_image.sh           # Image optimization script
└── convert_media2mjpeg.sh     # Video/GIF to MJPEG conversion script
```
