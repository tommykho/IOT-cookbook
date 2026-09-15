/*
 *  cyd-video-player Version 2026.09A
 *  Board: ESP32-3248S035, ESP32-2432S028
 *  Based on: https://github.com/kiwiholmberg/cyd-video-player (MIT)
 *  Author: tommyho510@gmail.com
 */
// Tutorial : https://youtu.be/jYcxUgxz9ks
// Use board "ESP32 Dev Module" (last tested on v3.2.0)

// *** BEGIN editing of your settings ...
#define ESP32_3248S035    /* 3.5" ST7796 320x480 */
//#define ESP32_2432S028  /* 2.8" ILI9341 240x320 */
// *** END editing of your settings ...

#include <Arduino_GFX_Library.h> // Install "GFX Library for Arduino" with the Library Manager (last tested on v1.6.0)
                                 // Install "JPEGDEC" with the Library Manager (last tested on v1.8.2)
#define MJPEG_BUF_SIZE 64000     // Max bytes per JPEG frame, larger frames are dropped
#include "MjpegClass.h"          // Included in this project
#include "SD.h"                  // Included with the Espressif Arduino Core (last tested on v3.2.0)
#if defined(ESP32_2432S028)
#include <TFT_Touch.h>           // Touch screen library - Install "TFT_Touch" with the Library Manager
#define HAS_TOUCH
#endif

// Touch screen pins (XPT2046 on ESP32-2432S028)
#define T_DO   39   // Data out pin (MISO)
#define T_DIN  32   // Data in pin (MOSI)
#define T_CS   33   // Chip select pin
#define T_CLK  25   // Clock pin

// Pins for the display
#if defined(ESP32_3248S035)
#define BL_PIN 27
#else
#define BL_PIN 21 // On some cheap yellow display model, BL pin is 27
#endif
#define SD_CS 5
#define SD_MISO 19
#define SD_MOSI 23
#define SD_SCK 18

#define BOOT_PIN 0                   // Boot pin
#define BOOT_BUTTON_DEBOUCE_TIME 400 // Debounce time when reading the boot button in milliseconds
#define TOUCH_POLL_INTERVAL_MS 100   // Poll touch screen every 100ms instead of every frame
#define TOUCH_SKIP_DEBOUNCE_MS 1000  // Only skip one video per second when touch is held

// Some model of cheap Yellow display works only at 40Mhz
#if defined(ESP32_3248S035)
#define DISPLAY_SPI_SPEED 80000000L // 80MHz, drop to 40MHz if the picture is scrambled
#else
// #define DISPLAY_SPI_SPEED 80000000L // 80MHz
#define DISPLAY_SPI_SPEED 40000000L // 40MHz
#endif


#define SD_SPI_SPEED 20000000L      // 20Mhz, 80Mhz mounts unreliably on some cards

const char *MJPEG_FOLDER = "/mjpeg"; // Name of the mjpeg folder on the SD Card

// Storage for files to read on the SD card
#define MAX_FILES 20 // Maximum number of files, adjust as needed
String mjpegFileList[MAX_FILES];
uint32_t mjpegFileSizes[MAX_FILES] = {0}; // Store each GIF file's size in bytes
int mjpegCount = 0;
static int currentMjpegIndex = 0;
static File mjpegFile; // temp gif file holder

// Global variables for mjpeg
MjpegClass mjpeg;
uint8_t *mjpeg_buf[2];           // Double buffer for dual-core
int32_t mjpeg_buf_len[2] = {0};  // Frame size in each buffer

// Dual-core synchronization
#define NUM_BUFFERS 2
#define READER_TASK_STACK_SIZE 4096
#define READER_TASK_PRIORITY 1
SemaphoreHandle_t bufferFull[NUM_BUFFERS];   // Signals buffer has frame data
SemaphoreHandle_t bufferEmpty[NUM_BUFFERS];  // Signals buffer is ready for refill
TaskHandle_t readerTaskHandle = NULL;
volatile bool playbackDone = false;          // Signals end of video to reader
volatile bool readerFinished = false;        // Reader task has exited
volatile bool endOfFile = false;             // No more frames to read

