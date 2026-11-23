<div align="center">

# 🖼️ ESP32 CYD Photo & Video Frame

**A smooth, silent digital photo frame and MJPEG video player for the ESP32-2432S028 (CYD) board.**

No cloud, no app, no subscription — just an SD card and a $10 display.

![Platform](https://img.shields.io/badge/platform-ESP32-blue?logo=espressif&logoColor=white)
![Board](https://img.shields.io/badge/board-ESP32--2432S028%20%2F%20CYD-informational)
![Display](https://img.shields.io/badge/display-ILI9341%20320%C3%97240-success)
![Firmware](https://img.shields.io/badge/firmware-Arduino%20%7C%20PlatformIO-orange)
![Media](https://img.shields.io/badge/media-MJPEG%20%2B%20JPEG-important)
![Requires](https://img.shields.io/badge/requires-ffmpeg-3C873A?logo=ffmpeg&logoColor=white)

[▶️ Watch the demo](https://files.catbox.moe/728548.mp4) &nbsp;·&nbsp; [Quick start](#-quick-start) &nbsp;·&nbsp; [Controls](#-controls) &nbsp;·&nbsp; [Troubleshooting](#-troubleshooting)

</div>

<img width="9346" height="5451" alt="ESP32 CYD photo frame in action" src="https://github.com/user-attachments/assets/657ef804-0049-4a1a-9e7d-ef228b0ccdc2" />

---

## ✨ Overview

This project turns an **ESP32-2432S028 (CYD)** board — ILI9341 display, XPT2046 touch controller and an SD card slot — into a self-contained photo frame that also plays video and GIFs.

Instead of the two usual approaches (an unstable `AnimatedGIF` library, or storing thousands of separate JPG frames), every video is converted into **a single `.mjpeg` file**: the firmware reads it sequentially from the SD card and decodes each frame straight from RAM. The result is smoother playback, no screen artifacts, and almost no loading time.

|  | Classic approach | **This project** |
| :--- | :--- | :--- |
| **Storage** | Thousands of separate JPG frames | One `.mjpeg` file per video |
| **Reading** | Random access across many files | Sequential streaming from the SD card |
| **Decoding** | Frame-by-frame from files, library dependent | Directly from RAM with `TJpg_Decoder` |
| **Result** | Stutters, artifacts, long loading pauses | Smooth playback, clean image, near-zero loading |

---

## 📑 Table of Contents

| | |
| :--- | :--- |
| [✨ Overview](#-overview) | [🛠️ Installation](#-installation) |
| [⚡ Quick Start](#-quick-start) | [🎬 Preparing Your Media](#-preparing-your-media) |
| [🧠 How It Works](#-how-it-works) | [🎛️ Controls](#-controls) |
| [🧰 Hardware Requirements](#-hardware-requirements) | [💾 SD Card Layout](#-sd-card-layout) |
| [🚀 Performance Tuning](#-performance-tuning) | [🩺 Troubleshooting](#-troubleshooting) |
| [❓ FAQ](#-faq) | [🤝 Contributing](#-contributing) |

---

## ⚡ Quick Start

1. **Flash the firmware** — open the project in VS Code with PlatformIO, then *Build & Upload* ([details](#-installation)).
2. **Convert your media** — run `./convert_image.sh` and `./convert_media2mjpeg.sh` next to your photos and videos ([details](#-preparing-your-media)).
3. **Copy to the SD card** — put the results on a FAT32 microSD, insert it, reset the board. Done.

---

## 🧠 How It Works

```text
  video.mp4 / animation.gif
            │
            │  convert_media2mjpeg.sh  (ffmpeg)
            ▼
     video.mjpeg  ──copy──►  MicroSD card (FAT32)
                                  │
                                  │  sequential read
                                  ▼
                          RAM frame buffer
                                  │
                                  │  TJpg_Decoder
                                  ▼
                    TFT_eSPI → ILI9341  (320×240)
```

The MJPEG container is nothing more than a stream of JPEG images glued together. Because the file is read front-to-back, the SD card never has to seek, and every frame is decoded from memory — exactly the pattern the ESP32 is good at.

---

## 🧰 Hardware Requirements

| Item | Detail |
| :--- | :--- |
| **Board** | ESP32-2432S028 — the "CYD" (ESP32-WROOM-32 based) |
| **Display** | ILI9341 TFT, 320×240, SPI |
| **Touch controller** | XPT2046 (on-board) |
| **Storage** | microSD card, formatted **FAT32** |
| **Controls** | BOOT button on **GPIO 0**, plus the Serial Monitor |
| **Connection** | USB cable (data, not charge-only) |
| **Software** | PlatformIO *or* Arduino IDE, and `ffmpeg` on your PC |

---

## 🛠️ Installation

Choose the environment you prefer.

### Option A — PlatformIO (recommended, easiest)

| Step | Action |
| :---: | :--- |
| 1 | Clone or download this repository. |
| 2 | Open the project folder in **Visual Studio Code** with the PlatformIO extension installed. |
| 3 | PlatformIO automatically fetches the dependencies (`TFT_eSPI`, `TJpg_Decoder`) and the board configuration from `platformio.ini`. |
| 4 | Connect the ESP32 CYD over USB, then **Build** and **Upload**. |

### Option B — Arduino IDE

| Step | Action |
| :---: | :--- |
| 1 | **File ▸ Preferences** → add the board manager URL:<br>`https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json` |
| 2 | **Tools ▸ Board ▸ Boards Manager** → search `esp32` (Espressif Systems) → install version `2.x.x` or newer. |
| 3 | Select your board — usually **ESP32 Dev Module** (or the equivalent ESP32-WROOM-32 entry). |
| 4 | **Library Manager** → install `TFT_eSPI` (Bodmer) and `TJpg_Decoder` (Bodmer). |
| 5 | Configure `User_Setup.h` inside the `TFT_eSPI` library folder to match the CYD pinout (ILI9341 display, XPT2046 touch, SD card SPI pins). |
| 6 | Compile and upload the firmware. |

> **Tip — free performance:** a higher `SPI_FREQUENCY` in `User_Setup.h` (e.g. **40–55 MHz**, as far as your display tolerates) noticeably improves video smoothness. If you see glitches, step back down.

---

## 🎬 Preparing Your Media

Drop the two conversion scripts into a folder **next to your media files**, run them on your PC, then copy the results to the microSD card. **[ffmpeg](https://ffmpeg.org/) must be installed.**

| Script | What it does |
| :--- | :--- |
| **`convert_image.sh`** | Resizes, crops and optimizes every loose image (`.jpg`, `.png`, …) to the exact **320×240** needed for photo mode. |
| **`convert_media2mjpeg.sh`** | Converts videos (`.mp4`, `.mov`, …) and GIFs into one **320×240 `.mjpeg`** per clip, saved in the `mjpeg/` folder. Already processed files are skipped. |

```bash
./convert_image.sh
./convert_media2mjpeg.sh
```

Run them from a terminal that understands shell scripts — **Linux**, **macOS**, or **Git Bash / WSL** on Windows.

> **Windows users:** double-clicking a `.sh` file just opens it in an editor. Install [Git for Windows](https://git-scm.com/) (it ships Git Bash) or WSL, open a terminal *inside the folder* containing the script and your videos, and run the commands above. `ffmpeg -version` must print a version in that same terminal.

### Adjusting FPS and quality (optional)

The defaults — **24 fps, quality 5** — work well, so you can leave them alone. Two ways to change them:

**1. Inline, for a single run** (nothing is modified on disk):

```bash
./convert_media2mjpeg.sh                    # default: 24 fps, quality 5
FPS=30 ./convert_media2mjpeg.sh             # only the frame rate
FPS=30 QUALITY=7 ./convert_media2mjpeg.sh   # both values
```

**2. Edit the script** in any text editor (Notepad, VS Code, …) — change the numbers after `:-` near the top, then save:

```bash
FPS="${FPS:-24}"          # change 24 to the frame rate you want
QUALITY="${QUALITY:-5}"   # change 5 to the quality you want
```

Change only the number and keep the rest of the line — quotes, braces and `:-` — exactly as it is.

| Variable | Default | Description |
| :--- | :---: | :--- |
| `FPS` | `24` | Frames per second of the output video (integer). The player shows frames as fast as the ESP32 can decode them, so for natural-speed playback set this close to the average FPS printed in the Serial Monitor. |
| `QUALITY` | `5` | ffmpeg JPEG quality, from `2` (best, largest files) to `31` (worst, smallest). Values around `5`–`9` work best; heavier frames decode more slowly. |

---

## 🎛️ Controls

### Physical button

| Input | Action |
| :--- | :--- |
| **BOOT button — single click (GPIO 0)** | Next image in photo mode, or next `.mjpeg` video in video mode. Built-in debounce prevents accidental double triggers. |

### Serial Monitor

Connect at **115200 baud** (Arduino IDE or PlatformIO), type a command and press Enter.

| Command | Description |
| :--- | :--- |
| `help` | List all supported commands. |
| `status` | Current system status: active mode, rotation, speeds, total files, free memory. |
| `scan` | Rescan the SD card to index newly added photos and videos. |
| `mode photo` | Play static photos (`.jpg` files in the root directory). |
| `mode video` | Play videos (`.mjpeg` files in `mjpeg/` or the root). `mode gif` works as an alias. |
| `next` | Skip to the next item — same as pressing the BOOT button. |
| `interval <ms>` | Delay between static photos (minimum `100` ms). |
| `brightness <0-100>` | Screen backlight brightness (`TFT_BL`). |
| `rotation <0-3>` | Hardware display orientation. |
| `rgb <HEX>` | Status RGB LED colour, e.g. `rgb #FF00FF` — or pass three values `r g b`. |
| `rgb off` | Turn the RGB LED off. |
| `save` | Write the current settings to `/config.txt` on the SD card. |

When a video ends, the **real average frame rate** is printed to the Serial Monitor — handy for tuning `QUALITY` and `FPS`. With several videos loaded they play one after another; a single video loops.

---

## 💾 SD Card Layout

```text
📂 MicroSD Card (FAT32)
├── 📂 mjpeg/                  # Videos as single .mjpeg files (e.g. XiaomiSu7.mjpeg)
├── 📂 converted_landscape/    # Processed still images (if you use it)
├── config.txt                 # Auto-saved system configuration
├── convert_image.sh           # Image optimization script
└── convert_media2mjpeg.sh     # Video/GIF → MJPEG conversion script
```

---

## 🚀 Performance Tuning

| Goal | What to change |
| :--- | :--- |
| **Smoother video** | Raise `SPI_FREQUENCY` in `User_Setup.h` (40–55 MHz) — stop as soon as you see artifacts. |
| **Faster decoding** | Increase `QUALITY` (e.g. `7`–`9`): smaller frames decode quicker, at the cost of image detail. |
| **Natural playback speed** | Match the average FPS reported in the Serial Monitor, then set that value as `FPS` for the next conversion. |
| **Longer battery / dimmer screen** | Lower `brightness` (and `save` it). |
| **Slow or flaky SD reads** | Use a genuine, well-formatted FAT32 card — a slow card caps your frame rate. |
| **No stutter on start** | Keep the number of files per folder moderate so `scan` stays quick. |

---

## 🩺 Troubleshooting

| Symptom | Fix |
| :--- | :--- |
| `$'\r': command not found` when running a script | Windows (CRLF) line endings. In VS Code click `CRLF` in the bottom-right corner and pick `LF`, or run `dos2unix convert_media2mjpeg.sh`. |
| `ffmpeg: command not found` | Install ffmpeg and make sure `ffmpeg -version` works in the same terminal. |
| Photos don't appear | `.jpg` files must sit in the **root** of the card; type `scan` after copying. |
| Videos don't appear | Files must end in `.mjpeg` and live in `mjpeg/` or the root; run `scan`. |
| Video plays but is too fast / too slow | Reconvert with a `FPS` close to the average FPS printed when the clip ends. |
| Screen glitches during playback | Lower `SPI_FREQUENCY` a step, or use a better SD card. |
| Settings reset after reboot | Run `save` (`/config.txt` must be writable on the card). |
| Nothing on screen after upload | Check the `TFT_eSPI` pin configuration in `User_Setup.h` and that the board is the CYD variant. |

---

## ❓ FAQ

**Can it play an MP4 straight from the card?**
No. The ESP32 decodes JPEG, not H.264 — run the conversion script first; the result is a single `.mjpeg` file.

**Why MJPEG instead of GIF?**
GIF decoding on the ESP32 is slow and memory-hungry, and the classic `AnimatedGIF` path is unstable. MJPEG gives you far better quality *and* speed for the same storage.

**Do I have to re-flash to add new media?**
No. Drop the files on the card and either reset the board or type `scan`.

**Can I use a different ESP32 board?**
The project is tuned for the CYD. Another board needs its own `TFT_eSPI` pin setup, and the SD card must share the same SPI wiring scheme.

**What resolution are photos and videos?**
320×240 — the native panel size. The scripts handle resizing and cropping for you.

**Does it need Wi-Fi?**
No. Everything runs offline from the SD card.

---

## 🤝 Contributing

Issues and pull requests are welcome. If you report a problem, please include your board revision, `TFT_eSPI` configuration, the Serial Monitor output, and the conversion settings used — it makes debugging much faster.

---

## 📄 License

See the `LICENSE` file in this repository for details.

<div align="center">

---

**If this project saved you some time, consider giving it a ⭐ — it really helps.**

Made with ❤️ for the ESP32 CYD community.

</div>
