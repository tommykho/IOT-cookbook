/*
 *  cyd-video-player Version 2026.10A
 *  config.h: board selection, pins, limits and intervals
 *  Board: ESP32-3248S035, ESP32-2432S028
 *  Author: tommyho510@gmail.com
 */
#pragma once

// *** BEGIN editing of your settings ...
#define ESP32_3248S035    /* 3.5" ST7796 320x480, XPT2046 touch on the display SPI bus */
//#define ESP32_2432S028  /* 2.8" ILI9341 240x320, XPT2046 touch on its own pins (media features untested) */
#define DEBUG 1           /* 1 = Serial logging at 115200, 0 = quiet */
// *** END editing of your settings ...

#if DEBUG
#define LOGF(...) Serial.printf(__VA_ARGS__)
#else
#define LOGF(...) do {} while (0)
#endif

// ---------------------------------------------------------------- Display (HSPI)
#define TFT_DC   2
#define TFT_CS   15
#define TFT_SCK  14
#define TFT_MOSI 13
#define TFT_MISO 12
#if defined(ESP32_3248S035)
#define BL_PIN 27
#define DISPLAY_SPI_SPEED 80000000L // 80MHz, drop to 40MHz if the picture is scrambled
#define DISPLAY_ROTATION 1          // landscape 480x320
#else
#define BL_PIN 21                   // On some cheap yellow display model, BL pin is 27
#define DISPLAY_SPI_SPEED 40000000L // Some model of cheap Yellow display works only at 40Mhz
#define DISPLAY_ROTATION 0          // portrait 240x320
#endif

// ---------------------------------------------------------------- Touch (XPT2046)
#if defined(ESP32_3248S035)
#define TOUCH_CS  33 // shares SCK 14 / MOSI 13 / MISO 12 with the display
#define TOUCH_IRQ 36 // low while pressed, -1 to poll over SPI instead
#else
#define T_DO   39    // Data out pin (MISO)
#define T_DIN  32    // Data in pin (MOSI)
#define T_CS   33    // Chip select pin
#define T_CLK  25    // Clock pin
#endif
#define TOUCH_SPI_SPEED   2000000 // XPT2046 max is 2.5MHz
#define TOUCH_Z_THRESHOLD 300     // minimum pressure for a valid touch
#define TOUCH_POLL_MS     50      // touch poll interval outside LVGL screens (slideshow, video)
// Fallback calibration (raw 0..4095). A 3-point calibration runs on first boot
// (and when BOOT is held 3 s in the menu) and is saved to NVS.
#define TOUCH_RAW_X_MIN 300
#define TOUCH_RAW_X_MAX 3800
#define TOUCH_RAW_Y_MIN 300
#define TOUCH_RAW_Y_MAX 3800
#define TOUCH_SWAP_XY   1
#define TOUCH_CAL_TIMEOUT_MS 20000 // skip calibration if nobody touches the screen

// ---------------------------------------------------------------- SD card (VSPI)
#define SD_CS   5
#define SD_MISO 19
#define SD_MOSI 23
#define SD_SCK  18
#define SD_SPI_SPEED 20000000L // 20Mhz, 80Mhz mounts unreliably on some cards

#define PHOTO_FOLDER "/photos"
#define AUDIO_FOLDER "/audio"
#define MJPEG_FOLDER "/mjpeg"
#define UPLOAD_TMP   "/upload.tmp"
#define MAX_LIST_FILES 100 // files listed per folder

// ---------------------------------------------------------------- Buttons and audio
#ifndef BOOT_PIN
#define BOOT_PIN 0                   // Boot pin (also defined by the board variant)
#endif
#define BOOT_BUTTON_DEBOUCE_TIME 400 // Debounce time when reading the boot button in milliseconds
#define SPEAKER_DAC_PIN 26           // DAC2 drives the onboard amplifier
#define AUDIO_DEFAULT_VOLUME 60      // percent
#define AUDIO_MAX_GAIN 1.0f          // gain at 100% volume

// ---------------------------------------------------------------- Media
#define MJPEG_BUF_SIZE 64000      // Max bytes per JPEG frame, larger frames are dropped
                                  // The two MJPEG buffers double as the shared media arena
#define SLIDE_INTERVAL_MS 5000    // time per photo in the slideshow
#define GIF_MAX_SHOW_MS   20000   // a long GIF plays at most this long per slide
#define GIF_MAX_UPSCALE   2       // small GIFs are scaled up by at most this integer factor

// ---------------------------------------------------------------- Receive (Wi-Fi upload)
#define AP_SSID_PREFIX "CYD-"     // + last 4 hex digits of the MAC
#define AP_PASS_LEN    8          // random WPA2 password per session
#define AP_CHANNEL     6
#define RECEIVE_IDLE_TIMEOUT_MS (3UL * 60 * 1000) // stop the AP after 3 min without activity
#define UPLOAD_WRITE_CHUNK 32768  // bytes buffered before each SD write
#define MAX_JPG_BYTES   (500UL * 1024)
#define MAX_GIF_BYTES   (2UL * 1024 * 1024)
#define MAX_AUDIO_BYTES (8UL * 1024 * 1024)
#define MAX_MJPEG_BYTES (64UL * 1024 * 1024)
