/*
 *  cyd-video-player Version 2026.10A
 *  slideshow.cpp: JPG/GIF slideshow and photo viewer from /photos
 *  Board: ESP32-3248S035, ESP32-2432S028
 *  Author: tommyho510@gmail.com
 *
 *  No framebuffer (no PSRAM): JPEGs are decoded MCU by MCU and GIFs line by line
 *  straight to the panel. Both decoder objects live in the media arena, not the heap.
 */
#include "slideshow.h"
#include "app.h"
#include "display.h"
#include "storage.h"
#include <JPEGDEC.h>     // Install "JPEGDEC" with the Library Manager (last tested on v1.8.4)
#include <AnimatedGIF.h> // Install "AnimatedGIF" with the Library Manager (last tested on v2.2.0)
#include <new>

static_assert(sizeof(JPEGDEC) <= MJPEG_BUF_SIZE, "JPEGDEC does not fit in the media arena");
static_assert(sizeof(AnimatedGIF) <= MJPEG_BUF_SIZE, "AnimatedGIF does not fit in the media arena");

static std::vector<String> photos;
static int current = 0;
static bool autoMode = true;
static uint32_t shownAt = 0;

static File photoFile;
static AnimatedGIF *gif = nullptr; // open GIF, placed in arenaPhoto()
static bool gifLoopedOnce = false;
static bool gifStatic = false;     // single-frame GIF: draw once
static int gifFrames = 0;
static uint32_t nextFrameAt = 0;

// GIF scale-to-fit: integer upscale (up) or downscale by dropping lines/pixels (down)
static struct
{
    int up, down, offX, offY;
} gs;
static uint16_t lineBuf[480]; // one output run, never wider than the panel

// ---------------------------------------------------------------- SD callbacks

static void *fileOpen(const char *name, int32_t *size)
{
    photoFile = SD.open(name, FILE_READ);
    if (!photoFile)
        return nullptr;
    *size = photoFile.size();
    return &photoFile;
}

static void fileClose(void *handle)
{
    ((File *)handle)->close();
}

static int32_t jpegRead(JPEGFILE *p, uint8_t *buf, int32_t len)
{
    File *f = (File *)p->fHandle;
    int32_t n = f->read(buf, len);
    p->iPos = f->position();
    return n;
}

static int32_t jpegSeek(JPEGFILE *p, int32_t pos)
{
    File *f = (File *)p->fHandle;
    f->seek(pos);
    p->iPos = pos;
    return pos;
}

static int32_t gifRead(GIFFILE *p, uint8_t *buf, int32_t len)
{
    File *f = (File *)p->fHandle;
    int32_t n = len;
    if ((p->iSize - p->iPos) < len)
        n = p->iSize - p->iPos - 1; // same end-of-file guard as the AnimatedGIF SD examples
    if (n <= 0)
        return 0;
    n = f->read(buf, n);
    p->iPos = f->position();
    return n;
}

static int32_t gifSeek(GIFFILE *p, int32_t pos)
{
    File *f = (File *)p->fHandle;
    f->seek(pos);
    p->iPos = (int32_t)f->position();
    return p->iPos;
}

// ---------------------------------------------------------------- Draw callbacks

static int jpegDraw(JPEGDRAW *d)
{
    gfx->draw16bitBeRGBBitmap(d->x, d->y, d->pPixels, d->iWidth, d->iHeight);
    return 1;
}

static void gifDraw(GIFDRAW *d)
{
    int srcY = d->iY + d->y;
    if (gs.down > 1 && (srcY % gs.down))
        return; // dropped line
    int dstY = gs.offY + (gs.down > 1 ? srcY / gs.down : srcY * gs.up);
    if (dstY >= gfx->height())
        return;
    uint8_t *s = d->pPixels;
    uint16_t *pal = d->pPalette;
    bool transparent = d->ucHasTransparency;
    if (d->ucDisposalMethod == 2) // restore to background
    {
        for (int x = 0; x < d->iWidth; x++)
            if (s[x] == d->ucTransparent)
                s[x] = d->ucBackground;
        transparent = false;
    }
    int x = 0;
    while (x < d->iWidth)
    {
        if (transparent) // skip a transparent run, the previous frame shows through
            while (x < d->iWidth && s[x] == d->ucTransparent)
                x++;
        int start = x, n = 0;
        while (x < d->iWidth && !(transparent && s[x] == d->ucTransparent))
        {
            int srcX = d->iX + x;
            if (gs.down > 1)
            {
                if (srcX % gs.down == 0 && n < 480)
                    lineBuf[n++] = pal[s[x]];
            }
            else
            {
                for (int k = 0; k < gs.up && n < 480; k++)
                    lineBuf[n++] = pal[s[x]];
            }
            x++;
        }
        if (n)
        {
            int srcX = d->iX + start;
            int dstX = gs.offX + (gs.down > 1 ? (srcX + gs.down - 1) / gs.down : srcX * gs.up);
            int rows = gs.down > 1 ? 1 : gs.up;
            for (int k = 0; k < rows; k++)
                gfx->draw16bitRGBBitmap(dstX, dstY + k, lineBuf, n, 1);
        }
    }
}

// ---------------------------------------------------------------- Photo display

static void showError(const char *what, const String &name)
{
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(10, gfx->height() / 2 - 10);
    gfx->printf("%s %s", what, name.c_str());
}

static void closeGif()
{
    if (gif)
    {
        gif->close();
        gif->~AnimatedGIF();
        gif = nullptr;
    }
}

