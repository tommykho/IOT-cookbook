/*
 * gif_source.h
 *
 * Lets gifdec read a GIF from either a filesystem File or a const array in
 * flash. gifdec only ever needs two operations - read N bytes, seek to an
 * absolute offset - so this is all the abstraction required.
 *
 * WHY THIS EXISTS
 *   Wokwi cannot upload files to LittleFS/SPIFFS, and boards without a
 *   microSD slot have no convenient file source either. Embedding the GIF
 *   in flash solves both. This header keeps the same gifdec working for
 *   both cases instead of forking it.
 *
 * WIRING IT INTO gifdec
 *   1. #include "gif_source.h" at the top of gifdec.h
 *   2. In the gd_GIF struct, replace the File/fd member with:  gd_src src;
 *   3. In gifdec.cpp, replace every read  ->  gd_src_read(&gif->src, buf, n)
 *                     replace every seek  ->  gd_src_seek(&gif->src, pos)
 *   4. Add the two open helpers below to gd_open_gif (see gd_open_gif_mem).
 */

#ifndef GIF_SOURCE_H
#define GIF_SOURCE_H

#include <Arduino.h>
#include <FS.h>

typedef struct {
  const uint8_t *mem;   // non-NULL => read from flash
  size_t         len;   // total size of the flash blob
  size_t         pos;   // cursor, flash mode only
  File           file;  // used when mem == NULL
} gd_src;

// ---- construction ----

static inline void gd_src_from_mem(gd_src *s, const uint8_t *data, size_t len) {
  s->mem = data;
  s->len = len;
  s->pos = 0;
}

static inline void gd_src_from_file(gd_src *s, File f) {
  s->mem  = nullptr;
  s->len  = f.size();
  s->pos  = 0;
  s->file = f;
}

// ---- operations ----

static inline size_t gd_src_read(gd_src *s, void *buf, size_t n) {
  if (s->mem) {
    if (s->pos >= s->len) return 0;
    size_t avail = s->len - s->pos;
    if (n > avail) n = avail;
    // memcpy_P matters on AVR; harmless on ESP32 where flash is memory-mapped.
    memcpy_P(buf, s->mem + s->pos, n);
    s->pos += n;
    return n;
  }
  return s->file.read((uint8_t *)buf, n);
}

static inline bool gd_src_seek(gd_src *s, size_t pos) {
  if (s->mem) {
    if (pos > s->len) return false;
    s->pos = pos;
    return true;
  }
  return s->file.seek(pos);
}

static inline size_t gd_src_tell(gd_src *s) {
  return s->mem ? s->pos : s->file.position();
}

static inline void gd_src_close(gd_src *s) {
  if (!s->mem && s->file) s->file.close();
}

#endif  // GIF_SOURCE_H
