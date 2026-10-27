# Cheap Yellow Display Video Player (ESP32-2432S028)

## Media player with Wi-Fi upload (2026.10A, ESP32-3248S035)
Photo slideshow (JPG, GIF), MJPEG video and MP3/WAV audio from the SD card. Media is sent from a phone over the device's own Wi-Fi: no cable, no SD removal, no internet.

**Status: compiles, not yet tested on hardware.**

### Flow
1. Power on: slideshow from `/photos` (videos from `/mjpeg` if there are no photos, else the menu).
2. Tap the screen: main menu with Photos, Slideshow, Video, Audio and Receive. Wi-Fi client and Settings are greyed-out placeholders.
3. **Receive** starts an access point `CYD-XXXX` with a random password per session and shows a Wi-Fi QR code plus the SSID and password.
4. Scan the QR with the phone camera. Android shows "Sign in to network" and opens the upload page; otherwise open `http://192.168.4.1`.
5. Tap **Upload** and pick files. Photos are resized in the browser to fit 480x320 and re-encoded as JPEG (EXIF rotation honoured).
6. After the last file the AP and web server stop. Photos open in the slideshow, audio starts playing, and video plays.

The AP also stops on Cancel or after 3 minutes without activity.

### Controls
| Screen | Tap | BOOT button |
|---|---|---|
| Slideshow, Video | open the menu | next photo / video |
| Photos | left third = previous, right third = next, centre = menu | next photo |
| Menu | select | hold 3 s = recalibrate touch |

### Setup
- Board "ESP32 Dev Module", **Partition Scheme "Huge APP (3MB No OTA/1MB SPIFFS)"**. The default 1.2MB app partition is too small.
- Libraries (Library Manager), compiled with esp32 core 3.2.0:

  | Library | Version |
  |---|---|
  | GFX Library for Arduino | 1.6.9 |
  | JPEGDEC | 1.8.4 |
  | AnimatedGIF | 2.2.0 |
  | lvgl | 9.6.0 (9.4.0 also compiles) |
  | ESP8266Audio | **2.2.0**. 2.3.0+ fails to compile on core 3.2.x (`AudioOutputPDM.cpp` needs ESP-IDF 5.5), so use core 3.3+ with those |
- LVGL is configured by `build_opt.h` in this folder. No `lv_conf.h` copy is needed. After editing `build_opt.h`, delete the build cache so libraries recompile.
- Board, pins, folders, size limits and intervals: `config.h`.
- Touch calibration (3 taps on crosses) runs on first boot and is saved to NVS. If nobody taps within 20 s, defaults are used.

### SD card layout (FAT32)
| Folder | Files | Upload limit |
|---|---|---|
| `/photos` | `.jpg` `.jpeg` `.gif` | JPG 500 KB, GIF 2 MB |
| `/mjpeg` | `.mjpeg` `.mjpg` | 64 MB |
| `/audio` | `.mp3` `.wav` | 8 MB |

Uploads are sanitised (`[A-Za-z0-9._-]`), checked by extension and magic bytes, and renamed with the phone's timestamp when the name already exists.

### Known limits
- No PSRAM: nothing is framebuffered. The two 64 KB MJPEG buffers are shared as a media arena (MP3 decoder, JPEG/GIF decoder, upload buffer). Audio therefore stops when a video plays or Receive starts. It keeps playing during the slideshow.
- GIF: max 480 px wide (AnimatedGIF `MAX_WIDTH`, checked in the browser). Scaled on the device by integer factors: up to 2x, or down by dropping lines and pixels.
- JPEG: scaled by 1/2, 1/4 or 1/8 only. Phone uploads are already resized.
- Audio: mono, 8-bit, internal DAC on GPIO26 into the onboard amplifier. Quality is basic.
- Video must be MJPEG. Convert MP4 with the ffmpeg command below before uploading.
- Uploads run at an estimated 300-600 KB/s over the synchronous WebServer, so a 64 MB video takes minutes.
- Android's captive-portal window may not open file pickers on some phones. If Upload does nothing, open `http://192.168.4.1` in Chrome while connected to `CYD-XXXX`.
- `ESP32_2432S028` still compiles with the media features but is untested.

## ESP32-3248S035 (3.5" ST7796) port
Select the board in `config.h` (`#define ESP32_3248S035`, the default). Touch shares the display SPI bus (XPT2046, CS 33, IRQ 36). A tap during a video opens the menu, and BOOT skips to the next video.
Plays landscape (480x320), smaller videos are centered. Frames play as fast as the ESP32 can decode+draw them, so match the encode fps to what it reaches or video runs slow-motion. Measured:
- 320x214 @ 24 fps: 22.6 fps over a full 2400-frame video (decode 27 ms, draw 17 ms), about 6% slower than real time — recommended
- 480x320 @ 10 fps: 11 fps (decode 52 ms, draw 38 ms)
```
ffmpeg -y -i input.mp4 -pix_fmt yuvj420p -q:v 7 -vf "fps=24,scale=320:-2:flags=lanczos" -an output.mjpeg
```
SD card must be FAT32 (exFAT is not supported); use a ≤32GB partition.
## Instructions for Kiwi's screens
### Uploading new software/firmware to the board
1. Install Arduino IDE https://www.arduino.cc/en/software/
2. In the Arduino IDE settings, configure "Additional boards manager URLs" with `https://espressif.github.io/arduino-esp32/package_esp32_index.json`. 
3. In the Arduino IDE Board manager, add the board "esp32 by Espressif Systems".
4. In the Arduino IDE Library manager, add "GFX Library for Arduino", "JPEGDEC", "AnimatedGIF", "lvgl" and "ESP8266Audio".
5. In the Arduino IDE Sketch menu, choose "Include Library..." --> "Add ZIP library". Select "TFT_Touch_v0.3.zip"" from this root folder.
6. In the Arduino IDE Tools menu, select the board "ESP32 Dev Module", Partition Scheme "Huge APP (3MB No OTA/1MB SPIFFS)" and upload speed 115200.

