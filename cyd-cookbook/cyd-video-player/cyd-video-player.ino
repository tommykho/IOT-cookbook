/*
 *  cyd-video-player Version 2026.10A
 *  Board: ESP32-3248S035, ESP32-2432S028
 *  Based on: https://github.com/kiwiholmberg/cyd-video-player (MIT)
 *  Author: tommyho510@gmail.com
 *
 *  MJPEG video player, JPG/GIF slideshow and MP3/WAV player with Wi-Fi upload from a
 *  phone (Receive: SoftAP + QR code + captive portal). Boots into the slideshow, a tap
 *  opens the menu. Board selection, pins and limits are in config.h.
 */
// Tutorial : https://youtu.be/jYcxUgxz9ks
// Use board "ESP32 Dev Module", Partition Scheme "Huge APP (3MB No OTA/1MB SPIFFS)" (last tested on v3.2.0)
// LVGL is configured by build_opt.h in this folder, no lv_conf.h needed

#include "config.h"              // *** edit your settings there
#include "app.h"
#include "display.h"             // Install "GFX Library for Arduino" (last tested on v1.6.9) and "lvgl" (v9.6.0)
#include "storage.h"
#include "slideshow.h"
#include "audio.h"
#include "menu.h"
#include "receive.h"
#include "MjpegClass.h"          // Included in this project
                                 // Install "JPEGDEC" with the Library Manager (last tested on v1.8.4)

// mjpeg files to play from the SD card
static std::vector<String> mjpegFiles;
static int currentMjpegIndex = 0;

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

// Interrupt to skip to the next mjpeg when the boot button is pressed
volatile bool skipRequested = false; // set in ISR, read in loop()
volatile uint32_t isrTick = 0;       // tick of the last accepted press
volatile bool exitRequested = false; // tap during a video: back to the menu

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

// ---------------------------------------------------------------- App state machine

static AppState state = STATE_MENU;
static volatile bool statePending = false;
static AppState pendingState;
static String pendingPath;
static uint32_t bootHeldSince = 0; // BOOT held in the menu: recalibrate touch

void appRequestState(AppState s, const char *path)
{
    pendingState = s;
    pendingPath = path ? path : "";
    statePending = true;
}

static void showMenu(const char *message)
{
    char summary[96];
    if (!message)
    {
        snprintf(summary, sizeof(summary), "%d photos, %d videos, %d tracks",
                 countMedia(PHOTO_FOLDER, isPhoto), countMedia(MJPEG_FOLDER, isVideo), countMedia(AUDIO_FOLDER, isAudio));
        message = summary;
    }
    menuShow(message);
    lvglResume();
}