// Detailed timing stats structure
struct VideoStats {
    unsigned long totalRead = 0;
    unsigned long totalDecode = 0;
    unsigned long totalDisplay = 0;
    unsigned long totalTouchPoll = 0;
    unsigned long totalOverhead = 0;
    unsigned long totalWait = 0;         // Time waiting for buffer (dual-core)
    unsigned long maxRead = 0;
    unsigned long maxDecode = 0;
    unsigned long maxDisplay = 0;
    unsigned long maxWait = 0;           // Max wait time for buffer
    int frames = 0;
    unsigned long startTime = 0;
};

// Set to true to enable per-frame logging (verbose)
#define VERBOSE_FRAME_LOG false

// Global stats instance (reset per video)
VideoStats stats;

// Display global variables (using ESP32SPI with DMA for better performance)
Arduino_DataBus *bus = new Arduino_ESP32SPI(
    2 /* DC */, 15 /* CS */, 14 /* SCK */, 13 /* MOSI */, 12 /* MISO */,
    HSPI /* SPI bus - display uses HSPI pins */
);
#if defined(ESP32_3248S035)
Arduino_GFX *gfx = new Arduino_ST7796(bus);
#else
Arduino_GFX *gfx = new Arduino_ILI9341(bus);
#endif

// SD Card reader is on a separate SPI
SPIClass sd_spi(VSPI);

// Touch screen instance
// ponytail: 3248S035R touch (XPT2046) shares the display SPI pins, so the bit-banged TFT_Touch
// can't be used there; BOOT button skips instead. Add XPT2046 on the HSPI bus if touch-skip matters.
#ifdef HAS_TOUCH
TFT_Touch touch = TFT_Touch(T_CS, T_CLK, T_DIN, T_DO);
#endif

// Interrupt to skip to the next mjpeg when the boot button is pressed
volatile bool skipRequested = false; // set in ISR, read in loop()
volatile uint32_t isrTick = 0;       // tick of the last accepted press
uint32_t lastTouchSkip = 0;          // debounce for touch screen skips

void IRAM_ATTR onButtonPress()
{
    uint32_t now = xTaskGetTickCountFromISR(); // safe, 1-tick resolution
    if (now - isrTick < pdMS_TO_TICKS(BOOT_BUTTON_DEBOUCE_TIME))
        return; // contact bounce of press or release
    isrTick = now;
    skipRequested = true; // flag handled in the playback loop
}

// Reader task runs on Core 0 - reads MJPEG frames into double buffer
void readerTask(void *param)
{
    int writeIndex = 0;
    Serial.printf("[Reader] Started on Core %d\n", xPortGetCoreID());

    while (!playbackDone)
    {
        // Wait for buffer to be empty (available for writing)
        if (xSemaphoreTake(bufferEmpty[writeIndex], pdMS_TO_TICKS(100)) == pdTRUE)
        {
            if (playbackDone) break;

            // Read next frame into this buffer
            bool hasFrame = mjpeg.readMjpegBufTo(mjpeg_buf[writeIndex], &mjpeg_buf_len[writeIndex]);

            if (hasFrame)
            {
                // Signal that buffer is full and ready for decoding
                xSemaphoreGive(bufferFull[writeIndex]);
                writeIndex = (writeIndex + 1) % NUM_BUFFERS;
            }
            else
            {
                // No more frames - signal end of file
                endOfFile = true;
                xSemaphoreGive(bufferFull[writeIndex]); // Wake up consumer
                break;
            }
        }
    }

    Serial.printf("[Reader] Finished on Core %d\n", xPortGetCoreID());
    readerFinished = true;
    vTaskDelete(NULL);
}

