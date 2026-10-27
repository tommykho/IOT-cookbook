/*
 *  cyd-video-player Version 2026.10A
 *  receive.cpp: Wi-Fi upload (SoftAP, captive portal, QR code, upload handler)
 *  Board: ESP32-3248S035, ESP32-2432S028
 *  Author: tommyho510@gmail.com
 *
 *  The phone joins the AP by scanning a Wi-Fi QR code. Every DNS name resolves to the
 *  device and the OS connectivity probes get a 302, so Android shows "Sign in to network"
 *  and opens the upload page. Uploads are raw POST bodies (faster than multipart parsing
 *  in WebServer), buffered in the arena, written to a temp file, then renamed by type.
 */
#include "receive.h"
#include "app.h"
#include "display.h"
#include "web_ui.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

static const IPAddress apIP(192, 168, 4, 1);
static WebServer *server = nullptr;
static DNSServer *dns = nullptr;
static String ssid, pass;

static lv_obj_t *scr = nullptr, *qr, *ssidLbl, *passLbl, *statusLbl;
static uint32_t lastActivity, lastStatus, doneAt;
static bool cancelReq, doneReq;

// Session results
static int count;
static MediaType lastType;
static String lastPath, firstPhoto;

// Current upload
static File upFile;
static size_t upWritten, upFill, upTotal;
static int upCode;
static String upErr, upName, upStamp;
static MediaType upType;

// ---------------------------------------------------------------- QR screen

static void onCancel(lv_event_t *e) { cancelReq = true; }

static void setStatus(const char *text)
{
    lv_label_set_text(statusLbl, text);
}

