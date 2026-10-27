/*
 *  cyd-video-player Version 2026.10A
 *  menu.cpp: LVGL main menu and audio player screens
 *  Board: ESP32-3248S035, ESP32-2432S028
 *  Author: tommyho510@gmail.com
 */
#include "menu.h"
#include "app.h"
#include "audio.h"
#include "display.h"

enum MenuItem
{
    MI_PHOTOS,
    MI_SLIDESHOW,
    MI_VIDEO,
    MI_AUDIO,
    MI_RECEIVE,
    MI_WIFI,     // placeholder: Wi-Fi client
    MI_SETTINGS  // placeholder
};

static lv_obj_t *menuScr, *statusLbl;
static lv_obj_t *audioScr, *trackList, *nowLbl, *playLbl, *volSlider;
static int listedIndex = -2; // track highlighted in the list

// ---------------------------------------------------------------- Main menu

static void onMenu(lv_event_t *e)
{
    switch ((int)(intptr_t)lv_event_get_user_data(e))
    {
    case MI_PHOTOS: appRequestState(STATE_PHOTO_VIEW); break;
    case MI_SLIDESHOW: appRequestState(STATE_SLIDESHOW); break;
    case MI_VIDEO: appRequestState(STATE_VIDEO); break;
    case MI_AUDIO: appRequestState(STATE_AUDIO_PLAYER); break;
    case MI_RECEIVE: appRequestState(STATE_RECEIVE); break;
    }
}

static void makeTile(lv_obj_t *parent, const char *symbol, const char *text, int id, bool enabled, int w, int h)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_size(b, w, h);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *icon = lv_label_create(b);
    lv_label_set_text(icon, symbol);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_28, 0);
    lv_obj_t *label = lv_label_create(b);
    lv_label_set_text(label, text);
    if (enabled)
        lv_obj_add_event_cb(b, onMenu, LV_EVENT_CLICKED, (void *)(intptr_t)id);
    else
        lv_obj_add_state(b, LV_STATE_DISABLED); // greyed-out placeholder
}

static void buildMenu()
{
    int W = gfx->width(), H = gfx->height();
    menuScr = lv_obj_create(NULL);

    lv_obj_t *title = lv_label_create(menuScr);
    lv_label_set_text(title, "CYD Media");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 10, 10);

    statusLbl = lv_label_create(menuScr);
    lv_label_set_long_mode(statusLbl, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(statusLbl, W - 20);
    lv_obj_align(statusLbl, LV_ALIGN_TOP_LEFT, 10, 36);
    lv_label_set_text(statusLbl, "");

    const int top = 58, gap = 8;
    int cols = W > H ? 4 : 3;
    int rows = (7 + cols - 1) / cols;
    int tileW = (W - gap * (cols + 1)) / cols;
    int tileH = (H - top - gap * (rows + 1)) / rows;

    lv_obj_t *grid = lv_obj_create(menuScr);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, W, H - top);
    lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_all(grid, gap, 0);
    lv_obj_set_style_pad_gap(grid, gap, 0);
    lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    makeTile(grid, LV_SYMBOL_IMAGE, "Photos", MI_PHOTOS, true, tileW, tileH);
    makeTile(grid, LV_SYMBOL_PLAY, "Slideshow", MI_SLIDESHOW, true, tileW, tileH);
    makeTile(grid, LV_SYMBOL_VIDEO, "Video", MI_VIDEO, true, tileW, tileH);
    makeTile(grid, LV_SYMBOL_AUDIO, "Audio", MI_AUDIO, true, tileW, tileH);
    makeTile(grid, LV_SYMBOL_DOWNLOAD, "Receive", MI_RECEIVE, true, tileW, tileH);
    makeTile(grid, LV_SYMBOL_WIFI, "Wi-Fi", MI_WIFI, false, tileW, tileH);
    makeTile(grid, LV_SYMBOL_SETTINGS, "Settings", MI_SETTINGS, false, tileW, tileH);
}

void menuShow(const char *status)
{
    lv_label_set_text(statusLbl, status ? status : "");
    lv_screen_load(menuScr);
}

// ---------------------------------------------------------------- Audio player

static void onBack(lv_event_t *e) { appRequestState(STATE_MENU); }
static void onPrev(lv_event_t *e) { audioNext(-1); }
static void onNext(lv_event_t *e) { audioNext(1); }

static void onPlay(lv_event_t *e)
{
    if (audioIsActive())
        audioTogglePause();
    else if (!audioTracks().empty())
        audioPlayIndex(0);
}

static void onTrack(lv_event_t *e)
{
    audioPlayIndex((int)(intptr_t)lv_event_get_user_data(e));
}

static void onVolume(lv_event_t *e)
{
    audioSetVolume(lv_slider_get_value((lv_obj_t *)lv_event_get_target(e)));
}