static void drawJpeg(const String &path)
{
    int W = gfx->width(), H = gfx->height();
    JPEGDEC *jpeg = new (arenaPhoto()) JPEGDEC();
    if (jpeg->open(path.c_str(), fileOpen, fileClose, jpegRead, jpegSeek, jpegDraw))
    {
        int w = jpeg->getWidth(), h = jpeg->getHeight(), div = 1;
        while ((w / div > W || h / div > H) && div < 8)
            div *= 2; // JPEGDEC scales by 1/2, 1/4, 1/8 only
        int opt = div == 1 ? 0 : div == 2 ? JPEG_SCALE_HALF : div == 4 ? JPEG_SCALE_QUARTER : JPEG_SCALE_EIGHTH;
        int x = max(0, (W - w / div) / 2), y = max(0, (H - h / div) / 2);
        jpeg->setPixelType(RGB565_BIG_ENDIAN);
        uint32_t t = millis();
        if (!jpeg->decode(x, y, opt))
            LOGF("JPEG decode error %d: %s\n", jpeg->getLastError(), path.c_str());
        LOGF("JPEG %dx%d 1/%d in %lu ms: %s\n", w, h, div, millis() - t, path.c_str());
        jpeg->close();
    }
    else
    {
        LOGF("JPEG open error %d: %s\n", jpeg->getLastError(), path.c_str());
        showError("Cannot show", path);
    }
    jpeg->~JPEGDEC();
}

static void openGif(const String &path)
{
    int W = gfx->width(), H = gfx->height();
    gif = new (arenaPhoto()) AnimatedGIF();
    gif->begin(GIF_PALETTE_RGB565_LE);
    if (!gif->open(path.c_str(), fileOpen, fileClose, gifRead, gifSeek, gifDraw))
    {
        LOGF("GIF open error %d: %s\n", gif->getLastError(), path.c_str());
        gif->~AnimatedGIF();
        gif = nullptr;
        showError("Cannot show", path);
        return;
    }
    int cw = gif->getCanvasWidth(), ch = gif->getCanvasHeight();
    if (cw <= W && ch <= H)
    {
        gs.down = 1;
        gs.up = constrain(min(W / cw, H / ch), 1, GIF_MAX_UPSCALE);
        gs.offX = (W - cw * gs.up) / 2;
        gs.offY = (H - ch * gs.up) / 2;
    }
    else
    {
        gs.up = 1;
        gs.down = max((cw + W - 1) / W, (ch + H - 1) / H);
        gs.offX = (W - cw / gs.down) / 2;
        gs.offY = (H - ch / gs.down) / 2;
    }
    LOGF("GIF %dx%d up %d down %d: %s\n", cw, ch, gs.up, gs.down, path.c_str());
    gifLoopedOnce = false;
    gifStatic = false;
    gifFrames = 0;
    nextFrameAt = millis();
}

static void showPhoto(int i)
{
    closeGif();
    int n = photos.size();
    current = ((i % n) + n) % n;
    String path = String(PHOTO_FOLDER) + "/" + photos[current];
    gfx->fillScreen(RGB565_BLACK);
    if (mediaTypeOf(path) == MEDIA_GIF)
        openGif(path);
    else
        drawJpeg(path);
    shownAt = millis();
}

// Play the next GIF frame when its delay has passed
static void gifTick()
{
    if (!gif || gifStatic || (int32_t)(millis() - nextFrameAt) < 0)
        return;
    int delayMs = 0;
    int r = gif->playFrame(false, &delayMs);
    gifFrames++;
    if (r < 0)
    {
        LOGF("GIF frame error %d\n", gif->getLastError());
        closeGif();
        gifLoopedOnce = true;
        return;
    }
    if (r == 0) // that was the last frame
    {
        gifLoopedOnce = true;
        if (gifFrames == 1)
        {
            gifStatic = true; // still image, nothing more to draw
            return;
        }
        gif->reset();
    }
    nextFrameAt = millis() + (delayMs > 0 ? delayMs : 100); // 0 means "as fast as possible", browsers use 100 ms
}

bool slideshowBegin(const char *path, bool autoAdvance)
{
    slideshowEnd();
    listMedia(PHOTO_FOLDER, isPhoto, photos);
    if (photos.empty())
        return false;
    autoMode = autoAdvance;
    int start = 0;
    if (path && *path)
    {
        String name = path;
        name = name.substring(name.lastIndexOf('/') + 1);
        for (size_t i = 0; i < photos.size(); i++)
            if (photos[i] == name)
                start = i;
    }
    showPhoto(start);
    return true;
}

bool slideshowLoop()
{
    if (photos.empty())
        return true;
    int16_t tx, ty;
    if (touchTapped(tx, ty))
    {
        if (autoMode)
            return true;
        int W = gfx->width();
        if (tx < W / 3)
            showPhoto(current - 1);
        else if (tx > W * 2 / 3)
            showPhoto(current + 1);
        else
            return true;
        return false;
    }
    if (skipRequested) // BOOT button: next photo
    {
        skipRequested = false;
        showPhoto(current + 1);
        return false;
    }
    gifTick();
    if (autoMode && photos.size() > 1)
    {
        uint32_t shown = millis() - shownAt;
        bool due = shown >= SLIDE_INTERVAL_MS;
        if (gif)
            due = (due && gifLoopedOnce) || shown >= GIF_MAX_SHOW_MS; // let a GIF finish one loop
        if (due)
            showPhoto(current + 1);
    }
    return false;
}

void slideshowEnd()
{
    closeGif();
}

int photoCount()
{
    return photos.size();
}
