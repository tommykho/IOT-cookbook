/*
 *  cyd-video-player Version 2026.10A
 *  storage.cpp: SD card, media folders and file naming
 *  Board: ESP32-3248S035, ESP32-2432S028
 *  Author: tommyho510@gmail.com
 */
#include "storage.h"
#include "config.h"
#include <algorithm>
#include <functional>

// SD Card reader is on a separate SPI
static SPIClass sd_spi(VSPI);

bool storageBegin()
{
    LOGF("SD Card initialization\n");
    bool sdOk = false;
    for (int attempt = 0; attempt < 3 && !sdOk; attempt++) // card may still be busy after a soft reset
    {
        sd_spi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
        sdOk = SD.begin(SD_CS, sd_spi, SD_SPI_SPEED, "/sd");
        if (!sdOk)
        {
            SD.end();
            delay(500);
        }
    }
    if (!sdOk)
    {
        LOGF("ERROR: File system mount failed! card type %d, free heap %lu\n", SD.cardType(), ESP.getFreeHeap());
        return false;
    }
    const char *dirs[] = {PHOTO_FOLDER, AUDIO_FOLDER, MJPEG_FOLDER};
    for (const char *d : dirs)
    {
        if (!SD.exists(d))
            SD.mkdir(d);
    }
    SD.remove(UPLOAD_TMP); // left over from an interrupted upload
    LOGF("SD card %llu MB\n", SD.cardSize() / (1024 * 1024));
    return true;
}

MediaType mediaTypeOf(const String &name)
{
    String n = name;
    n.toLowerCase();
    if (n.endsWith(".jpg") || n.endsWith(".jpeg")) return MEDIA_JPEG;
    if (n.endsWith(".gif")) return MEDIA_GIF;
    if (n.endsWith(".mp3")) return MEDIA_MP3;
    if (n.endsWith(".wav")) return MEDIA_WAV;
    if (n.endsWith(".mjpeg") || n.endsWith(".mjpg")) return MEDIA_MJPEG;
    return MEDIA_NONE;
}

const char *mediaFolder(MediaType t)
{
    switch (t)
    {
    case MEDIA_JPEG:
    case MEDIA_GIF: return PHOTO_FOLDER;
    case MEDIA_MP3:
    case MEDIA_WAV: return AUDIO_FOLDER;
    case MEDIA_MJPEG: return MJPEG_FOLDER;
    default: return nullptr;
    }
}

uint32_t mediaLimit(MediaType t)
{
    switch (t)
    {
    case MEDIA_JPEG: return MAX_JPG_BYTES;
    case MEDIA_GIF: return MAX_GIF_BYTES;
    case MEDIA_MP3:
    case MEDIA_WAV: return MAX_AUDIO_BYTES;
    case MEDIA_MJPEG: return MAX_MJPEG_BYTES;
    default: return 0;
    }
}

bool isPhoto(MediaType t) { return t == MEDIA_JPEG || t == MEDIA_GIF; }
bool isAudio(MediaType t) { return t == MEDIA_MP3 || t == MEDIA_WAV; }
bool isVideo(MediaType t) { return t == MEDIA_MJPEG; }

// Calls fn(name) for each matching file, stops after MAX_LIST_FILES
static void forEachMedia(const char *dir, bool (*accept)(MediaType), std::function<void(const String &)> fn)
{
    File d = SD.open(dir);
    if (!d || !d.isDirectory())
    {
        LOGF("Failed to open %s folder\n", dir);
        return;
    }
    for (int n = 0; n < MAX_LIST_FILES;)
    {
        File f = d.openNextFile();
        if (!f)
            break;
        String name = f.name();
        if (!f.isDirectory() && !name.startsWith(".") && accept(mediaTypeOf(name))) // skip macOS "._" files
        {
            fn(name);
            n++;
        }
        f.close();
    }
    d.close();
}

void listMedia(const char *dir, bool (*accept)(MediaType), std::vector<String> &out)
{
    out.clear();
    forEachMedia(dir, accept, [&](const String &name) { out.push_back(name); });
    std::sort(out.begin(), out.end());
    LOGF("%u files in %s\n", out.size(), dir);
}

int countMedia(const char *dir, bool (*accept)(MediaType))
{
    int n = 0;
    forEachMedia(dir, accept, [&](const String &) { n++; });
    return n;
}

String sanitizeName(const String &name)
{
    String base = name;
    int slash = base.lastIndexOf('/');
    if (slash >= 0)
        base = base.substring(slash + 1);
    String out;
    for (size_t i = 0; i < base.length() && out.length() < 48; i++)
    {
        char c = base[i];
        bool ok = isalnum((unsigned char)c) || c == '-' || c == '_' || (c == '.' && out.length() > 0);
        out += ok ? c : '_';
    }
    int dot = out.lastIndexOf('.');
    if (dot >= 0) // keep the extension lower case so the type checks match
    {
        String ext = out.substring(dot);
        ext.toLowerCase();
        out = out.substring(0, dot) + ext;
    }
    return out;
}

String uniquePath(const char *dir, const String &name, const String &stamp)
{
    String path = String(dir) + "/" + name;
    if (!SD.exists(path))
        return path;
    int dot = name.lastIndexOf('.');
    String base = dot > 0 ? name.substring(0, dot) : name;
    String ext = dot > 0 ? name.substring(dot) : "";
    path = String(dir) + "/" + base + "_" + stamp + ext;
    for (int i = 2; SD.exists(path); i++)
        path = String(dir) + "/" + base + "_" + stamp + "_" + i + ext;
    return path;
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