static lv_obj_t *makeButton(lv_obj_t *parent, const char *text, lv_event_cb_t cb, int w, int h)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_size(b, w, h);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    return b;
}

// Refresh "now playing", the play/pause icon and the list highlight
static void audioUiTimer(lv_timer_t *t)
{
    if (lv_screen_active() != audioScr)
        return;
    int cur = audioCurrentIndex();
    const auto &tracks = audioTracks();
    if (cur >= 0 && cur < (int)tracks.size())
        lv_label_set_text_fmt(nowLbl, "%s %s", audioIsPaused() ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY, tracks[cur].c_str());
    else
        lv_label_set_text(nowLbl, tracks.empty() ? "No tracks" : "Stopped");
    lv_label_set_text(playLbl, audioIsActive() && !audioIsPaused() ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    if (cur != listedIndex)
    {
        for (uint32_t i = 0; i < lv_obj_get_child_count(trackList); i++)
        {
            lv_obj_t *item = lv_obj_get_child(trackList, i);
            if ((int)i == cur)
            {
                lv_obj_add_state(item, LV_STATE_CHECKED);
                lv_obj_scroll_to_view(item, LV_ANIM_ON);
            }
            else
            {
                lv_obj_remove_state(item, LV_STATE_CHECKED);
            }
        }
        listedIndex = cur;
    }
}

static void buildAudio()
{
    int W = gfx->width(), H = gfx->height();
    const int barH = 44, ctlH = 56, pad = 6;
    audioScr = lv_obj_create(NULL);
    lv_obj_remove_flag(audioScr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *back = makeButton(audioScr, LV_SYMBOL_LEFT " Menu", onBack, 100, barH - 2 * pad);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, pad, pad);

    nowLbl = lv_label_create(audioScr);
    lv_label_set_long_mode(nowLbl, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_obj_set_width(nowLbl, W - 100 - 3 * pad);
    lv_obj_align(nowLbl, LV_ALIGN_TOP_RIGHT, -pad, 14);

    trackList = lv_list_create(audioScr);
    lv_obj_set_size(trackList, W - 2 * pad, H - barH - ctlH - pad);
    lv_obj_align(trackList, LV_ALIGN_TOP_MID, 0, barH);

    int bw = 56;
    lv_obj_t *prev = makeButton(audioScr, LV_SYMBOL_PREV, onPrev, bw, ctlH - 2 * pad);
    lv_obj_align(prev, LV_ALIGN_BOTTOM_LEFT, pad, -pad);
    lv_obj_t *play = makeButton(audioScr, LV_SYMBOL_PLAY, onPlay, bw, ctlH - 2 * pad);
    lv_obj_align_to(play, prev, LV_ALIGN_OUT_RIGHT_MID, pad, 0);
    playLbl = lv_obj_get_child(play, 0);
    lv_obj_t *next = makeButton(audioScr, LV_SYMBOL_NEXT, onNext, bw, ctlH - 2 * pad);
    lv_obj_align_to(next, play, LV_ALIGN_OUT_RIGHT_MID, pad, 0);

    lv_obj_t *vol = lv_label_create(audioScr);
    lv_label_set_text(vol, LV_SYMBOL_VOLUME_MAX);
    lv_obj_align_to(vol, next, LV_ALIGN_OUT_RIGHT_MID, 2 * pad, 0);

    volSlider = lv_slider_create(audioScr);
    lv_slider_set_range(volSlider, 0, 100);
    lv_obj_set_width(volSlider, W - 3 * bw - 9 * pad - 24);
    lv_obj_align(volSlider, LV_ALIGN_BOTTOM_RIGHT, -3 * pad, -(ctlH / 2) + 4);
    lv_obj_add_event_cb(volSlider, onVolume, LV_EVENT_VALUE_CHANGED, NULL);

    lv_timer_create(audioUiTimer, 400, NULL);
}

void audioScreenShow()
{
    audioRescan(); // no-op while a track plays
    lv_obj_clean(trackList);
    const auto &tracks = audioTracks();
    for (size_t i = 0; i < tracks.size(); i++)
    {
        lv_obj_t *b = lv_list_add_button(trackList, LV_SYMBOL_AUDIO, tracks[i].c_str());
        lv_obj_add_flag(b, LV_OBJ_FLAG_CHECKABLE);
        lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICK_FOCUSABLE);
        lv_obj_add_event_cb(b, onTrack, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    if (tracks.empty())
        lv_list_add_text(trackList, "No MP3/WAV in " AUDIO_FOLDER ". Use Receive to add some.");
    lv_slider_set_value(volSlider, audioVolume(), LV_ANIM_OFF);
    listedIndex = -2;
    lv_screen_load(audioScr);
    audioUiTimer(nullptr);
}

void menuBegin()
{
    buildMenu();
    buildAudio();
}