static void buildScreen()
{
    int W = gfx->width(), H = gfx->height();
    bool landscape = W > H;
    int qrSize = landscape ? H - 60 : W - 110;
    scr = lv_obj_create(NULL);
    lv_obj_set_flex_flow(scr, landscape ? LV_FLEX_FLOW_ROW : LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(scr, 8, 0);
    lv_obj_set_style_pad_gap(scr, 10, 0);

    qr = lv_qrcode_create(scr);
    lv_qrcode_set_size(qr, qrSize);
    lv_qrcode_set_dark_color(qr, lv_color_black());
    lv_qrcode_set_light_color(qr, lv_color_white());
    lv_obj_set_style_border_color(qr, lv_color_white(), 0); // quiet zone
    lv_obj_set_style_border_width(qr, 6, 0);

    lv_obj_t *col = lv_obj_create(scr);
    lv_obj_remove_style_all(col);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_size(col, landscape ? LV_SIZE_CONTENT : W - 16, landscape ? H - 16 : LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(col, 6, 0);

    lv_obj_t *title = lv_label_create(col);
    lv_label_set_text(title, "Scan with the phone camera");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_width(title, lv_pct(100));
    lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_WRAP);

    ssidLbl = lv_label_create(col);
    passLbl = lv_label_create(col);
    lv_obj_t *hint = lv_label_create(col);
    lv_obj_set_width(hint, lv_pct(100));
    lv_label_set_long_mode(hint, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_text(hint, "Tap \"Sign in to network\", or open http://192.168.4.1");

    statusLbl = lv_label_create(col);
    lv_obj_set_width(statusLbl, lv_pct(100));
    lv_label_set_long_mode(statusLbl, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_color(statusLbl, lv_palette_main(LV_PALETTE_YELLOW), 0);

    lv_obj_t *cancel = lv_button_create(col);
    lv_obj_t *l = lv_label_create(cancel);
    lv_label_set_text(l, LV_SYMBOL_CLOSE " Cancel");
    lv_obj_add_event_cb(cancel, onCancel, LV_EVENT_CLICKED, NULL);
}

// ---------------------------------------------------------------- HTTP handlers

static void redirect()
{
    lastActivity = millis();
    server->sendHeader("Location", "http://192.168.4.1/", true);
    server->send(302, "text/plain", "");
}

static void handleRoot()
{
    lastActivity = millis();
    server->sendHeader("Cache-Control", "no-store");
    server->send_P(200, "text/html", WEB_UI);
    setStatus("Phone connected. Choose files to upload.");
}

static void handleInfo()
{
    char json[160];
    snprintf(json, sizeof(json), "{\"w\":%d,\"h\":%d,\"gifw\":480,\"jpg\":%lu,\"gif\":%lu,\"audio\":%lu,\"mjpeg\":%lu}",
             gfx->width(), gfx->height(), MAX_JPG_BYTES, MAX_GIF_BYTES, MAX_AUDIO_BYTES, MAX_MJPEG_BYTES);
    server->send(200, "application/json", json);
}

static void fail(int code, const char *why)
{
    if (upCode == 200)
    {
        upCode = code;
        upErr = why;
        LOGF("Upload rejected (%d): %s\n", code, why);
    }
    if (upFile)
        upFile.close();
}

// The first bytes must match the extension
static bool magicOk(MediaType t, const uint8_t *b, size_t n)
{
    if (n < 12)
        return false;
    switch (t)
    {
    case MEDIA_JPEG: return b[0] == 0xFF && b[1] == 0xD8 && b[2] == 0xFF;
    case MEDIA_MJPEG: return b[0] == 0xFF && b[1] == 0xD8;
    case MEDIA_GIF: return memcmp(b, "GIF8", 4) == 0;
    case MEDIA_WAV: return memcmp(b, "RIFF", 4) == 0 && memcmp(b + 8, "WAVE", 4) == 0;
    case MEDIA_MP3: return memcmp(b, "ID3", 3) == 0 || (b[0] == 0xFF && (b[1] & 0xE0) == 0xE0);
    default: return false;
    }
}

static void flushUpload()
{
    if (upFill && upFile && upCode == 200)
    {
        if (upFile.write(arenaAudio(), upFill) != upFill)
            fail(507, "SD card full or write error");
    }
    upFill = 0;
}

static void showProgress(bool force)
{
    uint32_t now = millis();
    if (!force && now - lastStatus < 300)
        return;
    lastStatus = now;
    char s[96];
    snprintf(s, sizeof(s), "Receiving %s\n%u%% of %s", upName.c_str(),
             upTotal ? (unsigned)(100ULL * upWritten / upTotal) : 0, formatBytes(upTotal).c_str());
    setStatus(s);
    lvglTask(); // the body is read inside handleClient(), refresh the screen from here
}

static void handleUploadRaw()
{
    HTTPRaw &raw = server->raw();
    lastActivity = millis();
    switch (raw.status)
    {
    case RAW_START:
        upName = sanitizeName(server->arg("name"));
        upStamp = sanitizeName(server->arg("ts"));
        upType = mediaTypeOf(upName);
        upTotal = server->clientContentLength();
        upWritten = upFill = 0;
        upCode = 200;
        upErr = "";
        LOGF("Upload start %s, %u bytes, free heap %lu\n", upName.c_str(), upTotal, ESP.getFreeHeap());
        if (upType == MEDIA_NONE)
            fail(415, "Unsupported file type");
        else if (upTotal == 0 || upTotal > mediaLimit(upType))
            fail(413, "File too large");
        else
        {
            SD.remove(UPLOAD_TMP);
            upFile = SD.open(UPLOAD_TMP, FILE_WRITE);
            if (!upFile)
                fail(500, "Cannot create file on SD card");
        }
        showProgress(true);
        break;
    case RAW_WRITE:
        if (upCode == 200 && upWritten == 0 && !magicOk(upType, raw.buf, raw.currentSize))
            fail(415, "File content does not match its type");
        if (upCode == 200)
        {
            size_t n = raw.currentSize, off = 0;
            while (n) // HTTP_RAW_BUFLEN chunks, written to SD in UPLOAD_WRITE_CHUNK blocks
            {
                size_t take = min(n, (size_t)UPLOAD_WRITE_CHUNK - upFill);
                memcpy(arenaAudio() + upFill, raw.buf + off, take);
                upFill += take;
                off += take;
                n -= take;
                if (upFill == UPLOAD_WRITE_CHUNK)
                    flushUpload();
            }
        }
        upWritten += raw.currentSize;
        showProgress(false);
        break;
    case RAW_END:
        flushUpload();
        if (upFile)
            upFile.close();
        if (upCode == 200)
        {
            String dest = uniquePath(mediaFolder(upType), upName, upStamp);
            if (SD.rename(UPLOAD_TMP, dest))
            {
                count++;
                lastType = upType;
                lastPath = dest;
                if (isPhoto(upType) && firstPhoto.isEmpty())
                    firstPhoto = dest;
                LOGF("Saved %s (%u bytes)\n", dest.c_str(), upWritten);
                char s[96];
                snprintf(s, sizeof(s), "Saved %s (%d file%s)", dest.c_str(), count, count == 1 ? "" : "s");
                setStatus(s);
            }
            else
            {
                fail(500, "Cannot rename file on SD card");
            }
        }
        if (upCode != 200)
            SD.remove(UPLOAD_TMP);
        break;
    case RAW_ABORTED:
        fail(400, "Upload aborted");
        SD.remove(UPLOAD_TMP);
        setStatus("Upload interrupted");
        break;
    }
}

static void handleUploadDone()
{
    if (upCode == 200)
        server->send(200, "application/json", "{\"ok\":true}");
    else
        server->send(upCode, "text/plain", upErr);
}

static void handleDone()
{
    server->send(200, "text/plain", "ok");
    doneReq = true;
    doneAt = millis() + 800; // let the response reach the phone before the AP goes away
    setStatus("Done. Closing Wi-Fi...");
}

// ---------------------------------------------------------------- Session

static String randomPassword()
{
    const char *set = "abcdefghjkmnpqrstuvwxyz23456789"; // no 0/o, 1/l/i: easy to type by hand
    String p;
    for (int i = 0; i < AP_PASS_LEN; i++)
        p += set[esp_random() % strlen(set)]; // true RNG once the radio is on
    return p;
}

void receiveStart()
{
    count = 0;
    lastType = MEDIA_NONE;
    lastPath = firstPhoto = "";
    cancelReq = doneReq = false;
    if (!scr)
        buildScreen();

    WiFi.persistent(false); // nothing written to NVS
    WiFi.mode(WIFI_AP);
    uint8_t mac[6];
    WiFi.softAPmacAddress(mac);
    char id[8];
    snprintf(id, sizeof(id), "%02X%02X", mac[4], mac[5]);
    ssid = String(AP_SSID_PREFIX) + id;
    pass = randomPassword();
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    bool apOk = WiFi.softAP(ssid.c_str(), pass.c_str(), AP_CHANNEL, 0, 2);
    WiFi.setSleep(false); // full throughput while receiving

    dns = new DNSServer();
    dns->setErrorReplyCode(DNSReplyCode::NoError);
    dns->start(53, "*", apIP); // every name resolves to us

    server = new WebServer(80);
    server->on("/", HTTP_GET, handleRoot);
    server->on("/info", HTTP_GET, handleInfo);
    server->on("/upload", HTTP_POST, handleUploadDone, handleUploadRaw);
    server->on("/done", HTTP_POST, handleDone);
    const char *probes[] = {"/generate_204", "/gen_204", "/hotspot-detect.html", "/connecttest.txt",
                            "/ncsi.txt", "/fwlink", "/canonical.html", "/success.txt", "/redirect"};
    for (const char *p : probes)
        server->on(p, redirect); // 302, not 204, so the phone shows "Sign in to network"
    server->onNotFound(redirect);
    server->begin();

    String wifiQr = "WIFI:T:WPA;S:" + ssid + ";P:" + pass + ";;";
    lv_qrcode_update(qr, wifiQr.c_str(), wifiQr.length());
    lv_label_set_text_fmt(ssidLbl, "Wi-Fi: %s", ssid.c_str());
    lv_label_set_text_fmt(passLbl, "Password: %s", pass.c_str());
    setStatus(apOk ? "Waiting for the phone..." : "Wi-Fi AP failed to start");
    lv_screen_load(scr);

    lastActivity = millis();
    LOGF("Receive: AP %s started=%d, free heap %lu\n", ssid.c_str(), apOk, ESP.getFreeHeap());
}

bool receiveLoop()
{
    dns->processNextRequest();
    server->handleClient();
    lvglTask();
    uint32_t now = millis();
    if (now - lastStatus > 1000 && !doneReq) // idle countdown and station count
    {
        lastStatus = now;
        if (WiFi.softAPgetStationNum() == 0 && count == 0)
        {
            uint32_t left = (RECEIVE_IDLE_TIMEOUT_MS - min(RECEIVE_IDLE_TIMEOUT_MS, now - lastActivity)) / 1000;
            char s[64];
            snprintf(s, sizeof(s), "Waiting for the phone... (closes in %lu:%02lu)", left / 60, left % 60);
            setStatus(s);
        }
    }
    if (cancelReq)
        return true;
    if (doneReq && (int32_t)(now - doneAt) >= 0)
        return true;
    if (now - lastActivity > RECEIVE_IDLE_TIMEOUT_MS)
    {
        LOGF("Receive: idle timeout\n");
        return true;
    }
    return false;
}

void receiveStop()
{
    if (server)
    {
        server->stop();
        delete server;
        server = nullptr;
    }
    if (dns)
    {
        dns->stop();
        delete dns;
        dns = nullptr;
    }
    if (upFile)
        upFile.close();
    SD.remove(UPLOAD_TMP);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    LOGF("Receive: stopped, %d files, free heap %lu\n", count, ESP.getFreeHeap());
}

int receiveCount() { return count; }
MediaType receiveLastType() { return lastType; }
String receiveLastPath() { return lastPath; }
String receiveFirstPhotoPath() { return firstPhoto; }