static void enterState(AppState s, const String &path)
{
    AppState prev = state;
    if (prev == STATE_SLIDESHOW || prev == STATE_PHOTO_VIEW)
        slideshowEnd();
    if (prev == STATE_RECEIVE)
        receiveStop();
    state = s;
    skipRequested = false;
    LOGF("State %d -> %d, free heap %lu, largest block %u\n", prev, s, ESP.getFreeHeap(),
         heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    switch (s)
    {
    case STATE_MENU:
        showMenu(nullptr);
        break;
    case STATE_AUDIO_PLAYER:
        audioScreenShow();
        lvglResume();
        if (path.length())
            audioPlay(path.c_str());
        break;
    case STATE_SLIDESHOW:
    case STATE_PHOTO_VIEW:
        lvglSuspend(); // audio keeps playing: it only uses arenaAudio()
        if (!slideshowBegin(path.c_str(), s == STATE_SLIDESHOW))
        {
            state = STATE_MENU;
            showMenu("No photos in " PHOTO_FOLDER ". Tap Receive to add some.");
        }
        break;
    case STATE_VIDEO:
        listMedia(MJPEG_FOLDER, isVideo, mjpegFiles);
        if (mjpegFiles.empty())
        {
            state = STATE_MENU;
            showMenu("No .mjpeg files in " MJPEG_FOLDER ". Tap Receive to add some.");
            break;
        }
        audioStop(true); // video needs both arena buffers
        currentMjpegIndex = 0;
        for (size_t i = 0; i < mjpegFiles.size(); i++)
            if (path.endsWith("/" + mjpegFiles[i]))
                currentMjpegIndex = i;
        exitRequested = false;
        lvglSuspend();
        break;
    case STATE_RECEIVE:
        audioStop(true); // frees arenaAudio() for the upload buffer and the CPU for Wi-Fi
        receiveStart();
        lvglResume();
        break;
    }
}

// Receive finished: show what arrived
static void finishReceive()
{
    MediaType t = receiveLastType();
    if (receiveCount() == 0)
        appRequestState(STATE_MENU);
    else if (isPhoto(t))
        appRequestState(STATE_SLIDESHOW, receiveFirstPhotoPath().c_str());
    else if (isAudio(t))
        appRequestState(STATE_AUDIO_PLAYER, receiveLastPath().c_str());
    else
        appRequestState(STATE_VIDEO, receiveLastPath().c_str());
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

    if (!displayBegin())
    {
        while (true)
        {
            /* no need to continue */
        }
    }

    if (!storageBegin())
    {
        displayMessage("SD card not found", "Insert a FAT32 microSD card (<=32GB partition) and reset.");
        while (true)
        {
            delay(1000);
        }
    }

    touchCalibrate(false); // runs once, then loads the saved calibration
    lvglBegin();
    menuBegin();
    audioBegin();

    // BOOT button: next photo / video / track
    pinMode(BOOT_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(BOOT_PIN), // fast ISR
                    onButtonPress, FALLING);         // press == LOW

    // Boot into the slideshow, or video when there are no photos, else the menu
    if (countMedia(PHOTO_FOLDER, isPhoto))
        appRequestState(STATE_SLIDESHOW);
    else if (countMedia(MJPEG_FOLDER, isVideo))
        appRequestState(STATE_VIDEO);
    else
        appRequestState(STATE_MENU);
    LOGF("Setup done, free heap %lu\n", ESP.getFreeHeap());
}

void loop()
{
    if (statePending)
    {
        statePending = false;
        enterState(pendingState, pendingPath);
    }
    switch (state)
    {
    case STATE_MENU:
    case STATE_AUDIO_PLAYER:
        lvglTask();
        if (state == STATE_MENU && digitalRead(BOOT_PIN) == LOW)
        {
            if (!bootHeldSince)
                bootHeldSince = millis();
            else if (millis() - bootHeldSince > 3000)
            {
                lvglSuspend();
                touchCalibrate(true);
                bootHeldSince = 0;
                showMenu("Touch calibrated");
            }
        }
        else
        {
            bootHeldSince = 0;
        }
        delay(5);
        break;
    case STATE_SLIDESHOW:
    case STATE_PHOTO_VIEW:
        if (slideshowLoop())
            appRequestState(STATE_MENU);
        delay(2);
        break;
    case STATE_VIDEO:
        playSelectedMjpeg(currentMjpegIndex);
        if (exitRequested)
        {
            appRequestState(STATE_MENU);
            break;
        }
        currentMjpegIndex++;
        if (currentMjpegIndex >= (int)mjpegFiles.size())
        {
            currentMjpegIndex = 0;
        }
        break;
    case STATE_RECEIVE:
        if (receiveLoop())
            finishReceive();
        break;
    }
}

// Play the current mjpeg
void playSelectedMjpeg(int mjpegIndex)
{
    // Build the full path for the selected mjpeg
    String fullPath = String(MJPEG_FOLDER) + "/" + mjpegFiles[mjpegIndex];
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
        unsigned long frameStart, waitStart, waitEnd, decodeStart, decodeEnd, touchPollStart, touchPollEnd;
        int readIndex = 0;

        while (!skipRequested && !exitRequested) // exits on the reader's zero-length end-of-file frame
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
            int16_t tx, ty;
            if (touchTapped(tx, ty)) // throttled to TOUCH_POLL_MS, IRQ pin checked first
            {
                exitRequested = true; // tap anywhere: back to the menu
            }
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
