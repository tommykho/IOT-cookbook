/*
 *  cyd-video-player Version 2026.10A
 *  audio.cpp: MP3/WAV player on the internal DAC (GPIO26 speaker)
 *  Board: ESP32-3248S035, ESP32-2432S028
 *  Author: tommyho510@gmail.com
 *
 *  Decoding runs in its own task on core 0 so LVGL and the slideshow keep core 1.
 *  Other tasks talk to it through a command queue. The MP3 decoder state lives in
 *  arenaAudio(), so audio must be stopped before a video plays or Receive starts.
 */
#include "audio.h"
#include "app.h"
#include "storage.h"
#include <AudioFileSourceFS.h> // Install "ESP8266Audio" with the Library Manager (last tested on v2.2.0;
                               // v2.3.0+ needs ESP32 core 3.3 or later)
#include <AudioFileSourceID3.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutput.h>
#include <driver/dac_continuous.h>

static_assert(AudioGeneratorMP3::preAllocSize() <= MJPEG_BUF_SIZE, "MP3 decoder does not fit in the media arena");

#define AUDIO_TASK_STACK_SIZE 8192
#define AUDIO_TASK_PRIORITY 2

// Mono 8-bit output on DAC channel 2 (GPIO26). ESP8266Audio's AudioOutputInternalDAC
// drives channel 1 (GPIO25) for mono sources, which is not wired to the speaker here.
class AudioOutputDAC2 : public AudioOutput
{
public:
    AudioOutputDAC2()
    {
        hertz = 44100;
        channels = 2;
        pinsLow();
    }
    bool begin() override { return true; }
    bool SetRate(int hz) override
    {
        if (hz != hertz)
        {
            hertz = hz;
            close(); // reopened at the new rate on the next sample
        }
        return true;
    }
    bool SetChannels(int ch) override
    {
        channels = ch; // downmixed to mono in ConsumeSample()
        return true;
    }
    bool ConsumeSample(int16_t sample[2]) override
    {
        if (!handle && !open())
            return true; // drop samples rather than stall the decoder
        MakeSampleStereo16(sample);
        int16_t mono = ((int32_t)sample[LEFTCHANNEL] + sample[RIGHTCHANNEL]) / 2;
        buf[fill++] = (uint8_t)((Amplify(mono) + 32768) >> 8);
        if (fill == sizeof(buf))
        {
            dac_continuous_write(handle, buf, fill, NULL, -1); // blocks while DMA is busy, paces the decoder
            fill = 0;
        }
        return true;
    }
    bool stop() override
    {
        if (handle)
        {
            memset(buf, 0x80, sizeof(buf)); // mid-level silence before shutting the DAC off
            dac_continuous_write(handle, buf, sizeof(buf), NULL, -1);
        }
        close();
        return true;
    }

private:
    dac_continuous_handle_t handle = nullptr;
    uint8_t buf[512];
    size_t fill = 0;

    bool open()
    {
        dac_continuous_config_t cfg = {};
        cfg.chan_mask = DAC_CHANNEL_MASK_CH1; // DAC2 = GPIO26
        cfg.desc_num = 4;
        cfg.buf_size = 1024;
        cfg.freq_hz = hertz;
        cfg.offset = 0;
        cfg.clk_src = DAC_DIGI_CLK_SRC_APLL; // accurate 44.1/48 kHz
        cfg.chan_mode = DAC_CHANNEL_MODE_SIMUL;
        if (dac_continuous_new_channels(&cfg, &handle) != ESP_OK)
        {
            handle = nullptr;
            return false;
        }
        dac_continuous_enable(handle);
        fill = 0;
        return true;
    }
    void close()
    {
        if (handle)
        {
            dac_continuous_disable(handle);
            dac_continuous_del_channels(handle);
            handle = nullptr;
        }
        pinsLow();
    }
    void pinsLow()
    {
        // DAC pins float high after deinit, keep the amplifier input low
        pinMode(SPEAKER_DAC_PIN, OUTPUT);
        digitalWrite(SPEAKER_DAC_PIN, LOW);
    }
};

enum AudioCmd : uint8_t
{
    CMD_PLAY, // arg = track index
    CMD_PAUSE,
    CMD_NEXT, // arg = +1 / -1
    CMD_STOP
};
struct AudioMsg
{
    AudioCmd cmd;
    int arg;
};

static QueueHandle_t audioQueue = nullptr;
static SemaphoreHandle_t tracksLock = nullptr;
static std::vector<String> tracks;
static AudioOutputDAC2 *out = nullptr;

// Owned by the audio task
static AudioGenerator *gen = nullptr;
static AudioFileSourceFS *file = nullptr;
static AudioFileSourceID3 *id3 = nullptr;

// Status for the UI
static volatile int curIndex = -1;
static volatile bool paused = false;
static volatile bool idle = true; // decoder released the arena
static uint8_t volumePct = AUDIO_DEFAULT_VOLUME;

static void stopTrack()
{
    if (gen)
    {
        gen->stop();
        delete gen;
        gen = nullptr;
    }
    delete id3;
    id3 = nullptr;
    delete file;
    file = nullptr;
    out->stop();
    curIndex = -1;
    paused = false;
    idle = true;
}