void setup()
{
    Serial.begin(115200);

    // Allocate the frame buffers first, before display/SD init fragments the heap (no PSRAM)
    Serial.printf("Largest free block: %u bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    for (int i = 0; i < NUM_BUFFERS; i++)
    {
        mjpeg_buf[i] = (uint8_t *)heap_caps_malloc(MJPEG_BUF_SIZE, MALLOC_CAP_8BIT);
        if (!mjpeg_buf[i])
        {
            Serial.printf("mjpeg_buf[%d] allocation failed!\n", i);
            while (true)
            {
                /* no need to continue */
            }
        }
    }
    Serial.printf("Allocated %d MJPEG buffers of %d bytes each\n", NUM_BUFFERS, MJPEG_BUF_SIZE);

    // Set display backlight to High
    pinMode(BL_PIN, OUTPUT);
    digitalWrite(BL_PIN, HIGH);

    // Display initialization
    Serial.println("Display initialization");
    if (!gfx->begin(DISPLAY_SPI_SPEED))
    {
        Serial.println("Display initialization failed!");
        while (true)
        {
            /* no need to continue */
        }
    }
#if defined(ESP32_3248S035)
    gfx->setRotation(1); // landscape 480x320
#else
    gfx->setRotation(0);
#endif
    gfx->fillScreen(RGB565_BLACK);
    // gfx->invertDisplay(true); // on some cheap yellow models, display must be inverted
    Serial.printf("Screeen size Width=%d,Height=%d\n", gfx->width(), gfx->height());

    // Touch screen initialization
#ifdef HAS_TOUCH
    touch.setRotation(0); // Match display rotation
#endif

    // SD card initialization
    Serial.println("SD Card initialization");
    bool sdOk = false;
    for (int attempt = 0; attempt < 3 && !sdOk; attempt++) // card may still be busy after a soft reset
    {
        sdOk = SD.begin(SD_CS, sd_spi, SD_SPI_SPEED, "/sd");
        if (!sdOk)
        {
            SD.end();
            delay(500);
        }
    }
    if (!sdOk)
    {
        Serial.printf("ERROR: File system mount failed! card type %d, free heap %u\n", SD.cardType(), ESP.getFreeHeap());
        while (true)
        {
            /* no need to continue */
        }
    }

    loadMjpegFilesList(); // Load the list of mjpeg to play from the SD card
    if (mjpegCount == 0)
    {
        Serial.printf("No .mjpeg files in %s\n", MJPEG_FOLDER);
        gfx->setTextColor(RGB565_WHITE);
        gfx->setTextSize(2);
        gfx->setCursor(10, 10);
        gfx->printf("No .mjpeg files in %s", MJPEG_FOLDER);
        while (true)
        {
            delay(1000);
        }
    }

    // Set the boot button to skip the current mjpeg playing and go to the next
    pinMode(BOOT_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(BOOT_PIN), // fast ISR
                    onButtonPress, FALLING);         // press == LOW
}

void loop()
{
    playSelectedMjpeg(currentMjpegIndex);
    currentMjpegIndex++;
    if (currentMjpegIndex >= mjpegCount)
    {
        currentMjpegIndex = 0;
    }
}

// Play the current mjpeg
void playSelectedMjpeg(int mjpegIndex)
{
    // Build the full path for the selected mjpeg
    String fullPath = String(MJPEG_FOLDER) + "/" + mjpegFileList[mjpegIndex];
    char mjpegFilename[128];
    fullPath.toCharArray(mjpegFilename, sizeof(mjpegFilename));

    Serial.printf("Playing %s\n", mjpegFilename);
    mjpegPlayFromSDCard(mjpegFilename);
}

// Callback function to draw a JPEG
int jpegDrawCallback(JPEGDRAW *pDraw)
{
    unsigned long s = micros();
    gfx->draw16bitBeRGBBitmap(pDraw->x, pDraw->y, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
    unsigned long elapsed = micros() - s;
    stats.totalDisplay += elapsed;
    if (elapsed > stats.maxDisplay) {
        stats.maxDisplay = elapsed;
    }
    return 1;
}

// Print detailed video statistics
void printVideoStats(const char *filename)
{
    unsigned long totalTime = millis() - stats.startTime;
    float fps = (totalTime > 0) ? (1000.0f * stats.frames / totalTime) : 0;

    // Convert micros to millis for display
    float avgRead = (stats.frames > 0) ? (stats.totalRead / 1000.0f / stats.frames) : 0;
    float avgDecode = (stats.frames > 0) ? (stats.totalDecode / 1000.0f / stats.frames) : 0;
    float avgDisplay = (stats.frames > 0) ? (stats.totalDisplay / 1000.0f / stats.frames) : 0;
    float avgTouchPoll = (stats.frames > 0) ? (stats.totalTouchPoll / 1000.0f / stats.frames) : 0;
    float avgOverhead = (stats.frames > 0) ? (stats.totalOverhead / 1000.0f / stats.frames) : 0;
    float avgWait = (stats.frames > 0) ? (stats.totalWait / 1000.0f / stats.frames) : 0;
    float avgTotal = avgRead + avgDecode + avgDisplay + avgTouchPoll + avgOverhead + avgWait;

    // Calculate percentages
    float pctRead = (avgTotal > 0) ? (100.0f * avgRead / avgTotal) : 0;
    float pctDecode = (avgTotal > 0) ? (100.0f * avgDecode / avgTotal) : 0;
    float pctDisplay = (avgTotal > 0) ? (100.0f * avgDisplay / avgTotal) : 0;
    float pctTouchPoll = (avgTotal > 0) ? (100.0f * avgTouchPoll / avgTotal) : 0;
    float pctOverhead = (avgTotal > 0) ? (100.0f * avgOverhead / avgTotal) : 0;
    float pctWait = (avgTotal > 0) ? (100.0f * avgWait / avgTotal) : 0;

    Serial.println();
    Serial.println(F("═══════════════════════════════════════════════════"));
    Serial.printf("Video Stats: %s\n", filename);
    Serial.println(F("═══════════════════════════════════════════════════"));
    Serial.printf("Frames: %d | Duration: %lu ms | FPS: %.1f\n", stats.frames, totalTime, fps);
    Serial.println(F("Mode: DUAL-CORE"));
    Serial.printf("  Reader:  Core 0 (PRO_CPU)\n");
    Serial.printf("  Decoder: Core %d (%s)\n", xPortGetCoreID(), xPortGetCoreID() == 1 ? "APP_CPU" : "PRO_CPU");
    Serial.println();
    Serial.println(F("Breakdown (avg per frame):"));
    Serial.printf("  Buf Wait:    %6.2f ms (%5.1f%%) <- lower = better parallelism\n", avgWait, pctWait);
    Serial.printf("  SD Read:     %6.2f ms (%5.1f%%) [on Core 0]\n", avgRead, pctRead);
    Serial.printf("  JPEG Decode: %6.2f ms (%5.1f%%)\n", avgDecode, pctDecode);
    Serial.printf("  Display:     %6.2f ms (%5.1f%%)\n", avgDisplay, pctDisplay);
    Serial.printf("  Touch Poll:  %6.2f ms (%5.1f%%)\n", avgTouchPoll, pctTouchPoll);
    Serial.printf("  Overhead:    %6.2f ms (%5.1f%%)\n", avgOverhead, pctOverhead);
    Serial.println(F("  ───────────────────────────────────"));
    Serial.printf("  Total:       %6.2f ms (100.0%%)\n", avgTotal);
    Serial.println();
    Serial.println(F("Peak times:"));
    Serial.printf("  Wait max: %.2f ms | Decode max: %.2f ms | Display max: %.2f ms\n",
                  stats.maxWait / 1000.0f, stats.maxDecode / 1000.0f, stats.maxDisplay / 1000.0f);
    Serial.printf("Video size: %d×%d, scale factor: %d\n", mjpeg.getWidth(), mjpeg.getHeight(), mjpeg.getScale());
    Serial.println(F("═══════════════════════════════════════════════════"));
    Serial.println();
}

// Play a mjpeg stored on the SD card (dual-core implementation)
void mjpegPlayFromSDCard(char *mjpegFilename)
{
    Serial.printf("Opening %s\n", mjpegFilename);
    Serial.printf("[Decoder] Running on Core: %d (%s)\n", xPortGetCoreID(), xPortGetCoreID() == 1 ? "APP_CPU" : "PRO_CPU");

    File mjpegFile = SD.open(mjpegFilename, "r");

    if (!mjpegFile || mjpegFile.isDirectory())
    {
        Serial.printf("ERROR: Failed to open %s file for reading\n", mjpegFilename);
    }
    else
    {
        Serial.println("MJPEG start (dual-core)");
        gfx->fillScreen(RGB565_BLACK);

        // Reset stats for this video
        stats = VideoStats();
        stats.startTime = millis();

        // Setup mjpeg with first buffer (used for scaling detection on first frame)
        mjpeg.setup(
            &mjpegFile, mjpeg_buf[0], jpegDrawCallback, true /* useBigEndian */,
            0 /* x */, 0 /* y */, gfx->width() /* widthLimit */, gfx->height() /* heightLimit */);

        // Create semaphores for dual-core synchronization
        for (int i = 0; i < NUM_BUFFERS; i++)
        {
            bufferFull[i] = xSemaphoreCreateBinary();
            bufferEmpty[i] = xSemaphoreCreateBinary();
            xSemaphoreGive(bufferEmpty[i]); // Initially all buffers are empty/available
            mjpeg_buf_len[i] = 0;
        }

        // Reset control flags
        playbackDone = false;
        readerFinished = false;
        endOfFile = false;

        // Start reader task on Core 0 (PRO_CPU)
        xTaskCreatePinnedToCore(
            readerTask,
            "ReaderTask",
            READER_TASK_STACK_SIZE,
            NULL,
            READER_TASK_PRIORITY,
            &readerTaskHandle,
            0  // Core 0 (PRO_CPU)
        );

        // Consumer loop on Core 1 (APP_CPU)
        unsigned long lastTouchPollTime = 0;
        unsigned long frameStart, waitStart, waitEnd, decodeStart, decodeEnd, touchPollStart, touchPollEnd;
        int readIndex = 0;

        while (!skipRequested) // exits on the reader's zero-length end-of-file frame
        {
            frameStart = micros();

            // === Wait for buffer to be filled by reader task ===
            waitStart = micros();
            if (xSemaphoreTake(bufferFull[readIndex], pdMS_TO_TICKS(1000)) != pdTRUE)
            {
                // Timeout - check if we should exit
                if (endOfFile || playbackDone) break;
                continue;
            }
            waitEnd = micros();

            // Check if reader signaled end of file
            if (endOfFile && mjpeg_buf_len[readIndex] == 0) break;

            unsigned long waitElapsed = waitEnd - waitStart;
            stats.totalWait += waitElapsed;
            if (waitElapsed > stats.maxWait) {
                stats.maxWait = waitElapsed;
            }

            // === Touch Polling (throttled) ===
            touchPollStart = micros();
            unsigned long now = millis();
#ifdef HAS_TOUCH
            if (now - lastTouchPollTime >= TOUCH_POLL_INTERVAL_MS)
            {
                lastTouchPollTime = now;
                if (touch.Pressed() && (now - lastTouchSkip >= TOUCH_SKIP_DEBOUNCE_MS))
                {
                    int16_t tx = touch.X();
                    int16_t ty = touch.Y();

                    // Define center touch zone (50% of screen, centered)
                    int16_t zoneLeft   = gfx->width() / 4;
                    int16_t zoneRight  = gfx->width() * 3 / 4;
                    int16_t zoneTop    = gfx->height() / 4;
                    int16_t zoneBottom = gfx->height() * 3 / 4;
                    // Only skip if touch is within the center zone, to avoid accidental
                    // skips when pressing the edges of the screen or case.
                    if (tx >= zoneLeft && tx <= zoneRight && ty >= zoneTop && ty <= zoneBottom)
                    {
                        lastTouchSkip = now;
                        skipRequested = true;
                    }
                }
            }
#endif
            touchPollEnd = micros();
            stats.totalTouchPoll += (touchPollEnd - touchPollStart);

            // === JPEG Decode from buffer (display time tracked in callback) ===
            decodeStart = micros();
            unsigned long displayBefore = stats.totalDisplay;  // Snapshot before decode
            mjpeg.drawJpgFrom(mjpeg_buf[readIndex], mjpeg_buf_len[readIndex]);
            decodeEnd = micros();

            // Signal buffer is now empty and available for reader
            xSemaphoreGive(bufferEmpty[readIndex]);
            readIndex = (readIndex + 1) % NUM_BUFFERS;

            // Decode time = total decode call time - display time added during decode
            unsigned long displayDuring = stats.totalDisplay - displayBefore;
            unsigned long decodeElapsed = (decodeEnd - decodeStart) - displayDuring;
            stats.totalDecode += decodeElapsed;
            if (decodeElapsed > stats.maxDecode) {
                stats.maxDecode = decodeElapsed;
            }

            // === Calculate overhead (loop management, etc.) ===
            unsigned long frameEnd = micros();
            unsigned long accountedTime = waitElapsed + (touchPollEnd - touchPollStart) + (decodeEnd - decodeStart);
            unsigned long overhead = (frameEnd - frameStart) - accountedTime;
            stats.totalOverhead += overhead;

            stats.frames++;

            // Verbose per-frame logging (optional)
            #if VERBOSE_FRAME_LOG
            Serial.printf("[Frame %d] Wait:%.1fms Decode:%.1fms Display:%.1fms Poll:%.1fms Total:%.1fms\n",
                          stats.frames,
                          waitElapsed / 1000.0f,
                          decodeElapsed / 1000.0f,
                          displayDuring / 1000.0f,
                          (touchPollEnd - touchPollStart) / 1000.0f,
                          (frameEnd - frameStart) / 1000.0f);
            #endif
        }

        // Signal reader task to stop and wait for it to finish
        playbackDone = true;

        // Give semaphores to unblock reader if it's waiting
        for (int i = 0; i < NUM_BUFFERS; i++)
        {
            xSemaphoreGive(bufferEmpty[i]);
        }

        // Wait for reader task to finish, no timeout: deleting the semaphores under a running reader asserts
        while (!readerFinished)
        {
            xSemaphoreGive(bufferEmpty[0]);
            xSemaphoreGive(bufferEmpty[1]);
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        // Cleanup semaphores
        for (int i = 0; i < NUM_BUFFERS; i++)
        {
            if (bufferFull[i]) { vSemaphoreDelete(bufferFull[i]); bufferFull[i] = NULL; }
            if (bufferEmpty[i]) { vSemaphoreDelete(bufferEmpty[i]); bufferEmpty[i] = NULL; }
        }
        readerTaskHandle = NULL;

        skipRequested = false; // debounced in onButtonPress()

        Serial.println(F("MJPEG end"));
        mjpegFile.close();

        // Print detailed statistics
        printVideoStats(mjpegFilename);
    }
}

// Read the mjpeg file list in the mjpeg folder of the SD card
void loadMjpegFilesList()
{
    File mjpegDir = SD.open(MJPEG_FOLDER);
    if (!mjpegDir)
    {
        Serial.printf("Failed to open %s folder\n", MJPEG_FOLDER);
        while (true)
        {
            /* code */
        }
    }
    mjpegCount = 0;
    while (true)
    {
        File file = mjpegDir.openNextFile();
        if (!file)
            break;
        if (!file.isDirectory())
        {
            String name = file.name();
            if (name.endsWith(".mjpeg"))
            {
                mjpegFileList[mjpegCount] = name;
                mjpegFileSizes[mjpegCount] = file.size(); // Save file size (in bytes)
                mjpegCount++;
                if (mjpegCount >= MAX_FILES)
                    break;
            }
        }
        file.close();
    }
    mjpegDir.close();
    Serial.printf("%d mjpeg files read\n", mjpegCount);
    // Optionally, print out each file's size for debugging:
    for (int i = 0; i < mjpegCount; i++)
    {
        Serial.printf("File %d: %s, Size: %lu bytes (%s)\n", i, mjpegFileList[i].c_str(), mjpegFileSizes[i],formatBytes(mjpegFileSizes[i]).c_str());
    }
}

// Function helper display sizes on the serial monitor
String formatBytes(size_t bytes)
{
    if (bytes < 1024)
    {
        return String(bytes) + " B";
    }
    else if (bytes < (1024 * 1024))
    {
        return String(bytes / 1024.0, 2) + " KB";
    }
    else
    {
        return String(bytes / 1024.0 / 1024.0, 2) + " MB";
    }
}