You can now modify "esp32-2432S028_video_player.ino" in the Arduino IDE and upload it to the board.

### Add videos
1. Get the videos from youtube, using yt-dlp: `brew install yt-dlp`.
2. Download a video: `yt-dlp -o myvideo.mp4 --merge-output-format mp4 -f "bv*" https://www.youtube.com/watch\?v\=6jVHREdafT8`
3. Use ffmpeg to convert the video into mjpeg format. There are a bunch of different parameters you can use to tweak the format to fit the tiny screen, here's an example: 
```
ffmpeg -y -i myvideo.mp4 -pix_fmt yuvj420p -q:v 2 -vf "transpose=1,fps=14,scale=240:320:flags=lanczos" myvideo.mjpeg
```
Unfortunately, VLC wont play these mjpeg files. You can however put it in an .avi container and play it: `ffmpeg -i myvideo.mjpeg -c:v copy myvideo.avi`

### Converting from the 39c3 video generator
Video generator: https://39c3.o1y.de/

Crop out the center part of the square video file, scale it to size:
`ffmpeg -y -i 39C3_06.webm -pix_fmt yuvj420p -q:v 2 -vf "crop=iw:ih/2:0:ih/4,transpose=1,fps=14,scale=240:320:flags=lanczos" 39c3_06.mjpeg`



#### Add to the SD card
1. Remove the micro SD card from the board. You don't need to power down when doing this.
2. Using a SD card reader, you can add video files to the /mjpeg folder. The files need to be in the /mjpeg folder.
3. Reinsert the SD card in the board.


## Youtube Tutorial

<a href="https://www.buymeacoffee.com/thelastoutpostworkshop" target="_blank">
<img src="https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png" alt="Buy Me A Coffee">
</a>

>⚠️ Make sure you have the board model ESP32-2432S028 with the **ILI9341 Display controller** — not the ST7789 (parallel) controller, which isn't supported by the graphic library [GFX Library for Arduino](https://github.com/moononournation/Arduino_GFX)

[<img src="https://github.com/thelastoutpostworkshop/images/blob/main/Cheay%20Yellow%20Display-3.png" width="500">](https://youtu.be/jYcxUgxz9ks)

## Notes
> Some model of Cheap Yellow Display works only at speed of 40Mhz, change the DISPLAY_SPI_SPEED to 40000000L:
```cmd
#define DISPLAY_SPI_SPEED 40000000L // 40MHz 
```

⚠️ One of Kiwi's boards needs this. You can tell if you have this lemon board because the screen is scrambled on the normal 80Mhz setting. 

## 🎬 How to Use These FFmpeg Commands

Each of the following commands generates a `.mjpeg` file — a Motion JPEG video format — from an input `.mp4` or `.mov` video, optimized for use in frame-by-frame playback with an SD card reader.

Make sure you have [FFmpeg](https://ffmpeg.org/download.html) installed and accessible from your terminal or command prompt.

---

## Convert video of 16:9 (horizontal) to aspect ratio to 4:3
```cmd
ffmpeg -y -i input.mp4 -pix_fmt yuvj420p -q:v 7 -vf "transpose=1,fps=24,scale=-1:320:flags=lanczos" output.mjpeg
```

## Convert video of 9:16 (vertical) to aspect ratio to 3:4
```cmd
ffmpeg -y -i input.mp4 -pix_fmt yuvj420p -q:v 7 -vf "fps=24,scale=-1:320:flags=lanczos" output.mjpeg
```
## Command for a horizontal video already of aspect ratio 4:3
```cmd
ffmpeg -y -i cropped_4x3.mp4 -pix_fmt yuvj420p -q:v 7 -vf "transpose=1,fps=24,scale=240:320:flags=lanczos" final_240x320.mjpeg
```

## Command for a vertical video already of aspect ratio 3:4
```cmd
ffmpeg -y -i cropped.mp4 -pix_fmt yuvj420p -q:v 7 -vf "fps=24,scale=240:320:flags=lanczos" scaled.mjpeg
```

## Example To reduce brightness of the output video (helps with bright colors)
see [issue 7](https://github.com/thelastoutpostworkshop/esp32-2432S028_video_player/issues/7)
```cmd
ffmpeg -y -i "input.mp4" -q:v 6 -filter:v "scale=-1:ih/2,eq=brightness=-0.05" -c:v mjpeg -an "output.mjpeg"
```

### Options explained
- -pix_fmt yuvj420p: Ensures JPEG-compatible pixel format
- -q:v 7: Controls image quality (lower is better; 1 = best, 31 = worst)
- -vf: Specifies the video filters:
- fps=24: Extracts 24 frames per second
- scale: Resizes the video
- transpose=1: Rotates the video 90° clockwise
- eq: Applies an equalizer filter to slightly darken the video. Values range from -1.0 to 1.0.
- .mjpeg: Output format used when streaming or storing a series of JPEG frames as a video