static bool startTrack(int i)
{
    stopTrack();
    xSemaphoreTake(tracksLock, portMAX_DELAY);
    String path = (i >= 0 && i < (int)tracks.size()) ? String(AUDIO_FOLDER) + "/" + tracks[i] : String();
    xSemaphoreGive(tracksLock);
    if (path.isEmpty())
        return false;
    file = new AudioFileSourceFS(SD, path.c_str());
    if (!file->isOpen())
    {
        LOGF("Audio: cannot open %s\n", path.c_str());
        stopTrack();
        return false;
    }
    AudioFileSource *src = file;
    idle = false;
    if (mediaTypeOf(path) == MEDIA_MP3)
    {
        id3 = new AudioFileSourceID3(file); // skips ID3v2 tags (album art)
        src = id3;
        gen = new AudioGeneratorMP3(arenaAudio(), MJPEG_BUF_SIZE);
    }
    else
    {
        gen = new AudioGeneratorWAV();
    }
    if (!gen->begin(src, out))
    {
        LOGF("Audio: cannot decode %s\n", path.c_str());
        stopTrack();
        return false;
    }
    curIndex = i;
    LOGF("Audio: playing %s\n", path.c_str());
    return true;
}

// Start track i, skipping files that fail, in direction dir
static void startFrom(int i, int dir)
{
    int n = tracks.size();
    for (int tries = 0; tries < n; tries++, i += dir)
    {
        if (startTrack(((i % n) + n) % n))
            return;
    }
    stopTrack(); // nothing playable: mark idle
}

static void audioTask(void *param)
{
    LOGF("[Audio] Started on Core %d\n", xPortGetCoreID());
    uint32_t spins = 0;
    while (true)
    {
        AudioMsg m;
        bool busy = gen && !paused;
        if (xQueueReceive(audioQueue, &m, busy ? 0 : pdMS_TO_TICKS(50)) == pdTRUE)
        {
            switch (m.cmd)
            {
            case CMD_PLAY:
                startFrom(m.arg, 1);
                break;
            case CMD_PAUSE:
                if (gen)
                {
                    paused = !paused;
                    if (paused)
                        out->stop(); // silence; the DAC reopens on the next sample
                }
                break;
            case CMD_NEXT:
                if (!tracks.empty())
                    startFrom((curIndex < 0 ? 0 : curIndex) + m.arg, m.arg);
                break;
            case CMD_STOP:
                stopTrack();
                break;
            }
        }
        if (gen && !paused && !gen->loop())
        {
            int last = curIndex;
            stopTrack();
            if (!tracks.empty())
                startFrom(last + 1, 1); // continue with the next track
        }
        if (++spins % 64 == 0)
            vTaskDelay(1); // let IDLE0 run so the task watchdog stays fed
    }
}

void audioBegin()
{
    tracksLock = xSemaphoreCreateMutex();
    audioQueue = xQueueCreate(8, sizeof(AudioMsg));
    out = new AudioOutputDAC2();
    audioSetVolume(volumePct);
    audioRescan();
    xTaskCreatePinnedToCore(audioTask, "AudioTask", AUDIO_TASK_STACK_SIZE, NULL, AUDIO_TASK_PRIORITY, NULL, 0);
}

void audioRescan()
{
    if (!idle)
        return; // the task may be reading the list
    xSemaphoreTake(tracksLock, portMAX_DELAY);
    listMedia(AUDIO_FOLDER, isAudio, tracks);
    xSemaphoreGive(tracksLock);
}

const std::vector<String> &audioTracks()
{
    return tracks;
}

static void send(AudioCmd c, int arg = 0)
{
    AudioMsg m = {c, arg};
    xQueueSend(audioQueue, &m, pdMS_TO_TICKS(100));
}

void audioPlay(const char *path)
{
    String name = path;
    name = name.substring(name.lastIndexOf('/') + 1);
    auto find = [&]() {
        for (size_t i = 0; i < tracks.size(); i++)
            if (tracks[i] == name)
                return (int)i;
        return -1;
    };
    int i = find();
    if (i < 0) // new file, e.g. just uploaded
    {
        audioStop(true);
        audioRescan();
        i = find();
    }
    if (i >= 0)
        audioPlayIndex(i);
}

void audioPlayIndex(int i)
{
    idle = false; // set here so a rescan cannot race the task picking up the command
    send(CMD_PLAY, i);
}

void audioTogglePause() { send(CMD_PAUSE); }
void audioNext(int dir) { send(CMD_NEXT, dir); }

void audioStop(bool wait)
{
    if (idle && !gen)
        return;
    send(CMD_STOP);
    uint32_t start = millis();
    while (wait && !idle && millis() - start < 2000)
        delay(5);
}

void audioSetVolume(uint8_t pct)
{
    volumePct = min<uint8_t>(pct, 100);
    out->SetGain(volumePct / 100.0f * AUDIO_MAX_GAIN);
}

uint8_t audioVolume() { return volumePct; }
bool audioIsActive() { return curIndex >= 0; }
bool audioIsPaused() { return paused; }
int audioCurrentIndex() { return curIndex; }
